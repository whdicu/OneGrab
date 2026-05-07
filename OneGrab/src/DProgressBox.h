#pragma once
#pragma execution_character_set("utf-8")
#include <QDialog>

class QLabel;
class QProgressBar;

class DProgressBox : public QDialog
{
	Q_OBJECT

public:
	DProgressBox(QWidget* parent, const QString& title, const QString& text);
	~DProgressBox();

	// 设置在进度达到 100% 后是否自动关闭窗口，默认关闭
	void setAutoCloseOnComplete(bool autoClose);

public slots:
	// 设置当前进度（0.0 ~ 100.0）
	void setProgress(double percent);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	void initUI();

	QLabel* labelTitle_;
	QLabel* labelText_;
	QLabel* labelPercent_;
	QProgressBar* progressBar_;

	double progress_;
	bool autoCloseOnComplete_;
	bool isPressed_;
	int pressX_;
	int pressY_;
};
