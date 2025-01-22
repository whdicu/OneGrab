#pragma once
#include <QLabel>
#include <QGraphicsView>

class QGraphicsScene;
class QPropertyAnimation;

class LabelIsland2 : public QGraphicsView
{
	Q_OBJECT

public:
	LabelIsland2(const QPixmap& pixmap, const QPoint& pos, QWidget* parent = nullptr);
	~LabelIsland2();
	void onRefreshSetting();

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
	QGraphicsScene* scene_;
};

