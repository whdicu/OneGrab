#pragma once
#pragma execution_character_set("utf-8")
#include <QWidget>
#include "ui_SettingDialog.h"

class SettingDialog : public QWidget
{
	Q_OBJECT

public:
	static SettingDialog* getInstance();

signals:
	void sigRefreshSetting();

private slots:
	void on_btn_close_clicked();
	void on_cb_start_by_pc_clicked();
	void on_cb_use_default_save_path_stateChanged(int state);
	void on_cb_bright_border_stateChanged(int state);
	void on_btn_base_clicked();
	void on_btn_about_clicked();
	void on_btn_color_clicked();

private:
	SettingDialog(QWidget *parent = Q_NULLPTR);
	~SettingDialog();
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);

	QString convertDateFormat(const QString& strDate);

	Ui::SettingDialog ui;
	QPoint pressPos_;
	bool isMoveWindow_;
};
