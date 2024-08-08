#pragma once
#include <QWidget>

class LabelIsland : public QWidget
{
	Q_OBJECT

public:
	LabelIsland(const QPixmap& pixmap, QWidget* parent = nullptr);
	~LabelIsland();
	void move(QPoint toPoint);

private:
	void keyPressEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	bool isMove_;
	QPoint pressPoint_;
};

