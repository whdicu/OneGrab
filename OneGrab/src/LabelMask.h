#pragma once
#include <QWidget>

class LabelMask : public QWidget
{
	Q_OBJECT

public:
	LabelMask(QWidget* parent = nullptr);
	void setSelectionRect(const QRect& rect);
	QRect getSelectionRect() { return selectionRect_; }
	void clearSelectionRect();
	void moveSelectionRect(int dX, int dY);
	bool moveSelectionRectLeft(int dX);
	bool moveSelectionRectRight(int dX);
	bool moveSelectionRectTop(int dY);
	bool moveSelectionRectBottom(int dY);

signals:
	void sigSelectionChanged(const QRect& rect);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QRect selectionRect_;
};

