#pragma once
#pragma execution_character_set("utf-8")
#include "DGrabView.h"
#include <QGraphicsItem>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>


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

	virtual void onMouseRelese() {};

	virtual void setBoundingRect(const QRectF& r)
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


class DGraphicsLinesItem : public DGraphicsItem
{
public:
	DGraphicsLinesItem(const QPointF& point, const QColor& color = QColor(255, 0, 0), ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: DGraphicsItem(QRectF(point, point), color, imgView, parent) {}

	virtual void setBoundingRect(const QRectF& r) override
	{
		addPoint(r.bottomRight());
	}

	void addPoint(const QPointF& point)
	{
		if (point.x() < rect_.left())
			rect_.setLeft(point.x());
		else if (point.x() > rect_.right())
			rect_.setRight(point.x());
		if (point.y() < rect_.top())
			rect_.setTop(point.y());
		else if (point.y() > rect_.bottom())
			rect_.setBottom(point.y());
		
		points_.push_back(point);
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
	{
		QPen pen(color_, penScale_);
		painter->setPen(pen);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->drawPolyline(points_);
	}

private:
	QPolygonF points_;
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


class DGraphicsTextItem : public DGraphicsItem
{
	class TextItem : public QGraphicsTextItem
	{
	public:
		TextItem(DGraphicsTextItem* parent) : QGraphicsTextItem(parent), parent_(parent) {}

	protected:
		void focusOutEvent(QFocusEvent* event) override
		{
			QGraphicsTextItem::focusOutEvent(event);
			auto cursor = textCursor();
			cursor.clearSelection();
			setTextCursor(cursor);

			QFontMetrics metrics(font());
			int w = metrics.width(toPlainText() + '-') + 2;
			QRectF r = parent_->boundingRect();
			r.setWidth(w);
			parent_->setBoundingRect(r);
			setTextWidth(w);
		}

	private:
		DGraphicsTextItem* parent_;
	};

public:
	DGraphicsTextItem(const QRectF& r, const QColor& color = QColor(255, 0, 0), ViewType* imgView = nullptr, QGraphicsItem* parent = nullptr)
		: DGraphicsItem(r, color, imgView, parent)
	{
		textItem_ = new TextItem(this);
		textItem_->setTextInteractionFlags(Qt::TextEditorInteraction);
		textItem_->setDefaultTextColor(color_);
		textItem_->setPlainText("输入文字");
		textItem_->setFont(QFont("Microsoft YaHei", 0));
		textItem_->setPos(rect_.topLeft());
		textItem_->setTextWidth(rect_.width());
		textItem_->setFocus();
	}

	~DGraphicsTextItem()
	{
		if (textItem_)
			delete textItem_;
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
	{
		//QPen pen(color_, penScale_);
		//pen.setStyle(Qt::DashLine);
		//painter->setPen(pen);
		//painter->drawRect(rect_);
	}

	virtual void onMouseRelese()
	{
		auto cursor = textItem_->textCursor();
		cursor.select(QTextCursor::Document);
		textItem_->setTextCursor(cursor);
	}

	virtual void setBoundingRect(const QRectF& r) override
	{
		DGraphicsItem::setBoundingRect(r);
		QRectF normalRect = r.normalized();
		textItem_->setPos(normalRect.topLeft());
		textItem_->setTextWidth(normalRect.width());
		textItem_->setFont(QFont("Microsoft YaHei", normalRect.height() / 1.75976));
	}

	TextItem* textItem_;
};
