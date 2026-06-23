#pragma once
#pragma execution_character_set("utf-8")
#include <QWidget>
#include "ui_SettingDialog.h"

class SettingDialog : public QWidget
{
	Q_OBJECT

public:
	static SettingDialog* getInstance();
	void show();

signals:
	void sigRefreshSetting();
	void sigCheckUpdate();

private slots:
	void on_btn_close_clicked();
	void on_cb_start_by_pc_clicked();
	void on_cb_check_update_on_start_stateChanged(int state);
	void on_cb_use_default_save_path_stateChanged(int state);
	void on_cb_auto_grab_window_stateChanged(int state);
	void on_cb_bright_border_stateChanged(int state);
	void on_cb_copy2file_stateChanged(int state);
	void on_sb_scale_num_valueChanged(int value);
	void on_sb_island_num_valueChanged(int value);
	void on_btn_base_clicked();
	void on_btn_advanced_clicked();
	void on_btn_about_clicked();
	void on_btn_color_clicked();
	void on_btn_check_update_clicked();
	void on_btn_visti_gitee_clicked();
	void on_cmb_img_type_currentIndexChanged(int index);

private:
	SettingDialog(QWidget *parent = Q_NULLPTR);
	~SettingDialog();
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);

	void refreshByStruct();
	QString convertDateFormat(const QString& strDate);

	Ui::SettingDialog ui;
	QPoint pressPos_;
	bool isMoveWindow_;
};
