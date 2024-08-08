#include "BtnBar.h"

BtnBar::BtnBar(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
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
