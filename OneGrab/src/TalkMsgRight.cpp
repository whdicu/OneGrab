#include "TalkMsgRight.h"
#include "AutoWrapLabel.h"
#include <QHBoxLayout>


const static QFont FONT = QFont("Microsoft YaHei UI", 10);


TalkMsgRight::TalkMsgRight(const QString& text, QWidget* parent)
	: TalkMsgBase(text, parent)
{
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setMargin(0);
	layout->addStretch();
	layout->addWidget(label);
	setLayout(layout);

	setStyleSheet("QLabel { \
		background-color: #9DF29F; \
		border: none; \
		border-top-left-radius: 10px; \
		border-top-right-radius: 10px; \
		border-bottom-right-radius: 4px; \
		border-bottom-left-radius: 10px; \
	}");
}

TalkMsgRight::~TalkMsgRight()
{
}
