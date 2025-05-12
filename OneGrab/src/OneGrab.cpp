#include "OneGrab.h"
#include "BtnBar.h"
#include "LabelIsland.h"
#include "LabelIsland2.h"
#include "MouseWindow.h"
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include "SettingDialog.h"
#include "SettingHandler.h"


const static int MARGIN = 5;

OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, btnBar_(new BtnBar)
	, mouseWindow_(new MouseWindow)
{
    ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	QGraphicsScene* lpScene = new QGraphicsScene;
	ui.view->setScene(lpScene);

	connect(btnBar_, &BtnBar::sigDrawing, ui.view, &DGrabView::setDrawingState);
	connect(btnBar_, &BtnBar::sigClose, this, &OneGrab::finishGrab);
	connect(btnBar_, &BtnBar::sigFixed, this, &OneGrab::slotFixed);
	connect(btnBar_, &BtnBar::sigSave, this, &OneGrab::slotSave);
	connect(btnBar_, &BtnBar::sigCopy, this, &OneGrab::slotCopy);
	connect(mouseWindow_, &MouseWindow::sigNeedRefresh, this, &OneGrab::slotRefreshPixelInfo);
	connect(mouseWindow_, &MouseWindow::sigMousePress, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseMove, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseRelease, this, &OneGrab::slotMouseEventInWindow);
	connect(ui.view, &DGrabView::sigMousePressed, this, [this]()
	{
		mouseWindow_->hide();
		mouseWindow_->show();
		btnBar_->hide();
		btnBar_->show();
	});
	connect(ui.view, &DGrabView::sigMouseReleased, this, [this]()
	{
		mouseWindow_->hide();
		mouseWindow_->show();
		btnBar_->hide();
		btnBar_->show();
	});
	connect(ui.view, &DGrabView::sigPosChanged, this, &OneGrab::slotPosChanged);
	connect(ui.view, &DGrabView::sigSelectionChanged, this, &OneGrab::slotSelectionChanged);
	connect(ui.view, &DGrabView::sigResetDrawingState, this, [this]()
	{
		ui.view->setDrawingState(btnBar_->getDrawingType());
	});
}

void OneGrab::doGrab()
{
	if (!isHidden())
		return;

	// x y 可以是负数
	QRect screenRect(0, 0, 0, 0);
	fullPixmap_ = getFullPixmap(screenRect);
	ui.view->setImg(fullPixmap_);
	setGeometry(screenRect);
	show();

	mouseWindow_->moveAndRefresh(QCursor::pos() - pos(), geometry());
	mouseWindow_->show();
}

QColor OneGrab::getPixelColor(const QPoint& pos)
{
	return fullPixmap_.toImage().pixelColor(pos);
}

void OneGrab::slotKeyPressed(const KeyInfo& info)
{
	if (isHidden())
	{
		switch (info.key)
		{
		case 112ul:  // F1
			doGrab();
			break;
		}
	}
	else
	{
		switch (info.key)
		{
		case 27ul:  // ESC
			// 有顶层dialog时，按esc只是隐藏顶层dialog
			// 不然整个程序会退出，因为dialog推出后已经做一遍finishGrab了
			if (!isHidden() && (QApplication::activeModalWidget() == nullptr))
				finishGrab();
			break;
		case 46ul:  // delete
			ui.view->deleteHoverItem();
			break;
		case 67ul:  // C
			if (info.ctrlPressed)
			{
				QApplication::clipboard()->
					setText(mouseWindow_->getCurrentColorStr());
				finishGrab();
			}
			else
				slotCopy();  // 这个函数里已调 finishGrab
			break;
		case 81ul:  // Q
			if (!info.ctrlPressed)
				finishGrab();
			break;
		case 83ul:  // S
			if (!info.ctrlPressed)
				slotSave();  // 这个函数里已调 finishGrab
			break;
		case 84ul:  // T
			if (!info.ctrlPressed)
				slotFixed();  // 这个函数里已调 finishGrab
			break;
		case 90ul:  // Z
			if (info.ctrlPressed)
			{
				ui.view->zItem(info.shiftPressed);
			}
			break;
		case 160ul:  // SHIFT
			mouseWindow_->switchColorStrMode();
			break;
		}
	}
}

void OneGrab::slotFixed()
{
	QRect croppedRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(croppedRect);
	LabelIsland* island = new LabelIsland(croppedPixmap, croppedRect.topLeft() + pos());
	connect(SettingDialog::getInstance(), &SettingDialog::sigRefreshSetting, island, &LabelIsland::onRefreshSetting);
	island->show();
	finishGrab();
}

void OneGrab::slotSave()
{
	QRect uselessRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(uselessRect);
	// 保存截图到文件
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	QString filename = QString("OneGrab_%1.png").arg(timestamp);

	SettingStruct settingStruct = SETTING->getSettingStruct();
	QString fileurl;
	if (settingStruct.UseDefaultSavePath)
	{
		if (settingStruct.DefaultSavePath.isEmpty())
		{
			QString fileurl = QFileDialog::getSaveFileName(this, tr("保存文件"), settingStruct.LastSavePath + '/' + filename);
			if (!fileurl.isEmpty())
			{
				fileurl = fileurl.replace('\\', '/');
				int i = fileurl.lastIndexOf('/');
				settingStruct.DefaultSavePath = fileurl.mid(0, i);
			}
		}
		else
			fileurl = settingStruct.DefaultSavePath + '/' + filename;
	}
	else
	{
		QString fileurl = QFileDialog::getSaveFileName(this, tr("保存文件"), settingStruct.LastSavePath + '/' + filename);
		if (!fileurl.isEmpty())
		{
			fileurl = fileurl.replace('\\', '/');
			int i = fileurl.lastIndexOf('/');
			settingStruct.LastSavePath = fileurl.mid(0, i);
		}
	}

	SETTING->setSettingStruct(settingStruct);
	croppedPixmap.save(fileurl);
	finishGrab();
}

void OneGrab::slotCopy()
{
	QRect uselessRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(uselessRect);
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setPixmap(croppedPixmap);
	finishGrab();
}

void OneGrab::slotSelectionChanged(const QRectF& rectf)
{
	QRect rect = rectf.toRect();
	rect.moveLeft(rect.left() + x());
	rect.moveTop(rect.top() + y());
	int toX = rect.right() - btnBar_->width();
	int toY = rect.bottom() + MARGIN;

	if (toX < x() + MARGIN)
		toX = x() + MARGIN;
	else if (toX > x() + width() - btnBar_->width() - MARGIN)
		toX = x() + width() - btnBar_->width() - MARGIN;

	if (toY < y() + MARGIN)
		toY = y() + MARGIN;
	else if (toY > y() + height() - btnBar_->height() - MARGIN)
		toY = y() + height() - btnBar_->height() - MARGIN;

	btnBar_->move(toX, toY);
	btnBar_->setSizeLabelText(rect.size());
}

void OneGrab::slotRefreshPixelInfo(const QPoint& mousePos)
{
	QColor color = getPixelColor(mousePos);

	// 8倍放大
	QSize windowSize = mouseWindow_->getWindowSize() / 8;
	QPixmap windowPixmap = fullPixmap_.copy(QRect(mousePos
		- QPoint(windowSize.width() / 2, windowSize.height() / 2), windowSize));

	// 画鼠标所在像素的矩形框
	QColor mainColor = SETTING->getMainColor();
	QPainter painter(&windowPixmap);
	painter.setPen(QPen(mainColor, 1));
	painter.drawRect(QRect(windowSize.width() / 2 - 1, windowSize.height() / 2 - 1, 2, 2));
	
	mouseWindow_->refreshInfo(mousePos, color, windowPixmap.scaled(windowSize * 8));
}

void OneGrab::slotMouseEventInWindow(QMouseEvent* event)
{
	QMouseEvent* newEvent = new QMouseEvent(event->type(), event->localPos() + mouseWindow_->pos() - pos(), event->screenPos(),
		event->button(), event->buttons(), event->modifiers());
	
	switch (event->type())
	{
	case QMouseEvent::MouseButtonPress:
		ui.view->mousePressEvent(newEvent);
		break;
	case QMouseEvent::MouseMove:
		ui.view->mouseMoveEvent(newEvent);
		break;
	case QMouseEvent::MouseButtonRelease:
		ui.view->mouseReleaseEvent(newEvent);
		break;
	}
}

void OneGrab::slotPosChanged(const QPoint& pos)
{
	mouseWindow_->moveAndRefresh(pos, geometry());
}

QPixmap OneGrab::getFullPixmap(QRect& screenRect)
{
	QList<QScreen*> screens = QGuiApplication::screens();

	// 获取容纳所有显示器图片的矩形
	for (QScreen *screen : screens)
	{
		QRect scRect = screen->geometry();
		if (scRect.x() < screenRect.x())
			screenRect.setX(scRect.x());
		if (scRect.y() < screenRect.y())
			screenRect.setY(scRect.y());
		screenRect = screenRect.united(scRect);
	}
	QPixmap combinedPixmap(screenRect.size());
	combinedPixmap.fill(Qt::transparent);

	// 将每个屏幕的内容绘制到 combinedPixmap
	QPainter painter(&combinedPixmap);
	for (QScreen *screen : screens)
	{
		QPixmap pixmap = screen->grabWindow(0);
		painter.drawPixmap(screen->geometry().topLeft() - screenRect.topLeft(), pixmap);
	}
	painter.end();
	return combinedPixmap;
}

void OneGrab::finishGrab()
{
	ui.view->onFinishGrab();
	mouseWindow_->hide();
	btnBar_->onFinishGrab();
	hide();
}

void OneGrab::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

//void OneGrab::mousePressEvent(QMouseEvent* event)
//{
//	if (event->button() == Qt::LeftButton)
//	{
//		mouseWindow_->hide();
//		mouseWindow_->show();
//
//		selectionStart_ = event->pos();
//		selectionEnd_ = event->pos();
//		
//		QRect selectionRect = labelMask_->getSelectionRect();
//		mouseState_ = 0;
//
//		int isInBorder = 0;
//		if (dAbs(selectionRect.left() - selectionStart_.x()) < DRAG_SPACE)
//		{
//			isInBorder |= dragLeft;
//		}
//		else if (dAbs(selectionRect.right() - selectionStart_.x()) < DRAG_SPACE)
//		{
//			isInBorder |= dragRight;
//		}
//		if (dAbs(selectionRect.top() - selectionStart_.y()) < DRAG_SPACE)
//		{
//			isInBorder |= dragTop;
//		}
//		else if (dAbs(selectionRect.bottom() - selectionStart_.y()) < DRAG_SPACE)
//		{
//			isInBorder |= dragBottom;
//		}
//
//		if (isInBorder)
//		{
//			mouseState_ = isInBorder;
//		}
//		else
//		{
//			if (selectionRect.contains(selectionStart_))
//			{
//				mouseState_ = MoveState;
//			}
//			else
//			{
//				btnBar_->hide();
//				mouseState_ = SelectState;
//				labelMask_->setSelectionRect(QRect(selectionStart_, selectionEnd_));
//			}
//		}
//	}
//}

//void OneGrab::mouseMoveEvent(QMouseEvent* event)
//{
//	mouseWindow_->moveAndRefresh(event->globalPos(), geometry());
//
//	switch (mouseState_)
//	{
//	case FreeState:
//		break;
//	case SelectState:
//		labelMask_->setSelectionRect(QRect(selectionStart_, event->pos()));
//		break;
//	case MoveState:
//	{
//		int dX = event->pos().x() - selectionEnd_.x();
//		int dY = event->pos().y() - selectionEnd_.y();
//		labelMask_->moveSelectionRect(dX, dY);
//		break;
//	}
//	default:
//	{
//		int dX = event->pos().x() - selectionEnd_.x();
//		int dY = event->pos().y() - selectionEnd_.y();
//		if (mouseState_ & dragLeft)
//		{
//			if (labelMask_->moveSelectionRectLeft(dX))
//			{
//				mouseState_ = (mouseState_ | dragRight) & (~dragLeft);
//			}
//		}
//		else if (mouseState_ & dragRight)
//		{
//			if (labelMask_->moveSelectionRectRight(dX))
//			{
//				mouseState_ = (mouseState_ | dragLeft) & (~dragRight);
//			}
//		}
//		if (mouseState_ & dragTop)
//		{
//			if (labelMask_->moveSelectionRectTop(dY))
//			{
//				mouseState_ = (mouseState_ | dragBottom) & (~dragTop);
//			}
//		}
//		else if (mouseState_ & dragBottom)
//		{
//			if (labelMask_->moveSelectionRectBottom(dY))
//			{
//				mouseState_ = (mouseState_ | dragTop) & (~dragBottom);
//			}
//		}
//		break;
//	}
//	}
//	selectionEnd_ = event->pos();
//}

//void OneGrab::mouseReleaseEvent(QMouseEvent* event)
//{
//	if (event->button() == Qt::LeftButton)
//	{
//		mouseState_ = FreeState;
//		mouseWindow_->show();
//		btnBar_->show();
//	}
//}
