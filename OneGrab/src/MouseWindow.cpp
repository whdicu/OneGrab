#include "MouseWindow.h"
#include <QDebug>
#include <QMouseEvent>


const static int MARGIN_TO_MOUSE = 15;


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
	// 左上到右下
	auto func1 = [&](int x)
	{
		return fullPixmapRect.height() * x * 1.0 / fullPixmapRect.width();
	};
	// 坐下到右上
	auto func2 = [&](int x)
	{
		return fullPixmapRect.height() * x * -1.0 / fullPixmapRect.width() + fullPixmapRect.height();
	};
	// 右下到左上（func1 的逆：根据 y 求 x）
	auto func3 = [&](int y)
	{
		return fullPixmapRect.width() * y * 1.0 / fullPixmapRect.height();
	};
	// 右上到左下（func2 的逆：根据 y 求 x）
	auto func4 = [&](int y)
	{
		return fullPixmapRect.width() * (fullPixmapRect.height() - y) * 1.0 / fullPixmapRect.height();
	};

	// 判断鼠标在两条对角线切割出的四个区域中的哪个
	double y1 = func1(pos.x());
	double y2 = func2(pos.x());
	double x1 = func3(pos.y());
	double x2 = func4(pos.y());
	int region = (pos.y() < y1) ? 0 : 2;
	region |= (pos.y() < y2) ? 0 : 1;
	// region: 0=上(两线之上)  1=右(y1下y2上)  2=左(y1上y2下)  3=下(两线之下)
	//qDebug() << "region2:" << region;

	QPoint p;
	switch (region)
	{
	case 0:  // 上
	{
		int dx = (pos.x() - x1) * (width() + MARGIN_TO_MOUSE * 2) / (x2 - x1);
		p.setX(pos.x() - dx);
		p.setY(pos.y() + MARGIN_TO_MOUSE);
		break;
	}
	case 1:  // 右
	{
		int dy = (pos.y() - y2) * (height() + MARGIN_TO_MOUSE * 2) / (y1 - y2);
		p.setX(pos.x() - width() - MARGIN_TO_MOUSE);
		p.setY(pos.y() - dy);
		break;
	}
	case 2:  // 左
	{
		int dy = (pos.y() - y1) * (height() + MARGIN_TO_MOUSE * 2) / (y2 - y1);
		p.setX(pos.x() + MARGIN_TO_MOUSE);
		p.setY(pos.y() - dy);
		break;
	}
	default:  // 3 下
	{
		int dx = (pos.x() - x2) * (width() + MARGIN_TO_MOUSE * 2) / (x1 - x2);
		p.setX(pos.x() - dx);
		p.setY(pos.y() - height() - MARGIN_TO_MOUSE);
		break;
	}
	}


	//int yy = pos.y() * (height() + 15 * 2) * 1.0 / fullPixmapRect.height();
	//qDebug() << yy << pos.y() << height() << fullPixmapRect.height();
	//QPoint p = QPoint(pos.x() + 15, pos.y() - 15 - yy);



	fullPixmapRect_ = fullPixmapRect;
	QPoint toPos = p + fullPixmapRect.topLeft();
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
