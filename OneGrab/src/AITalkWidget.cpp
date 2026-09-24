#include "AITalkWidget.h"
#include "HDQt/include/DToast.h"
#include "LabelIsland1.h"
#include <QDebug>
#include <QEvent>
#include <QLayout>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QUuid>
#include "SettingDialog.h"
#include "SettingHandler.h"
#include "TalkMsgLeft.h"
#include "TalkMsgRight.h"

// test
const static QString SYSTEM_STR = "你是一名助手，需要回答主人关于这张图片的提问。";
// 毛玻璃调参：改下面这几行就够了
const static QColor GLASS_TINT = QColor(255, 255, 255, 30);   // 颜色 + 深浅（alpha 越小越透、模糊越明显）
const static WindowsGlassEffect::BlurLevel GLASS_BLUR_LEVEL = WindowsGlassEffect::BlurLight;  // 模糊档位
const static bool GLASS_DARK_TITLE_BAR = false;                // 浅色玻璃要设 false，否则系统 backdrop 底色发黑
// 窗口圆角半径：按系统分开取值（0 = 直角）
// Win10：accent 模糊按窗口矩形铺、不认窗口区域（已实测），圆角只会让四角漏出一块模糊，
//        半径越大越明显（20 明显 / 8 基本看不出 / 0 最干净）→ 所以取 0
// Win11：有官方圆角，材质和圆角天生对齐，不会漏 → 取 20
const static int GLASS_CORNER_RADIUS_WIN10 = 0;
const static int GLASS_CORNER_RADIUS_WIN11 = 20;
// 窗口高度按消息内容自动匹配，上限 900；没内容时高度就落在 scrollArea 自己的最小高度上（.ui 里设的 1）
const static int TALK_MAX_HEIGHT = 900;

AITalkWidget::AITalkWidget(LabelIsland* island, QWidget *parent)
	: QWidget(parent)
	, island_(island)
	, aiHandler_(nullptr)
{
	ui.setupUi(this);
	// 这几个 flag 本来就要求是「无边框 + 独立顶层窗口」，正好满足窗口级毛玻璃的前提，不用改
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);

	QString apiKey = SETTING_HANDLER->getAIApiKey();
	aiHandler_ = new AIHandler(apiKey);
	if (apiKey.isEmpty())
		ui.btn_goto_set_apikey->show();
	else
		ui.btn_goto_set_apikey->hide();

	ui.scrollArea->hide();

	// 毛玻璃：Win11 走官方 DWM System Backdrop，Win10 按档位走 Acrylic 或更跟手的 BlurBehind，
	// 都不支持时只剩 tint 底色。调参看文件开头的 GLASS_* 常量
	WindowsGlassEffect::Params glassParams;
	glassParams.blurLevel = GLASS_BLUR_LEVEL;
	glassParams.tint = GLASS_TINT;
	glassParams.darkTitleBar = GLASS_DARK_TITLE_BAR;
	glassParams.cornerRadius = WindowsGlassEffect::isDwmCornersSupported()
		? GLASS_CORNER_RADIUS_WIN11
		: GLASS_CORNER_RADIUS_WIN10;
	// Win11 默认走官方圆角（抗锯齿最好、跟系统素材一致，但半径是系统的约 8px）；
	// 想让上面那个 20 精确生效就打开这行，代价是改回自己裁区域
	//glassParams.preferDwmCorners = false;

	// 运行时想试手感可以用：WindowsGlassEffect::setTint(this, QColor(255, 255, 255, 70));
	//                     WindowsGlassEffect::setBlurLevel(this, WindowsGlassEffect::BlurLight);
	WindowsGlassEffect::Result glassResult = WindowsGlassEffect::enable(this, glassParams);
	glassMode_ = WindowsGlassEffect::mode(this);
	qDebug() << __FUNCTION__ << "glass result =" << (int)glassResult << "mode =" << (int)glassMode_;

	// island 移动或缩放时，talkWidget 跟随
	// （窗口高度会随消息条数变化，所以位置统一交给 moveToIsland 算）
	connect(island, &LabelIsland::sigGeometryChanged, this, &AITalkWidget::moveToIsland);

	// 消息在 widget_talks 内部追加 / 换行引起的高度变化，LayoutRequest 只会发给 widget_talks 自己，
	// 顶层布局不会被动失效，所以这里额外盯着它
	ui.widget_talks->installEventFilter(this);

	// "滚到最新"不能只靠一次性 setValue：内容变高之后滚动条范围是稍后才更新的，
	// 那一刻会差一截没到底。所以盯着 rangeChanged —— 范围一变（内容长高/变短）就贴到底部
	connect(ui.scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int max)
	{
		if (followBottom_)
			ui.scrollArea->verticalScrollBar()->setValue(max);
	});

	// 用户自己往上翻的时候就别再把他拽回来；滚回底部（或拖到最底）时自动恢复跟随
	connect(ui.scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value)
	{
		followBottom_ = (value >= ui.scrollArea->verticalScrollBar()->maximum());
	});

	// 等布局跑完一帧再按内容定高，这时候量出来的尺寸才准
	requestHeightUpdate();

	connect(island, &LabelIsland::sigHide, this, &QWidget::hide);
	//connect(island, &LabelIsland::sigShow, this, &QWidget::show);
	connect(island, &LabelIsland::sigNeedShowAITalk, this, &QWidget::show);

	connect(SettingDialog::getInstance(), &SettingDialog::sigRefreshAIApiKey, this, [this](const QString& apiKey)
	{
		aiHandler_->setApiKey(apiKey);
		if (apiKey.isEmpty())
			ui.btn_goto_set_apikey->show();
		else
			ui.btn_goto_set_apikey->hide();
	});

	// 创建聊天msg
	aiParams_.model = "deepseek-flash";
	aiParams_.stream = true;
	aiParams_.baseUrl = "https://api.deepseek.com/v1";

	// 思考模式开关：持久化在设置里，靠按钮切换（颜色/文字也在里面刷）
	aiParams_.reasoningEffort = "max";
	refreshThinkBtn();

	// 主色可能被设置界面改掉，改完按钮要跟着重刷一遍
	connect(SettingDialog::getInstance(), &SettingDialog::sigRefreshSetting, this, &AITalkWidget::refreshThinkBtn);

	AIHandler::ChatMessage m1;
	m1.role = AIHandler::System;
	m1.content = SYSTEM_STR;
	aiParams_.msgs.append(m1);

	// 非流式，一次性回复所有文字
	connect(aiHandler_, &AIHandler::replyReady, this, [this](const QString& reply, const QString& UUID)
	{
		AIHandler::ChatMessage m1;
		m1.role = AIHandler::Assistant;
		m1.content = reply;
		aiParams_.msgs.append(m1);
		qDebug() << "[AI消息接收]" << reply;
		addTalkMsg(UUID, true, reply);
	});

	// 流式回复文字
	connect(aiHandler_, &AIHandler::streamChunk, this, &AITalkWidget::appendTalkMsg);

	// 流式回复思考文字
	connect(aiHandler_, &AIHandler::thinkingChunk, this, &AITalkWidget::appendThinkMsg);

	// test
	//addTalkMsg(true, "asdasda阿三大苏打的是大大撒撒大大是大大萨达萨达撒啊时代的阿三大苏打的是大大撒撒大大是大大萨达萨达撒啊时代的");
}

AITalkWidget::~AITalkWidget()
{
}

void AITalkWidget::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	// 颜色和深浅统一由 appTint 算（按当前模式把 alpha 调好），各窗口的 GLASS_TINT 最终都汇到那里
	// 注意取实时状态：窗口重新 show 之后系统效果可能才贴上去
	const WindowsGlassEffect::Params params = WindowsGlassEffect::params(this);
	const QColor tint = WindowsGlassEffect::appTint(this);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(tint);
	// 窗口本身已经裁过圆角了，这里再画一层抗锯齿圆角，把 SetWindowRgn 那 1bit 区域的锯齿边糊软一点；
	// 但 Win11 官方圆角是 DWM 自己裁的、半径由系统定，这时必须铺满整块，
	// 否则两边半径不一致会在角上留一条没着色的缝
	const QRectF full(0, 0, width(), height());
	const int cornerRadius = WindowsGlassEffect::isDwmCornersActive(this) ? 0 : params.cornerRadius;
	if (cornerRadius > 0)
		painter.drawRoundedRect(full, cornerRadius, cornerRadius);
	else
		painter.drawRect(full);
}

void AITalkWidget::updateHeightToTalks(bool keepLatestVisible /*= false*/)
{
	// 固定部分（输入框 + 按钮行 + 边距间距）的高度用**纯约束**算：布局最小高度 − 滚动区最小高度。
	// 为什么不能用"窗口高度 − 滚动区高度"实测：窗口还没被布局摆布过时滚动区的高度是垃圾值
	// （实测刚 setupUi 完是 30，会算出偏高 40px 的高度），而这个窗口又是"要用才 show"的，
	// 各种时序都能撞上"未摆布"状态。约束计算跟当前几何完全无关，什么时候算都对。
	// 滚动区最小高度按 qSmartMinSize 的口径取：显式 minimumSize 优先，没设才回落 minimumSizeHint
	const int scrollMinHeight = ui.scrollArea->minimumSize().height() > 0
		? ui.scrollArea->minimumSize().height()
		: ui.scrollArea->minimumSizeHint().height();
	const int chromeHeight = layout() ? layout()->minimumSize().height() - scrollMinHeight : 0;
	if (chromeHeight <= 0)
		return;   // 参数还没配好，等下一次请求再来

	// 内容需要多高，只能靠布局算：
	//   ① 不能用 widget_talks->height()：它是 scrollArea 的 content widget，
	//      widgetResizable 会把它撑满视口，量出来永远等于视口高度 → 窗口就再也不动了；
	//   ② 也不能只信 sizeHint()：气泡是换行的，AutoWrapLabel 的 sizeHint 按"理想宽度"折行，
	//      宽度不够时会少算行数。
	// 所以用 heightForWidth 按实际宽度算 —— 和 QScrollArea 内部给内容 widget 定高用的是同一个办法；
	// 它不支持时返回 -1，这时退回 sizeHint。
	// 宽度还没摆布出来（比如窗口还没 show）就别拿 0 去问 heightForWidth，退给 sizeHint
	const int talksWidth = ui.widget_talks->width();
	int contentHeight = talksWidth > 0 ? ui.widget_talks->heightForWidth(talksWidth) : -1;
	if (ui.widget_talks->layout())
		contentHeight = qMax(contentHeight, ui.widget_talks->layout()->sizeHint().height());
	if (contentHeight < 0)
		contentHeight = 0;

	// 没内容时 contentHeight = 0，窗口高度落在 chrome + scrollArea 的最小高度（.ui 里那个 1）上，
	// 也就是"没内容时聊天区高度就是 1"；上限由 TALK_MAX_HEIGHT 和 .ui 里的 maximumSize 兜着
	const int targetHeight = qMin(contentHeight + chromeHeight, TALK_MAX_HEIGHT);
	if (targetHeight != height())
		resize(width(), targetHeight);

	// 高度变了，贴着 island 的位置要跟着重算
	moveToIsland();

	// 有新内容时："只有视图本来就在最底部"才跟到底，用户正在往上翻看前面的消息就一律不动。
	// 滚动条的 value 是"离顶部多少像素"，内容在底部变高不会改这个值，所以什么都不做就是保持原地
	if (keepLatestVisible && followBottom_)
		scrollToBottom();
}

void AITalkWidget::scrollToBottom()
{
	// 重新开始跟随底部，并立刻贴一次（范围万一还没更新，rangeChanged 会兜住）
	followBottom_ = true;
	QScrollBar* bar = ui.scrollArea->verticalScrollBar();
	bar->setValue(bar->maximum());
}

void AITalkWidget::moveToIsland()
{
	if (!island_)
		return;

	move(island_->x() + island_->width() + 10
		, island_->y() + island_->height() - height());
}

bool AITalkWidget::event(QEvent* event)
{
	// 自己的布局需要重算（消息增减）时跟着重算窗口高度
	if (event->type() == QEvent::LayoutRequest)
		requestHeightUpdate();
	// 这个窗口是"要用的时候才 show"的：构造函数里那次定高跑在 show 之前，布局还没摆布过，
	// 量出来的固定高度是错的（会量成整个窗口高度）。所以 show 之后再定一次
	else if (event->type() == QEvent::Show)
		requestHeightUpdate();

	return QWidget::event(event);
}

bool AITalkWidget::eventFilter(QObject* watched, QEvent* event)
{
	// 消息气泡内部变高（流式追加文字、文字重新换行）时，LayoutRequest 是发给 widget_talks 的，
	// 顶层收不到，所以在这里补一刀
	if (watched == ui.widget_talks && event->type() == QEvent::LayoutRequest)
		requestHeightUpdate();

	return QWidget::eventFilter(watched, event);
}

void AITalkWidget::requestHeightUpdate(bool keepLatestVisible /*= false*/)
{
	// 同一批请求合并成每帧一次：流式追加一秒钟可能来几十次，每次 resize 一遍太浪费。
	// keepLatestVisible 是"或"的关系，只要这批里有任意一次要滚到最新，就滚
	keepLatestVisible_ = keepLatestVisible_ || keepLatestVisible;
	if (heightUpdatePending_)
		return;

	heightUpdatePending_ = true;
	QTimer::singleShot(0, this, [this]()
	{
		heightUpdatePending_ = false;
		const bool keep = keepLatestVisible_;
		keepLatestVisible_ = false;
		updateHeightToTalks(keep);
	});
}

void AITalkWidget::on_btn_close_clicked()
{
	hide();
}

void AITalkWidget::on_btn_goto_set_apikey_clicked()
{
	SettingDialog::getInstance()->show();
	SettingDialog::getInstance()->gotoSetApiKey();
}

void AITalkWidget::on_btn_think_clicked()
{
	// set 内部会写文件，不用再手动 syncToFile
	SETTING_HANDLER->setEnableAIThink(!SETTING_HANDLER->getEnableAIThink());
	refreshThinkBtn();
}

void AITalkWidget::refreshThinkBtn()
{
	// 开：主色底 + "思考"；关：白底 + "不思考"
	const bool enableThink = SETTING_HANDLER->getEnableAIThink();
	const QColor bgColor = enableThink ? SETTING_HANDLER->getMainColor() : QColor(255, 255, 255, 223);
	const QString color = enableThink ? "white" : "black";

	// 只覆盖 background-color：控件自己的样式表优先于 .ui 里那条 QPushButton 规则，
	// 圆角 / padding 这些还是沿用原来那份
	ui.btn_think->setStyleSheet(QString("background-color: rgb(%1, %2, %3, %4); color: %5;")
		.arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()).arg(bgColor.alpha()).arg(color));
	ui.btn_think->setText(enableThink ? tr("思考") : tr("不思考"));

	// 请求参数跟着按钮走，免得按钮显示的和真正发出去的模式不一致
	aiParams_.thinking = enableThink;
}

void AITalkWidget::on_btn_send_clicked()
{
	if (SETTING_HANDLER->getAIApiKey().isEmpty())
	{
		DToast::toast(tr("未设置Api Key"), DToast::FromMouse, DToast::Slow);
		return;
	}

	ui.label_tips->hide();
	ui.scrollArea->show();

	QString str = ui.edit_send->toPlainText();
	ui.edit_send->clear();

	qDebug() << "[发送给AI]" << str;

	AIHandler::ChatMessage m1;
	m1.role = AIHandler::User;
	m1.content = str;
	if (aiParams_.msgs.size() == 1)  // 仅第一次提问带上image
	{
		if (island_)
		{
			const QPixmap* pixmap = island_->pixmap();
			if (pixmap)
			{
				QString base64 = AIHandler::imageFileToDataUrl(*pixmap);
				if (!base64.isEmpty())
					m1.images.append(base64);
			}
		}
	}
	aiParams_.msgs.append(m1);
	aiHandler_->callDeepSeek(aiParams_);

	const QString msgUuid = QUuid::createUuid().toString();
	addTalkMsg(msgUuid, false, str);

	// 自己发的消息，直接跟到底部看最新
	scrollToBottom();
}

void AITalkWidget::addTalkMsg(const QString& UUID, bool isLeft, const QString& text)
{
	TalkMsgBase* talkMsg;
	if (isLeft)
		talkMsg = new TalkMsgLeft(text);
	else
		talkMsg = new TalkMsgRight(text);

	QVBoxLayout* hLayout = qobject_cast<QVBoxLayout*>(ui.widget_talks->layout());
	if (hLayout)
		hLayout->addWidget(talkMsg);

	mapTalkMsgs_.insert(UUID, talkMsg);

	// 流式追加时气泡会自己变高，这里跟着长高、并让最新文字保持可见
	connect(talkMsg, &TalkMsgBase::sigTextAppended, this, [this]()
	{
		requestHeightUpdate(true);
	});

	// 等布局跑完一帧再量高度（换行气泡要等布局算完才知道自己多高），并让最新一条可见
	requestHeightUpdate(true);
}

void AITalkWidget::appendTalkMsg(const QString& UUID, const QString& text)
{
	// 这条消息收到的第一段文本
	if (!mapTalkMsgs_.contains(UUID))
	{
		addTalkMsg(UUID, true, text);
		return;
	}
	
	TalkMsgBase* msg = mapTalkMsgs_.value(UUID, nullptr);
	if (!msg)
	{
		qWarning() << __FUNCTION__ << "uuid:" << UUID << "not exists!";
		return;
	}
	msg->appendText(text);
}

void AITalkWidget::appendThinkMsg(const QString& UUID, const QString& text)
{
	// 这条消息收到的第一段文本
	if (!mapTalkMsgs_.contains(UUID))
	{
		addTalkMsg(UUID, true, "");
	}

	TalkMsgLeft* msg = static_cast<TalkMsgLeft*>(mapTalkMsgs_.value(UUID, nullptr));
	if (!msg)
	{
		qWarning() << __FUNCTION__ << "uuid:" << UUID << "not exists or not left msg!";
		return;
	}
	msg->appendAboveText(text);
}
