#include "MouseWindow.h"
#include <QDebug>
#include <QMouseEvent>


MouseWindow::MouseWindow(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, fullPixmapRect_(0, 0, 0, 0)
	, isNumColor_(false)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TransparentForMouseEvents, true);
	setMouseTracking(true);
	ui.label_img->setMouseTracking(true);
	ui.widget_color->setMouseTracking(true);
	ui.label_color->setMouseTracking(true);
	ui.label_pos->setMouseTracking(true);
	ui.label_img->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui.widget_color->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui.label_color->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui.label_pos->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

MouseWindow::~MouseWindow()
{
}

void MouseWindow::moveAndRefresh(const QPoint& pos, const QRect& fullPixmapRect)
{
	emit sigNeedRefresh(pos);

	fullPixmapRect_ = fullPixmapRect;
	QPoint toPos = pos + QPoint(15, 0) + fullPixmapRect.topLeft();
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
	pixelColor_ = color;
	ui.label_img->setPixmap(pixmap);
	ui.label_pos->setText(QString("X: %1 Y: %2").arg(pos.x()).arg(pos.y()));
	ui.widget_color->setStyleSheet(QString("background-color: rgb(%1, %2, %3)")
		.arg(color.red()).arg(color.green()).arg(color.blue()));
	ui.label_color->setText(getCurrentColorStr());
}

QSize MouseWindow::getWindowSize()
{
	return ui.label_img->size();
}

QString MouseWindow::getCurrentColorStr()
{
	if (isNumColor_)
		return QString("%1, %2, %3").arg(pixelColor_.red()).arg(pixelColor_.green()).arg(pixelColor_.blue());
	else
		return pixelColor_.name().toUpper();
}

void MouseWindow::switchColorStrMode()
{
	isNumColor_ = !isNumColor_;
	ui.label_color->setText(getCurrentColorStr());
}

void MouseWindow::mousePressEvent(QMouseEvent* event)
{
	emit sigMousePress(event);
}

void MouseWindow::mouseMoveEvent(QMouseEvent* event)
{
	QPoint toPos = pos() + event->pos() + QPoint(15, 0);
	if (toPos.x() < fullPixmapRect_.left())
		toPos.setX(fullPixmapRect_.left());
	else if (toPos.x() > fullPixmapRect_.right() - width())
		toPos.setX(fullPixmapRect_.right() - width());

	if (toPos.y() < fullPixmapRect_.top())
		toPos.setY(fullPixmapRect_.top());
	else if (toPos.y() > fullPixmapRect_.bottom() - height())
		toPos.setY(fullPixmapRect_.bottom() - height());

	QWidget::move(toPos);
	emit sigMouseMove(event);
}

void MouseWindow::mouseReleaseEvent(QMouseEvent* event)
{
	emit sigMouseRelease(event);
}
