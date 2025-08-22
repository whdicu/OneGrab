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
		KeyInfo info;
		info.key = pkbhs->vkCode;
		info.ctrlPressed = GetAsyncKeyState(VK_CONTROL);
		info.shiftPressed = GetAsyncKeyState(VK_SHIFT);
		//qDebug() << pkbhs->vkCode;
        switch (pkbhs->vkCode)
        {
        case 112ul:
            Hook::getInstance()->sendSignal(info);
            return true;
		case 27ul:
		case 37ul:
		case 38ul:
		case 39ul:
		case 40ul:
		case 46ul:  // delete
		case 67ul:  // C
		case 81ul:  // Q
		case 83ul:  // S
		case 84ul:  // T
		case 90ul:  // Z
		case 160ul:  // shift
			if (Hook::getInstance()->sendSignal(info))
				return true;
			break;
        }
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

bool Hook::sendSignal(const KeyInfo& info)
{
	emit sendKeyType(info);
	if (block_)
		return true;
}

Hook::Hook()
{
	QThread* thread = new QThread(this);
	moveToThread(thread);
}

#endif  // Q_OS_WIN
