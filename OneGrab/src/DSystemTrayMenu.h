#pragma once
#pragma execution_character_set("utf-8")
#include <QMenu>

class DSystemTrayMenu : public QMenu
{
	Q_OBJECT

public:
	DSystemTrayMenu(QWidget *parent = nullptr);
	~DSystemTrayMenu();

signals:
	void sigGrab();
	void sigSetting();
	void sigQuit();
};
