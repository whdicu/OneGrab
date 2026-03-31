#include "BtnBar.h"
#include <QColorDialog>
#include <QDebug>

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
	ui.widget_2->hide();
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
	ui.widget_2->hide();
}

void BtnBar::setDrawMode(MouseState drawMode)
{
	QPushButton* btn = nullptr;
	switch (drawMode)
	{
	case DrawRectS:
		btn = ui.btn_rect;
		break;
	case DrawLineS:
		btn = ui.btn_line;
		break;
	case DrawArrowS:
		btn = ui.btn_arrow;
		break;
	case DrawPenS:
		btn = ui.btn_pen;
		break;
	case DrawWordS:
		btn = ui.btn_text;
		break;
	default:
		qWarning() << __FUNCTION__ << "error drawMode:" << drawMode;
		return;
	}

	if (nullptr != btn)
		drawBtnClicked(btn, drawMode);
}

void BtnBar::on_btn_rect_clicked()
{
	drawBtnClicked(ui.btn_rect, DrawRectS);
}

void BtnBar::on_btn_line_clicked()
{
	drawBtnClicked(ui.btn_line, DrawLineS);
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
	drawBtnClicked(ui.btn_text, DrawWordS);
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

void BtnBar::on_btn_color_clicked()
{
	SettingStruct stru = SETTING_HANDLER->getSettingStruct();

	QColor* c = nullptr;
	switch (isDrawing_)
	{
	case DrawRectS:
		c = &stru.RectColor;
		break;
	case DrawLineS:
		c = &stru.LineColor;
		break;
	case DrawArrowS:
		c = &stru.ArrowColor;
		break;
	case DrawPenS:
		c = &stru.PenColor;
		break;
	case DrawWordS:
		c = &stru.TextColor;
		break;
	default:
		return;
	}

	QColorDialog dlg(this);
	dlg.setStyleSheet("QPushButton { backgorund-color: white; border: 1px solid #5c5c66; border-radius: 4px; padding: 5px 15px; }");
	dlg.setWindowFlag(Qt::WindowStaysOnTopHint);
	dlg.setCurrentColor(*c);
	emit sigSetIgnoreKey(true);
	int ret = dlg.exec();
	emit sigSetIgnoreKey(false);
	if (ret != QDialog::Accepted)
		return;
	*c = dlg.selectedColor();
	SETTING_HANDLER->setSettingStruct(stru);

	// 为了刷新界面颜色显示
	refreshUI();
}

void BtnBar::on_btn_line1_clicked()
{
	setLineWidth(Line1);
	refreshLineBtn(Line1);
}

void BtnBar::on_btn_line2_clicked()
{
	setLineWidth(Line2);
	refreshLineBtn(Line2);
}

void BtnBar::on_btn_line3_clicked()
{
	setLineWidth(Line3);
	refreshLineBtn(Line3);
}

void BtnBar::on_btn_line4_clicked()
{
	setLineWidth(Line4);
	refreshLineBtn(Line4);
}

void BtnBar::drawBtnClicked(QPushButton* btn, int drawType)
{
	if (nullptr != choosedBtn_)
		choosedBtn_->setStyleSheet(NORMAL_STYLE);
	choosedBtn_ = btn;
	isDrawing_ = (drawType == isDrawing_) ? 0 : drawType;

	refreshUI();
	emit sigDrawing(isDrawing_);
}

void BtnBar::refreshUI()
{
	if (nullptr == choosedBtn_)
		return;

	LineWidth lineWidth = Line1;
	QColor color = SETTING_HANDLER->getMainColor();
	switch (isDrawing_)
	{
	case DrawRectS:
		color = SETTING_HANDLER->getRectColor();
		lineWidth = SETTING_HANDLER->getRectLineWidth();
		break;
	case DrawLineS:
		color = SETTING_HANDLER->getLineColor();
		lineWidth = SETTING_HANDLER->getLineLineWidth();
		break;
	case DrawArrowS:
		color = SETTING_HANDLER->getArrowColor();
		lineWidth = SETTING_HANDLER->getArrowLineWidth();
		break;
	case DrawPenS:
		color = SETTING_HANDLER->getPenColor();
		lineWidth = SETTING_HANDLER->getPenLineWidth();
		break;
	case DrawWordS:
		color = SETTING_HANDLER->getTextColor();
		lineWidth = SETTING_HANDLER->getTextLineWidth();
		break;
	}
	ui.btn_color->setStyleSheet(QString("background-color: %1;").arg(color.name().toUpper()));
	refreshLineBtn(lineWidth);

	if (isDrawing_)
	{
		ui.widget_2->show();
		choosedBtn_->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
			.arg(color.red()).arg(color.green()).arg(color.blue()));
	}
	else
	{
		ui.widget_2->hide();
		choosedBtn_->setStyleSheet(NORMAL_STYLE);
	}
}

void BtnBar::refreshLineBtn(LineWidth lineWidth)
{
	ui.btn_line1->show();
	ui.btn_line2->show();
	ui.btn_line3->show();
	ui.btn_line4->show();
	QString mainColorStr = SETTING_HANDLER->getMainColor().name().toUpper();
	switch (isDrawing_)
	{
	case DrawRectS:
		mainColorStr = SETTING_HANDLER->getRectColor().name().toUpper();
		break;
	case DrawLineS:
		mainColorStr = SETTING_HANDLER->getLineColor().name().toUpper();
		break;
	case DrawArrowS:
		mainColorStr = SETTING_HANDLER->getArrowColor().name().toUpper();
		break;
	case DrawPenS:
		mainColorStr = SETTING_HANDLER->getPenColor().name().toUpper();
		break;
	case DrawWordS:
		mainColorStr = SETTING_HANDLER->getTextColor().name().toUpper();
		ui.btn_line1->hide();
		ui.btn_line2->hide();
		ui.btn_line3->hide();
		ui.btn_line4->hide();
		break;
	}
	
	ui.btn_line1->setStyleSheet("border: none;");
	ui.btn_line2->setStyleSheet("border: none;");
	ui.btn_line3->setStyleSheet("border: none;");
	ui.btn_line4->setStyleSheet("border: none;");
	switch (lineWidth)
	{
	case Line1:
		ui.btn_line1->setStyleSheet(QString("border: 2px dashed %1; border-radius: 6px;")
			.arg(mainColorStr));
		break;
	case Line2:
		ui.btn_line2->setStyleSheet(QString("border: 2px dashed %1; border-radius: 6px;")
			.arg(mainColorStr));
		break;
	case Line3:
		ui.btn_line3->setStyleSheet(QString("border: 2px dashed %1; border-radius: 6px;")
			.arg(mainColorStr));
		break;
	case Line4:
		ui.btn_line4->setStyleSheet(QString("border: 2px dashed %1; border-radius: 6px;")
			.arg(mainColorStr));
		break;
	}
}

void BtnBar::setLineWidth(LineWidth lineWidth)
{
	switch (isDrawing_)
	{
	case DrawRectS:
		SETTING_HANDLER->setRectLineWidth(lineWidth);
		break;
	case DrawLineS:
		SETTING_HANDLER->setLineLineWidth(lineWidth);
		break;
	case DrawArrowS:
		SETTING_HANDLER->setArrowLineWidth(lineWidth);
		break;
	case DrawPenS:
		SETTING_HANDLER->setPenLineWidth(lineWidth);
		break;
	case DrawWordS:
		SETTING_HANDLER->setTextLineWidth(lineWidth);
		break;
	}
	SETTING_HANDLER->syncToFile();
}

void BtnBar::enterEvent(QEvent* event)
{
	emit sigMouseEnter();
}
