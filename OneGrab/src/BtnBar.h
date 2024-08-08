#pragma once

#include <QWidget>
#include "ui_BtnBar.h"

class BtnBar : public QWidget
{
	Q_OBJECT

public:
	BtnBar(QWidget *parent = Q_NULLPTR);
	~BtnBar();
	void dMove(int dX, int dY);
	void setSizeLabelText(const QSize& sz);

signals:
	void sigFixed();
	void sigSave();
	void sigCopy();

private slots:
	void on_btn_fixed_clicked();
	void on_btn_save_clicked();
	void on_btn_copy_clicked();

private:
	Ui::BtnBar ui;
};
