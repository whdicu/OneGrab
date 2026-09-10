#include "AITalkWidget.h"
#include "LabelIsland1.h"
#include <QUuid>
#include "TalkMsgLeft.h"
#include "TalkMsgRight.h"

// test
const static QString API_KEY = "sk-778ef85c1ff444b2a718b8b7cc032a02";
const static QString SYSTEM_STR = "你是一名助手，需要回答主人关于这张图片的提问。";


AITalkWidget::AITalkWidget(LabelIsland* island, QWidget *parent)
	: QWidget(parent)
	, island_(island)
	, aiHandler_(new AIHandler(API_KEY))

{
	ui.setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);

	// island 移动或缩放时，talkWidget 跟随
	connect(island, &LabelIsland::sigGeometryChanged, this, [island, this]()
	{
		move(island->x() + island->width() + 10
			, island->y() + island->height() - this->height());
	});
	connect(island, &LabelIsland::sigHide, this, &QWidget::hide);
	connect(island, &LabelIsland::sigShow, this, &QWidget::show);
	connect(island, &LabelIsland::sigNeedShowAITalk, this, &QWidget::show);

	// 创建聊天msg
	aiParams_.model = "deepseek-flash";
	aiParams_.stream = false;
	aiParams_.baseUrl = "https://api.deepseek.com/v1";

	// 思考模式开关：默认关闭
	const bool thinkOn = false;
	aiParams_.thinking = thinkOn;
	aiParams_.reasoningEffort = "max";

	AIHandler::ChatMessage m1;
	m1.role = AIHandler::System;
	m1.content = SYSTEM_STR;
	aiParams_.msgs.append(m1);

	connect(aiHandler_, &AIHandler::replyReady, this, [this](const QString& reply)
	{
		AIHandler::ChatMessage m1;
		m1.role = AIHandler::Assistant;
		m1.content = reply;
		aiParams_.msgs.append(m1);
		qDebug() << "[AI消息接收]" << reply;
		addTalkMsg(true, reply);
	});

	// test
	//addTalkMsg(true, "asdasda阿三大苏打的是大大撒撒大大是大大萨达萨达撒啊时代的阿三大苏打的是大大撒撒大大是大大萨达萨达撒啊时代的");
}

AITalkWidget::~AITalkWidget()
{
}

void AITalkWidget::on_btn_close_clicked()
{
	hide();
}

void AITalkWidget::on_btn_send_clicked()
{
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

	addTalkMsg(false, str);
}

QString AITalkWidget::addTalkMsg(bool isLeft, const QString& text)
{
	TalkMsgBase* talkMsg;
	if (isLeft)
		talkMsg = new TalkMsgLeft(text);
	else
		talkMsg = new TalkMsgRight(text);

	QVBoxLayout* hLayout = qobject_cast<QVBoxLayout*>(ui.widget_talks->layout());
	if (hLayout)
		hLayout->addWidget(talkMsg);

	const QString msgUuid = QUuid::createUuid().toString();
	mapTalkMsgs_.insert(msgUuid, talkMsg);
	return msgUuid;
}
