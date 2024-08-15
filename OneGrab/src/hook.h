#define Q_OS_WIN
#ifdef Q_OS_WIN
#ifndef HOOK_H
#define HOOK_H
#include <QObject>
#include "windows.h"

struct KeyInfo
{
	DWORD key;
	bool ctrlPressed;
	bool shiftPressed;
};
Q_DECLARE_METATYPE(KeyInfo)

class Hook : public QObject
{
	Q_OBJECT
public:
	static Hook* getInstance();
	void installHook();
	void unInstallHook();
    void sendSignal(const KeyInfo& info);

signals:
	void sendKeyType(const KeyInfo& info);

private:
	Hook();
	~Hook() = default;
};

#endif // HOOK_H
#endif // Q_OS_WIN