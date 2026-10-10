#pragma once

#include <QWidget>
#include "ui_QRCodeBar.h"

class QRCodeBar : public QWidget
{
	Q_OBJECT

public:
	QRCodeBar(QWidget *parent = Q_NULLPTR);
	~QRCodeBar();
	void onFinishGrab();
	void setQRCodeTexts(const QStringList& texts);
	void clearQRCodeTexts();

signals:
	void sigNeedFinishGrab();
	void sigMouseEnter();
	void sigMouseMoveGlobal(const QPoint& screenPos);

private:
	QWidget* makeOneBar(const QString& text);

	void enterEvent(QEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

	Ui::QRCodeBar ui;
};
