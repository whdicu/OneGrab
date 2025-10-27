#include "LabelIsland.h"
#include "DMenu.h"
#include "HDBase/DStringList.hpp"
#include "HDQt/HD2QT.hpp"
#include "ImageThread.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include "SettingHandler.h"


const static int SCALE_ANIMATION_TIME = 150;
const static int BORDER_SIZE = 2;
const static int ORIGIN_SIZE_INDEX = 8;
const static QVector<int> SIZE_V =
//{10, 15, 22, 33, 51, 76, 114, 171, 256, 384, 577, 865/*, 1297, 1946, 2919, 4379*/};
{10, 13, 17, 22, 29, 37, 48, 63, 82, 106, 138, 179, 232, 303, 394, 512, 665};
const static DStringList MENU_TEXT = { "存下来", "复制", "变大", "变小", "返回", "关掉" };

LabelIsland::LabelIsland(const QPixmap& pixmap, const QPoint& pos, QWidget* parent /*= nullptr*/)
	: QLabel(parent)
	, sizeIndex_(ORIGIN_SIZE_INDEX)
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

	connect(menu_, &DMenu::sigBtnClicked, this, &LabelIsland::slotBtnClicked);

	onRefreshSetting();
}

LabelIsland::~LabelIsland()
{
	qDebug() << __FUNCTION__;
	sizeLabel_->deleteLater();
}

void LabelIsland::onRefreshSetting()
{
	QColor mainColor = SETTING_HANDLER->getMainColor();
	setStyleSheet(QString("QLabel { border: %1px solid %2; }")
		.arg(BORDER_SIZE).arg(mainColor.name().toUpper()));
	menu_->setBgColor(mainColor);
}

void LabelIsland::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		break;
	}
}

void LabelIsland::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	scale(event->angleDelta().y() > 0);
	sizeLabel_->move(event->globalPosition().toPoint() + QPoint(15, 0));
#else
	scale(event->delta() > 0);
	sizeLabel_->move(event->globalPos() + QPoint(15, 0));
#endif
	sizeLabel_->show();

	//move(originRect_.topLeft() + QPoint(dSize.width() / 2, dSize.height() / 2));
	//resize(newSize);
}

void LabelIsland::mousePressEvent(QMouseEvent* event)
{
	switch (event->button())
	{
	case Qt::LeftButton:
		pressPoint_ = event->pos();
		break;
	case Qt::RightButton:
		menu_->show(event->pos());
		break;
	default:
		break;
	}
	menu_->setInLoseFocue(false);
}

void LabelIsland::mouseMoveEvent(QMouseEvent* event)
{
	Qt::MouseButtons btns = event->buttons();
	if (btns & Qt::LeftButton)
	{
		originRect_.moveTo(originRect_.topLeft() - pressPoint_ + event->pos());
		move(pos() - pressPoint_ + event->pos());
	}
}

void LabelIsland::mouseReleaseEvent(QMouseEvent* event)
{
	
}

void LabelIsland::slotBtnClicked(const QString& text)
{
	// "存下来", "复制", "变大", "变小", "返回", "关掉"
	DSizeType i = MENU_TEXT.indexOf(HD2QT::QString2DString(text));
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
		deleteLater();
		break;
	case 4:
	default:
		break;
	}
}

void LabelIsland::scale(bool bigger)
{
	sizeIndex_ += bigger ? 1 : -1;
	if (sizeIndex_ < 0)
		sizeIndex_ = 0;
	else if (sizeIndex_ > SIZE_V.size() - 1)
		sizeIndex_ = SIZE_V.size() - 1;

	double scale = SIZE_V.at(sizeIndex_) / (double)SIZE_V.at(ORIGIN_SIZE_INDEX);
	sizeLabel_->setText(QString("%1%").arg((int)(scale * 100)));
	QSize newSize = originRect_.size() * scale;
	QSize dSize = originRect_.size() - newSize;

	animation_->stop();
	animation_->setEasingCurve(QEasingCurve::Linear);
	animation_->setStartValue(geometry());
	animation_->setEndValue(QRect(originRect_.topLeft() + QPoint(dSize.width() / 2, dSize.height() / 2), newSize));
	animation_->start();
}
