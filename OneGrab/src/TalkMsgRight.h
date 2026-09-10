#pragma once
#include "TalkMsgBase.h"


class QLabel;


class TalkMsgRight : public TalkMsgBase
{
	Q_OBJECT

public:
	TalkMsgRight(const QString& text = "", QWidget* parent = Q_NULLPTR);
	~TalkMsgRight();

private:
};
