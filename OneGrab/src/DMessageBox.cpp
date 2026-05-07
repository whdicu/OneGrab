#include "DMessageBox.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QSpacerItem>

const static int TOP_DRAG_SPACE = 40;  // 顶部可拖动标题区域
const static int BORDER_PADDING = 10;  // 左右下padding


int DMessageBox::information(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn)
{
	bool quitOnClose = QApplication::quitOnLastWindowClosed();
	QApplication::setQuitOnLastWindowClosed(false);  // 临时关闭功能：当最后一个窗口关闭时退出程序
	DMessageBox box(parent, title, text, INFORMATION_BOX, btn);
	int result = box.exec();
	QApplication::setQuitOnLastWindowClosed(quitOnClose);
	return result;
}

int DMessageBox::warning(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn)
{
	bool quitOnClose = QApplication::quitOnLastWindowClosed();
	QApplication::setQuitOnLastWindowClosed(false);  // 临时关闭功能：当最后一个窗口关闭时退出程序
	DMessageBox box(parent, title, text, WARNING_BOX, btn);
	int result = box.exec();
	QApplication::setQuitOnLastWindowClosed(quitOnClose);
	return result;
}

int DMessageBox::critical(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn)
{
	bool quitOnClose = QApplication::quitOnLastWindowClosed();
	QApplication::setQuitOnLastWindowClosed(false);  // 临时关闭功能：当最后一个窗口关闭时退出程序
	DMessageBox box(parent, title, text, CRITICAL_BOX, btn);
	int result = box.exec();
	QApplication::setQuitOnLastWindowClosed(quitOnClose);
	return result;
}

DMessageBox::DMessageBox(QWidget* parent, const QString& title, const QString& text, OneMessageBoxType type, OneMessageBoxButton btn)
	: QDialog(parent)
	, isPressed_(false)
	, pressX_(0)
	, pressY_(0)
{
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);

	initUI();

	setWindowTitle(title);
	labelTitle_->setText(title);

	label2_->setText(text);

	connect(btn_accept_, &QPushButton::clicked, this, &QDialog::accept);
	connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);

	if (btn & ACCEPT_BTN)
		btn_accept_->show();
	else
		btn_accept_->hide();

	if (btn & CANCEL_BTN)
		btn_cancel_->show();
	else
		btn_cancel_->hide();
}

DMessageBox::~DMessageBox()
{}

void DMessageBox::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(Qt::white));

	painter.drawRoundedRect(rect(), 20, 20);
}

void DMessageBox::mousePressEvent(QMouseEvent* event)
{
	// 只有顶部 TOP_DRAG_SPACE 像素才是按住可拖动
	if (event->pos().y() > TOP_DRAG_SPACE)
		return;

	isPressed_ = true;
	pressX_ = event->pos().x();
	pressY_ = event->pos().y();
}

void DMessageBox::mouseMoveEvent(QMouseEvent* event)
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

void DMessageBox::mouseReleaseEvent(QMouseEvent* event)
{
	isPressed_ = false;
}

void DMessageBox::initUI()
{
	// 按钮样式
	setStyleSheet(
		"QPushButton"
		"{"
		"	border-radius: 18px;"
		"	border: 2px solid #5c5c66;"
		"	color: #5c5c66;"
		"	background-color: white;"
		"}"
		"QPushButton:hover"
		"{"
		"	background-color: #d5d5d5;"
		"}"
		"#btn_accept"
		"{"
		"	border: none;"
		"	color: white;"
		"	background-color: #ee5555;"
		"}"
		"#btn_accept:hover"
		"{"
		"	color: #bbbbbb;"
		"	background-color: #aa3333;"
		"}"
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

	// title文本标签
	labelTitle_ = new QLabel(this);
	//labelTitle_->setStyleSheet("background-color: blue;");
	labelTitle_->setMinimumHeight(TOP_DRAG_SPACE);
	labelTitle_->setMaximumHeight(TOP_DRAG_SPACE);
	labelTitle_->setFont(font);
	labelTitle_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	labelTitle_->setWordWrap(true);

	// 顶部关闭按钮
	QPushButton* btnClose = new QPushButton;
	connect(btnClose, &QPushButton::clicked, this, &DMessageBox::reject);
	btnClose->setObjectName("btn_close");
	btnClose->setEnabled(true);
	btnClose->setMinimumSize(20, 20);
	btnClose->setMaximumSize(20, 20);

	// title容器
	QHBoxLayout* titleLayout = new QHBoxLayout;
	titleLayout->setSpacing(9);
	titleLayout->setContentsMargins(0, 0, 0, 0);
	titleLayout->addWidget(labelTitle_);
	titleLayout->addWidget(btnClose);

	// 主文本标签
	label2_ = new QLabel(this);
	label2_->setObjectName("label2");
	label2_->setMinimumHeight(40);
	label2_->setFont(font);
	label2_->setAlignment(Qt::AlignCenter);
	label2_->setWordWrap(true);

	// 取消按钮
	btn_cancel_ = new QPushButton(this);
	btn_cancel_->setObjectName("btn_cancel");
	btn_cancel_->setMinimumSize(80, 36);
	btn_cancel_->setMaximumSize(80, 36);
	btn_cancel_->setFont(font);
	btn_cancel_->setCursor(Qt::PointingHandCursor);
	btn_cancel_->setText(tr("取消"));

	// 确定按钮
	btn_accept_ = new QPushButton(this);
	btn_accept_->setObjectName("btn_accept");
	btn_accept_->setEnabled(true);
	btn_accept_->setMinimumSize(80, 36);
	btn_accept_->setMaximumSize(80, 36);
	btn_accept_->setFont(font);
	btn_accept_->setCursor(Qt::PointingHandCursor);
	btn_accept_->setText(tr("确定"));

	QHBoxLayout* horizontalLayout = new QHBoxLayout;
	horizontalLayout->setSpacing(9);
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->addWidget(btn_cancel_);
	horizontalLayout->addWidget(btn_accept_);

	// 主垂直布局
	QVBoxLayout* verticalLayout = new QVBoxLayout;
	verticalLayout->setSpacing(9);
	verticalLayout->setContentsMargins(BORDER_PADDING, 0, BORDER_PADDING, BORDER_PADDING);
	verticalLayout->addLayout(titleLayout);
	verticalLayout->addWidget(label2_);
	verticalLayout->addLayout(horizontalLayout);

	setLayout(verticalLayout);
}
