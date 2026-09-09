#include "AIHandler.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>


const static QHash<AIHandler::MsgType, QString> CSHASH_MSGTYPE_STR = 
{
	{ AIHandler::System, "system" },
	{ AIHandler::User, "user" },
	{ AIHandler::Assistant, "assistant" }
};


AIHandler::AIHandler(const QString& apiKey)
	: QObject(nullptr),
	m_apiKey(apiKey),
	m_manager(new QNetworkAccessManager(this))
{
}

AIHandler::~AIHandler()
{
	qDeleteAll(m_contexts);
	m_contexts.clear();
}

void AIHandler::setApiKey(const QString& apiKey)
{
	m_apiKey = apiKey;
}

QString AIHandler::apiKey() const
{
	return m_apiKey;
}

QString AIHandler::imageFileToDataUrl(const QString& filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly))
		return QString();

	const QByteArray data = file.readAll();
	file.close();

	QString mime;
	const QString suffix = QFileInfo(filePath).suffix().toLower();
	if (suffix == "png")
		mime = "image/png";
	else if (suffix == "gif")
		mime = "image/gif";
	else if (suffix == "webp")
		mime = "image/webp";
	else
		mime = "image/jpeg";  // jpg/jpeg 及未知扩展名默认按 jpeg 处理

	return QString("data:%1;base64,%2").arg(mime, QString::fromUtf8(data.toBase64()));
}

void AIHandler::callDeepSeek(const Params& params)
{
	if (m_apiKey.isEmpty())
	{
		emit errorOccured("API Key 为空", "");
		return;
	}

	const QString msgUuid = QUuid::createUuid().toString();

	if (params.stream)
		sendStream(params, m_apiKey, msgUuid);
	else
		sendNonStream(params, m_apiKey, msgUuid);
}

QJsonObject AIHandler::buildRequestJson(const Params& params) const
{
	QJsonObject root;
	root["model"] = params.model;
	root["max_tokens"] = params.maxTokens;
	root["temperature"] = params.temperature;
	root["stream"] = params.stream;

	QJsonObject responseFormat;
	responseFormat["type"] = params.response_format_type.isEmpty() ? "text" : params.response_format_type;
	root["response_format"] = responseFormat;

	// 思考模式：开启时服务端先逐段返回 reasoning_content，再返回 content
	QJsonObject thinking;
	thinking["type"] = params.thinking ? "enabled" : "disabled";
	root["thinking"] = thinking;
	if (params.thinking)
		root["reasoning_effort"] = params.reasoningEffort;

	QJsonArray messages;
	for (const auto& msg : params.msgs)
	{
		QJsonObject one;
		one["role"] = CSHASH_MSGTYPE_STR.value(msg.role);

		if (msg.images.isEmpty())
		{
			// 纯文本消息：content 直接是字符串
			one["content"] = msg.content;
		}
		else
		{
			// 图像消息：content 变为块数组（text + image_url...），仅 user 消息合法
			QJsonArray contentBlocks;
			if (!msg.content.isEmpty())
			{
				QJsonObject textBlock;
				textBlock["type"] = "text";
				textBlock["text"] = msg.content;
				contentBlocks.append(textBlock);
			}
			for (const QString& image : msg.images)
			{
				QJsonObject imageBlock;
				imageBlock["type"] = "image_url";
				QJsonObject imageUrlObj;
				imageUrlObj["url"] = image;
				if (!msg.imageDetail.isEmpty())
					imageUrlObj["detail"] = msg.imageDetail;
				imageBlock["image_url"] = imageUrlObj;
				contentBlocks.append(imageBlock);
			}
			one["content"] = contentBlocks;
		}

		messages.append(one);
	}
	root["messages"] = messages;

	return root;
}

void AIHandler::sendNonStream(const Params& params, const QString& effectiveKey, const QString& msgUuid)
{
	QUrl url(params.baseUrl + "/chat/completions");
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + effectiveKey.toUtf8());

	QJsonObject root = buildRequestJson(params);
	root["stream"] = false;

	QJsonDocument doc(root);
	QByteArray body = doc.toJson(QJsonDocument::Compact);

	QNetworkReply* reply = m_manager->post(request, body);

	auto* ctx = new RequestContext;
	ctx->msgUuid = msgUuid;
	ctx->stream = false;
	m_contexts.insert(reply, ctx);

	connect(reply, &QNetworkReply::finished, this, &AIHandler::onReplyFinished);
}

void AIHandler::sendStream(const Params& params, const QString& effectiveKey, const QString& msgUuid)
{
	QUrl url(params.baseUrl + "/chat/completions");
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + effectiveKey.toUtf8());

	QJsonObject root = buildRequestJson(params);
	root["stream"] = true;

	QJsonDocument doc(root);
	QByteArray body = doc.toJson(QJsonDocument::Compact);

	QNetworkReply* reply = m_manager->post(request, body);

	static int ii = 0;
	qDebug() << ++ii;

	auto* ctx = new RequestContext;
	ctx->msgUuid = msgUuid;
	ctx->stream = true;
	m_contexts.insert(reply, ctx);

	connect(reply, &QNetworkReply::readyRead, this, &AIHandler::onReplyReadyRead);
	connect(reply, &QNetworkReply::finished, this, &AIHandler::onReplyFinished);
}

void AIHandler::onReplyReadyRead()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply || !m_contexts.contains(reply))
		return;

	RequestContext* ctx = m_contexts.value(reply);
	if (!ctx || !ctx->stream)
		return;

	ctx->buffer.append(reply->readAll());

	while (true)
	{
		int newlinePos = ctx->buffer.indexOf('\n');
		if (newlinePos < 0)
			break;

		QByteArray line = ctx->buffer.left(newlinePos);
		ctx->buffer.remove(0, newlinePos + 1);

		line = line.trimmed();
		if (line.isEmpty())
			continue;

		QByteArray payload;
		if (line.startsWith("data:"))
			payload = line.mid(5).trimmed();
		else
			payload = line;

		if (payload == "[DONE]")
		{
			if (!ctx->completed)
			{
				ctx->completed = true;
				emit streamComplete(ctx->msgUuid);
			}
			continue;
		}

		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
		if (err.error != QJsonParseError::NoError || !doc.isObject())
			continue;

		QJsonObject obj = doc.object();
		QJsonArray choices = obj.value("choices").toArray();
		if (choices.isEmpty())
			continue;

		QJsonObject choice0 = choices.at(0).toObject();
		QJsonObject delta = choice0.value("delta").toObject();
		QString chunk = delta.value("content").toString();

		if (!chunk.isEmpty())
		{
			emit streamChunk(chunk, ctx->msgUuid);
		}
		else
		{
			// 思考阶段：delta 里只有 reasoning_content
			QString thinking = delta.value("reasoning_content").toString();
			if (!thinking.isEmpty())
				emit thinkingChunk(thinking, ctx->msgUuid);
		}
	}
}

void AIHandler::onReplyFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply || !m_contexts.contains(reply))
		return;

	RequestContext* ctx = m_contexts.take(reply);

	QByteArray raw = reply->readAll();
	reply->deleteLater();

	if (!ctx)
		return;

	if (reply->error() != QNetworkReply::NoError)
	{
		emit errorOccured("网络错误: " + reply->errorString(), ctx->msgUuid);
		delete ctx;
		return;
	}

	if (!ctx->stream)
	{
		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
		if (err.error != QJsonParseError::NoError)
		{
			emit errorOccured("JSON 解析失败: " + err.errorString(), ctx->msgUuid);
			delete ctx;
			return;
		}

		if (!doc.isObject())
		{
			emit errorOccured("返回内容不是对象", ctx->msgUuid);
			delete ctx;
			return;
		}

		QJsonObject obj = doc.object();
		QJsonArray choices = obj.value("choices").toArray();
		if (choices.isEmpty())
		{
			emit errorOccured("返回里没有 choices", ctx->msgUuid);
			delete ctx;
			return;
		}

		QJsonObject choice0 = choices.at(0).toObject();
		QJsonObject message = choice0.value("message").toObject();
		QString replyText = message.value("content").toString();

		// 非流式：思考内容随完整 message 一起返回
		QString thinkingText = message.value("reasoning_content").toString();
		if (!thinkingText.isEmpty())
			emit thinkingReady(thinkingText);

		emit replyReady(replyText);
	}
	else
	{
		// 流式模式下，如果 [DONE] 没提前来，这里补一个完成通知
		if (!ctx->completed)
			emit streamComplete(ctx->msgUuid);
	}

	delete ctx;
}