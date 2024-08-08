#include "SettingHandler.h"
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


QByteArray readFile(const QString& filePath)
{
	QByteArray data;
	QFile file(filePath);
	if (!file.exists())
	{
		qWarning() << filePath << "not exist";
		bool ret = file.open(QIODevice::WriteOnly);
		if (!ret)
			qWarning() << filePath << "create failed";
		else
			file.close();
		return data;
	}

	bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
	if (!ok)
	{
		qWarning() << "File" << filePath << "open failed";
		return data;
	}

	data = file.readAll();
	file.close();
	return data;
}

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

	// 如果路径中有不存在的文件夹则创建
	QFileInfo fileInfo(strFile);
	QDir().mkpath(fileInfo.absolutePath());

	QJsonDocument doc(wholeObject);
	QByteArray data = doc.toJson();
	QFile file(strFile);
	bool ok = file.open(QIODevice::WriteOnly);
	if (ok)
	{
		file.write(data);
		file.close();
	}
	else
	{
		qWarning() << "File" << strFile << "open failed!";
	}
}

void SettingHandler::readAll()
{
	QString strFile = QCoreApplication::applicationDirPath();
	QString filePath = "/config/Setting.json";
	strFile += filePath;

	QByteArray data = readFile(strFile);
	if (0 == data.size())
	{
		qWarning() << "File" << filePath << "is empty";
		return;
	}

	QJsonParseError parseError;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
	if (QJsonParseError::NoError != parseError.error)
	{
		qWarning() << "File" << filePath << "analyze failed";
		return;
	}

	QJsonObject obj = jsonDoc.object();
	SettingStruct settingStruct;
	settingStruct.LastSavePath = obj["LastSavePath"].toString();

	setSettingStruct(std::move(settingStruct));
}

SettingHandler::~SettingHandler()
{
	
}
