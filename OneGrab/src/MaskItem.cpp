#include "MaskItem.h"
#include <QDebug>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QRegion>
#include "SettingHandler.h"


const static int BORDER_SIZE = 3;


MaskItem::MaskItem(const QRect& rect, QGraphicsItem* parent /*= nullptr*/)
	: QGraphicsItem(parent)
	, fullRect_(rect)
	, selectionRect_(0, 0, 0, 0)
	, border_(None)
{
	setFlag(QGraphicsItem::ItemHasNoContents, false);
}

void MaskItem::setSelectionRect(const QRect& rect)
{
	selectionRect_ = rect.normalized();
}

void MaskItem::setBorderBright(Border border, bool bright)
{
	if (bright)
		border_ |= border;
	else
		border_ &= ~border;
	update();
}

void MaskItem::removeBorderBright()
{
	if (border_ == None)
		return;
	border_ = None;
	update();
}

void MaskItem::moveSelectionRect(const QPoint& dPos)
{
	selectionRect_.moveTopLeft(selectionRect_.topLeft() + dPos);
}

bool MaskItem::moveSelectionRectLeft(int dX)
{
	bool ret = false;
	int toLeft = selectionRect_.left() + dX;
	if (toLeft > selectionRect_.right())
	{
		selectionRect_.setLeft(selectionRect_.right());
		moveSelectionRectRight(toLeft - selectionRect_.right());
		ret = true;
	}
	else
		selectionRect_.setLeft(toLeft);

	return ret;
}

bool MaskItem::moveSelectionRectRight(int dX)
{
	bool ret = false;
	int toRight = selectionRect_.right() + dX;
	if (toRight < selectionRect_.left())
	{
		selectionRect_.setRight(selectionRect_.left());
		moveSelectionRectLeft(toRight - selectionRect_.left());
		ret = true;
	}
	else
		selectionRect_.setRight(toRight);

	return ret;
}

bool MaskItem::moveSelectionRectTop(int dY)
{
	bool ret = false;
	int toTop = selectionRect_.top() + dY;
	if (toTop > selectionRect_.bottom())
	{
		selectionRect_.setTop(selectionRect_.bottom());
		moveSelectionRectBottom(toTop - selectionRect_.bottom());
		ret = true;
	}
	else
		selectionRect_.setTop(toTop);

	return ret;
}

bool MaskItem::moveSelectionRectBottom(int dY)
{
	bool ret = false;
	int toBottom = selectionRect_.bottom() + dY;
	if (toBottom < selectionRect_.top())
	{
		selectionRect_.setBottom(selectionRect_.top());
		moveSelectionRectTop(toBottom - selectionRect_.top());
		ret = true;
	}
	else
		selectionRect_.setBottom(toBottom);

	return ret;
}

void MaskItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->setPen(Qt::NoPen);
	painter->setBrush(Qt::NoBrush);

	// 绘制半透明的黑色矩形
	QRegion region(fullRect_);
	region -= selectionRect_;
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setClipRegion(region);
	painter->setBrush(QColor(0, 0, 0, 192));
	painter->drawRect(fullRect_);

	if (SETTING_HANDLER->getBrightBorder())
	{
		painter->setBrush(SETTING_HANDLER->getMainColor());
		QRect borderRect = QRect(selectionRect_.left() - BORDER_SIZE, selectionRect_.top() - BORDER_SIZE
			, selectionRect_.width() + BORDER_SIZE * 2, selectionRect_.height() + BORDER_SIZE * 2);

		if (Left & border_)
		{
			QPoint p1 = selectionRect_.topLeft();
			QPoint p2 = selectionRect_.bottomLeft();
			QPoint p3 = borderRect.bottomLeft();
			QPoint p4 = borderRect.topLeft();
			painter->drawPolygon(QPolygon({ p1, p2, p3, p4 }));
		}
		if (Top & border_)
		{
			QPoint p1 = selectionRect_.topLeft();
			QPoint p2 = selectionRect_.topRight();
			QPoint p3 = borderRect.topRight();
			QPoint p4 = borderRect.topLeft();
			painter->drawPolygon(QPolygon({ p1, p2, p3, p4 }));
		}
		if (Right & border_)
		{
			QPoint p1 = selectionRect_.topRight() + QPoint(1, 0);
			QPoint p2 = selectionRect_.bottomRight() + QPoint(1, 0);
			QPoint p3 = borderRect.bottomRight() + QPoint(1, 0);
			QPoint p4 = borderRect.topRight() + QPoint(1, 0);
			painter->drawPolygon(QPolygon({ p1, p2, p3, p4 }));
		}
		if (Bottom & border_)
		{
			QPoint p1 = selectionRect_.bottomLeft() + QPoint(0, 1);
			QPoint p2 = selectionRect_.bottomRight() + QPoint(0, 1);
			QPoint p3 = borderRect.bottomRight() + QPoint(0, 1);
			QPoint p4 = borderRect.bottomLeft() + QPoint(0, 1);
			painter->drawPolygon(QPolygon({ p1, p2, p3, p4 }));
		}
	}
}
