#define Q_OS_WIN
#ifdef Q_OS_WIN
#ifndef HOOK_H
#define HOOK_H
#include <QObject>
#include "windows.h"
#include <atomic>


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
    bool sendSignal(const KeyInfo& info);
	void blockOnce() { blockOnce_ = true; }

signals:
	void sendKeyType(const KeyInfo& info);

private:
	Hook();
	~Hook() = default;
	std::atomic<bool> blockOnce_;
};

#endif // HOOK_H
#endif // Q_OS_WIN