#include "GlassBlurBackground.h"
#include <QDebug>
#include <QEvent>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPath>


namespace
{
	const char* const LAYER_NAME = "GlassBlurBackgroundLayer";

	// Qt 自带的高斯模糊：走 QGraphicsBlurEffect，先降采样再放大，避免大图卡界面
	QPixmap blurPixmap(const QPixmap& source, int radius, int downsample)
	{
		if (source.isNull() || radius <= 0)
			return source;

		const int scale = qBound(1, downsample, 8);
		const QPixmap small = scale > 1
			? source.scaled(source.size() / scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
			: source;

		QGraphicsScene scene;
		QGraphicsPixmapItem* item = new QGraphicsPixmapItem(small);   // scene 接管所有权
		QGraphicsBlurEffect* blur = new QGraphicsBlurEffect;
		// 降采样过的图，半径也要跟着缩，否则糊得过分
		blur->setBlurRadius(qMax(1.0, radius / static_cast<qreal>(scale)));
		blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
		item->setGraphicsEffect(blur);
		scene.addItem(item);

		QPixmap result(small.size());
		result.fill(Qt::transparent);
		QPainter painter(&result);
		scene.render(&painter, QRectF(), QRectF(QPointF(0, 0), small.size()));
		painter.end();

		return scale > 1
			? result.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
			: result;
	}
}


GlassBlurLayer::GlassBlurLayer(QWidget* source, const GlassBlurBackground::Params& params)
	: QWidget(source)
	, source_(source)
	, params_(params)
{
	setObjectName(LAYER_NAME);
	setAttribute(Qt::WA_TransparentForMouseEvents);   // 不吃鼠标事件
	setAttribute(Qt::WA_NoSystemBackground);
	setFocusPolicy(Qt::NoFocus);
	lower();                                          // 永远在其它兄弟控件下面
}

void GlassBlurLayer::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event)

	if (cache_.isNull())
		return;

	QPainter painter(this);
	if (params_.cornerRadius > 0)
	{
		QPainterPath path;
		path.addRoundedRect(rect(), params_.cornerRadius, params_.cornerRadius);
		painter.setClipPath(path);
	}

	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawPixmap(-offset_, cache_);
}

void GlassBlurLayer::rebuild()
{
	if (rebuilding_ || !source_ || source_->size().isEmpty())
		return;

	rebuilding_ = true;

	// 1. 取控件后方的父控件背景。只渲染父控件自己的背景（不带 children），
	//    既不会把本控件渲染进去，也不会造成重绘递归
	QWidget* behind = source_->parentWidget();
	const QRect region = source_->geometry();
	// 向外多截一圈再裁回来，避免模糊在控件边缘处出现一圈发暗的软边
	const int pad = qMax(0, params_.blurRadius);
	QRect grabRect = region.adjusted(-pad, -pad, pad, pad);
	if (behind)
		grabRect = grabRect.intersected(behind->rect());
	offset_ = region.topLeft() - grabRect.topLeft();

	QPixmap base(grabRect.size());
	base.fill(behind ? behind->palette().color(QPalette::Window) : Qt::transparent);
	if (behind)
	{
		behind->render(&base, QPoint(0, 0), grabRect
			, QWidget::DrawWindowBackground | QWidget::IgnoreMask);
	}

	// 2. 模糊 + tint
	cache_ = blurPixmap(base, params_.blurRadius, params_.downsample);
	if (params_.tint.alpha() > 0)
	{
		QPainter painter(&cache_);
		painter.fillRect(cache_.rect(), params_.tint);
	}

	rebuilding_ = false;

	qDebug() << __FUNCTION__ << "rebuilt" << cache_.size() << offset_;
	update();
}


GlassBlurBackground::GlassBlurBackground(QWidget* widget, const Params& params)
	: QObject(widget)
	, widget_(widget)
	, params_(params)
{
}

void GlassBlurBackground::buildLayer()
{
	if (!widget_)
		return;

	if (GlassBlurLayer* old = widget_->findChild<GlassBlurLayer*>(LAYER_NAME))
		old->deleteLater();

	GlassBlurLayer* layer = new GlassBlurLayer(widget_, params_);
	layer->setGeometry(widget_->rect());
	layer->lower();
	layer->show();
	layer_ = layer;
	layer->rebuild();
}

void GlassBlurBackground::invalidate()
{
	if (!widget_ || !layer_)
		return;

	layer_->setGeometry(widget_->rect());
	layer_->lower();
	layer_->rebuild();
}

bool GlassBlurBackground::attach(QWidget* widget, const Params& params)
{
	if (!widget)
	{
		qWarning() << __FUNCTION__ << "widget is null";
		return false;
	}

	if (widget->isWindow())
	{
		qWarning() << __FUNCTION__ << "top level window should use WindowsGlassEffect";
		return false;
	}

	if (widget->findChild<GlassBlurBackground*>())
	{
		qWarning() << __FUNCTION__ << "already attached";
		return true;
	}

	GlassBlurBackground* self = new GlassBlurBackground(widget, params);
	widget->installEventFilter(self);
	self->buildLayer();

	qDebug() << __FUNCTION__ << "attached";
	return true;
}

void GlassBlurBackground::detach(QWidget* widget)
{
	if (!widget)
		return;

	if (GlassBlurBackground* self = widget->findChild<GlassBlurBackground*>())
	{
		widget->removeEventFilter(self);
		self->deleteLater();
	}

	if (GlassBlurLayer* layer = widget->findChild<GlassBlurLayer*>(LAYER_NAME))
		layer->deleteLater();

	qDebug() << __FUNCTION__ << "detached";
}

void GlassBlurBackground::refresh(QWidget* widget)
{
	if (!widget)
		return;

	if (GlassBlurBackground* self = widget->findChild<GlassBlurBackground*>())
		self->invalidate();
}

bool GlassBlurBackground::isAttached(const QWidget* widget)
{
	return widget && widget->findChild<GlassBlurBackground*>() != nullptr;
}

bool GlassBlurBackground::eventFilter(QObject* watched, QEvent* event)
{
	if (!widget_)
		return QObject::eventFilter(watched, event);

	switch (event->type())
	{
	case QEvent::Resize:
	case QEvent::Move:
	case QEvent::Show:
	case QEvent::ParentChange:
		invalidate();
		break;
	case QEvent::Hide:
		if (layer_)
			layer_->hide();
		break;
	default:
		break;
	}

	return QObject::eventFilter(watched, event);
}
