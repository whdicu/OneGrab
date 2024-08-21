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


class DGraphicsArrowItem : public DGraphicsItem
{
public:
	const static int ARROW_LENGTH = 8;  // 箭头长度倍数
	const static int ARROW_WIDTH = 3;  // 箭头两边宽度倍数

	DGraphicsArrowItem(const QRectF& r, const QColor& color = QColor(255, 0, 0), ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: DGraphicsItem(r, color, imgView, parent) {}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
	{
		int al = penScale_ * ARROW_LENGTH;
		int aw = penScale_ * ARROW_WIDTH;

		QPointF dp = rect_.bottomRight() - rect_.topLeft();
		double bei1 = al / std::pow((dp.x() * dp.x() + dp.y() * dp.y()), 0.5);
		QPointF arrowCenter = rect_.bottomRight() - bei1 * dp;

		double bei2 = bei1 / al * aw;
		QPointF p2 = arrowCenter - QPointF(bei2 * dp.y(), -bei2 * dp.x());
		QPointF p3 = arrowCenter + QPointF(bei2 * dp.y(), -bei2 * dp.x());

		QPen pen1(color_, penScale_);
		painter->setPen(pen1);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->drawLine(rect_.topLeft(), arrowCenter);

		QPen pen2(color_, 0);
		painter->setPen(pen2);
		painter->setBrush(color_);
		painter->drawPolygon(QPolygonF({ rect_.bottomRight(), p2, p3}));
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
