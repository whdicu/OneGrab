#pragma once
#include <QGraphicsView>
#include <QPixmap>
#include <QVariantAnimation>

class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class DMenu;
class QLabel;

class LabelIsland5 : public QGraphicsView
{
	Q_OBJECT

public:
	LabelIsland5(const QPixmap& pixmap, const QPoint& pos, QWidget* parent = nullptr);
	~LabelIsland5();
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

	QGraphicsScene* scene_;
	QGraphicsRectItem* borderItem_;    // 边框 item，缩放时改变它的 scale
	QGraphicsPixmapItem* pixmapItem_;  // 图片 item，作为 borderItem_ 的子 item
	QPixmap origPixmap_;
	QPoint pressedPoint_;              // 拖拽时鼠标在 widget 内的偏移
	int sizeIndex_;
	QSize originPixmapSize_;           // 原始图片尺寸（不含边框）
	QSize originBorderSize_;           // 100% 时 图片+边框 总尺寸
	QSize maxWidgetSize_;              // widget 最大尺寸（固定不变）
	qreal maxScale_;
	QVariantAnimation* animation_;
	QLabel* sizeLabel_;
	DMenu* menu_;
	qreal currentScale_;
	QColor borderColor_;
};
