#pragma once

#include <QWidget>
#include "ui_MouseWindow.h"

class MouseWindow : public QWidget
{
	Q_OBJECT

public:
	MouseWindow(QWidget *parent = Q_NULLPTR);
	~MouseWindow();
	void moveAndRefresh(const QPoint& pos, const QRect& fullPixmapRect);
	void refreshInfo(const QPoint& pos, const QColor& color, const QPixmap& pixmap);
	QSize getWindowSize();

signals:
	void sigNeedRefresh(const QPoint& mousePos);

private:
	void mouseMoveEvent(QMouseEvent* event) override;

	Ui::MouseWindow ui;
	QRect fullPixmapRect_;
};
