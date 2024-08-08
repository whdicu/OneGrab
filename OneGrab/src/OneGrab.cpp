#include "OneGrab.h"
#include "BtnBar.h"
#include "HDCore/DGlobal.h"
#include "LabelIsland.h"
#include "LabelMask.h"
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include "SettingHandler.h"


const static int MARGIN = 5;
const static int DRAG_SPACE = 15;  // 鼠标放到矩形边缘，可以开始拖动的左右留白

OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, btnBar_(new BtnBar)
	, labelMask_(nullptr)
	, mouseState_(FreeState)
	, selectionStart_(0, 0)
	, selectionEnd_(0, 0)
{
    ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	labelMask_ = new LabelMask(ui.label);

	connect(btnBar_, &BtnBar::sigFixed, this, &OneGrab::slotFixed);
	connect(btnBar_, &BtnBar::sigSave, this, &OneGrab::slotSave);
	connect(btnBar_, &BtnBar::sigCopy, this, &OneGrab::slotCopy);
	connect(labelMask_, &LabelMask::sigSelectionChanged, this, &OneGrab::slotSelectionChanged);
}

void OneGrab::doGrab()
{
	if (!isHidden())
		return;

	// x y 可以是负数
	QRect screenRect(0, 0, 0, 0);
	fullPixmap_ = getFullPixmap(screenRect);
	ui.label->setPixmap(fullPixmap_);
	setGeometry(screenRect);
	show();
}

void OneGrab::slotKeyPressed(DWORD key)
{
	switch (key)
	{
	case 112ul:  // F1
		doGrab();
		break;
	case 27ul:  // ESC
		if (!isHidden())
			finishGrab();
		break;
	}
}

void OneGrab::slotFixed()
{
	QPixmap croppedPixmap = getSelectionPixmap();
	LabelIsland* island = new LabelIsland(croppedPixmap);
	island->move(labelMask_->getSelectionRect().topLeft() + pos());
	island->show();
	finishGrab();
}

void OneGrab::slotSave()
{
	QPixmap croppedPixmap = getSelectionPixmap();
	// 保存截图到文件
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	QString filename = QString("OneGrab_%1.png").arg(timestamp);

	SettingStruct settingStruct = SETTING->getSettingStruct();
	settingStruct.LastSavePath = QFileDialog::getExistingDirectory(this, tr("选择保存文件夹"), settingStruct.LastSavePath);
	SETTING->setSettingStruct(settingStruct);

	croppedPixmap.save(settingStruct.LastSavePath + '/' + filename);
	finishGrab();
}

void OneGrab::slotCopy()
{
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setPixmap(getSelectionPixmap());
	finishGrab();
}

void OneGrab::slotSelectionChanged(QRect rect)
{
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

QPixmap OneGrab::getSelectionPixmap()
{
	return fullPixmap_.copy(labelMask_->getSelectionRect());
}

void OneGrab::finishGrab()
{
	labelMask_->clearSelectionRect();
	btnBar_->hide();
	hide();
}

void OneGrab::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	labelMask_->resize(size());
}

void OneGrab::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		selectionStart_ = event->pos();
		selectionEnd_ = event->pos();
		
		QRect selectionRect = labelMask_->getSelectionRect();
		mouseState_ = 0;

		int isInBorder = 0;
		if (dAbs(selectionRect.left() - selectionStart_.x()) < DRAG_SPACE)
		{
			isInBorder |= dragLeft;
		}
		else if (dAbs(selectionRect.right() - selectionStart_.x()) < DRAG_SPACE)
		{
			isInBorder |= dragRight;
		}
		if (dAbs(selectionRect.top() - selectionStart_.y()) < DRAG_SPACE)
		{
			isInBorder |= dragTop;
		}
		else if (dAbs(selectionRect.bottom() - selectionStart_.y()) < DRAG_SPACE)
		{
			isInBorder |= dragBottom;
		}

		if (isInBorder)
		{
			mouseState_ = isInBorder;
		}
		else
		{
			if (selectionRect.contains(selectionStart_))
			{
				mouseState_ = MoveState;
			}
			else
			{
				btnBar_->hide();
				mouseState_ = SelectState;
				labelMask_->setSelectionRect(QRect(selectionStart_, selectionEnd_));
			}
		}
	}
}

void OneGrab::mouseMoveEvent(QMouseEvent* event)
{
	//static int i = 0;
	//btnBar_->setTestText(QString("test: %1").arg(++i));
	//QRect selectionRect = labelMask_->getSelectionRect();
	//int isInBorder = 0;
	//if (dAbs(selectionRect.left() - event->pos().x()) < DRAG_SPACE)
	//{
	//	isInBorder |= dragLeft;
	//}
	//else if (dAbs(selectionRect.right() - event->pos().x()) < DRAG_SPACE)
	//{
	//	isInBorder |= dragRight;
	//}
	//if (dAbs(selectionRect.top() - event->pos().y()) < DRAG_SPACE)
	//{
	//	isInBorder |= dragTop;
	//}
	//else if (dAbs(selectionRect.bottom() - event->pos().y()) < DRAG_SPACE)
	//{
	//	isInBorder |= dragBottom;
	//}
	//qDebug() << isInBorder;
	//switch (isInBorder)
	//{
	//case dragLeft:
	//case dragRight:
	//	QApplication::setOverrideCursor(Qt::SizeHorCursor);
	//	break;
	//case dragTop:
	//case dragBottom:
	//	QApplication::setOverrideCursor(Qt::SizeVerCursor);
	//	break;
	//case dragLeft | dragTop:
	//case dragRight | dragBottom:
	//	QApplication::setOverrideCursor(Qt::SizeFDiagCursor);
	//	break;
	//case dragLeft | dragBottom:
	//case dragRight | dragTop:
	//	QApplication::setOverrideCursor(Qt::SizeBDiagCursor);
	//	break;
	//default:
	//	QApplication::restoreOverrideCursor();
	//	break;
	//}

	switch (mouseState_)
	{
	case FreeState:
		break;
	case SelectState:
		labelMask_->setSelectionRect(QRect(selectionStart_, event->pos()));
		break;
	case MoveState:
	{
		int dX = event->pos().x() - selectionEnd_.x();
		int dY = event->pos().y() - selectionEnd_.y();
		labelMask_->moveSelectionRect(dX, dY);
		break;
	}
	default:
	{
		int dX = event->pos().x() - selectionEnd_.x();
		int dY = event->pos().y() - selectionEnd_.y();
		if (mouseState_ & dragLeft)
		{
			if (labelMask_->moveSelectionRectLeft(dX))
			{
				mouseState_ = (mouseState_ | dragRight) & (~dragLeft);
			}
		}
		else if (mouseState_ & dragRight)
		{
			if (labelMask_->moveSelectionRectRight(dX))
			{
				mouseState_ = (mouseState_ | dragLeft) & (~dragRight);
			}
		}
		if (mouseState_ & dragTop)
		{
			if (labelMask_->moveSelectionRectTop(dY))
			{
				mouseState_ = (mouseState_ | dragBottom) & (~dragTop);
			}
		}
		else if (mouseState_ & dragBottom)
		{
			if (labelMask_->moveSelectionRectBottom(dY))
			{
				mouseState_ = (mouseState_ | dragTop) & (~dragBottom);
			}
		}
		break;
	}
	}
	selectionEnd_ = event->pos();
}

void OneGrab::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		mouseState_ = FreeState;
		btnBar_->show();
	}
}
