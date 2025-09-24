#include "SettingDialog.h"
#include <mutex>
#include <QColorDialog>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QSettings>
#include "SettingHandler.h"
#include <shlobj_core.h>
#include <windows.h>


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
	emit sigRefreshSetting();
	SETTING_HANDLER->syncToFile();
}

void SettingDialog::on_cb_start_by_pc_clicked()
{
	bool startByPC = ui.cb_start_by_pc->isChecked();
	QString applicationName = QApplication::applicationName();  // 获取应用名称
	QString applicationPath = QApplication::applicationFilePath();  // 找到应用的目录

	QString exePath = QApplication::applicationDirPath() + "/StartByPC.exe";
	if (!QFile::exists(exePath))
	{
		QMessageBox::warning(nullptr, tr("有些事情好像不太行"), tr("文件缺失：%1").arg(exePath));
		return;
	}

	QStringList args = { "name=" + applicationName, "path=" + applicationPath, "start=" + QString::number(startByPC) };
	if (!QProcess::startDetached(exePath, args))
		ui.cb_start_by_pc->setChecked(!startByPC);
}

void SettingDialog::on_cb_use_default_save_path_stateChanged(int state)
{
	ui.edit_default_save_path->setEnabled(state);
	SETTING_HANDLER->setUseDefaultSavePath(state);
}

void SettingDialog::on_cb_bright_border_stateChanged(int state)
{
	SETTING_HANDLER->setBrightBorder(state);
}

void SettingDialog::on_cb_copy2file_stateChanged(int state)
{
	SETTING_HANDLER->setCopy2File(state);
}

void SettingDialog::on_btn_base_clicked()
{
	ui.stackedWidget->setCurrentIndex(0);
}

void SettingDialog::on_btn_about_clicked()
{
	ui.stackedWidget->setCurrentIndex(2);
}

void SettingDialog::on_btn_color_clicked()
{
	SettingStruct stru = SETTING_HANDLER->getSettingStruct();

	QColorDialog dlg;
	dlg.setCurrentColor(stru.MainColor);
	if (dlg.exec() != QDialog::Accepted)
		return;

	stru.MainColor = dlg.selectedColor();
	SETTING_HANDLER->setSettingStruct(stru);

	ui.btn_color->setStyleSheet(QString("border-radius: 4px; background-color: rgb(%1, %2, %3);")
		.arg(stru.MainColor.red()).arg(stru.MainColor.green()).arg(stru.MainColor.blue()));
}

SettingDialog::SettingDialog(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint)
	, pressPos_(0, 0)
	, isMoveWindow_(false)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);

	ui.btn_other->hide();
	ui.btn_about->hide();

	ui.stackedWidget->setCurrentIndex(0);
	ui.label_version->setText(tr("软件版本：V%1").arg(convertDateFormat(__DATE__)));

	QString applicationName = QApplication::applicationName();  // 获取应用名称
	QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
		QSettings::NativeFormat);
	ui.cb_start_by_pc->setChecked(settings.allKeys().contains(applicationName));

	SettingStruct stru = SETTING_HANDLER->getSettingStruct();
	ui.btn_color->setStyleSheet(QString("border-radius: 4px; background-color: rgb(%1, %2, %3);")
		.arg(stru.MainColor.red()).arg(stru.MainColor.green()).arg(stru.MainColor.blue()));
	ui.cb_use_default_save_path->setChecked(stru.UseDefaultSavePath);
	ui.edit_default_save_path->setEnabled(stru.UseDefaultSavePath);
	ui.edit_default_save_path->setText(stru.DefaultSavePath);
	ui.edit_default_save_path->setEditable(false);
	ui.cb_bright_border->setChecked(stru.BrightBorder);
	ui.cb_copy2file->setChecked(stru.Copy2File);

	connect(ui.edit_default_save_path, &DLineEdit::sigBtnClicked, this, [this]()
	{
		QString oldPath = SETTING_HANDLER->getDefaultSavePath();
		QString selectedPath = QFileDialog::getExistingDirectory(this, tr("选择虚拟相机目录"), oldPath, QFileDialog::ShowDirsOnly);
		if (selectedPath.isEmpty())
			return;

		selectedPath.replace(QRegExp("\\"), "/");
		ui.edit_default_save_path->setText(selectedPath);
		SETTING_HANDLER->setDefaultSavePath(selectedPath);
	});
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
		auto globalPos = event->globalPosition().toPoint();
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

QString SettingDialog::convertDateFormat(const QString& strDate)
{
	QStringList strList = strDate.split(" ");
	int iSize = strList.size();
	if (3 != iSize &&
		4 != iSize)
	{
		return QString();
	}

	//月
	QString strMonth = strList.at(0);
	if ("Jan" == strMonth)
	{
		strMonth = "01";
	}
	else if ("Feb" == strMonth)
	{
		strMonth = "02";
	}
	else if ("Mar" == strMonth)
	{
		strMonth = "03";
	}
	else if ("Apr" == strMonth)
	{
		strMonth = "04";
	}
	else if ("May" == strMonth)
	{
		strMonth = "05";
	}
	else if ("Jun" == strMonth)
	{
		strMonth = "06";
	}
	else if ("Jul" == strMonth)
	{
		strMonth = "07";
	}
	else if ("Aug" == strMonth)
	{
		strMonth = "08";
	}
	else if ("Sep" == strMonth)
	{
		strMonth = "09";
	}
	else if ("Oct" == strMonth)
	{
		strMonth = "10";
	}
	else if ("Nov" == strMonth)
	{
		strMonth = "11";
	}
	else if ("Dec" == strMonth)
	{
		strMonth = "12";
	}

	//日
	QString strDay = strList.at(iSize - 2);
	if (1 == strDay.length())
	{
		strDay = "0" + strDay;
	}

	//年
	QString strYear = strList.at(iSize - 1);

	return strYear + strMonth + strDay;
}
