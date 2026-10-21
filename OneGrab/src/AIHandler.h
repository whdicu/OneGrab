#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
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
		Assistant,
		Tool        // 工具执行结果：content 里放结果，toolCallId 对应 assistant 的 tool_call
	};

	// 模型发起的一次工具调用。
	struct ToolCall
	{
		QString id;         // 服务端给的 id，回传 tool 消息时用它对应
		QString name;       // 函数名，目前只有 web_search
		QString arguments;  // JSON 字符串，模型给的，可能是非法 JSON
	};

	struct ChatMessage
	{
		MsgType role;
		QString content;

		// 模型返回的思考内容（仅 Assistant）。请求里带 tools 时必须原样回传、包括跨轮次，否则服务端 400。
		QString reasoningContent;
		// Assistant：这一轮发起的工具调用；其他角色忽略。
		QList<ToolCall> toolCalls;
		// Tool：对应哪一次工具调用（assistant tool_calls 里的 id）。
		QString toolCallId;

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
		int maxTokens = 65536;
		double temperature = 1.0;
		QString baseUrl = "https://api.deepseek.com/v1";

		// 思考模式开关：true -> thinking.type=enabled（先输出思考内容再回答）
		bool thinking = false;
		// 思考强度：low / high / max（仅在 thinking=true 时随请求发送）
		QString reasoningEffort = "high";

		// 联网工具：true 时把 web_search / web_fetch 一起发给模型，搜不搜、抓不抓由模型自己判断，
		// 需要时 AIHandler 会内部调用 /v1/tools/search、/v1/tools/fetch 再带着结果继续对话
		// （对外仍是一次 callDeepSeek）。
		bool webSearch = false;
		// 一次提问内最多允许几轮工具调用，到上限后不再提供工具，逼模型用已有结果作答
		int maxToolRounds = 3;
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

	// 网页搜索请求参数（DeepSeek: POST /v1/tools/search）。
	// query 必填，1~128 个 Unicode 字符，支持 site: / -site: / lang: / after: / before: 等搜索语法。
	struct SearchParams
	{
		QString query;
		int count = 10;            // 返回条数，1~50（超出范围会被夹到边界）
		QString oriQuestion;       // 触发搜索的用户原始问题，可留空
		QString localization;      // 用于本地化排序的 IP 地址，可留空
		bool needContent = false;  // true 时结果里的 content 返回网页正文
		QString baseUrl = "https://api.deepseek.com/v1";
	};

	// 单条搜索结果。摘要类字段可能为空，使用前请判断。
	struct SearchResult
	{
		double rerankScore = 0.0;  // 排序分数 0~3，>=1.5 为优质结果
		QString title;
		QString url;
		QString snippet;           // 短摘要（约 100~200 字符），适合展示
		QString summary;           // 长摘要（约 600~800 字符），适合喂给模型
		QString content;           // 网页正文，needContent=false 时为空
		QString siteName;
		QString siteIcon;
		QString date;              // UTC RFC3339 时间，未知为空
		int authority = -1;        // 权威性 0~4，-1 表示未知，>=3 为极权威
	};

	// 网页抓取结果（DeepSeek: POST /v1/tools/fetch，接口只收 url，正文固定返回 Markdown）。
	struct FetchResult
	{
		QString title;    // 页面标题，缺失时为空字符串
		QString content;  // Markdown 正文
	};

public:
	explicit AIHandler(const QString& apiKey);
	~AIHandler();

	AIHandler(const AIHandler&) = delete;
	AIHandler& operator=(const AIHandler&) = delete;

	// 你原本没有这些接口，我这里新增，不影响原有接口
	void setApiKey(const QString& apiKey);
	QString apiKey() const;

	// 这就是 Qt 版的“调用 AI”。
	// params.webSearch=true 时进入「模型自主联网」模式：模型在对话中自己决定要不要搜/抓，
	// 需要时 AIHandler 内部调 /v1/tools/search 或 /v1/tools/fetch，把结果作为 tool 消息塞回上下文再请求，
	// 直到模型给出最终答案。整个过程对外仍然只有一次调用、一个 msgUuid，调用方不用管中间过程。
	// 注意：params.msgs 不会被修改，工具调用产生的消息只存在内部工作副本里。
	void callDeepSeek(const Params& params);

	// 把本地图片文件读成 base64 data URL（按扩展名推断 mime，未知默认 image/jpeg）。
	// 读取失败返回空字符串。结果可直接填入 ChatMsessage::images。
	static QString imageFileToDataUrl(const QPixmap& pixmap);
	static QString imageFileToDataUrl(const QString& filePath);

	// 查询账户余额（DeepSeek: GET /user/balance，对应 Java 版 DeepSeekApi::callBalance）。
	// baseUrl 为空时用 Params::baseUrl 的默认值；成功走 balanceReady，失败走 errorOccured。
	// 与聊天消息无关，所以这两个信号都不带 msgUuid（errorOccured 的 msgUuid 为空）。
	void callBalance(const QString& baseUrl=QString());

	// 网页搜索（DeepSeek: POST /v1/tools/search），由 DeepSeek 自研搜索引擎提供结果。
	// 成功走 searchReady，失败走 errorOccured；两者都带回本次搜索的 uuid，便于并发调用时区分。
	// 注意：与聊天接口一样需要 API Key，且有 QPS 限制，429 时请稍后重试。
	void callSearch(const SearchParams& params);

	// 网页抓取（DeepSeek: POST /v1/tools/fetch），抓单个 URL、返回 Markdown 正文。
	// 成功走 fetchReady，失败走 errorOccured；都带回本次请求的 uuid。
	// url 必须是含 host 的 http/https 地址，且不能带用户名密码；接口没有长度/超时参数，截断得自己来。
	void callFetch(const QString& url, const QString& baseUrl=QString());

signals:
	// 非流式：一次性返回完整答案
	void replyReady(const QString& reply, const QString& msgUuid);

	// 本轮对话新增的全部消息（最终 assistant 回答；触发过工具调用时，中间还夹着
	// assistant[tool_calls] 和 tool 结果），顺序和内部上下文一致，调用方直接 append 到历史即可。
	// 注意：带 tools 的请求要求回传 reasoning_content，这些消息里的请原样保留，别裁掉。
	void turnMessagesReady(const QList<AIHandler::ChatMessage>& newMessages, const QString& msgUuid);

	// 流式：每来一小段就发一次
	void streamChunk(const QString& msgUuid, const QString& chunk);

	// 流式结束
	void streamComplete(const QString& msgUuid);

	// 思考内容（非流式：一次性返回完整思考文本）
	void thinkingReady(const QString& thinking);

	// 思考内容（流式：逐段到达）
	void thinkingChunk(const QString& msgUuid, const QString& chunk);

	// 出错
	void errorOccured(const QString& errorMessage, const QString& msgUuid);

	// 余额查询成功（余额不属于任何一条聊天消息，所以不带 msgUuid；失败走 errorOccured，其 msgUuid 为空）
	void balanceReady(const AIHandler::Balance& balance);

	// 网页搜索成功。results 为空表示搜到了结果集为空（不计费，也不算失败）。
	void searchReady(const QList<AIHandler::SearchResult>& results, const QString& searchUuid);

	// AI 自己决定联网搜索时发出（参数里的 webSearch=true 才会触发），可用于 UI 提示；
	// 搜索本身不会失败到中断对话：失败的搜索结果会作为 tool 消息回给模型。
	void webSearchStarted(const QString& query, const QString& msgUuid);

	// AI 决定抓取某个网页时发出（同样由 webSearch 开关控制）
	void webFetchStarted(const QString& url, const QString& msgUuid);

	// 网页抓取成功
	void fetchReady(const AIHandler::FetchResult& result, const QString& fetchUuid);

private slots:
	void onReplyReadyRead();
	void onReplyFinished();
	void onBalanceReplyFinished();
	void onSearchReplyFinished();
	void onFetchReplyFinished();
	void onToolSearchFinished();
	void onToolFetchFinished();

private:
	// 一次 callDeepSeek 的完整生命周期，包含工具调用引发的多次子请求
	struct RequestContext
	{
		QString msgUuid;
		bool stream = false;
		Params params;              // 本次调用的参数（msgs 用下面的工作副本，不动调用方的历史）
		QList<ChatMessage> msgs;    // 工作副本：工具调用会往里追加 assistant / tool 消息
		int baseCount = 0;          // 调用方原有历史条数，msgs.mid(baseCount) 就是本轮新增的
		QByteArray buffer;

		int toolRounds = 0;
		// 当前这一轮（子请求）累积到的东西
		QString assistantContent;
		QString assistantThinking;
		QList<ToolCall> roundToolCalls;
		QList<ToolCall> pendingToolCalls;
	};

	// 正在执行的工具请求（搜索/抓取）：一次工具调用对应一个，结果回来后拼成 tool 消息
	struct PendingToolRequest
	{
		RequestContext* ctx = nullptr;
		QString toolCallId;
	};

	void dispatchRequest(RequestContext* ctx);
	QJsonObject buildRequestJson(const Params& params, const QList<ChatMessage>& msgs, bool withTools) const;
	static QJsonObject buildWebSearchTool();
	static QJsonObject buildWebFetchTool();
	static QList<ToolCall> parseToolCalls(const QJsonArray& array);
	static void appendToolCallDeltas(QList<ToolCall>& calls, const QJsonArray& deltas);
	static QString parseApiErrorMessage(const QByteArray& raw);
	static QJsonObject buildSearchResultJson(const QList<SearchResult>& results);

	void processStreamBuffer(RequestContext* ctx, bool atEnd);
	void processStreamPayload(RequestContext* ctx, const QByteArray& payload);

	void handleToolCalls(RequestContext* ctx);
	void runNextToolCall(RequestContext* ctx);
	void sendToolSearch(RequestContext* ctx, const ToolCall& call);
	void sendToolFetch(RequestContext* ctx, const ToolCall& call);

private:
	QString m_apiKey;
	QNetworkAccessManager* m_manager;

	// 用 reply 指针找到它对应的上下文
	QHash<QNetworkReply*, RequestContext*> m_contexts;
	// 工具调用触发的搜索/抓取请求
	QHash<QNetworkReply*, PendingToolRequest> m_pendingTools;
};