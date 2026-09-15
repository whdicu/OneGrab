#pragma once
#include <QWidget>


class AutoWrapLabel;


class TalkMsgBase : public QWidget
{
	Q_OBJECT

public:
	TalkMsgBase(const QString& text, QWidget* parent = Q_NULLPTR);
	~TalkMsgBase();
	void appendText(const QString& text);

signals:
	// 追加了内容（流式输出时，外层靠它跟着长高 / 滚到最新一条）
	void sigTextAppended();

protected:
	AutoWrapLabel* label;
};
