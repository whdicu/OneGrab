#pragma once
#include <QLabel>

class QPropertyAnimation;
class DMenu;

class LabelIsland4 : public QLabel
{
	Q_OBJECT

public:
	LabelIsland4(const QPixmap& pixmap, const QPoint& pos, QWidget* parent = nullptr);
	~LabelIsland4();
	void onRefreshSetting();

signals:
	void sigHide();

private:
	void keyPressEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	void slotBtnClicked(const QString& text);
	void scale(bool bigger);

	QPixmap originalPixmap_;
	QPixmap maxScaledPixmap_;
	QPoint pressedPoint_;
	int sizeIndex_;
	QRect originRect_;
	QPoint pointToOrigin_;
	QPropertyAnimation* animation_;
	QLabel* sizeLabel_;
	DMenu* menu_;
};
