#pragma once
#include <QLabel>

class QPropertyAnimation;

class LabelIsland : public QLabel
{
	Q_OBJECT

public:
	LabelIsland(const QPixmap& pixmap, const QPoint& pos, QWidget* parent = nullptr);
	~LabelIsland();

private:
	void keyPressEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	bool isMove_;
	QPoint pressPoint_;
	int sizeIndex_;
	QRect originRect_;
	QPropertyAnimation* animation_;
	QLabel* sizeLabel_;
};

