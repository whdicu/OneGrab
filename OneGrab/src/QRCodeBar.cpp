#include "QRCodeBar.h"
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>


static void setupChildMouseTracking(QObject* filterObj, QWidget* parent)
{
	for (QObject* child : parent->children())
	{
		if (child->isWidgetType())
		{
			QWidget* w = static_cast<QWidget*>(child);
			w->installEventFilter(filterObj);
			w->setMouseTracking(true);
			setupChildMouseTracking(filterObj, w);  // 递归处理孙子控件
		}
	}
}


QRCodeBar::QRCodeBar(QWidget* parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	// 给所有子控件递归安装事件过滤器 + 启用鼠标追踪，拦截鼠标移动事件
	setupChildMouseTracking(this, this);
}

QRCodeBar::~QRCodeBar()
{
}

void QRCodeBar::onFinishGrab()
{
	hide();
}

void QRCodeBar::setQRCodeTexts(const QStringList& texts)
{
	qDebug() << __FUNCTION__ << "QR codes:" << texts;

	clearQRCodeTexts();

	for (const QString& text : texts)
	{
		QWidget* widget = makeOneBar(text);
		layout()->addWidget(widget);
		// addWidget 后控件要等事件循环才被真正显示，这期间布局会把它当成空项(QWidgetItem::isEmpty)，
		// 导致立刻读 sizeHint/adjustSize 全是 0（定位就会错），所以这里主动显示一次
		widget->show();
	}
}

void QRCodeBar::clearQRCodeTexts()
{
	QLayoutItem* child;
	while (child = layout()->itemAt(0))
	{
		layout()->removeItem(child);
		if (child->widget())
		{
			child->widget()->hide();
			child->widget()->deleteLater();
		}
		else if (child->layout())
			child->layout()->deleteLater();
		else if (child->spacerItem())
			delete child->spacerItem();
	}
}

QWidget* QRCodeBar::makeOneBar(const QString& text)
{
	QLabel* label = new QLabel(text);
	label->setFont(QFont("Microsoft YaHei UI", 9));

	QPushButton* btnUrl = new QPushButton;
	btnUrl->setFlat(true);
	btnUrl->setFixedSize(32, 32);
	btnUrl->setIconSize(QSize(24, 24));
	btnUrl->setIcon(QIcon(":/svgs/url.svg"));
	connect(btnUrl, &QPushButton::clicked, this, [this, text]()
	{
		QDesktopServices::openUrl(QUrl(text));
		emit sigNeedFinishGrab();
	});

	QPushButton* btnCopy = new QPushButton;
	btnCopy->setFlat(true);
	btnCopy->setFixedSize(32, 32);
	btnCopy->setIconSize(QSize(24, 24));
	btnCopy->setIcon(QIcon(":/svgs/copy.svg"));
	connect(btnCopy, &QPushButton::clicked, this, [this, text]()
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(text);
		emit sigNeedFinishGrab();
	});

	QHBoxLayout* btnLayout = new QHBoxLayout;
	btnLayout->setSpacing(4);
	btnLayout->setContentsMargins(10, 2, 2, 2);
	btnLayout->addWidget(label);
	btnLayout->addWidget(btnUrl);
	btnLayout->addWidget(btnCopy);

	QWidget* whiteWidget = new QWidget;
	whiteWidget->setFixedHeight(36);
	whiteWidget->setLayout(btnLayout);

	QHBoxLayout* layout = new QHBoxLayout;
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->addStretch();
	layout->addWidget(whiteWidget);

	QWidget* widget = new QWidget;
	widget->setLayout(layout);
	//widget->setStyleSheet("QLabel{ background: red;}");
	return widget;
}

void QRCodeBar::enterEvent(QEvent* event)
{
	emit sigMouseEnter();
}

void QRCodeBar::mouseMoveEvent(QMouseEvent* event)
{
	// 将鼠标在 BtnBar 上的坐标转为屏幕坐标，供 OneGrab 更新 MouseWindow 位置
	emit sigMouseMoveGlobal(mapToGlobal(event->pos()));
}

bool QRCodeBar::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::MouseMove)
	{
		QMouseEvent* me = static_cast<QMouseEvent*>(event);
		//qDebug() << __FUNCTION__ << me->globalPos();
		emit sigMouseMoveGlobal(me->globalPos());
	}
	return QWidget::eventFilter(watched, event);
}
