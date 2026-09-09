#include "AITalkWidget.h"

AITalkWidget::AITalkWidget(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
}

AITalkWidget::~AITalkWidget()
{
}
