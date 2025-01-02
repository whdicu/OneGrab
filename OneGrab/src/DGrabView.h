#pragma once
#include "HDBase/DStack.hpp"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QMap>

enum MouseState
{
	FreeState = 0,
	SelectState = 1,
	MoveState = 2,
	DrawRectS = 3,
	DrawArrowS = 4,
	DrawPenS = 5,
	DrawTextS = 6,
	MoveItem = 7,
	dragLeft = 0x10,
	dragTop = 0x20,
	dragRight = 0x40,
	dragBottom = 0x80,
};

class DGraphicsItem;
class MaskItem;

class DGrabView : public QGraphicsView
{
	Q_OBJECT

public:
	DGrabView(QWidget* parent = nullptr);
	virtual ~DGrabView() = default;

	QGraphicsPixmapItem* setImg(const QPixmap& img);
	QGraphicsPixmapItem* imgItem() { return imgItem_; }
	void onFinishGrab();
	void zItem(bool shift = false);
	void setDrawingState(int isDrawing);
	QPixmap getSelectionPixmap(QRect& rect = QRect());
	void deleteHoverItem();

	virtual void wheelEvent(QWheelEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void closeEvent(QCloseEvent* event);


public slots:
	void onItemClicked(DGraphicsItem* item, const QPointF& pos);
	void onMouseHover(DGraphicsItem* item, bool isEnter);

signals:
	void sigMousePressed();
	void sigPosChanged(const QPoint& pos);
	void sigMouseReleased();
	void sigSelectionChanged(const QRect& rect);
	void sigResetDrawingState();

private:
	void addRect(const QRect& rect, const QColor& color);
	void addArrow(const QRect& rect, const QColor& color);
	void addLines(const QPoint& point, const QColor& color);
	void addText(const QPoint& point, const QColor& color);
	void clearItems();

	QGraphicsPixmapItem* imgItem_;
	DGraphicsItem* editingItem_;
	DGraphicsItem* hoverItem_;
	DStack<QGraphicsItem*> itemList_;
	DStack<QGraphicsItem*> removedList_;
	MaskItem* maskItem_;
	int mouseState_;
	QPoint selectionStart_;
	QPoint mousePosBeforeMove_;
	int choosedBorder_;
};
