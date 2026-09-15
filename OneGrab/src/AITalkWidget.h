#pragma once
#pragma execution_character_set("utf-8")
#include "AIHandler.h"
#include "HDBase/DMap.hpp"
#include "OneGrab.h"
#include <QWidget>
#include "WindowsGlassEffect.h"
#include "ui_AITalkWidget.h"


class TalkMsgBase;


class AITalkWidget : public QWidget
{
	Q_OBJECT

public:
	AITalkWidget(LabelIsland* island, QWidget* parent = Q_NULLPTR);
	~AITalkWidget();

protected:
	// 画毛玻璃的 tint 层：既是系统毛玻璃的着色，也是系统不支持时的兜底底色
	void paintEvent(QPaintEvent* event) override;
	// 布局需要重算（消息增减、气泡换行高度变化）时跟着重算窗口高度
	bool event(QEvent* event) override;

private slots:
	void on_btn_close_clicked();
	void on_btn_send_clicked();

private:
	QString addTalkMsg(bool isLeft, const QString& text);
	// 窗口高度按消息内容自动匹配（上限 TALK_MAX_HEIGHT；没内容时只剩滚动区自己的最小高度）
	void updateHeightToTalks(bool keepLatestVisible = false);
	// 贴着 island 右下角待着（窗口高度变了得重新贴一次）
	void moveToIsland();

	Ui::AITalkWidget ui;
	LabelIsland* island_;
	AIHandler::Params aiParams_;
	AIHandler* aiHandler_;
	DMap<QString, TalkMsgBase*> mapTalkMsgs_;
	WindowsGlassEffect::Mode glassMode_ = WindowsGlassEffect::ModeNone;
	bool heightUpdatePending_ = false;   // 同一批布局请求合并成一次高度重算
};
