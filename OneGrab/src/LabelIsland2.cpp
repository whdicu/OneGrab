#include "LabelIsland2.h"
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include "SettingHandler.h"


const static int SCALE_ANIMATION_TIME = 160;
const static int BORDER_SIZE = 2;
const static int ORIGIN_SIZE_INDEX = 8;
const static QVector<int> SIZE_V =
//{10, 15, 22, 33, 51, 76, 114, 171, 256, 384, 577, 865/*, 1297, 1946, 2919, 4379*/};
{10, 13, 17, 22, 29, 37, 48, 63, 82, 106, 138, 179, 232, 303, 394, 521, 665};

LabelIsland2::LabelIsland2(const QPixmap& pixmap, const QPoint& pos, QWidget* parent /*= nullptr*/)
	: QGraphicsView(parent)
	, isMove_(false)
	, sizeIndex_(ORIGIN_SIZE_INDEX)
	, sizeLabel_(new QLabel("100%"))
	, scene_(new QGraphicsScene)
{
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

	originRect_.moveTo(pos - QPoint(BORDER_SIZE, BORDER_SIZE));
	originRect_.setSize(pixmap.size() + QSize(BORDER_SIZE, BORDER_SIZE) * 2);

	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	QGraphicsPixmapItem* pixmapItem = scene_->addPixmap(pixmap);
	setScene(scene_);
	pixmapItem->setOffset(-pixmap.width() / 2.0, -pixmap.height() / 2.0);
	//scene_->setSceneRect(originRect_);
	
	setGeometry(originRect_);
	centerOn(0, 0);
	//fitInView(pixmapItem, Qt::KeepAspectRatio);


	animation_ = new QPropertyAnimation(this, "geometry");
	animation_->setDuration(SCALE_ANIMATION_TIME);
	connect(animation_, &QPropertyAnimation::finished, this, [this]()
	{
		sizeLabel_->hide();
	});

	sizeLabel_->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	sizeLabel_->resize(80, 36);
	sizeLabel_->setFont(QFont("Microsoft YaHei UI", 15));
	sizeLabel_->setStyleSheet("QLabel { color: #5c5c66; background-color: white; padding-left: 5px; }");
	sizeLabel_->setAlignment(Qt::AlignCenter);

	onRefreshSetting();
}

LabelIsland2::~LabelIsland2()
{
	qDebug() << __FUNCTION__;
	sizeLabel_->deleteLater();
}

void LabelIsland2::onRefreshSetting()
{
	QColor mainColor = SETTING_HANDLER->getMainColor();
	setStyleSheet(QString("QLabel { border: %1px solid rgb(%2, %3, %4); }").arg(BORDER_SIZE)
		.arg(mainColor.red()).arg(mainColor.green()).arg(mainColor.blue()));
}

void LabelIsland2::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		deleteLater();
		break;
	}
}

void LabelIsland2::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	sizeIndex_ += (event->angleDelta().y() > 0) ? 1 : -1;
#else
	sizeIndex_ += (event->delta() > 0) ? 1 : -1;
#endif
	if (sizeIndex_ < 0)
		sizeIndex_ = 0;
	else if (sizeIndex_ > SIZE_V.size() - 1)
		sizeIndex_ = SIZE_V.size() - 1;

	double scale = SIZE_V.at(sizeIndex_) / (double)SIZE_V.at(ORIGIN_SIZE_INDEX);
	sizeLabel_->setText(QString("%1%").arg((int)(scale * 100)));
	QSize newSize = originRect_.size() * scale;
	QSize dSize = originRect_.size() - newSize;
	

	this->scale(scale, scale); // 仅在限制范围内缩放

	//animation_->stop();
	//animation_->setEasingCurve(QEasingCurve::Linear);
	//animation_->setStartValue(geometry());
	//animation_->setEndValue(QRect(originRect_.topLeft() + QPoint(dSize.width() / 2, dSize.height() / 2), newSize));
	//animation_->start();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	sizeLabel_->move(event->globalPosition().toPoint() + QPoint(15, 0));
#else
	sizeLabel_->move(event->globalPos() + QPoint(15, 0));
#endif
	sizeLabel_->show();

	move(originRect_.topLeft() + QPoint(dSize.width() / 2, dSize.height() / 2));
	resize(newSize);
}

void LabelIsland2::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		isMove_ = true;
		pressPoint_ = event->pos();
	}
}

void LabelIsland2::mouseMoveEvent(QMouseEvent* event)
{
	if (isMove_)
	{
		originRect_.moveTo(originRect_.topLeft() - pressPoint_ + event->pos());
		move(pos() - pressPoint_ + event->pos());
	}
}

void LabelIsland2::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		isMove_ = false;
}
