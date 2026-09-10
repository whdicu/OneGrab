#include "TalkMsgLeft.h"
#include "AutoWrapLabel.h"
#include <QHBoxLayout>


const static QFont FONT = QFont("Microsoft YaHei UI", 10);


TalkMsgLeft::TalkMsgLeft(const QString& text, QWidget* parent)
	: TalkMsgBase(text, parent)
{
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setMargin(0);
	layout->addWidget(label);
	layout->addStretch();
	setLayout(layout);

	setStyleSheet("QLabel { \
		background-color: #EEEEF0; \
		border: none; \
		border-top-left-radius: 10px; \
		border-top-right-radius: 10px; \
		border-bottom-right-radius: 10px; \
		border-bottom-left-radius: 4px; \
	}");
}

TalkMsgLeft::~TalkMsgLeft()
{
}
