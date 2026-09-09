#pragma once

#include <QWidget>
#include "ui_AITalkWidget.h"

class AITalkWidget : public QWidget
{
	Q_OBJECT

public:
	AITalkWidget(QWidget *parent = Q_NULLPTR);
	~AITalkWidget();

private:
	Ui::AITalkWidget ui;
};
