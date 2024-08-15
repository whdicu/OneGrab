#pragma once
#include <QGraphicsItem>


class MaskItem : public QGraphicsItem
{
public:
	MaskItem(const QRect& rect, QGraphicsItem* parent = nullptr);
	virtual QRectF boundingRect() const override { return fullRect_; }
	QRect getSelectionRect() { return selectionRect_; }
	void setSelectionRect(const QRect& rect);
	void moveSelectionRect(const QPoint& dPos);
	bool moveSelectionRectLeft(int dX);
	bool moveSelectionRectRight(int dX);
	bool moveSelectionRectTop(int dY);
	bool moveSelectionRectBottom(int dY);

protected:
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
	QRect fullRect_;
	QRect selectionRect_;
};