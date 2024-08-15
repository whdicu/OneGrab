#pragma once
#include "DGrabView.h"
#include <QGraphicsItem>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>


using ViewType = DGrabView;
const static int DEFAULT_PEN_SCALE = 2;


class DGraphicsItem : public QGraphicsItem
{
public:
	DGraphicsItem(const QRectF& r, const QColor& color = QColor(255, 0, 0),
		ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: QGraphicsItem(parent)
		, rect_(r)
		, penScale_(DEFAULT_PEN_SCALE)
		, color_(color)
		, imgView_(imgView)
	{
		setAcceptHoverEvents(true);
		setFlag(ItemIsMovable);
	}

	void dMove(const QPoint& dPos)
	{
		setPos(pos() + dPos);
	}

	void setBoudingRect(const QRectF& r)
	{
		rect_ = r;
		//update();
	}

	virtual QRectF boundingRect() const override
	{
		return rect_;
	}

	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
	{
		penScale_ = DEFAULT_PEN_SCALE * 2;
		if (nullptr != imgView_)
			imgView_->onMouseHover(this, true);
		//update();
	}

	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
	{
		penScale_ = DEFAULT_PEN_SCALE;
		if (nullptr != imgView_)
			imgView_->onMouseHover(this, false);
		//update();
	}

	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override
	{
		if (nullptr != imgView_)
			imgView_->onItemClicked(this, event->scenePos());
		QGraphicsItem::mousePressEvent(event);
	}

	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
	{
		QGraphicsItem::mouseMoveEvent(event);
	}

	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
	{
		QGraphicsItem::mouseReleaseEvent(event);
	}

signals:
	void sigPressed();

protected:
	QRectF rect_;
	int penScale_;
	QColor color_;
	ViewType* imgView_;
};
Q_DECLARE_METATYPE(DGraphicsItem*)


class DGraphicsRectItem : public DGraphicsItem
{
public:
	DGraphicsRectItem(const QRectF& r, const QColor& color = QColor(255, 0, 0), ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: DGraphicsItem(r, color, imgView, parent) {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
	{
		QPen pen(color_, penScale_);
		painter->setPen(pen);
		painter->drawRect(rect_);
	}
};


class DGraphicsEllipseItem : public DGraphicsItem
{
public:
	DGraphicsEllipseItem(const QRectF& r, const QColor& color = QColor(255, 0, 0), ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: DGraphicsItem(r, color, imgView, parent) {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
	{
		qreal scale = 1.0 / painter->worldTransform().m11();
		QPen pen(color_, scale * penScale_);
		painter->setPen(pen);
		painter->drawEllipse(rect_);
	}
};
