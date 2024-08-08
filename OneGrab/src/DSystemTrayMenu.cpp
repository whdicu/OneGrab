#include "DSystemTrayMenu.h"

const static QString MENU_STYLE_SHEET = R"(
QMenu {
	/*border: 3px solid red*/;
	width: 100px;
	border-radius: 5px;
}
/*QMenu::item {
	background-color: white;
}
QMenu::item:selected {
	background-color: #ff6666;
}*/)";

DSystemTrayMenu::DSystemTrayMenu(QWidget *parent)
	: QMenu(parent)
{
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setStyleSheet(MENU_STYLE_SHEET);
	
	QFont font("Microsoft YaHei UI", 9);

	QAction* grabAction = new QAction(QObject::tr("截屏"), this);
	grabAction->setFont(font);
	grabAction->setIcon(QIcon(":svgs/logo.svg"));
	connect(grabAction, &QAction::triggered, this, &DSystemTrayMenu::sigGrab);
	addAction(grabAction);
	
	QAction* settingAction = new QAction(QObject::tr("设置"), this);
	settingAction->setFont(font);
	settingAction->setIcon(QIcon(":svgs/setting.svg"));
	connect(settingAction, &QAction::triggered, this, &DSystemTrayMenu::sigSetting);
	addAction(settingAction);

	QAction* quitAction = new QAction(QObject::tr("退出"), this);
	quitAction->setFont(font);
	quitAction->setIcon(QIcon(":svgs/shutdown.svg"));
	connect(quitAction, &QAction::triggered, this, &DSystemTrayMenu::sigQuit);
	addAction(quitAction);
}

DSystemTrayMenu::~DSystemTrayMenu()
{
}
