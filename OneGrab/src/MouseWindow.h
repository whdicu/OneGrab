#pragma once

#include <QWidget>
#include "ui_MouseWindow.h"

class MouseWindow : public QWidget
{
	Q_OBJECT

public:
	MouseWindow(QWidget* parent = Q_NULLPTR);
	~MouseWindow();
	void moveAndRefresh(const QPoint& pos, const QRect& fullPixmapRect);
	void refreshInfo(const QPoint& pos, const QColor& color, const QPixmap& pixmap);
	QSize getWindowSize();
	QString getCurrentColorStr();
	void switchColorStrMode();

signals:
	void sigMousePress(QMouseEvent* event);
	void sigMouseMove(QMouseEvent* event);
	void sigMouseRelease(QMouseEvent* event);

private:
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

	Ui::MouseWindow ui;
	QRect fullPixmapRect_;
	QColor pixelColor_;
	bool isNumColor_;
};
