#include "OneGrab.h"
#include <QApplication>
#include <QSystemTrayIcon>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OneGrab w;

	QSystemTrayIcon trayIcon(QIcon(":/icon.ico"));
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
	trayIcon.show();

    return a.exec();
}
