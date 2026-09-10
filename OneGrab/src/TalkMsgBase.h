#pragma once
#include <QWidget>


class AutoWrapLabel;


class TalkMsgBase : public QWidget
{
	Q_OBJECT

public:
	TalkMsgBase(const QString& text, QWidget* parent = Q_NULLPTR);
	~TalkMsgBase();

protected:
	AutoWrapLabel* label;
};
