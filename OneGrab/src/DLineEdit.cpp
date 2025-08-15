#include "DLineEdit.h"
#include <QBoxLayout>
#include <QDebug>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>

const static int BTN_MARGIN = 5;
const static int BORDER_SIZE = 2;
const static int MIN_HEIGHT = 30;
const static QSize ICON_SIZE = QSize(24, 24);
const static QString BTN_SVG_ICON = "<?xml version=\"1.0\" standalone=\"no\"?> \
	<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"> \
	<svg t=\"1693577481969\" class=\"icon\" viewBox=\"0 0 1024 1024\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" p-id=\"3186\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"200\" height=\"200\"> \
	<path d=\"M510.4 243.2c8 27.2 33.6 44.8 60.8 44.8h259.2c0-35.2-28.8-64-64-64H504l6.4 19.2zM484.8 160h281.6c70.4 0 128 57.6 128 128v25.6c30.4 24 51.2 60.8 51.2 102.4v384c0 70.4-57.6 128-128 128 \
		H208c-70.4 0-128-57.6-128-128V224c0-70.4 57.6-128 128-128h164.8c46.4 0 89.6 25.6 112 64z m-112 0 \
		H208c-35.2 0-64 28.8-64 64v576c0 35.2 28.8 64 64 64h608c35.2 0 64-28.8 64-64V416c0-35.2-28.8-64-64-64H574.4c-56 0-105.6-36.8-121.6-89.6l-19.2-57.6c-8-27.2-32-44.8-60.8-44.8zM272 704h256 \
		c17.6 0 32 14.4 32 32s-14.4 32-32 32H272c-17.6 0-32-14.4-32-32s14.4-32 32-32z\" \
		fill=\"#5c5c66\" p-id=\"3187\"></path></svg>";


DLineEdit::DLineEdit(QWidget* parent)
	: QWidget(parent)
	, lineEdit_(new QLineEdit)
	, btn_(new QPushButton)
{
	lineEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	btn_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	btn_->setFlat(true);
	btn_->setCursor(Qt::PointingHandCursor);
	btn_->setFixedWidth(30);
	connect(btn_, &QPushButton::clicked, this, &DLineEdit::sigBtnClicked);

	QByteArray svgBytes = BTN_SVG_ICON.toUtf8();
	QSvgRenderer renderer(svgBytes);
	if (renderer.isValid())
	{
		QPixmap pixmap(ICON_SIZE);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		btn_->setIcon(QIcon(pixmap));
		btn_->setIconSize(ICON_SIZE);
	}

	QHBoxLayout* btnLayout = new QHBoxLayout;
	btnLayout->setMargin(BTN_MARGIN - BORDER_SIZE);
	btnLayout->addWidget(btn_);

	QWidget* box = new QWidget;
	box->setObjectName("dlineedit_box");
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(BTN_MARGIN, BORDER_SIZE, BORDER_SIZE, BORDER_SIZE);
	layout->setSpacing(BORDER_SIZE);
	layout->addWidget(lineEdit_);
	layout->addLayout(btnLayout);
	box->setLayout(layout);

	QHBoxLayout* l = new QHBoxLayout;
	l->setMargin(0);
	l->addWidget(box);
	setLayout(l);

	setMinimumHeight(MIN_HEIGHT);
	setStyleSheet(QString("#dlineedit_box { border: %1px solid #5c5c66; border-radius: 10px; } \
		QLineEdit { border: none; background-color: transparent; } \
		QPushButton { background-color: white; border-radius: 5px; } \
		QPushButton:hover { background-color: rgba(92, 92, 102, 0.5); }").arg(BORDER_SIZE));
}

DLineEdit::~DLineEdit()
{

}

void DLineEdit::setFont(const QFont& font)
{
	lineEdit_->setFont(font);
}

void DLineEdit::setText(const QString& text)
{
	lineEdit_->setText(text);
}

QString DLineEdit::text() const
{
	return lineEdit_->text();
}

void DLineEdit::setEditable(bool editable)
{
	lineEdit_->setEnabled(editable);
}
