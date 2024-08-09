#include "DSystemTrayMenu.h"
#include "hook.h"
#include "OneGrab.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include "SettingHandler.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OneGrab w;

	QObject::connect(&a, &QApplication::aboutToQuit, SETTING, &SettingHandler::syncToFile);

	QSystemTrayIcon trayIcon(QIcon(":/svgs/logo.svg"));
	trayIcon.setToolTip("OneGrab");
	QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &w, [&w](QSystemTrayIcon::ActivationReason reason)
	{
		switch (reason)
		{
		case QSystemTrayIcon::Unknown:
			break;
		case QSystemTrayIcon::Context:
			break;
		case QSystemTrayIcon::DoubleClick:
			break;
		case QSystemTrayIcon::Trigger:
			w.doGrab();
			break;
		case QSystemTrayIcon::MiddleClick:
			break;
		default:
			break;
		}
	});

	// ´´½¨ÍÐÅÌ²Ëµ¥
	DSystemTrayMenu trayMenu;
	QObject::connect(&trayMenu, &DSystemTrayMenu::sigGrab, &w, &OneGrab::doGrab);
	//QObject::connect(&trayMenu, &DSystemTrayMenu::sigSetting, &w, &QApplication::quit);
	QObject::connect(&trayMenu, &DSystemTrayMenu::sigQuit, &a, &QApplication::quit);

	trayIcon.setContextMenu(&trayMenu);
	trayIcon.show();

	Hook::getInstance()->installHook();
	QObject::connect(Hook::getInstance(), &Hook::sendKeyType, &w, &OneGrab::slotKeyPressed, Qt::QueuedConnection);
	QObject::connect(&a, &QApplication::aboutToQuit, Hook::getInstance(), &Hook::unInstallHook);

    return a.exec();
}
