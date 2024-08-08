#ifndef SETTINGHANDLER_H
#define SETTINGHANDLER_H
#include <QObject>
#include <QReadWriteLock>


#define REG_GET_FUNC(VALUE) inline auto get##VALUE##() \
{ \
	QReadLocker locker(&structLock_); \
	return settingStruct_.##VALUE##; \
}


struct SettingStruct
{
	QString LastSavePath;
};


class SettingHandler : public QObject
{
	Q_OBJECT

public:
    static SettingHandler* getInstance();
	SettingStruct getSettingStruct();
	void setSettingStruct(const SettingStruct& settingStruct);
	void syncToFile();

	// 上一次保存路径
	REG_GET_FUNC(LastSavePath)

private:
    SettingHandler();
	~SettingHandler();
	void readAll();
	void writeAll();

	QReadWriteLock structLock_;
	SettingStruct settingStruct_;
};

#ifndef SETTING_HANDLER
#define SETTING SettingHandler::getInstance()
#endif

#endif // SETTINGHANDLER_H
