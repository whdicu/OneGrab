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
	label->setFont(FONT);
	label->setMargin(10);
	label->setWordWrap(true);
	label->setMarkdown(text);  // 支持 Markdown，内部转成 HTML 渲染
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

TalkMsgBase::~TalkMsgBase()
{
}

void TalkMsgBase::appendText(const QString& text)
{
	// 追加的是 Markdown 源文本，由 AutoWrapLabel 累积起来整体重渲染
	label->appendMarkdown(text);

	emit sigTextAppended();
}
