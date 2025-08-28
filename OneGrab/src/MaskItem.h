#pragma once
#include <QGraphicsItem>


class MaskItem : public QGraphicsItem
{
public:
	enum Border : unsigned char
	{
		None = 0,
		Left = 1,
		Top = 2,
		Right = 4,
		Bottom = 8
	};

	MaskItem(const QRect& rect, QGraphicsItem* parent = nullptr);
	virtual QRectF boundingRect() const override { return fullRect_; }
	QRect getSelectionRect() { return selectionRect_; }
	void setSelectionRect(const QRect& rect);
	void setBorderBright(Border border, bool bright);
	void addBorderBright(Border border, bool bright);
	void removeBorderBright();
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
	unsigned char border_;
};