#include "LabelIsland1.h"
#include "DMenu.h"
#include "HDBase/DStringList.hpp"
#include "HDQt.h"
#include "ImageThread.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QVBoxLayout>
#include "SettingHandler.h"


const static int SCALE_ANIMATION_TIME = 150;
const static int BORDER_SIZE = 2;
const static int ORIGIN_SIZE_INDEX = 8;
const static int BORDER_MIN_PIXEL = 10;  // 边缘至少显示10像素
const static QVector<int> SIZE_V =
//{10, 15, 22, 33, 51, 76, 114, 171, 256, 384, 577, 865/*, 1297, 1946, 2919, 4379*/};
{10, 13, 17, 22, 29, 37, 48, 63, 82, 106, 138, 179, 232, 303, 394, 512, 665};
const static DStringList MENU_TEXT = { "存下来", "复制", "变大", "变小", "返回", "关掉" };

LabelIsland1::LabelIsland1(const QPixmap& pixmap, const QPoint& pos, QWidget* parent /*= nullptr*/)
	: QLabel(parent)
	, sizeIndex_(ORIGIN_SIZE_INDEX)
	, pointToOrigin_(0, 0)
	, sizeLabel_(new QLabel("100%"))
	, menu_(new DMenu(MENU_TEXT, this))
{
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	originRect_.moveTo(pos - QPoint(BORDER_SIZE, BORDER_SIZE));
	originRect_.setSize(pixmap.size() + QSize(BORDER_SIZE, BORDER_SIZE) * 2);
	setScaledContents(true);
	setPixmap(pixmap);
	setGeometry(originRect_);

	animation_ = new QPropertyAnimation(this, "geometry");
	animation_->setDuration(SCALE_ANIMATION_TIME);
	connect(animation_, &QPropertyAnimation::finished, sizeLabel_, &QLabel::hide);
	//connect(animation_, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value)
	//{
	//	update();
	//});

	sizeLabel_->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	sizeLabel_->resize(80, 36);
	sizeLabel_->setFont(QFont("Microsoft YaHei UI", 15));
	sizeLabel_->setStyleSheet("QLabel { color: #5c5c66; background-color: white; padding-left: 5px; }");
	sizeLabel_->setAlignment(Qt::AlignCenter);

	connect(menu_, &DMenu::sigBtnClicked, this, &LabelIsland1::slotBtnClicked);

	onRefreshSetting();
}

LabelIsland1::~LabelIsland1()
{
	qDebug() << __FUNCTION__;
	if (nullptr != sizeLabel_)
	{
		sizeLabel_->deleteLater();
		sizeLabel_ = nullptr;
	}
}

void LabelIsland1::onRefreshSetting()
{
	QColor mainColor = SETTING_HANDLER->getMainColor();
	setStyleSheet(QString("QLabel { border: %1px solid %2; }")
		.arg(BORDER_SIZE).arg(mainColor.name().toUpper()));
	menu_->setBgColor(mainColor);
}

void LabelIsland1::show()
{
	emit sigShow();
	QLabel::show();
}

void LabelIsland1::moveEvent(QMoveEvent* event)
{
	QLabel::moveEvent(event);
	emit sigGeometryChanged();
}

void LabelIsland1::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);
	emit sigGeometryChanged();
}

void LabelIsland1::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		sizeLabel_->hide();
		emit sigHide();
		break;
	}
}

void LabelIsland1::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	scale(event->angleDelta().y() > 0, event->globalPosition().toPoint());
	sizeLabel_->move(event->globalPosition().toPoint() + QPoint(15, 0));
#else
	scale(event->delta() > 0, event->globalPos());
	sizeLabel_->move(event->globalPos() + QPoint(15, 0));
#endif
	sizeLabel_->show();
}

void LabelIsland1::mousePressEvent(QMouseEvent* event)
{
	switch (event->button())
	{
	case Qt::LeftButton:
		pressedPoint_ = event->pos() + pointToOrigin_;
		break;
	case Qt::RightButton:
		menu_->show(event->pos());
		break;
	default:
		break;
	}
	menu_->setInLoseFocue(false);
}

void LabelIsland1::mouseMoveEvent(QMouseEvent* event)
{
	Qt::MouseButtons btns = event->buttons();
	if (btns & Qt::LeftButton)
	{
		// 一种保护，可不用
		// 获取容纳所有显示器图片的矩形
		//QRect okRect;
		//QList<QScreen*> screens = QGuiApplication::screens();
		//for (QScreen *screen : screens)
		//{
		//	QRect scRect = screen->geometry();
		//	if (scRect.x() < okRect.x())
		//		okRect.setX(scRect.x());
		//	if (scRect.y() < okRect.y())
		//		okRect.setY(scRect.y());
		//	okRect = okRect.united(scRect);
		//}

		//okRect.setTop(okRect.top() + BORDER_MIN_PIXEL);
		//okRect.setBottom(okRect.bottom() - BORDER_MIN_PIXEL);
		//okRect.setLeft(okRect.left() + BORDER_MIN_PIXEL);
		//okRect.setRight(okRect.right() - BORDER_MIN_PIXEL);

		// event->pos()的值有时会瞬间变化，导致图片岛漂移
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		auto globalPos = event->globalPosition().toPoint();
#else
		auto globalPos = event->globalPos();
#endif

		originRect_.moveTo(globalPos - pressedPoint_);

		// 一种保护，可不用
		//if (originRect_.top() > okRect.bottom())
		//	originRect_.moveTop(okRect.bottom());
		//else if (originRect_.bottom() < okRect.top())
		//	originRect_.moveBottom(okRect.top());

		//if (originRect_.left() > okRect.right())
		//	originRect_.moveLeft(okRect.right());
		//else if (originRect_.right() < okRect.left())
		//	originRect_.moveRight(okRect.left());

		move(originRect_.topLeft() + pointToOrigin_);
	}
}

void LabelIsland1::mouseReleaseEvent(QMouseEvent* event)
{
	
}

void LabelIsland1::slotBtnClicked(const QString& text)
{
	// "存下来", "复制", "变大", "变小", "返回", "关掉"
	DSizeType i = MENU_TEXT.indexOf(HDQt::QString2DString(text));
	switch (i)
	{
	case 0:
	{
		QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
		QString filename = QString("OneGrab_%1.png").arg(timestamp);

		SettingStruct settingStruct = SETTING_HANDLER->getSettingStruct();
		QString fileurl = QFileDialog::getSaveFileName(this, tr("保存文件"), settingStruct.LastSavePath + '/' + filename);
		if (!fileurl.isEmpty())
		{
			fileurl = fileurl.replace('\\', '/');
			int i = fileurl.lastIndexOf('/');
			settingStruct.LastSavePath = fileurl.mid(0, i);
			SETTING_HANDLER->setSettingStruct(settingStruct);

			ImageInfo info;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			info.pixmap = pixmap();
#else
			info.pixmap = *pixmap();
#endif
			info.abPath = fileurl;
			IMAGE_THREAD->addImage(info);
		}
		break;
	}
	case 1:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QApplication::clipboard()->setPixmap(pixmap());
#else
		QApplication::clipboard()->setPixmap(*pixmap());
#endif
		break;
	case 2:
		sizeLabel_->move(pos() + QPoint((width() - sizeLabel_->width()) / 2, (height() - sizeLabel_->height()) / 2));
		sizeLabel_->show();
		scale(true);
		break;
	case 3:
		sizeLabel_->move(pos() + QPoint((width() - sizeLabel_->width()) / 2, (height() - sizeLabel_->height()) / 2));
		sizeLabel_->show();
		scale(false);
		break;
	case 5:
		hide();
		sizeLabel_->hide();
		emit sigHide();
		break;
	case 4:
	default:
		break;
	}
}

void LabelIsland1::scale(bool bigger, const QPoint& mouseGlobalPos /*= QPoint(-1, -1)*/)
{
	int newIndex = sizeIndex_ + (bigger ? 1 : -1);
	if (newIndex < 0)
		newIndex = 0;
	else if (newIndex > SIZE_V.size() - 1)
		newIndex = SIZE_V.size() - 1;

	if (newIndex == sizeIndex_)
		return;

	sizeIndex_ = newIndex;

	double scale = SIZE_V.at(sizeIndex_) / (double)SIZE_V.at(ORIGIN_SIZE_INDEX);
	sizeLabel_->setText(QString("%1%").arg((int)(scale * 100)));
	QSize newSize = originRect_.size() * scale;

	if (mouseGlobalPos.x() >= 0 && mouseGlobalPos.y() >= 0)
	{
		// 鼠标锚点缩放：缩放前后，鼠标指向的图片位置在屏幕上不变
		QPoint curTopLeft = geometry().topLeft();
		QSize curSize = geometry().size();

		double fx = curSize.width() > 0
			? (double)(mouseGlobalPos.x() - curTopLeft.x()) / curSize.width()
			: 0.5;
		double fy = curSize.height() > 0
			? (double)(mouseGlobalPos.y() - curTopLeft.y()) / curSize.height()
			: 0.5;

		QPoint newTopLeft(
			mouseGlobalPos.x() - (int)(fx * newSize.width()),
			mouseGlobalPos.y() - (int)(fy * newSize.height())
		);
		pointToOrigin_ = newTopLeft - originRect_.topLeft();
	}
	else
	{
		// 无鼠标位置时（如菜单按钮），居中对齐缩放
		QSize dSize = originRect_.size() - newSize;
		pointToOrigin_ = QPoint(dSize.width() / 2, dSize.height() / 2);
	}

	animation_->stop();
	animation_->setEasingCurve(QEasingCurve::Linear);
	animation_->setStartValue(geometry());
	animation_->setEndValue(QRect(originRect_.topLeft() + pointToOrigin_, newSize));
	animation_->start();
}
