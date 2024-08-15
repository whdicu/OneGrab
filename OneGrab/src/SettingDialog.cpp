#include "SettingDialog.h"
#include <mutex>
#include <QMouseEvent>


static std::once_flag onceFlag;
static SettingDialog* setting_dialog = nullptr;
SettingDialog* SettingDialog::getInstance()
{
	std::call_once(onceFlag, [] { setting_dialog = new SettingDialog; });
	return setting_dialog;
}

void SettingDialog::on_btn_close_clicked()
{
	hide();
}

SettingDialog::SettingDialog(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint)
	, pressPos_(0, 0)
	, isMoveWindow_(false)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
}

SettingDialog::~SettingDialog()
{
}

void SettingDialog::mousePressEvent(QMouseEvent* event)
{
	if (Qt::LeftButton == event->button()
		&& ui.widget_top->geometry().contains(event->pos()))
	{
		pressPos_ = event->pos();
		isMoveWindow_ = true;
	}
}

void SettingDialog::mouseMoveEvent(QMouseEvent* event)
{
	if (isMoveWindow_)
	{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		auto globalPos = event->globalPosition();
#else
		auto globalPos = event->globalPos();
#endif
		move(globalPos - pressPos_);
	}
}

void SettingDialog::mouseReleaseEvent(QMouseEvent* event)
{
	isMoveWindow_ = false;
}
