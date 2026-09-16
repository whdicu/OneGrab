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

class QPixmap;

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

	// 余额查询（GET /user/balance）返回的单个币种余额。
	// 金额服务端给的就是字符串（如 "110.00"），这里原样保留，不做数值转换。
	struct BalanceInfo
	{
		QString currency;         // CNY / USD
		QString totalBalance;     // 总余额（赠金 + 充值）
		QString grantedBalance;   // 赠金余额
		QString toppedUpBalance;  // 充值余额
	};

	struct Balance
	{
		bool isAvailable = false;  // 账户是否可用
		QList<BalanceInfo> infos;  // 各币种余额（通常只有一条）
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
	// 读取失败返回空字符串。结果可直接填入 ChatMsessage::images。
	static QString imageFileToDataUrl(const QPixmap& pixmap);
	static QString imageFileToDataUrl(const QString& filePath);

	// 查询账户余额（DeepSeek: GET /user/balance，对应 Java 版 DeepSeekApi::callBalance）。
	// baseUrl 为空时用 Params::baseUrl 的默认值；成功走 balanceReady，失败走 errorOccured。
	// 与聊天消息无关，所以这两个信号都不带 msgUuid（errorOccured 的 msgUuid 为空）。
	void callBalance(const QString& baseUrl=QString());

signals:
	// 非流式：一次性返回完整答案
	void replyReady(const QString& reply, const QString& msgUuid);

	// 流式：每来一小段就发一次
	void streamChunk(const QString& msgUuid, const QString& chunk);

	// 流式结束
	void streamComplete(const QString& msgUuid);

	// 思考内容（非流式：一次性返回完整思考文本）
	void thinkingReady(const QString& thinking);

	// 思考内容（流式：逐段到达）
	void thinkingChunk(const QString& chunk, const QString& msgUuid);

	// 出错
	void errorOccured(const QString& errorMessage, const QString& msgUuid);

	// 余额查询成功（余额不属于任何一条聊天消息，所以不带 msgUuid；失败走 errorOccured，其 msgUuid 为空）
	void balanceReady(const AIHandler::Balance& balance);

private slots:
	void onReplyReadyRead();
	void onReplyFinished();
	void onBalanceReplyFinished();

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