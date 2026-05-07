#pragma once
#pragma execution_character_set("utf-8")
#include <QDialog>

class QLabel;
class QPushButton;

enum OneMessageBoxType
{
	INFORMATION_BOX,
	WARNING_BOX,
	CRITICAL_BOX
};

enum OneMessageBoxButton
{
	NO_BTN = 0x00,
	ACCEPT_BTN = 0x01,
	CANCEL_BTN = 0x02,
	ALL_BTN = ACCEPT_BTN | CANCEL_BTN
};

class DMessageBox : private QDialog
{
	Q_OBJECT

public:
	static int information(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn = ACCEPT_BTN);
	static int warning(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn = ACCEPT_BTN);
	static int critical(QWidget* parent, const QString& title, const QString& text, OneMessageBoxButton btn = ACCEPT_BTN);

	DMessageBox(QWidget* parent, const QString& title, const QString& text, OneMessageBoxType type = INFORMATION_BOX, OneMessageBoxButton btn = ACCEPT_BTN);
	~DMessageBox();

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	void initUI();

	QLabel* labelTitle_;
	QLabel* label2_;
	QPushButton* btn_accept_;
	QPushButton* btn_cancel_;

	bool isPressed_;
	int pressX_;
	int pressY_;
};
