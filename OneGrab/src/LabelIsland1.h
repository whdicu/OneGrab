#pragma once
#include <QLabel>

class QPropertyAnimation;
class DMenu;

class LabelIsland1 : public QLabel
{
	Q_OBJECT

public:
	LabelIsland1(const QPixmap& pixmap, const QPoint& pos, QWidget* parent = nullptr);
	~LabelIsland1();
	void onRefreshSetting();
	void show();

signals:
	void sigHide();
	void sigShow();
	// 位置或尺寸变化时发出（拖动移动、缩放动画都会触发）
	void sigGeometryChanged();
	void sigNeedShowAITalk();

private:
	void moveEvent(QMoveEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	void slotBtnClicked(const QString& text);
	void scale(bool bigger, const QPoint& mouseGlobalPos = QPoint(-1, -1));

	QPoint pressedPoint_;
	int sizeIndex_;
	QRect originRect_;
	QPoint pointToOrigin_;
	QPropertyAnimation* animation_;
	QLabel* sizeLabel_;
	DMenu* menu_;
};

