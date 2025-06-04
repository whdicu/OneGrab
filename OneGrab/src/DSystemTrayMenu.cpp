#include "DSystemTrayMenu.h"

const static QString MENU_STYLE_SHEET = R"(
QMenu {
	/*border: 3px solid red*/;
	width: 100px;
	border-radius: 5px;
}
/*QMenu::item {
	background-color: white;
}
QMenu::item:selected {
	background-color: #ff6666;
}*/)";

DSystemTrayMenu::DSystemTrayMenu(DList<TrayItemInfo> infos, QWidget *parent)
	: QMenu(parent)
{
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setStyleSheet(MENU_STYLE_SHEET);

	QFont font("Microsoft YaHei UI", 9);

	for (const TrayItemInfo& info : infos)
	{
		QAction* action = new QAction(info.text, this);
		action->setFont(font);
		if (!info.icon.isNull())
			action->setIcon(info.icon);
		if (!info.data.isNull())
			action->setData(info.data);
		connect(action, &QAction::triggered, this, [this, action]()
			{
				emit sigItemClicked(action->text(), action->data());
			});
		addAction(action);
	}
}

DSystemTrayMenu::~DSystemTrayMenu()
{
}
