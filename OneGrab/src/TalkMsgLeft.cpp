#include "TalkMsgLeft.h"
#include "AutoWrapLabel.h"
#include <QHBoxLayout>


const static QFont FONT = QFont("Microsoft YaHei UI", 10);


TalkMsgLeft::TalkMsgLeft(const QString& text, QWidget* parent)
	: TalkMsgBase(text, parent)
	, labelAbove_(new AutoWrapLabel)
{
	// 上面文字
	labelAbove_->setMaximumWidth(LABEL_WIDTH);
	labelAbove_->setMinimumWidth(20);
	labelAbove_->setFont(FONT);
	labelAbove_->setMargin(10);
	labelAbove_->setWordWrap(true);
	labelAbove_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	labelAbove_->setStyleSheet("QLabel { \
		color: #333; \
	}");

	QHBoxLayout* layoutAbove = new QHBoxLayout;
	layoutAbove->setMargin(0);
	layoutAbove->setSpacing(0);
	layoutAbove->addWidget(labelAbove_);

	// 下面正文
	QHBoxLayout* layoutBottom = new QHBoxLayout;
	layoutBottom->setMargin(0);
	layoutBottom->addWidget(label);

	label->setStyleSheet("QLabel { \
		background-color: rgba(238, 238, 240, 1); \
		border: none; \
		border-top-left-radius: 10px; \
		border-top-right-radius: 10px; \
		border-bottom-right-radius: 10px; \
		border-bottom-left-radius: 4px; \
	}");

	QWidget* widget = new QWidget;
	widget->setObjectName("widget");
	QVBoxLayout* layoutWidget = new QVBoxLayout;
	layoutWidget->setMargin(0);
	layoutWidget->setSpacing(0);
	layoutWidget->addLayout(layoutAbove);
	layoutWidget->addLayout(layoutBottom);
	widget->setLayout(layoutWidget);
	
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(widget);
	layout->addStretch();
	setLayout(layout);

	setStyleSheet("#widget { \
		background-color: rgba(238, 238, 240, 0.5); \
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

void TalkMsgLeft::appendAboveText(const QString& text)
{
	// 追加的是 Markdown 源文本，由 AutoWrapLabel 累积起来整体重渲染
	labelAbove_->appendMarkdown(text);

	emit sigTextAppended();
}
