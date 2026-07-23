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
class LabelIsland1;
class LabelIsland2;
class LabelIsland3;
class LabelIsland4;
class LabelIsland5;
class MouseWindow;

using LabelIsland = LabelIsland1;


class OneGrab : public QWidget
{
    Q_OBJECT

public:
    OneGrab(QWidget *parent = Q_NULLPTR);
	void doGrab();
	void checkUpdate(bool showDialogOnLatest = false);

signals:
	void sigFixedImageDownloadFinished(QPixmap pixmap);

public slots:
	void slotKeyPressed(const KeyInfo& info);

private slots:
	void slotFixed();
	void slotFixedOldOne();
	void slotFixedCopyOne();
	void slotFixedImageDownloadFinished(QPixmap pixmap);  // 其实这个函数就是把图片做成LabelIsland
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

	// 异步下载网络图片
	void DownloadImage(const QString& url);

	// 保存图片到缓冲区，返回图片的绝对路径
	QString save2Buffer(const QString& timestamp, const QPixmap& pixmap);

	// 获取所有LabelIsland的rect，onlyShowing控制是否只返回正在显示的
	DList<QRect> getLabelIslandRects(bool onlyShowing);

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
