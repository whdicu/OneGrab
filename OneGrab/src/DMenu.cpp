#include "DMenu.h"
#include "HDQt/HD2QT.hpp"
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QPropertyAnimation>
#include <QApplication>
#include <windows.h>

const static int TIME500 = 500;
const static int TIME300 = 300;
const static int TIME200 = 200;
const static int TIME150 = 150;
const static QString QSS_STYLE = " \
#menu_widget { \
    background-color: white; \
    border: 1px solid #5c5c66; \
    border-radius: 15px; \
} QPushButton { \
    border-radius: 10px; \
    background-color: %1; \
    color: white; \
}";

void show_top(WId winId)
{
#ifdef Q_OS_WIN32  // windows必须加这个，不然windows10 会不起作用，具体参看activateWindow 函数的文档
    HWND hForgroundWnd = GetForegroundWindow();
    DWORD dwForeID = ::GetWindowThreadProcessId(hForgroundWnd, NULL);
    DWORD dwCurID = ::GetCurrentThreadId();
    ::AttachThreadInput(dwCurID, dwForeID, TRUE);
    ::SetForegroundWindow((HWND)winId);
    ::AttachThreadInput(dwCurID, dwForeID, FALSE);
#endif
}

DMenu::DMenu(const DStringList& texts, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Drawer | Qt::WindowStaysOnTopHint)
    , widget_(new QWidget(this))
    , is_hidden_(true)
    , inLoseFocue_(false)
	, animateLabel_(new QLabel)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);

	animateLabel_->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
	animateLabel_->setAttribute(Qt::WA_TranslucentBackground);
	animateLabel_->setScaledContents(true);

    animation_ = new QPropertyAnimation(animateLabel_, "geometry");
	animation_->setDuration(TIME150);
    animation_->setEasingCurve(QEasingCurve::InOutQuad);
	connect(animation_, &QPropertyAnimation::finished, this, [this]()
	{
		// 在label hide前进行show，防止闪烁
		if (!is_hidden_)
			QWidget::show();

		animateLabel_->hide();

		// 在label hide之后，防止闪烁
		if (!is_hidden_)
			setFocus();

		setInLoseFocue(false);
	});

    move(QCursor::pos());
    widget_->move(0, 0);
    widget_->resize(130, 5 + 35 * texts.size());

    QVBoxLayout* lay = new QVBoxLayout;
    lay->setContentsMargins(5, 5, 5, 5);
    lay->setSpacing(5);
    widget_->setLayout(lay);
    widget_->setObjectName("menu_widget");

    for (const DString& text : texts)
    {
        QPushButton* btn = new QPushButton(HD2QT::DString2QString(text));
		btn->setFont(QFont("Microsoft YaHei UI", 10));
        connect(btn, &QPushButton::clicked, this, [this, text]()
        {
            emit sigBtnClicked(HD2QT::DString2QString(text));
            animateHide(true, true);
        });
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(30);
        lay->addWidget(btn);
    }
}

DMenu::~DMenu()
{
	if (animateLabel_)
		animateLabel_->deleteLater();
}

void DMenu::animateShow()
{
	animateLabel_->setPixmap(grab());

    animation_->stop();
    animateLabel_->show();
    is_hidden_ = false;
	move(QCursor::pos());
    int newx = QCursor::pos().x();
    int newy = QCursor::pos().y();

    //qDebug() << newx << newy << widget_->width() << widget_->height();
    animation_->setStartValue(QRect(newx, newy, 0, 0));
    animation_->setEndValue(QRect(newx, newy, widget_->width(), widget_->height()));
    animation_->start();
}

void DMenu::animateHide(bool setIsHidden, bool showTop)
{
    animation_->stop();
	animateLabel_->show();
	if (showTop)
		show_top(animateLabel_->winId());
	hide();
	if (setIsHidden)
		is_hidden_ = true;

    int newx = QCursor::pos().x();
    int newy = QCursor::pos().y();

    animation_->setStartValue(QRect(x(), y(), animateLabel_->width(), animateLabel_->height()));
    animation_->setEndValue(QRect(newx, newy, 0, 0));
    animation_->start();
}

bool DMenu::setFocus()
{
    show_top(winId());  // 防止setFocus失效
    QWidget::setFocus();
    return hasFocus();
}

void DMenu::show(QPoint pos)
{
	if (inLoseFocue_)
		return;

    if (isHidden())
        animateShow();
}

void DMenu::setBgColor(const QColor& color)
{
	setStyleSheet(QSS_STYLE.arg(color.name().toUpper()));
}

void DMenu::focusOutEvent(QFocusEvent* event)
{
    if (event->reason() == Qt::MouseFocusReason)
    {
//        qDebug() << "点击了菜单中的按钮";

    }
    else if (event->reason() == Qt::TabFocusReason)
    {
//        qDebug() << "Widget lost focus due to tab key!";
    }
    else
    {
        //qDebug() << "Widget lost focus.";
		inLoseFocue_ = true;
		animateHide();
    }

    QWidget::focusOutEvent(event);
}
