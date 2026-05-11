#pragma once
#include "HDBase/DQueue.hpp"
#include "hook.h"
#include <QSet>
#include <QWidget>
#include "ui_OneGrab.h"
#include <windows.h>

class BtnBar;
class DProgressBox;
class DUpdateHandler;
class LabelIsland;
class MouseWindow;

class OneGrab : public QWidget
{
    Q_OBJECT

public:
    OneGrab(QWidget *parent = Q_NULLPTR);
	void doGrab();
	void checkUpdate(bool showDialogOnLatest = false);

public slots:
	void slotKeyPressed(const KeyInfo& info);

private slots:
	void slotFixed();
	void slotFixedOldOne();
	void slotSave();
	void slotCopy();
	void slotSelectionChanged(const QRectF& rect);
	void slotRefreshPixelInfo(const QPoint& mousePos);
	void slotMouseEventInWindow(QMouseEvent* event);
	void slotPosChanged(const QPoint& pos);
	void slotNewVersionAvailable(const QString& version, const QString& url
		, const QString& notes, const QString& download);

private:
	// 获取所有显示器组成的一张图片
	QPixmap getFullPixmap(QRect& screenRect);
	QColor getPixelColor(const QPoint& pos);
	void finishGrab();

	// 保存图片到缓冲区，返回图片的绝对路径
	QString save2Buffer(const QString& timestamp, const QPixmap& pixmap);

	void resizeEvent(QResizeEvent* event) override;
	//void mousePressEvent(QMouseEvent* event) override;
	//void mouseMoveEvent(QMouseEvent* event) override;
	//void mouseReleaseEvent(QMouseEvent* event) override;

    Ui::OneGrabClass ui;
	BtnBar* btnBar_;
	MouseWindow* mouseWindow_;
	QPixmap fullPixmap_;
	bool ignoreKeyPress_;  // 忽略键盘按键
	DQueue<LabelIsland*> islandBuffer_;
	DUpdateHandler* updateHelper_;
	DProgressBox* progressBox_;
	bool showLatestDialog_ = false;
};
