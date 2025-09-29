#include "SettingHandler.h"
#include "HDQt/DStyle.hpp"
#include "HDQt/HDQt.hpp"
#include <mutex>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEasingCurve>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


static std::once_flag onceFlag;
static SettingHandler* setting_handler = nullptr;
SettingHandler* SettingHandler::getInstance()
{
	std::call_once(onceFlag, [] { setting_handler = new SettingHandler; });
    return setting_handler;
}

SettingStruct SettingHandler::getSettingStruct()
{
	QReadLocker locker(&structLock_);
	return settingStruct_;
}

void SettingHandler::setSettingStruct(const SettingStruct& settingStruct)
{
	QWriteLocker locker(&structLock_);
	settingStruct_ = settingStruct;
}

void SettingHandler::syncToFile()
{
	writeAll();
}

SettingHandler::SettingHandler()
	: structLock_(QReadWriteLock::Recursive)
{
    readAll();
}

void SettingHandler::writeAll()
{
	QString strFile = QCoreApplication::applicationDirPath();
	strFile += "/config/Setting.json";

	QJsonObject wholeObject;
	SettingStruct settingStruct = getSettingStruct();
	wholeObject.insert("LastSavePath", settingStruct.LastSavePath);
	wholeObject.insert("MainColor", DStyle::color2Int(settingStruct.MainColor));
	wholeObject.insert("RectColor", DStyle::color2Int(settingStruct.RectColor));
	wholeObject.insert("RectLineWidth", settingStruct.RectLineWidth);
	wholeObject.insert("LineColor", DStyle::color2Int(settingStruct.LineColor));
	wholeObject.insert("LineLineWidth", settingStruct.LineLineWidth);
	wholeObject.insert("ArrowColor", DStyle::color2Int(settingStruct.ArrowColor));
	wholeObject.insert("ArrowLineWidth", settingStruct.ArrowLineWidth);
	wholeObject.insert("PenColor", DStyle::color2Int(settingStruct.PenColor));
	wholeObject.insert("PenLineWidth", settingStruct.PenLineWidth);
	wholeObject.insert("TextColor", DStyle::color2Int(settingStruct.TextColor));
	wholeObject.insert("TextLineWidth", settingStruct.TextLineWidth);
	wholeObject.insert("UseDefaultSavePath", settingStruct.UseDefaultSavePath);
	wholeObject.insert("DefaultSavePath", settingStruct.DefaultSavePath);
	wholeObject.insert("BrightBorder", settingStruct.BrightBorder);
	wholeObject.insert("Copy2File", settingStruct.Copy2File);
	wholeObject.insert("MouseScaleNum", settingStruct.MouseScaleNum);
	HDQt::writeJson(strFile, wholeObject);
}

void SettingHandler::readAll()
{
	QString strFile = QCoreApplication::applicationDirPath();
	QString filePath = "/config/Setting.json";
	strFile += filePath;

	QJsonObject obj;
	HDQt::readJson(strFile, obj);
	SettingStruct settingStruct;
	settingStruct.LastSavePath = obj["LastSavePath"].toString();
	settingStruct.MainColor = DStyle::int2Color(obj["MainColor"].toInt(16737894));
	settingStruct.RectColor = DStyle::int2Color(obj["RectColor"].toInt(16737894));
	settingStruct.RectLineWidth = (LineWidth)obj["RectLineWidth"].toInt(2);
	settingStruct.LineColor = DStyle::int2Color(obj["LineColor"].toInt(16737894));
	settingStruct.LineLineWidth = (LineWidth)obj["LineLineWidth"].toInt(2);
	settingStruct.ArrowColor = DStyle::int2Color(obj["ArrowColor"].toInt(16737894));
	settingStruct.ArrowLineWidth = (LineWidth)obj["ArrowLineWidth"].toInt(2);
	settingStruct.PenColor = DStyle::int2Color(obj["PenColor"].toInt(16737894));
	settingStruct.PenLineWidth = (LineWidth)obj["PenLineWidth"].toInt(2);
	settingStruct.TextColor = DStyle::int2Color(obj["TextColor"].toInt(16737894));
	settingStruct.TextLineWidth = (LineWidth)obj["TextLineWidth"].toInt(2);
	settingStruct.UseDefaultSavePath = obj["UseDefaultSavePath"].toBool(false);
	settingStruct.DefaultSavePath = obj["DefaultSavePath"].toString();
	settingStruct.BrightBorder = obj["BrightBorder"].toBool(true);
	settingStruct.Copy2File = obj["Copy2File"].toBool(true);
	settingStruct.MouseScaleNum = obj["MouseScaleNum"].toInt(8);
	setSettingStruct(std::move(settingStruct));
}

SettingHandler::~SettingHandler()
{
	
}
