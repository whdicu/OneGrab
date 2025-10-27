#include "DSystemTrayMenu.h"
#include "HDBase/DList.hpp"
#include "hook.h"
#include "OneGrab.h"
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QSystemTrayIcon>
#include "SettingDialog.h"
#include "SettingHandler.h"
#include "WinHandler.h"


int main(int argc, char *argv[])
{
	QFileInfo fileInfo(argv[0]);
	QString processName = fileInfo.fileName();
	bool isRunning = WinHandler::isProcessRunning(processName);
	if (isRunning)
	{
		qWarning() << processName << "is running!";
		return 1;
	}

    QApplication a(argc, argv);
    OneGrab w;

	QObject::connect(&a, &QApplication::aboutToQuit, SETTING_HANDLER, &SettingHandler::syncToFile);

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
			//QApplication::quit();
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

	// 任务栏图标右键菜单
	const static DList<TrayItemInfo> infos = { TrayItemInfo(QObject::tr("截屏")), TrayItemInfo(QObject::tr("设置")), TrayItemInfo(QObject::tr("退出")) };
	DSystemTrayMenu trayMenu(infos);
	QObject::connect(&trayMenu, &DSystemTrayMenu::sigItemClicked, [&w, &a](QString text, QVariant data)
	{
		switch (infos.indexOf(text))
		{
		case 0:  // 截屏
			w.doGrab();
			break;
		case 1:  // 设置
			SettingDialog::getInstance()->show();
			break;
		case 2:  // 退出
			a.quit();
			break;
		}
	});

	trayIcon.setContextMenu(&trayMenu);
	trayIcon.show();

	Hook::getInstance()->installHook();
	QObject::connect(Hook::getInstance(), &Hook::sendKeyType, &w, &OneGrab::slotKeyPressed, Qt::DirectConnection);
	QObject::connect(Hook::getInstance(), &Hook::sendKeyTypeQueue, &w, &OneGrab::slotKeyPressed, Qt::QueuedConnection);
	QObject::connect(&a, &QApplication::aboutToQuit, Hook::getInstance(), &Hook::unInstallHook);

    return a.exec();
}
