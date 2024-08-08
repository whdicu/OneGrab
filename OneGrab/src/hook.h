#define Q_OS_WIN
#ifdef Q_OS_WIN
#ifndef HOOK_H
#define HOOK_H
#include <QObject>
#include "windows.h"

class Hook : public QObject
{
	Q_OBJECT
public:
	static Hook* getInstance();
	void installHook();
	void unInstallHook();
    void sendSignal(DWORD key);

signals:
	void sendKeyType(DWORD);

private:
	Hook();
	~Hook() = default;
};

#endif // HOOK_H
#endif // Q_OS_WIN