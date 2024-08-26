#include "DGrabView.h"
#include "DGraphicsItem.h"
#include "HDCore/DGlobal.h"
#include "MaskItem.h"
#include <QWheelEvent>
#include <QRectF>
#include <QDebug>


const static int DRAG_SPACE = 15;  // 鼠标放到矩形边缘，可以开始拖动的左右留白

DGrabView::DGrabView(QWidget* parent)
    : QGraphicsView(parent)
	, imgItem_(nullptr)
	, editingItem_(nullptr)
	, hoverItem_(nullptr)
	, mouseState_(FreeState)
	, selectionStart_(0, 0)
	, mousePosBeforeMove_(0, 0)
	, choosedBorder_(0)
{
	setMouseTracking(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	QGraphicsScene* sc = new QGraphicsScene;
	setScene(sc);
}

QGraphicsPixmapItem* DGrabView::setImg(const QPixmap& img)
{
	clearItems();
	imgItem_ = scene()->addPixmap(img);

	maskItem_ = new MaskItem(scene()->sceneRect().toRect());
	scene()->addItem(maskItem_);

	return imgItem_;
}

void DGrabView::onFinishGrab()
{
	mouseState_ = FreeState;
	hoverItem_ = nullptr;
	clearItems();
}

void DGrabView::zItem(bool shift /*= false*/)
{
	if (shift)
	{
		if (removedList_.isEmpty())
			return;
		auto item = removedList_.dequeue();
		item->setVisible(true);
		itemList_.enqueue(item);
	}
	else
	{
		if (itemList_.isEmpty())
			return;
		auto item = itemList_.dequeue();
		item->setVisible(false);
		removedList_.enqueue(item);
	}
	update();
}

void DGrabView::setDrawingState(int isDrawing)
{
	mouseState_ = isDrawing;
}

QPixmap DGrabView::getSelectionPixmap(QRect& rect)
{
	rect = maskItem_->getSelectionRect();
	QPixmap fullPixmap(viewport()->size());
	QPainter painter(&fullPixmap);
	render(&painter);
	return fullPixmap.copy(rect);
}

void DGrabView::deleteHoverItem()
{
	if (hoverItem_)
	{
		if (itemList_.isEmpty())
			return;
		scene()->removeItem(hoverItem_);
		hoverItem_ = nullptr;
		update();
	}
}

void DGrabView::wheelEvent(QWheelEvent* event)
{
	QGraphicsView::wheelEvent(event);
}

void DGrabView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
	{
		QGraphicsView::mousePressEvent(event);
		return;
	}
	
	selectionStart_ = event->pos();
	if (hoverItem_)  // 移动已经画好的item
	{
		mouseState_ = MoveItem;
	}
	else
	{
		switch (mouseState_)
		{
		case DrawRectS:  // 画矩形
			addRect(QRect(selectionStart_, selectionStart_), QColor(255, 0, 0));
			break;
		case DrawArrowS:  // 画箭头
			addArrow(QRect(selectionStart_, selectionStart_), QColor(255, 0, 0));
			break;
		case DrawPenS:  // 随便画
			addLines(selectionStart_, QColor(255, 0, 0));
			break;
		case DrawTextS:  // 随便画
			break;
		default:
			mousePosBeforeMove_ = event->pos();
			if (choosedBorder_)  // 要拖动边框
			{
				mouseState_ |= choosedBorder_;
			}
			else
			{
				QRectF selectionRect = maskItem_->getSelectionRect();
				if (selectionRect.contains(event->pos()))
				{
					mouseState_ = MoveState;  // 移动选择的截图区域
				}
				else  // 选择截图区域
				{
					emit sigMousePressed();
					mouseState_ = SelectState;
				}
			}
			break;
		}
	}
}

void DGrabView::mouseMoveEvent(QMouseEvent* event)
{
	QPoint curpos;
	if (imgItem_)
	{
		//curpos = (mpImgItem->mapFromScene(this->mapToScene(evt->pos())) +
		//	QPointF(mImgOriSize.width() / 2.0, mImgOriSize.height() / 2.0) + QPointF(-0.5, -0.5)).toPoint();
	}

	// 根据鼠标位置设置鼠标样式
	unsetCursor();
	switch (mouseState_)
	{
	case SelectState:
		maskItem_->setSelectionRect(QRect(selectionStart_, event->pos()));
		emit sigSelectionChanged(maskItem_->getSelectionRect());
		break;
	case MoveState:
	{
		setCursor(Qt::ClosedHandCursor);
		QPoint dPos = event->pos() - mousePosBeforeMove_;
		maskItem_->moveSelectionRect(dPos);
		emit sigSelectionChanged(maskItem_->getSelectionRect());
		break;
	}
	case MoveItem:
	{
		QPoint dPos = event->pos() - mousePosBeforeMove_;
		if (hoverItem_)
			hoverItem_->dMove(dPos);
		break;
	}
	case DrawRectS:  // 画矩形
	case DrawArrowS:  // 画箭头
	{
		if (nullptr != editingItem_)
		{
			editingItem_->setBoudingRect(QRect(selectionStart_, event->pos()));
		}
		break;
	}
	case DrawPenS:  // 随便画
	{
		if (nullptr != editingItem_)
		{
			editingItem_->setBoudingRect(QRect(selectionStart_, event->pos()));
		}
		break;
	}
	case DrawTextS:  // 画文字
	{
		break;
	}
	default:
	{
		QPoint dPos = event->pos() - mousePosBeforeMove_;
		if (mouseState_ & dragLeft)
		{
			if (maskItem_->moveSelectionRectLeft(dPos.x()))
			{
				mouseState_ = (mouseState_ | dragRight) & (~dragLeft);
			}
		}
		else if (mouseState_ & dragRight)
		{
			if (maskItem_->moveSelectionRectRight(dPos.x()))
			{
				mouseState_ = (mouseState_ | dragLeft) & (~dragRight);
			}
		}
		if (mouseState_ & dragTop)
		{
			if (maskItem_->moveSelectionRectTop(dPos.y()))
			{
				mouseState_ = (mouseState_ | dragBottom) & (~dragTop);
			}
		}
		else if (mouseState_ & dragBottom)
		{
			if (maskItem_->moveSelectionRectBottom(dPos.y()))
			{
				mouseState_ = (mouseState_ | dragTop) & (~dragBottom);
			}
		}
		if (mouseState_ >= dragLeft)
			emit sigSelectionChanged(maskItem_->getSelectionRect());

		QRect selectionRect = maskItem_->getSelectionRect();
		choosedBorder_ = 0;
		if (dAbs(selectionRect.left() - event->pos().x()) < DRAG_SPACE)
		{
			choosedBorder_ |= dragLeft;
		}
		else if (dAbs(selectionRect.right() - event->pos().x()) < DRAG_SPACE)
		{
			choosedBorder_ |= dragRight;
		}
		if (dAbs(selectionRect.top() - event->pos().y()) < DRAG_SPACE)
		{
			choosedBorder_ |= dragTop;
		}
		else if (dAbs(selectionRect.bottom() - event->pos().y()) < DRAG_SPACE)
		{
			choosedBorder_ |= dragBottom;
		}

		switch (choosedBorder_)
		{
		case dragLeft:
		case dragRight:
			setCursor(Qt::SizeHorCursor);
			break;
		case dragTop:
		case dragBottom:
			setCursor(Qt::SizeVerCursor);
			break;
		case dragLeft | dragTop:
		case dragRight | dragBottom:
			setCursor(Qt::SizeFDiagCursor);
			break;
		case dragRight | dragTop:
		case dragLeft | dragBottom:
			setCursor(Qt::SizeBDiagCursor);
			break;
		default:
			if (selectionRect.contains(event->pos()))
				setCursor(Qt::OpenHandCursor);
		}
		break;
	}
	}
	
	mousePosBeforeMove_ = event->pos();
	emit sigPosChanged(event->pos());
	QGraphicsView::mouseMoveEvent(event);
	update();
}

void DGrabView::mouseReleaseEvent(QMouseEvent *event)
{
	QGraphicsView::mouseReleaseEvent(event);
	if (imgItem_)
		imgItem_->setSelected(false);

	if (event->button() == Qt::LeftButton)
	{
		switch (mouseState_)
		{
		case DrawRectS:
		case DrawArrowS:
		case DrawPenS:
		case DrawTextS:
			break;
		case MoveItem:
			// 发送信号重设 mouseState_
			emit sigResetDrawingState();
			break;
		default:
			mouseState_ = FreeState;
			break;
		}
		
		editingItem_ = nullptr;
		emit sigMouseReleased();
	}
}

void DGrabView::closeEvent(QCloseEvent *evt)
{
    deleteLater();
}

void DGrabView::addRect(const QRect& rect, const QColor& color)
{
	editingItem_ = new DGraphicsRectItem(rect, color, this, imgItem_);
	itemList_.enqueue(editingItem_);
	removedList_.clear();
}

void DGrabView::addArrow(const QRect& rect, const QColor& color)
{
	editingItem_ = new DGraphicsArrowItem(rect, color, this, imgItem_);
	itemList_.enqueue(editingItem_);
	removedList_.clear();
}

void DGrabView::addLines(const QPoint& point, const QColor& color)
{
	editingItem_ = new DGraphicsLinesItem(point, color, this, imgItem_);
	itemList_.enqueue(editingItem_);
	removedList_.clear();
}

void DGrabView::clearItems()
{
	scene()->clear();
	imgItem_ = nullptr;
	itemList_.clear();
	removedList_.clear();
	//for (auto item : itemList_)
	//{
	//	if (nullptr == item)
	//		continue;
	//	scene()->removeItem(item);
	//	delete item;
	//}
}

void DGrabView::onItemClicked(DGraphicsItem* item, const QPointF& pos)
{

}

void DGrabView::onMouseHover(DGraphicsItem* item, bool isEnter)
{
	if (isEnter)
		hoverItem_ = item;
	else
		hoverItem_ = nullptr;
}
