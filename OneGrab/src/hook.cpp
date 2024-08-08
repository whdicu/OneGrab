#define Q_OS_WIN
#ifdef Q_OS_WIN
#include "hook.h"
#include <QDebug>
#include <QThread>

static HHOOK keyHook = nullptr;
static Hook* hook = nullptr;

Hook* Hook::getInstance()
{
	if (nullptr == hook)
		hook = new Hook();
	return hook;
}

LRESULT CALLBACK keyProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
	if (wParam == WM_KEYDOWN)
	{
        switch (pkbhs->vkCode)
        {
        case 112ul:
            Hook::getInstance()->sendSignal(pkbhs->vkCode);
            return true;
		case 27ul:
			Hook::getInstance()->sendSignal(pkbhs->vkCode);
			break;
        }
		//if (pkbhs->vkCode == 0x31 && GetAsyncKeyState(VK_CONTROL))
		//{//按下Ctrl+1
		//	Hook::getInstance().sendSignal(Hook::CHANGE);
        //}
	}
    return CallNextHookEx(keyHook, nCode, wParam, lParam);//继续原有的事件队列
}

void Hook::installHook()
{
	keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyProc, nullptr, 0);
}

void Hook::unInstallHook()
{
	UnhookWindowsHookEx(keyHook);
	keyHook = nullptr;
}

void Hook::sendSignal(DWORD key)
{
	emit sendKeyType(key);
}

Hook::Hook()
{
	QThread* thread = new QThread(this);
	moveToThread(thread);
}

#endif  // Q_OS_WIN
