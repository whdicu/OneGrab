#pragma once
#include <QWidget>
#include "TalkMsgBase.h"


class AutoWrapLabel;


class TalkMsgLeft : public TalkMsgBase
{
	Q_OBJECT

public:
	TalkMsgLeft(const QString& text = "", QWidget* parent = Q_NULLPTR);
	~TalkMsgLeft();
	void appendAboveText(const QString& text);

private:
	AutoWrapLabel* labelAbove_;
};
