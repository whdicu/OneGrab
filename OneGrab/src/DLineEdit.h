#pragma once
#pragma execution_character_set("utf-8")
#include <QWidget>

class QLineEdit;
class QPushButton;

class DLineEdit : public QWidget
{
    Q_OBJECT

public:
	DLineEdit(QWidget* parent = nullptr);
	~DLineEdit();

	void setFont(const QFont& font);
	void setText(const QString& text);
	QString text() const;
	void setEditable(bool editable);

signals:
	void sigBtnClicked();

private:
	QLineEdit* lineEdit_;
	QPushButton* btn_;
};
