#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class AIHandler : public QObject
{
	Q_OBJECT

public:
	enum MsgType
	{
		System,
		User,
		Assistant
	};

	struct ChatMessage
	{
		MsgType role;
		QString content;

		// 图像输入（仅 user 消息生效）。每项为 http(s) URL，或 data:image/...;base64,... 形式；
		// 为空表示纯文本消息。非空时 content 会自动转成「text + image_url」块数组。
		QStringList images;
		// image_url 的 detail 字段：low / high / original / auto；空字符串表示不发送 detail。
		QString imageDetail;
	};

	struct Params
	{
		QString model;
		QList<ChatMessage> msgs;
		QString response_format_type = "text";
		bool stream = false;
		int maxTokens = 1024;
		double temperature = 1.0;
		QString baseUrl = "https://api.deepseek.com/v1";

		// 思考模式开关：true -> thinking.type=enabled（先输出思考内容再回答）
		bool thinking = false;
		// 思考强度：low / high / max（仅在 thinking=true 时随请求发送）
		QString reasoningEffort = "high";
	};

public:
	explicit AIHandler(const QString& apiKey);
	~AIHandler();

	AIHandler(const AIHandler&) = delete;
	AIHandler& operator=(const AIHandler&) = delete;

	// 你原本没有这些接口，我这里新增，不影响原有接口
	void setApiKey(const QString& apiKey);
	QString apiKey() const;

	// 这就是 Qt 版的“调用 AI”
	void callDeepSeek(const Params& params);

	// 把本地图片文件读成 base64 data URL（按扩展名推断 mime，未知默认 image/jpeg）。
	// 读取失败返回空字符串。结果可直接填入 ChatMessage::images。
	static QString imageFileToDataUrl(const QString& filePath);

signals:
	// 非流式：一次性返回完整答案
	void replyReady(const QString& reply);

	// 流式：每来一小段就发一次
	void streamChunk(const QString& chunk, const QString& msgUuid);

	// 流式结束
	void streamComplete(const QString& msgUuid);

	// 思考内容（非流式：一次性返回完整思考文本）
	void thinkingReady(const QString& thinking);

	// 思考内容（流式：逐段到达）
	void thinkingChunk(const QString& chunk, const QString& msgUuid);

	// 出错
	void errorOccured(const QString& errorMessage, const QString& msgUuid);

private slots:
	void onReplyReadyRead();
	void onReplyFinished();

private:
	struct RequestContext
	{
		QString msgUuid;
		bool stream = false;
		QByteArray buffer;
		bool completed = false;
	};

	void sendNonStream(const Params& params, const QString& effectiveKey, const QString& msgUuid);
	void sendStream(const Params& params, const QString& effectiveKey, const QString& msgUuid);

	QJsonObject buildRequestJson(const Params& params) const;

private:
	QString m_apiKey;
	QNetworkAccessManager* m_manager;

	// 用 reply 指针找到它对应的上下文
	QHash<QNetworkReply*, RequestContext*> m_contexts;
};