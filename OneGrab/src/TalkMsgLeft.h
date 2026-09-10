#pragma once
#include <QWidget>
#include "TalkMsgBase.h"


class QLabel;


class TalkMsgLeft : public TalkMsgBase
{
	Q_OBJECT

public:
	TalkMsgLeft(const QString& text = "", QWidget* parent = Q_NULLPTR);
	~TalkMsgLeft();

private:
};
