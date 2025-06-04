#pragma once
#pragma execution_character_set("utf-8")
#include "HDBase/DList.hpp"
#include <QMenu>

struct TrayItemInfo
{
	TrayItemInfo(const QString& t, const QIcon& i = QIcon(), const QVariant& d = QVariant())
		: text(t), icon(i), data(d) {}
	TrayItemInfo(const QString& t, const QVariant& d)
		: text(t), data(d) {}
	bool operator==(const TrayItemInfo& info) const
	{
		return text == info.text && data == info.data;
	}

	QString text;
	QIcon icon;
	QVariant data;
};

class DSystemTrayMenu : public QMenu
{
	Q_OBJECT

public:
	DSystemTrayMenu(DList<TrayItemInfo> infos, QWidget* parent = nullptr);
	~DSystemTrayMenu();

signals:
	void sigItemClicked(QString text, QVariant data);
};
