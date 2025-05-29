#include "WinHandler.h"
#include <shlobj.h>
#include <shellapi.h>
#include <QDesktopServices>
#include <QMessageBox>
#include <QProcess>
#include <QUrl>

bool WinHandler::isRecycleBinEmpty()
{
	SHQUERYRBINFO qrbInfo;
	qrbInfo.cbSize = sizeof(SHQUERYRBINFO);

	HRESULT result = SHQueryRecycleBin(NULL, &qrbInfo);
	if (result == S_OK)
		return 0 == qrbInfo.i64NumItems;
	
	// 查询失败默认返回true
	return true;
}

void WinHandler::openFile(const QString& absolutePath)
{
	//if (absolutePath.endsWith(".txt"))  // 这些后缀名的文件用自带的文本编辑器打开
	//{
	//	int new_x = x() + app_lnk->x() + (app_lnk->width() - TextEdit::width_) / 2;
	//	if (new_x < 10)
	//		new_x = 10;
	//	else if (new_x > SETTING_HANDLER->getScreenWidth() - TextEdit::width_ - 10)
	//		new_x = SETTING_HANDLER->getScreenWidth() - TextEdit::width_ - 10;

	//	int pointer_center_x = x() + app_lnk->x() + app_lnk->width() / 2 - new_x;
	//	TextEdit* te = new TextEdit(absolute_path, pointer_center_x, this);
	//	te->move(new_x, y() - te->height() - 10);
	//	te->show();
	//}
	//else
	{
		bool ret = QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
		if (!ret)
			QMessageBox::critical(nullptr, QObject::tr("打开错误"), QObject::tr("我们遇到了一些意想不到的错误，\n打开 %1 失败").arg(absolutePath));
	}
}

bool WinHandler::isProcessRunning(const QString &processName)
{
	QProcess process;
	process.start("tasklist", QStringList());
	process.waitForFinished();
	QString output = process.readAllStandardOutput();
	int cnt = output.count(processName);
	return cnt > 1;
}

