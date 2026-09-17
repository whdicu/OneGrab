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
	// 监听 widget_talks 的布局请求：消息内部追加/换行导致的高度变化只会在它身上发 LayoutRequest
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void on_btn_close_clicked();
	void on_btn_goto_set_apikey_clicked();
	void on_btn_think_clicked();
	void on_btn_send_clicked();

private:
	void addTalkMsg(const QString& UUID, bool isLeft, const QString& text);
	void appendTalkMsg(const QString& UUID, const QString& text);
	void appendThinkMsg(const QString& UUID, const QString& text);
	// 窗口高度按消息内容自动匹配（上限 TALK_MAX_HEIGHT；没内容时只剩滚动区自己的最小高度）
	void updateHeightToTalks(bool keepLatestVisible = false);
	// 把"重算高度"的请求合并成每帧一次（keepLatestVisible 会累加，只要有一次要滚就滚）
	void requestHeightUpdate(bool keepLatestVisible = false);
	// 强制跟随到底部（用户自己发消息时用；流式追加只在"本来就在底部"时才会滚）
	void scrollToBottom();
	// 贴着 island 右下角待着（窗口高度变了得重新贴一次）
	void moveToIsland();
	// 思考按钮的外观完全由 EnableAIThink 决定（主色被改掉时也要重刷）
	void refreshThinkBtn();

	Ui::AITalkWidget ui;
	LabelIsland* island_;
	AIHandler::Params aiParams_;
	AIHandler* aiHandler_;
	DMap<QString, TalkMsgBase*> mapTalkMsgs_;
	WindowsGlassEffect::Mode glassMode_ = WindowsGlassEffect::ModeNone;
	bool heightUpdatePending_ = false;   // 同一批布局请求合并成一次高度重算
	bool keepLatestVisible_ = false;     // 这批请求里有"要滚到最新"的
	bool followBottom_ = true;           // 是否跟随底部（用户手动往上翻时置 false）
	QString currentAIMsgUUID_;  // 当前次AI消息的UUID
};
