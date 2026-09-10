#include "TalkMsgBase.h"
#include "AutoWrapLabel.h"
#include <QHBoxLayout>


const static QFont FONT = QFont("Microsoft YaHei UI", 10);
const static int LABEL_WIDTH = 350;


TalkMsgBase::TalkMsgBase(const QString& text, QWidget* parent)
	: QWidget(parent)
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	label = new AutoWrapLabel(this);
	label->setMaximumWidth(LABEL_WIDTH);
	label->setMinimumWidth(20);
	label->setText(text);
	label->setFont(FONT);
	label->setWordWrap(true);
	label->setMargin(5);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

TalkMsgBase::~TalkMsgBase()
{
}
