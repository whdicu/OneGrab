#pragma once
#pragma execution_character_set("utf-8")
#include "AIHandler.h"
#include "HDBase/DMap.hpp"
#include "OneGrab.h"
#include <QWidget>
#include "ui_AITalkWidget.h"


class TalkMsgBase;


class AITalkWidget : public QWidget
{
	Q_OBJECT

public:
	AITalkWidget(LabelIsland* island, QWidget* parent = Q_NULLPTR);
	~AITalkWidget();

private slots:
	void on_btn_close_clicked();
	void on_btn_send_clicked();

private:
	QString addTalkMsg(bool isLeft, const QString& text);

	Ui::AITalkWidget ui;
	LabelIsland* island_;
	AIHandler::Params aiParams_;
	AIHandler* aiHandler_;
	DMap<QString, TalkMsgBase*> mapTalkMsgs_;
};
