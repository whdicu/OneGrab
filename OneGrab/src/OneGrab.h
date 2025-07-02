#pragma once
#include "hook.h"
#include <QtWidgets/QWidget>
#include "ui_OneGrab.h"
#include <windows.h>

class MouseWindow;
class BtnBar;

class OneGrab : public QWidget
{
    Q_OBJECT

public:
    OneGrab(QWidget *parent = Q_NULLPTR);
	void doGrab();

public slots:
	void slotKeyPressed(const KeyInfo& info);

private slots:
	void slotFixed();
	void slotSave();
	void slotCopy();
	void slotSelectionChanged(const QRectF& rect);
	void slotRefreshPixelInfo(const QPoint& mousePos);
	void slotMouseEventInWindow(QMouseEvent* event);
	void slotPosChanged(const QPoint& pos);

private:
	// 获取所有显示器组成的一张图片
	QPixmap getFullPixmap(QRect& screenRect);
	QColor getPixelColor(const QPoint& pos);
	void finishGrab();

	void resizeEvent(QResizeEvent* event) override;
	//void mousePressEvent(QMouseEvent* event) override;
	//void mouseMoveEvent(QMouseEvent* event) override;
	//void mouseReleaseEvent(QMouseEvent* event) override;

    Ui::OneGrabClass ui;
	BtnBar* btnBar_;
	MouseWindow* mouseWindow_;
	QPixmap fullPixmap_;
	bool ignoreKeyPress_;  // 忽略键盘按键
};
