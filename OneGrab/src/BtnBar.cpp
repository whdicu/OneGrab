#include "BtnBar.h"

const static QString NORMAL_STYLE = R"(QPushButton
{
	background-color: transparent;
	border-radius: 3px;
}
QPushButton:hover
{
	background-color: rgba(0, 0, 0, .2);
})";

BtnBar::BtnBar(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, isDrawing_(false)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
}

BtnBar::~BtnBar()
{
}

void BtnBar::dMove(int dX, int dY)
{
	move(x() + dX, y() + dY);
}

void BtnBar::setSizeLabelText(const QSize& sz)
{
	ui.label_size->setText(QString("  W: %1 H: %2").arg(sz.width()).arg(sz.height()));
}

void BtnBar::onFinishGrab()
{
	ui.btn_rect->setStyleSheet(NORMAL_STYLE);
	hide();
}

void BtnBar::on_btn_rect_clicked()
{
	isDrawing_ = (1 == isDrawing_) ? 0 : 1;
	ui.btn_rect->setStyleSheet(isDrawing_ ? "background-color: #ff6666;" : NORMAL_STYLE);
	emit sigDrawing(isDrawing_);
}

void BtnBar::on_btn_close_clicked()
{
	emit sigClose();
}

void BtnBar::on_btn_fixed_clicked()
{
	emit sigFixed();
}

void BtnBar::on_btn_save_clicked()
{
	emit sigSave();
}

void BtnBar::on_btn_copy_clicked()
{
	emit sigCopy();
}
