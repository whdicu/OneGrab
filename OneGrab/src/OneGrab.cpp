#include "OneGrab.h"
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>


OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);

}

void OneGrab::doGrab()
{
	// x y 可以是负数
	QRect screenRect(0, 0, 0, 0);
	QPixmap fullPixmap = getFullPixmap(screenRect);

	ui.label->setPixmap(fullPixmap);

	setGeometry(screenRect);
	show();

	// 保存截图到文件
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	QString filename = QString("OneGrab_%1.png").arg(timestamp);
	QString filepath = "D://" + filename;
	fullPixmap.save(filepath);
}

QPixmap OneGrab::getFullPixmap(QRect& screenRect)
{
	QList<QScreen*> screens = QGuiApplication::screens();

	// 获取容纳所有显示器图片的矩形
	for (QScreen *screen : screens)
	{
		QRect scRect = screen->geometry();
		if (scRect.x() < screenRect.x())
			screenRect.setX(scRect.x());
		if (scRect.y() < screenRect.y())
			screenRect.setY(scRect.y());
		screenRect = screenRect.united(scRect);
	}
	QPixmap combinedPixmap(screenRect.size());
	combinedPixmap.fill(Qt::transparent);

	// 将每个屏幕的内容绘制到 combinedPixmap
	QPainter painter(&combinedPixmap);
	for (QScreen *screen : screens)
	{
		QPixmap pixmap = screen->grabWindow(0);
		painter.drawPixmap(screen->geometry().topLeft() - screenRect.topLeft(), pixmap);
	}
	painter.end();
	return combinedPixmap;
}

void OneGrab::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		break;
	default:
		break;
	}
}

void OneGrab::mousePressEvent(QMouseEvent *)
{

}

void OneGrab::mouseReleaseEvent(QMouseEvent *)
{

}

void OneGrab::mouseMoveEvent(QMouseEvent *)
{

}
