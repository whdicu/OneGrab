#pragma once
#pragma execution_character_set("utf-8")
#include <QWidget>
#include <QFocusEvent>
#include "HDBase/DStringList.hpp"

class QLabel;
class QPropertyAnimation;

class DMenu : public QWidget
{
    Q_OBJECT

public:
	DMenu(const DStringList& texts, QWidget* parent = nullptr);
	~DMenu();

    void animateShow();
    void animateHide(bool setIsHidden = true, bool showTop = false);
    bool isHidden() { return is_hidden_; }
    void setInLoseFocue(bool inLoseFocue) { inLoseFocue_ = inLoseFocue; }
    bool setFocus();
    void show(QPoint pos);
	void setBgColor(const QColor& color);

signals:
    void sigBtnClicked(const QString& text);

private:
    void focusOutEvent(QFocusEvent*);

    QWidget* widget_;
    bool is_hidden_;
	bool inLoseFocue_;
	QLabel* animateLabel_;
    QPropertyAnimation* animation_;
};
