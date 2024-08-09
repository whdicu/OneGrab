#include "MouseWindow.h"
#include <QDebug>
#include <QMouseEvent>


MouseWindow::MouseWindow(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
	ui.setupUi(this);
	setMouseTracking(true);
	ui.label_img->setMouseTracking(true);
	ui.widget_color->setMouseTracking(true);
	ui.label_pos->setMouseTracking(true);
}

MouseWindow::~MouseWindow()
{
}

void MouseWindow::moveAndRefresh(const QPoint& pos, const QRect& fullPixmapRect)
{
	emit sigNeedRefresh(pos);

	QPoint toPos = pos + QPoint(15, 0);
	if (toPos.x() < fullPixmapRect.left())
		toPos.setX(fullPixmapRect.left());
	else if (toPos.x() > fullPixmapRect.right() - width())
		toPos.setX(fullPixmapRect.right() - width());

	if (toPos.y() < fullPixmapRect.top())
		toPos.setY(fullPixmapRect.top());
	else if (toPos.y() > fullPixmapRect.bottom() - height())
		toPos.setY(fullPixmapRect.bottom() - height());

	QWidget::move(toPos);
}

void MouseWindow::refreshInfo(const QPoint& pos, const QColor& color, const QPixmap& pixmap)
{
	ui.label_img->setPixmap(pixmap);
	ui.label_pos->setText(QString("X: %1 Y: %2").arg(pos.x()).arg(pos.y()));
	ui.widget_color->setStyleSheet(QString("background-color: rgb(%1, %2, %3)")
		.arg(color.red()).arg(color.green()).arg(color.blue()));
	ui.label_color->setText(color.name().toUpper());
}

QSize MouseWindow::getWindowSize()
{
	return ui.label_img->size();
}

void MouseWindow::mouseMoveEvent(QMouseEvent* event)
{
	move(pos() + event->pos() + QPoint(15, 0));
}
