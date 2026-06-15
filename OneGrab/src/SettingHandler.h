#ifndef SETTINGHANDLER_H
#define SETTINGHANDLER_H
#include <QColor>
#include <QObject>
#include <QReadWriteLock>


#define REG_GET_FUNC(VALUE) inline auto get##VALUE##() \
{ \
	QReadLocker locker(&structLock_); \
	return settingStruct_.##VALUE##; \
} \
template <class T> \
inline void set##VALUE##(const T& v) \
{ \
	{ \
		QWriteLocker locker(&structLock_); \
		settingStruct_.##VALUE## = v; \
	} \
	syncToFile(); \
}


enum LineWidth : int
{
	Line1 = 1,
	Line2 = 2,
	Line3 = 4,
	Line4 = 7,
};

struct SettingStruct
{
	SettingStruct()
		: MainColor(QColor(255, 66, 66)), RectColor(QColor(255, 66, 66)), RectLineWidth(Line2)
		, LineColor(QColor(255, 66, 66)), LineLineWidth(Line2)
		, ArrowColor(QColor(255, 66, 66)), ArrowLineWidth(Line2)
		, PenColor(QColor(255, 66, 66)), PenLineWidth(Line2)
		, TextColor(QColor(255, 66, 66)), TextLineWidth(Line2)
		, UseDefaultSavePath(false), DefaultSavePath(""), BrightBorder(true)
		, Copy2File(false), MouseScaleNum(8), IslandNum(64), CheckUpdateOnStart(true) {}
	~SettingStruct() = default;

	QString LastSavePath;
	QColor MainColor;
	QColor RectColor;
	LineWidth RectLineWidth;
	QColor LineColor;
	LineWidth LineLineWidth;
	QColor ArrowColor;
	LineWidth ArrowLineWidth;
	QColor PenColor;
	LineWidth PenLineWidth;
	QColor TextColor;
	LineWidth TextLineWidth;
	bool UseDefaultSavePath;
	QString DefaultSavePath;
	bool AutoGrabWindow;
	bool BrightBorder;
	bool Copy2File;
	int MouseScaleNum;
	int IslandNum;
	bool CheckUpdateOnStart;
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
	REG_GET_FUNC(MainColor)
	REG_GET_FUNC(RectColor)
	REG_GET_FUNC(RectLineWidth)
	REG_GET_FUNC(LineColor)
	REG_GET_FUNC(LineLineWidth)
	REG_GET_FUNC(ArrowColor)
	REG_GET_FUNC(ArrowLineWidth)
	REG_GET_FUNC(PenColor)
	REG_GET_FUNC(PenLineWidth)
	REG_GET_FUNC(TextColor)
	REG_GET_FUNC(TextLineWidth)
	REG_GET_FUNC(UseDefaultSavePath)
	REG_GET_FUNC(DefaultSavePath)
	REG_GET_FUNC(AutoGrabWindow)
	REG_GET_FUNC(BrightBorder)
	REG_GET_FUNC(Copy2File)
	REG_GET_FUNC(MouseScaleNum)
	REG_GET_FUNC(IslandNum)
	REG_GET_FUNC(CheckUpdateOnStart)

private:
    SettingHandler();
	~SettingHandler();
	void readAll();
	void writeAll();

	QReadWriteLock structLock_;
	SettingStruct settingStruct_;
};

#ifndef SETTING_HANDLER
#define SETTING_HANDLER SettingHandler::getInstance()
#endif

#endif // SETTINGHANDLER_H
