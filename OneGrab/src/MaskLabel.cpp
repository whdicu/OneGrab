#include "MaskLabel.h"
#include <QDebug>
#include <QPainter>


MaskLabel::MaskLabel(QWidget* parent /*= nullptr*/)
	: QWidget(parent)
{
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setMouseTracking(true);
}

void MaskLabel::setSelectionRect(const QRect& rect)
{
	selectionRect_ = rect.normalized();
	update();
	emit sigSelectionChanged(selectionRect_);
}

void MaskLabel::clearSelectionRect()
{
	setSelectionRect(QRect());
}

void MaskLabel::moveSelectionRect(int dX, int dY)
{
	selectionRect_.moveLeft(selectionRect_.left() + dX);
	selectionRect_.moveTop(selectionRect_.top() + dY);
	update();
	emit sigSelectionChanged(selectionRect_);
}

bool MaskLabel::moveSelectionRectLeft(int dX)
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

	update();
	emit sigSelectionChanged(selectionRect_);
	return ret;
}

bool MaskLabel::moveSelectionRectRight(int dX)
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

	update();
	emit sigSelectionChanged(selectionRect_);
	return ret;
}

bool MaskLabel::moveSelectionRectTop(int dY)
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

	update();
	emit sigSelectionChanged(selectionRect_);
	return ret;
}

bool MaskLabel::moveSelectionRectBottom(int dY)
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

	update();
	emit sigSelectionChanged(selectionRect_);
	return ret;
}

void MaskLabel::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QRegion region(rect());
	region -= selectionRect_;
	painter.setClipRegion(region);
	painter.fillRect(rect(), QColor(0, 0, 0, 192));
}
