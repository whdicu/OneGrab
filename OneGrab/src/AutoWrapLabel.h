#pragma once
#include <QLabel>


class AutoWrapLabel : public QLabel
{
public:
	AutoWrapLabel(QWidget* parent = nullptr);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
};
