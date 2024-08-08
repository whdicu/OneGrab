#pragma once
#include <QtWidgets/QWidget>
#include "ui_OneGrab.h"
#include <windows.h>

class LabelMask;
class BtnBar;

enum MouseState
{
	FreeState = 0,
	SelectState = 1,
	MoveState = 2,
	dragLeft = 0x10,
	dragTop = 0x20,
	dragRight = 0x40,
	dragBottom = 0x80,
};

class OneGrab : public QWidget
{
    Q_OBJECT

public:
    OneGrab(QWidget *parent = Q_NULLPTR);
	void doGrab();

public slots:
	void slotKeyPressed(DWORD key);

private slots:
	void slotFixed();
	void slotSave();
	void slotCopy();
	void slotSelectionChanged(QRect rect);

private:
	// 获取所有显示器组成的一张图片
	QPixmap getFullPixmap(QRect& screenRect);
	QPixmap getSelectionPixmap();
	void finishGrab();

	void resizeEvent(QResizeEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

    Ui::OneGrabClass ui;
	BtnBar* btnBar_;
	LabelMask* labelMask_;
	int mouseState_;
	QPoint selectionStart_;
	QPoint selectionEnd_;
	QPixmap fullPixmap_;
};
