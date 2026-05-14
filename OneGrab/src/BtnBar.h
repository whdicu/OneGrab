#pragma once
#include <QWidget>
#include "SettingHandler.h"
#include "ui_BtnBar.h"
#include "DGrabView.h"

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
	void setDrawMode(MouseState drawMode);

signals:
	void sigDrawing(int isDrawing);
	void sigClose();
	void sigFixed();
	void sigSave();
	void sigCopy();
	void sigMouseEnter();
	void sigMouseMoveGlobal(const QPoint& screenPos);
	void sigSetIgnoreKey(bool ignore);

private slots:
	void on_btn_rect_clicked();
	void on_btn_line_clicked();
	void on_btn_arrow_clicked();
	void on_btn_pen_clicked();
	void on_btn_text_clicked();
	void on_btn_close_clicked();
	void on_btn_fixed_clicked();
	void on_btn_save_clicked();
	void on_btn_copy_clicked();

	void on_btn_color_clicked();
	void on_btn_line1_clicked();
	void on_btn_line2_clicked();
	void on_btn_line3_clicked();
	void on_btn_line4_clicked();

private:
	void drawBtnClicked(QPushButton* btn, int drawType);
	void refreshUI();
	void refreshLineBtn(LineWidth lineWidth);
	void setLineWidth(LineWidth lineWidth);
	void enterEvent(QEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

	Ui::BtnBar ui;
	int isDrawing_;  // 0没画 1在画矩形 2在画箭头 3随便画 4画文字
	QPushButton* choosedBtn_;
};
