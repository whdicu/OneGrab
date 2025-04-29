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
	QWriteLocker locker(&structLock_); \
	settingStruct_.##VALUE## = v; \
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
		, UseDefaultSavePath(false), DefaultSavePath("") {}
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
