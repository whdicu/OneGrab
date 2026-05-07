#include "DProgressBox.h"
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>

const static int TOP_DRAG_SPACE = 40;  // 顶部可拖动标题区域
const static int BORDER_PADDING = 10;  // 左右下padding


DProgressBox::DProgressBox(QWidget* parent, const QString& title, const QString& text)
	: QDialog(parent)
	, progress_(0.0)
	, autoCloseOnComplete_(true)
	, isPressed_(false)
	, pressX_(0)
	, pressY_(0)
{
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);

	initUI();

	setWindowTitle(title);
	labelTitle_->setText(title);
	labelText_->setText(text);
}

DProgressBox::~DProgressBox()
{}

void DProgressBox::setAutoCloseOnComplete(bool autoClose)
{
	autoCloseOnComplete_ = autoClose;
}

void DProgressBox::setProgress(double percent)
{
	progress_ = percent;

	// QProgressBar 以 0~10000 范围实现两位小数精度
	progressBar_->setValue(static_cast<int>(percent * 100));
	labelPercent_->setText(QString("%1%").arg(percent, 0, 'f', 2));

	if (autoCloseOnComplete_ && percent >= 100.0)
		accept();
}

void DProgressBox::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(Qt::white));

	painter.drawRoundedRect(rect(), 20, 20);
}

void DProgressBox::mousePressEvent(QMouseEvent* event)
{
	if (event->pos().y() > TOP_DRAG_SPACE)
		return;

	isPressed_ = true;
	pressX_ = event->pos().x();
	pressY_ = event->pos().y();
}

void DProgressBox::mouseMoveEvent(QMouseEvent* event)
{
	if (isPressed_)
	{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QPoint globalPos = event->globalPosition().toPoint();
#else
		QPoint globalPos = event->globalPos();
#endif
		move(globalPos.x() - pressX_, globalPos.y() - pressY_);
	}
}

void DProgressBox::mouseReleaseEvent(QMouseEvent* event)
{
	isPressed_ = false;
}

void DProgressBox::initUI()
{
	setStyleSheet(
		"#btn_close"
		"{"
		"	border: none;"
		"	border-radius: 10px;"
		"	background-color: #ee5555;"
		"}"
		"#btn_close:hover"
		"{"
		"	background-color: #aa3333;"
		"}"
		"#widget"
		"{"
		"	background-color: transparent;"
		"}"
	);

	QFont font("Microsoft YaHei UI", 10);

	// 标题
	labelTitle_ = new QLabel(this);
	labelTitle_->setMinimumHeight(TOP_DRAG_SPACE);
	labelTitle_->setMaximumHeight(TOP_DRAG_SPACE);
	labelTitle_->setFont(font);
	labelTitle_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	// 关闭按钮
	QPushButton* btnClose = new QPushButton;
	connect(btnClose, &QPushButton::clicked, this, &DProgressBox::reject);
	btnClose->setObjectName("btn_close");
	btnClose->setEnabled(true);
	btnClose->setMinimumSize(20, 20);
	btnClose->setMaximumSize(20, 20);

	// 标题容器
	QHBoxLayout* titleLayout = new QHBoxLayout;
	titleLayout->setSpacing(9);
	titleLayout->setContentsMargins(0, 0, 0, 0);
	titleLayout->addWidget(labelTitle_);
	titleLayout->addWidget(btnClose);

	// 文本标签
	labelText_ = new QLabel(this);
	labelText_->setObjectName("labelText");
	labelText_->setFont(font);
	labelText_->setAlignment(Qt::AlignCenter);
	labelText_->setWordWrap(true);

	// 进度百分比文本
	labelPercent_ = new QLabel(this);
	labelPercent_->setObjectName("labelPercent");
	labelPercent_->setFont(font);
	labelPercent_->setAlignment(Qt::AlignCenter);
	labelPercent_->setText("0.00%");

	// 进度条
	progressBar_ = new QProgressBar(this);
	progressBar_->setObjectName("progressBar");
	progressBar_->setFixedHeight(10);
	progressBar_->setTextVisible(false);
	progressBar_->setRange(0, 10000);
	progressBar_->setValue(0);
	progressBar_->setStyleSheet(
		"QProgressBar"
		"{"
		"	border: 1px solid #5c5c66;"
		"	border-radius: 5px;"
		"	background-color: white;"
		"}"
		"QProgressBar::chunk"
		"{"
		"	background-color: #ee5555;"
		"	border-radius: 4px;"
		"}"
	);

	// 主垂直布局
	QVBoxLayout* verticalLayout = new QVBoxLayout;
	verticalLayout->setSpacing(9);
	verticalLayout->setContentsMargins(BORDER_PADDING, 0, BORDER_PADDING, BORDER_PADDING);
	verticalLayout->addLayout(titleLayout);
	verticalLayout->addWidget(labelText_);
	verticalLayout->addWidget(labelPercent_);
	verticalLayout->addWidget(progressBar_);

	setLayout(verticalLayout);
}
