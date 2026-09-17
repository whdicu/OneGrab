#include "MouseWindow.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include "WindowsGlassEffect.h"


const static int MARGIN_TO_MOUSE = 15;

// 毛玻璃调参：改下面这几行就够了（和 AITalkWidget 同一套）
// GLASS_TINT 按当前模式调好深浅之后由 paintEvent 铺满整块窗口（放大图盖在上面不受影响）
const static QColor GLASS_TINT = QColor(255, 255, 255, 60);   // 颜色 + 深浅（alpha 越小越透、模糊越明显）
// 模糊档位：这个窗口在取色拖动时每帧都在移动，Win10 上只有 BlurLight 跟得上手
// （BlurStrong 是 Acrylic，窗口一动 DWM 就要重算模糊，会明显拖不动）
const static WindowsGlassEffect::BlurLevel GLASS_BLUR_LEVEL = WindowsGlassEffect::BlurLight;
const static bool GLASS_DARK_TITLE_BAR = false;                // 浅色玻璃要设 false，否则系统 backdrop 底色发黑
// 窗口形状圆角：0 = 直角，跟 .ui 的设计一致（放大图铺满整块，不该被切角）
const static int GLASS_CORNER_RADIUS = 0;


MouseWindow::MouseWindow(QWidget* parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, fullPixmapRect_(0, 0, 0, 0)
	, isNumColor_(false)
{
	ui.setupUi(this);
	// 毛玻璃：放大图那块要保持锐利，只有底部信息条是磨砂的（底色由 paintEvent 用 GLASS_TINT 铺）
	setAttribute(Qt::WA_TranslucentBackground);
	WindowsGlassEffect::Params glassParams;
	glassParams.blurLevel = GLASS_BLUR_LEVEL;
	glassParams.tint = GLASS_TINT;
	glassParams.darkTitleBar = GLASS_DARK_TITLE_BAR;
	glassParams.cornerRadius = GLASS_CORNER_RADIUS;
	const WindowsGlassEffect::Result glassResult = WindowsGlassEffect::enable(this, glassParams);
	qDebug() << __FUNCTION__ << "glass result =" << (int)glassResult
		<< "mode =" << (int)WindowsGlassEffect::mode(this);

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
		int denom = x2 - x1;
		int dx = (denom != 0) ? (pos.x() - x1) * (width() + MARGIN_TO_MOUSE * 2) / denom : 0;
		p.setX(pos.x() - dx + MARGIN_TO_MOUSE);
		p.setY(pos.y() + MARGIN_TO_MOUSE);
		break;
	}
	case 1:  // 右
	{
		int denom = y1 - y2;
		int dy = (denom != 0) ? (pos.y() - y2) * (height() + MARGIN_TO_MOUSE * 2) / denom : 0;
		p.setX(pos.x() - width() - MARGIN_TO_MOUSE);
		p.setY(pos.y() - dy + MARGIN_TO_MOUSE);
		break;
	}
	case 2:  // 左
	{
		int denom = y2 - y1;
		int dy = (denom != 0) ? (pos.y() - y1) * (height() + MARGIN_TO_MOUSE * 2) / denom : 0;
		p.setX(pos.x() + MARGIN_TO_MOUSE);
		p.setY(pos.y() - dy + MARGIN_TO_MOUSE);
		break;
	}
	default:  // 3 下
	{
		int denom = x1 - x2;
		int dx = (denom != 0) ? (pos.x() - x2) * (width() + MARGIN_TO_MOUSE * 2) / denom : 0;
		p.setX(pos.x() - dx + MARGIN_TO_MOUSE);
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

void MouseWindow::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	// 窗口底色：深浅由 appTint 按当前玻璃模式算好（没有原生效果时它会自动压深保证可读）。
	// 放大图是 label_img 盖在上面画的，所以这里铺满整块不会影响它的清晰度
	const QColor tint = WindowsGlassEffect::appTint(this);

	QPainter painter(this);
	painter.setPen(Qt::NoPen);
	painter.setBrush(tint);
	painter.drawRect(rect());
}

void MouseWindow::mousePressEvent(QMouseEvent* event)
{
	emit sigMousePress(event);
}

void MouseWindow::mouseMoveEvent(QMouseEvent* event)
{
	emit sigMouseMove(event);
}

void MouseWindow::mouseReleaseEvent(QMouseEvent* event)
{
	emit sigMouseRelease(event);
}
