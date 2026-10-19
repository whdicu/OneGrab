#include "AIHandler.h"

#include <QBuffer>
#include <QPixmap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>


// 把 reply 和本次搜索调用的 uuid 关联起来的动态属性名
const static char* C_SEARCH_UUID_PROPERTY = "searchUuid";

// 提供给模型的联网搜索工具名
const static char* C_WEB_SEARCH_TOOL = "web_search";

const static QHash<AIHandler::MsgType, QString> CSHASH_MSGTYPE_STR = 
{
	{ AIHandler::System, "system" },
	{ AIHandler::User, "user" },
	{ AIHandler::Assistant, "assistant" },
	{ AIHandler::Tool, "tool" }
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

	// 还在等搜索结果的上下文不在 m_contexts 里，这里单独回收
	for (auto it = m_toolSearches.begin(); it != m_toolSearches.end(); ++it)
		delete it.value().ctx;
	m_toolSearches.clear();
}

void AIHandler::setApiKey(const QString& apiKey)
{
	m_apiKey = apiKey;
}

QString AIHandler::apiKey() const
{
	return m_apiKey;
}

QString AIHandler::imageFileToDataUrl(const QPixmap& pixmap)
{
	if (pixmap.isNull())
		return QString();

	QByteArray data;
	QBuffer buffer(&data);
	if (!buffer.open(QIODevice::WriteOnly))
		return QString();

	if (!pixmap.save(&buffer, "PNG"))
		return QString();
	buffer.close();

	return QString("data:%1;base64,%2").arg("image/png", QString::fromUtf8(data.toBase64()));
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

void AIHandler::callBalance(const QString& baseUrl)
{
	if (m_apiKey.isEmpty())
	{
		emit errorOccured(tr("API Key 为空"), "");
		return;
	}

	// 余额接口挂在根地址下、不在 /v1 里（Java 版也是单独用 createBase 拼根地址）：
	// chat 是 https://api.deepseek.com/v1，余额是 https://api.deepseek.com/user/balance，
	// 所以这里把 chat 地址结尾的版本段（v1 / v2 ...）去掉再拼。baseUrl 为空时取和 chat 一样的默认地址。
	QString base = baseUrl.isEmpty() ? Params().baseUrl : baseUrl;
	while (base.endsWith('/'))
		base.chop(1);

	const QString tail = base.section('/', -1);
	if (tail.startsWith('v') && tail.mid(1).toInt() > 0)
		base.chop(tail.length() + 1);

	QNetworkRequest request(QUrl(base + "/user/balance"));
	request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

	// 余额请求不进 m_contexts（它不是聊天请求，也没有 msgUuid），自己一个槽收尾
	QNetworkReply* reply = m_manager->get(request);
	connect(reply, &QNetworkReply::finished, this, &AIHandler::onBalanceReplyFinished);
}

void AIHandler::callSearch(const SearchParams& params)
{
	if (m_apiKey.isEmpty())
	{
		emit errorOccured(tr("API Key 为空"), "");
		return;
	}

	if (params.query.trimmed().isEmpty())
	{
		emit errorOccured(tr("搜索词为空"), "");
		return;
	}

	QString base = params.baseUrl.trimmed().isEmpty() ? Params().baseUrl : params.baseUrl.trimmed();
	while (base.endsWith('/'))
		base.chop(1);

	QJsonObject root;
	root["query"] = params.query;
	root["count"] = qBound(1, params.count, 50);
	root["need_content"] = params.needContent;
	if (!params.oriQuestion.isEmpty())
		root["ori_question"] = params.oriQuestion;
	if (!params.localization.isEmpty())
		root["localization"] = params.localization;

	QNetworkRequest request(QUrl(base + "/tools/search"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

	QNetworkReply* reply = m_manager->post(request, QJsonDocument(root).toJson(QJsonDocument::Compact));

	// 搜索和聊天请求的解析逻辑不同，不进 m_contexts；uuid 挂在 reply 上，供回调取回
	reply->setProperty(C_SEARCH_UUID_PROPERTY, QUuid::createUuid().toString());
	connect(reply, &QNetworkReply::finished, this, &AIHandler::onSearchReplyFinished);
}

void AIHandler::callDeepSeek(const Params& params)
{
	if (m_apiKey.isEmpty())
	{
		emit errorOccured(tr("API Key 为空"), "");
		return;
	}

	auto* ctx = new RequestContext;
	ctx->msgUuid = QUuid::createUuid().toString();
	ctx->stream = params.stream;
	ctx->params = params;
	ctx->msgs = params.msgs;   // 工作副本：工具调用产生的消息只追加在这里
	ctx->baseCount = ctx->msgs.size();

	dispatchRequest(ctx);
}

QJsonObject AIHandler::buildRequestJson(const Params& params, const QList<ChatMessage>& msgs, bool withTools) const
{
	QJsonObject root;
	root["model"] = params.model;
	//root["max_tokens"] = params.maxTokens;
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

	// 联网搜索：只提供工具，搜不搜由模型自己决定
	if (withTools)
	{
		QJsonArray tools;
		tools.append(buildWebSearchTool());
		root["tools"] = tools;
	}

	QJsonArray messages;
	for (const auto& msg : msgs)
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

		// 带 tools 时 reasoning_content 是上下文的一部分：漏回传服务端直接 400。
		// 调用方没存（比如自己拼的历史）就传空串，至少保证字段存在
		if (msg.role == Assistant && withTools)
			one["reasoning_content"] = msg.reasoningContent;

		// 模型这一轮发起的工具调用要原样放回历史，tool 消息才有东西可对应
		if (msg.role == Assistant && !msg.toolCalls.isEmpty())
		{
			QJsonArray toolCalls;
			for (const ToolCall& call : msg.toolCalls)
			{
				QJsonObject function;
				function["name"] = call.name;
				function["arguments"] = call.arguments;

				QJsonObject oneCall;
				oneCall["id"] = call.id;
				oneCall["type"] = "function";
				oneCall["function"] = function;
				toolCalls.append(oneCall);
			}
			one["tool_calls"] = toolCalls;
		}

		// 工具结果消息：用 tool_call_id 对应上面某次调用
		if (msg.role == Tool)
			one["tool_call_id"] = msg.toolCallId;

		messages.append(one);
	}
	root["messages"] = messages;

	return root;
}

void AIHandler::dispatchRequest(RequestContext* ctx)
{
	if (!ctx)
		return;

	// 轮数用完就不再给工具，避免模型无限搜下去
	const bool withTools = ctx->params.webSearch && ctx->toolRounds < ctx->params.maxToolRounds;

	QJsonObject root = buildRequestJson(ctx->params, ctx->msgs, withTools);
	root["stream"] = ctx->stream;

	QNetworkRequest request(QUrl(ctx->params.baseUrl + "/chat/completions"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

	QNetworkReply* reply = m_manager->post(request, QJsonDocument(root).toJson(QJsonDocument::Compact));

	// 重新发起子请求前把「本轮」状态清干净，上下文 msgs 保留
	ctx->buffer.clear();
	ctx->assistantContent.clear();
	ctx->assistantThinking.clear();
	ctx->roundToolCalls.clear();

	m_contexts.insert(reply, ctx);
	if (ctx->stream)
		connect(reply, &QNetworkReply::readyRead, this, &AIHandler::onReplyReadyRead);
	connect(reply, &QNetworkReply::finished, this, &AIHandler::onReplyFinished);
}

QJsonObject AIHandler::buildWebSearchTool()
{
	QJsonObject queryProp;
	queryProp["type"] = "string";
	queryProp["description"] = tr("搜索词，1~128 个字符；支持 site: / -site: / lang: / after: / before: 等搜索语法");

	QJsonObject countProp;
	countProp["type"] = "integer";
	countProp["description"] = tr("返回条数，1~10，默认 8");
	countProp["minimum"] = 1;
	countProp["maximum"] = 10;

	QJsonObject properties;
	properties["query"] = queryProp;
	properties["count"] = countProp;

	QJsonArray required;
	required.append("query");

	QJsonObject schema;
	schema["type"] = "object";
	schema["properties"] = properties;
	schema["required"] = required;

	QJsonObject function;
	function["name"] = C_WEB_SEARCH_TOOL;
	function["description"] = tr("联网搜索实时网页信息。当问题涉及最新新闻、实时数据、近期事件、你不确定的事实，"
		"或需要给出可引用的来源时调用；闲聊、已知常识和自己的推理不需要调用。");
	function["parameters"] = schema;

	QJsonObject tool;
	tool["type"] = "function";
	tool["function"] = function;
	return tool;
}

QList<AIHandler::ToolCall> AIHandler::parseToolCalls(const QJsonArray& array)
{
	QList<ToolCall> calls;
	for (const QJsonValue& value : array)
	{
		const QJsonObject one = value.toObject();
		const QJsonObject function = one.value("function").toObject();

		ToolCall call;
		call.id = one.value("id").toString();
		call.name = function.value("name").toString();
		call.arguments = function.value("arguments").toString();
		calls.append(call);
	}
	return calls;
}

void AIHandler::appendToolCallDeltas(QList<ToolCall>& calls, const QJsonArray& deltas)
{
	// 流式下 tool_calls 是增量：每个 index 一条，arguments 要按 index 拼接
	for (const QJsonValue& value : deltas)
	{
		const QJsonObject one = value.toObject();
		const int index = one.value("index").toInt();
		if (index < 0)
			continue;

		while (calls.size() <= index)
			calls.append(ToolCall());

		ToolCall& call = calls[index];
		const QString id = one.value("id").toString();
		if (!id.isEmpty())
			call.id = id;

		const QJsonObject function = one.value("function").toObject();
		const QString name = function.value("name").toString();
		if (!name.isEmpty())
			call.name = name;

		call.arguments += function.value("arguments").toString();
	}
}

QString AIHandler::parseApiErrorMessage(const QByteArray& raw)
{
	const QJsonDocument doc = QJsonDocument::fromJson(raw);
	if (doc.isObject())
		return doc.object().value("error").toObject().value("message").toString();
	return QString();
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
	processStreamBuffer(ctx, false);
}

void AIHandler::processStreamBuffer(RequestContext* ctx, bool atEnd)
{
	if (!ctx)
		return;

	while (true)
	{
		const int newlinePos = ctx->buffer.indexOf('\n');
		if (newlinePos < 0)
		{
			if (!atEnd)
				break;

			// 流结束时缓冲区可能还剩最后一行、且服务端没补换行，丢了就可能丢 tool_calls 的尾巴
			const QByteArray tail = ctx->buffer.trimmed();
			ctx->buffer.clear();
			if (!tail.isEmpty())
				processStreamPayload(ctx, tail);
			break;
		}

		const QByteArray line = ctx->buffer.left(newlinePos).trimmed();
		ctx->buffer.remove(0, newlinePos + 1);

		if (line.isEmpty())
			continue;

		processStreamPayload(ctx, line.startsWith("data:") ? line.mid(5).trimmed() : line);
	}
}

void AIHandler::processStreamPayload(RequestContext* ctx, const QByteArray& payload)
{
	// [DONE] 只是这一轮子请求结束；要不要收尾/转去执行工具，由 onReplyFinished 判断
	if (payload == "[DONE]")
		return;

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return;

	const QJsonArray choices = doc.object().value("choices").toArray();
	if (choices.isEmpty())
		return;

	const QJsonObject delta = choices.at(0).toObject().value("delta").toObject();

	// 正文和思考内容可能同时出现，分开处理，不要用 if/else
	const QString chunk = delta.value("content").toString();
	if (!chunk.isEmpty())
	{
		ctx->assistantContent += chunk;
		emit streamChunk(ctx->msgUuid, chunk);
	}

	const QString thinking = delta.value("reasoning_content").toString();
	if (!thinking.isEmpty())
	{
		ctx->assistantThinking += thinking;
		emit thinkingChunk(ctx->msgUuid, thinking);
	}

	appendToolCallDeltas(ctx->roundToolCalls, delta.value("tool_calls").toArray());
}

void AIHandler::onReplyFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply || !m_contexts.contains(reply))
		return;

	RequestContext* ctx = m_contexts.take(reply);

	const QByteArray raw = reply->readAll();
	const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	const QNetworkReply::NetworkError networkError = reply->error();
	const QString errorString = reply->errorString();
	reply->deleteLater();

	if (!ctx)
		return;

	// 流式收尾：把没带换行的最后一行也吃掉
	if (ctx->stream)
	{
		ctx->buffer.append(raw);
		processStreamBuffer(ctx, true);
	}

	const int statusCode = status.isValid() ? status.toInt() : 0;
	if (statusCode != 0 && statusCode != 200)
	{
		QString message = parseApiErrorMessage(raw);
		if (message.isEmpty())
			message = errorString;
		emit errorOccured(tr("API 错误(%1): %2").arg(statusCode).arg(message), ctx->msgUuid);
		delete ctx;
		return;
	}

	if (networkError != QNetworkReply::NoError)
	{
		emit errorOccured(tr("网络错误: %1").arg(errorString), ctx->msgUuid);
		delete ctx;
		return;
	}

	if (!ctx->stream)
	{
		QJsonParseError err;
		const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
		if (err.error != QJsonParseError::NoError || !doc.isObject())
		{
			emit errorOccured(tr("JSON 解析失败: %1").arg(err.errorString()), ctx->msgUuid);
			delete ctx;
			return;
		}

		const QJsonArray choices = doc.object().value("choices").toArray();
		if (choices.isEmpty())
		{
			emit errorOccured(tr("返回里没有 choices"), ctx->msgUuid);
			delete ctx;
			return;
		}

		const QJsonObject message = choices.at(0).toObject().value("message").toObject();
		ctx->assistantContent = message.value("content").toString();
		ctx->assistantThinking = message.value("reasoning_content").toString();
		ctx->roundToolCalls = parseToolCalls(message.value("tool_calls").toArray());

		// 非流式：思考内容随完整 message 一起返回
		if (!ctx->assistantThinking.isEmpty())
			emit thinkingReady(ctx->assistantThinking);
	}

	// 模型要调工具：先把这一轮塞回历史，执行完再请求；对外仍然只是同一次调用
	if (!ctx->roundToolCalls.isEmpty())
	{
		handleToolCalls(ctx);
		return;
	}

	// 把最终回答也补进本轮新增消息，然后一次性交给调用方存历史。
	// 中间的工具往返也在这里面：下次请求带 tools 时 reasoning_content 必须回传
	ChatMessage assistant;
	assistant.role = Assistant;
	assistant.content = ctx->assistantContent;
	assistant.reasoningContent = ctx->assistantThinking;
	ctx->msgs.append(assistant);

	emit turnMessagesReady(ctx->msgs.mid(ctx->baseCount), ctx->msgUuid);

	if (ctx->stream)
		emit streamComplete(ctx->msgUuid);
	else
		emit replyReady(ctx->assistantContent, ctx->msgUuid);

	delete ctx;
}

void AIHandler::handleToolCalls(RequestContext* ctx)
{
	// 带 tools 的请求必须把 assistant 的思考 + 正文 + tool_calls 原样回传，
	// 少了 reasoning_content 服务端会直接 400（见官方 thinking_mode 文档）
	ChatMessage assistant;
	assistant.role = Assistant;
	assistant.content = ctx->assistantContent;
	assistant.reasoningContent = ctx->assistantThinking;
	assistant.toolCalls = ctx->roundToolCalls;
	ctx->msgs.append(assistant);

	ctx->pendingToolCalls = ctx->roundToolCalls;
	ctx->roundToolCalls.clear();

	++ctx->toolRounds;
	runNextToolCall(ctx);
}

void AIHandler::runNextToolCall(RequestContext* ctx)
{
	if (!ctx)
		return;

	if (ctx->pendingToolCalls.isEmpty())
	{
		// 工具结果都补齐了，带着历史继续请求，让模型给最终答案
		dispatchRequest(ctx);
		return;
	}

	const ToolCall call = ctx->pendingToolCalls.takeFirst();

	if (call.name != C_WEB_SEARCH_TOOL)
	{
		ChatMessage toolMsg;
		toolMsg.role = Tool;
		toolMsg.toolCallId = call.id;
		toolMsg.content = tr("未知工具: %1").arg(call.name);
		ctx->msgs.append(toolMsg);
		runNextToolCall(ctx);
		return;
	}

	sendToolSearch(ctx, call);
}

void AIHandler::sendToolSearch(RequestContext* ctx, const ToolCall& call)
{
	QJsonParseError argError;
	const QJsonDocument argDoc = QJsonDocument::fromJson(call.arguments.toUtf8(), &argError);
	const QJsonObject args = (argError.error == QJsonParseError::NoError && argDoc.isObject())
		? argDoc.object()
		: QJsonObject();

	const QString query = args.value("query").toString().trimmed();
	const int count = qBound(1, args.value("count").isDouble() ? args.value("count").toInt() : 8, 10);

	if (query.isEmpty())
	{
		ChatMessage toolMsg;
		toolMsg.role = Tool;
		toolMsg.toolCallId = call.id;
		toolMsg.content = tr("搜索词为空，请换个 query 再试");
		ctx->msgs.append(toolMsg);
		runNextToolCall(ctx);
		return;
	}

	emit webSearchStarted(query, ctx->msgUuid);

	QString base = ctx->params.baseUrl.trimmed().isEmpty() ? Params().baseUrl : ctx->params.baseUrl.trimmed();
	while (base.endsWith('/'))
		base.chop(1);

	QJsonObject root;
	root["query"] = query;
	root["count"] = count;
	root["need_content"] = false;   // 模型读 summary 就够，正文太长且额外占上下文

	// 最近的用户提问能帮搜索引擎理解意图
	for (int i = ctx->msgs.size() - 1; i >= 0; --i)
	{
		if (ctx->msgs.at(i).role != User)
			continue;

		const QString question = ctx->msgs.at(i).content.trimmed();
		if (!question.isEmpty())
			root["ori_question"] = question.left(128);
		break;
	}

	QNetworkRequest request(QUrl(base + "/tools/search"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

	QNetworkReply* reply = m_manager->post(request, QJsonDocument(root).toJson(QJsonDocument::Compact));

	PendingToolSearch pending;
	pending.ctx = ctx;
	pending.toolCallId = call.id;
	m_toolSearches.insert(reply, pending);

	connect(reply, &QNetworkReply::finished, this, &AIHandler::onToolSearchFinished);
}

void AIHandler::onToolSearchFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply || !m_toolSearches.contains(reply))
		return;

	const PendingToolSearch pending = m_toolSearches.take(reply);

	const QByteArray raw = reply->readAll();
	const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	const QString errorString = reply->errorString();
	reply->deleteLater();

	QString content;
	if (!status.isValid())
	{
		content = tr("搜索失败：网络错误 %1").arg(errorString);
	}
	else if (status.toInt() != 200)
	{
		QString message = parseApiErrorMessage(raw);
		if (message.isEmpty())
			message = errorString;
		content = tr("搜索失败(%1)：%2").arg(status.toInt()).arg(message);
	}
	else
	{
		const QJsonDocument doc = QJsonDocument::fromJson(raw);
		const QJsonArray items = doc.object().value("results").toArray();

		QList<SearchResult> results;
		for (const QJsonValue& value : items)
		{
			const QJsonObject one = value.toObject();

			SearchResult result;
			result.rerankScore = one.value("rerank_score").toDouble();
			result.title = one.value("title").toString();
			result.url = one.value("url").toString();
			result.snippet = one.value("snippet").toString();
			result.summary = one.value("summary").toString();
			result.content = one.value("content").toString();
			result.siteName = one.value("site_name").toString();
			result.siteIcon = one.value("site_icon").toString();
			result.date = one.value("date").toString();
			result.authority = one.value("authority").isDouble() ? one.value("authority").toInt() : -1;
			results.append(result);
		}

		if (results.isEmpty())
			content = tr("没有搜索到相关结果");
		else
			content = QString::fromUtf8(QJsonDocument(buildSearchResultJson(results)).toJson(QJsonDocument::Compact));
	}

	// 不管成功失败都把结果当 tool 消息回给模型，让它自己决定怎么答（搜索挂了也能继续对话）
	ChatMessage toolMsg;
	toolMsg.role = Tool;
	toolMsg.toolCallId = pending.toolCallId;
	toolMsg.content = content;
	pending.ctx->msgs.append(toolMsg);

	runNextToolCall(pending.ctx);
}

QJsonObject AIHandler::buildSearchResultJson(const QList<SearchResult>& results)
{
	QJsonArray items;
	for (const SearchResult& result : results)
	{
		QJsonObject one;
		one["title"] = result.title;
		one["url"] = result.url;
		// 长摘要更适合模型阅读，没有时退回短摘要
		one["summary"] = result.summary.isEmpty() ? result.snippet : result.summary;
		if (!result.siteName.isEmpty())
			one["site_name"] = result.siteName;
		if (!result.date.isEmpty())
			one["date"] = result.date;
		items.append(one);
	}

	QJsonObject root;
	root["results"] = items;
	return root;
}

void AIHandler::onBalanceReplyFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	const QByteArray raw = reply->readAll();
	const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	reply->deleteLater();

	// 非 2xx 时 QNetworkReply 也会带错误码，这里优先报 HTTP 状态码
	// （对齐 Java 版 onError("API错误: " + response.code())）
	if (status.isValid() && status.toInt() != 200)
	{
		emit errorOccured(tr("API 错误: %1").arg(status.toInt()), "");
		return;
	}

	// 压根没拿到状态码，那就是连不上了
	if (!status.isValid() && reply->error() != QNetworkReply::NoError)
	{
		emit errorOccured(tr("网络错误: %1").arg(reply->errorString()), "");
		return;
	}

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		emit errorOccured(tr("JSON 解析失败: %1").arg(err.errorString()), "");
		return;
	}

	const QJsonObject obj = doc.object();

	Balance balance;
	balance.isAvailable = obj.value("is_available").toBool();

	const QJsonArray infos = obj.value("balance_infos").toArray();
	for (const QJsonValue& value : infos)
	{
		const QJsonObject one = value.toObject();

		BalanceInfo info;
		info.currency = one.value("currency").toString();
		info.totalBalance = one.value("total_balance").toString();
		info.grantedBalance = one.value("granted_balance").toString();
		info.toppedUpBalance = one.value("topped_up_balance").toString();
		balance.infos.append(info);
	}

	emit balanceReady(balance);
}

void AIHandler::onSearchReplyFinished()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	const QString searchUuid = reply->property(C_SEARCH_UUID_PROPERTY).toString();
	const QByteArray raw = reply->readAll();
	const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	const QString networkError = reply->errorString();
	reply->deleteLater();

	if (!status.isValid())
	{
		emit errorOccured(tr("网络错误: %1").arg(networkError), searchUuid);
		return;
	}

	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		// 拿不到响应体时至少把状态码报出去
		emit errorOccured(tr("搜索失败(%1): JSON 解析失败 %2").arg(status.toInt()).arg(err.errorString()), searchUuid);
		return;
	}

	const QJsonObject obj = doc.object();

	// 400/401/402/403/404/408/413/429/500/504 都带 OpenAI 兼容的 error 对象，优先用它里面的 message
	if (status.toInt() != 200)
	{
		QString message = obj.value("error").toObject().value("message").toString();
		if (message.isEmpty())
			message = tr("未知错误");
		emit errorOccured(tr("搜索失败(%1): %2").arg(status.toInt()).arg(message), searchUuid);
		return;
	}

	QList<SearchResult> results;
	const QJsonArray items = obj.value("results").toArray();
	for (const QJsonValue& value : items)
	{
		const QJsonObject one = value.toObject();

		SearchResult result;
		result.rerankScore = one.value("rerank_score").toDouble();
		result.title = one.value("title").toString();
		result.url = one.value("url").toString();
		result.snippet = one.value("snippet").toString();
		result.summary = one.value("summary").toString();
		result.content = one.value("content").toString();
		result.siteName = one.value("site_name").toString();
		result.siteIcon = one.value("site_icon").toString();
		result.date = one.value("date").toString();
		result.authority = one.value("authority").isDouble() ? one.value("authority").toInt() : -1;
		results.append(result);
	}

	emit searchReady(results, searchUuid);
}