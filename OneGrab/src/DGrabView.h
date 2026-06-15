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
	DrawLineS = 4,
	DrawArrowS = 5,
	DrawPenS = 6,
	DrawWordS = 7,
	MoveItem = 8,
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
	inline QGraphicsPixmapItem* imgItem() { return imgItem_; }
	void onFinishGrab();
	void zItem(bool shift = false);
	void setDrawingState(int isDrawing);
	QPixmap getSelectionPixmap(QRect& rect);
	void deleteHoverItem();
	void removeBorderBright();
	void setWindowRects(const DList<QRect>& rects);

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
	void addLine(const QPoint& point, const QColor& color);
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

	// 窗口矩形吸附选择
	DList<QRect> windowRects_;
	int hoveredRectIndex_;  // 当前鼠标悬停的窗口矩形索引，-1 表示未悬停
	bool clickOnWindowRect_;  // 在矩形内按下左键但尚未释放，等待判断点击还是拖拽
	QRect savedSelectionRect_;  // 进入矩形前的已有选区，用于移出后恢复
	bool hadSelectionBeforeHover_;  // 悬停前是否已有非空选区
	bool selectionConfirmed_;  // 已经通过点击矩形确认了选区，之后不再自动吸附
};
