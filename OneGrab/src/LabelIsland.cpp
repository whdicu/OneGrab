#include "LabelIsland.h"
#include <QDebug>
#include <QLabel>
#include <QMouseEvent>


const static int BORDER_SIZE = 2;

LabelIsland::LabelIsland(const QPixmap& pixmap, QWidget* parent /*= nullptr*/)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, isMove_(false)
{
	setStyleSheet(QString("QLabel { border: %1px solid #ff6666; }").arg(BORDER_SIZE));

	QSize scIncludeBorder = pixmap.size() + QSize(BORDER_SIZE, BORDER_SIZE) * 2;
	QLabel* label = new QLabel(this);
	label->resize(scIncludeBorder);
	label->setPixmap(pixmap);
	resize(scIncludeBorder);
}

LabelIsland::~LabelIsland()
{
	qDebug() << __FUNCTION__;
}

void LabelIsland::move(QPoint toPoint)
{
	QWidget::move(toPoint.x() - BORDER_SIZE, toPoint.y() - BORDER_SIZE);
}

void LabelIsland::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		deleteLater();
		break;
	}
}

void LabelIsland::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		isMove_ = true;
		pressPoint_ = event->pos();
	}
}

void LabelIsland::mouseMoveEvent(QMouseEvent* event)
{
	if (isMove_)
	{
		move(pos() - pressPoint_ + event->pos());
	}
}

void LabelIsland::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		isMove_ = false;
}
