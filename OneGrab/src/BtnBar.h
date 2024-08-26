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
	void onFinishGrab();
	int getDrawingType() { return isDrawing_; }

signals:
	void sigDrawing(int isDrawing);
	void sigClose();
	void sigFixed();
	void sigSave();
	void sigCopy();

private slots:
	void on_btn_rect_clicked();
	void on_btn_arrow_clicked();
	void on_btn_pen_clicked();
	void on_btn_text_clicked();
	void on_btn_close_clicked();
	void on_btn_fixed_clicked();
	void on_btn_save_clicked();
	void on_btn_copy_clicked();

private:
	void drawBtnClicked(QPushButton* btn, int drawType);

	Ui::BtnBar ui;
	int isDrawing_;  // 0没画 1在画矩形 2在画箭头 3随便画 4画文字
	QPushButton* choosedBtn_;
};
