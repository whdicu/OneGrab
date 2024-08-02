#pragma once

#include <QtWidgets/QWidget>
#include "ui_OneGrab.h"

class OneGrab : public QWidget
{
    Q_OBJECT

public:
    OneGrab(QWidget *parent = Q_NULLPTR);
	void doGrab();

private:
	// 获取所有显示器组成的一张图片
	QPixmap getFullPixmap(QRect& screenRect);

	void keyPressEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseReleaseEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;

    Ui::OneGrabClass ui;
};
