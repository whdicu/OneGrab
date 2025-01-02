#include "BtnBar.h"
#include "DGrabView.h"
#include "SettingHandler.h"

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
	, choosedBtn_(nullptr)
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
	if (choosedBtn_)
		choosedBtn_->setStyleSheet(NORMAL_STYLE);
	choosedBtn_ = nullptr;
	isDrawing_ = 0;
	hide();
}

void BtnBar::on_btn_rect_clicked()
{
	drawBtnClicked(ui.btn_rect, DrawRectS);
}

void BtnBar::on_btn_arrow_clicked()
{
	drawBtnClicked(ui.btn_arrow, DrawArrowS);
}

void BtnBar::on_btn_pen_clicked()
{
	drawBtnClicked(ui.btn_pen, DrawPenS);
}

void BtnBar::on_btn_text_clicked()
{
	drawBtnClicked(ui.btn_text, DrawTextS);
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

void BtnBar::drawBtnClicked(QPushButton* btn, int drawType)
{
	if (nullptr != choosedBtn_)
		choosedBtn_->setStyleSheet(NORMAL_STYLE);
	choosedBtn_ = btn;
	isDrawing_ = (drawType == isDrawing_) ? 0 : drawType;
	QColor mainColor = SETTING->getMainColor();
	btn->setStyleSheet(isDrawing_ ? QString("background-color: rgb(%1, %2, %3);")
		.arg(mainColor.red()).arg(mainColor.green()).arg(mainColor.blue()): NORMAL_STYLE);
	emit sigDrawing(isDrawing_);
}
