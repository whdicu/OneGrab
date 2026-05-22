#include "LabelIsland5.h"
#include "DMenu.h"
#include "HDBase/DStringList.hpp"
#include "HDQt.h"
#include "ImageThread.h"
#include "SettingHandler.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QSurfaceFormat>


const static int SCALE_ANIMATION_TIME = 150;
const static int BORDER_SIZE = 2;
const static int ORIGIN_SIZE_INDEX = 8;
const static QVector<int> SIZE_V =
{10, 13, 17, 22, 29, 37, 48, 63, 82, 106, 138, 179, 232, 303, 394, 512, 665};
const static DStringList MENU_TEXT = { "存下来", "复制", "变大", "变小", "返回", "关掉" };

LabelIsland5::LabelIsland5(const QPixmap& pixmap, const QPoint& pos, QWidget* parent /*= nullptr*/)
	: QGraphicsView(parent)
	, scene_(new QGraphicsScene(this))
	, origPixmap_(pixmap)
	, sizeIndex_(ORIGIN_SIZE_INDEX)
	, originPixmapSize_(pixmap.size())
	, currentScale_(1.0)
	, borderColor_(Qt::red)
	, sizeLabel_(nullptr)
	, menu_(nullptr)
{
	setObjectName("labelIsland5");
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground, true);
	setAutoFillBackground(false);

	// === 计算 widget 最大尺寸（最大缩放级别），之后 widget 不再 resize ===
	maxScale_ = (qreal)SIZE_V.last() / SIZE_V.at(ORIGIN_SIZE_INDEX);
	originBorderSize_ = originPixmapSize_ + QSize(BORDER_SIZE * 2, BORDER_SIZE * 2);
	maxWidgetSize_ = originPixmapSize_ * maxScale_ + QSize(BORDER_SIZE * 2, BORDER_SIZE * 2);

	// 定位 widget：让 100% 缩放的 图片+边框 的中心与 widget 中心重合，
	// 且图片左上角出现在 pos - BORDER_SIZE
	QPoint widgetTopLeft;
	widgetTopLeft.setX(pos.x() - BORDER_SIZE - (maxWidgetSize_.width() - originBorderSize_.width()) / 2);
	widgetTopLeft.setY(pos.y() - BORDER_SIZE - (maxWidgetSize_.height() - originBorderSize_.height()) / 2);
	setGeometry(QRect(widgetTopLeft, maxWidgetSize_));

	// === OpenGL viewport，带 alpha 通道支持透明背景 ===
	QOpenGLWidget* glWidget = new QOpenGLWidget();
	QSurfaceFormat fmt = glWidget->format();
	fmt.setAlphaBufferSize(8);
	glWidget->setFormat(fmt);
	if (glWidget->isValid())
		setViewport(glWidget);
	else
		delete glWidget;

	setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
	setRenderHint(QPainter::SmoothPixmapTransform, true);
	setFrameShape(QFrame::NoFrame);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setStyleSheet("QGraphicsView#labelIsland5 { background: transparent; border: none; }");

	// === Scene：大小 = 最大内容区域 ===
	setScene(scene_);
	QSizeF maxContentSize = QSizeF(originPixmapSize_) * maxScale_;
	scene_->setSceneRect(QRectF(QPointF(0, 0), maxContentSize));
	scene_->setBackgroundBrush(Qt::transparent);

	// === 边框 item：放在 scene 中心，pixmapItem 作为子 item ===
	borderItem_ = new QGraphicsRectItem();
	borderColor_ = SETTING_HANDLER->getMainColor();
	borderItem_->setPen(QPen(borderColor_, BORDER_SIZE));
	borderItem_->setBrush(Qt::NoBrush);
	QRectF borderRect(0, 0, originBorderSize_.width(), originBorderSize_.height());
	borderItem_->setRect(borderRect);
	QPointF center(maxContentSize.width() / 2, maxContentSize.height() / 2);
	borderItem_->setPos(center);
	borderItem_->setTransformOriginPoint(borderRect.width() / 2, borderRect.height() / 2);
	scene_->addItem(borderItem_);

	// pixmapItem 在边框内部偏移 BORDER_SIZE，作为 borderItem_ 的子项同享缩放
	pixmapItem_ = new QGraphicsPixmapItem(origPixmap_);
	pixmapItem_->setPos(BORDER_SIZE, BORDER_SIZE);
	pixmapItem_->setParentItem(borderItem_);

	// === 缩放动画：只改变 borderItem_ 的 scale，widget 大小不动 ===
	animation_ = new QVariantAnimation;
	animation_->setDuration(SCALE_ANIMATION_TIME);
	animation_->setEasingCurve(QEasingCurve::Linear);
	connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value)
	{
		qreal s = value.toReal();
		currentScale_ = s;
		borderItem_->setScale(s);  // GPU 上完成，widget 不参与
	});
	connect(animation_, &QVariantAnimation::finished, this, [this]()
	{
		if (sizeLabel_)
			sizeLabel_->hide();
	});

	// === 百分比标签 ===
	sizeLabel_ = new QLabel("100%");
	sizeLabel_->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	sizeLabel_->resize(80, 36);
	sizeLabel_->setFont(QFont("Microsoft YaHei UI", 15));
	sizeLabel_->setStyleSheet("QLabel { color: #5c5c66; background-color: white; padding-left: 5px; }");
	sizeLabel_->setAlignment(Qt::AlignCenter);

	// === 右键菜单 ===
	menu_ = new DMenu(MENU_TEXT, this);
	connect(menu_, &DMenu::sigBtnClicked, this, &LabelIsland5::slotBtnClicked);
}

LabelIsland5::~LabelIsland5()
{
	qDebug() << __FUNCTION__;
	if (nullptr != sizeLabel_)
	{
		sizeLabel_->deleteLater();
		sizeLabel_ = nullptr;
	}
}

void LabelIsland5::onRefreshSetting()
{
	borderColor_ = SETTING_HANDLER->getMainColor();
	borderItem_->setPen(QPen(borderColor_, BORDER_SIZE));
	menu_->setBgColor(borderColor_);
}

void LabelIsland5::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_Escape:
		hide();
		emit sigHide();
		break;
	default:
		QGraphicsView::keyPressEvent(event);
		break;
	}
}

void LabelIsland5::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	scale(event->angleDelta().y() > 0);
	sizeLabel_->move(event->globalPosition().toPoint() + QPoint(15, 0));
#else
	scale(event->delta() > 0);
	sizeLabel_->move(event->globalPos() + QPoint(15, 0));
#endif
	sizeLabel_->show();
}

void LabelIsland5::mousePressEvent(QMouseEvent* event)
{
	switch (event->button())
	{
	case Qt::LeftButton:
		pressedPoint_ = event->pos();
		break;
	case Qt::RightButton:
		menu_->show(event->pos());
		break;
	default:
		break;
	}
	menu_->setInLoseFocue(false);
}

void LabelIsland5::mouseMoveEvent(QMouseEvent* event)
{
	Qt::MouseButtons btns = event->buttons();
	if (btns & Qt::LeftButton)
	{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		auto globalPos = event->globalPosition().toPoint();
#else
		auto globalPos = event->globalPos();
#endif
		move(globalPos - pressedPoint_);
	}
}

void LabelIsland5::mouseReleaseEvent(QMouseEvent* event)
{
	Q_UNUSED(event);
}

void LabelIsland5::slotBtnClicked(const QString& text)
{
	// "存下来", "复制", "变大", "变小", "返回", "关掉"
	DSizeType i = MENU_TEXT.indexOf(HDQt::QString2DString(text));
	switch (i)
	{
	case 0:  // 存下来
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
			info.pixmap = origPixmap_;
			info.abPath = fileurl;
			IMAGE_THREAD->addImage(info);
		}
		break;
	}
	case 1:  // 复制
		QApplication::clipboard()->setPixmap(origPixmap_);
		break;
	case 2:  // 变大
	{
		QPoint labelPos = geometry().center() - QPoint(sizeLabel_->width() / 2, sizeLabel_->height() / 2);
		sizeLabel_->move(labelPos);
		sizeLabel_->show();
		scale(true);
		break;
	}
	case 3:  // 变小
	{
		QPoint labelPos = geometry().center() - QPoint(sizeLabel_->width() / 2, sizeLabel_->height() / 2);
		sizeLabel_->move(labelPos);
		sizeLabel_->show();
		scale(false);
		break;
	}
	case 5:  // 关掉
		hide();
		emit sigHide();
		break;
	case 4:  // 返回
	default:
		break;
	}
}

void LabelIsland5::scale(bool bigger)
{
	sizeIndex_ += bigger ? 1 : -1;
	if (sizeIndex_ < 0)
		sizeIndex_ = 0;
	else if (sizeIndex_ > SIZE_V.size() - 1)
		sizeIndex_ = SIZE_V.size() - 1;

	double targetScale = SIZE_V.at(sizeIndex_) / (double)SIZE_V.at(ORIGIN_SIZE_INDEX);
	sizeLabel_->setText(QString("%1%").arg((int)(targetScale * 100)));

	animation_->stop();
	animation_->setStartValue(currentScale_);
	animation_->setEndValue(targetScale);
	animation_->start();
}
