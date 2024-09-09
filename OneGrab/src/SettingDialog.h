#pragma once

#include <QWidget>
#include "ui_SettingDialog.h"

class SettingDialog : public QWidget
{
	Q_OBJECT

public:
	static SettingDialog* getInstance();

private slots:
	void on_btn_close_clicked();
	void on_cb_start_by_pc_clicked();

private:
	SettingDialog(QWidget *parent = Q_NULLPTR);
	~SettingDialog();
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);

	Ui::SettingDialog ui;
	QPoint pressPos_;
	bool isMoveWindow_;
};
