#pragma once
#include <QString>


namespace WinHandler
{
	// 回收站是否为空
	bool isRecycleBinEmpty();

	void openFile(const QString& absolutePath);

	// 某个程序是否已经运行
	bool isProcessRunning(const QString &processName);
}

