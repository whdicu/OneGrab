#include "AutoWrapLabel.h"
#include <QFontMetrics>
#include <QSizePolicy>


AutoWrapLabel::AutoWrapLabel(QWidget *parent /*= nullptr*/)
	: QLabel(parent)
{
	setWordWrap(true);

	// 水平 Preferred、垂直 Preferred：宽度受 min/max 约束，高度随内容自适应
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

QSize AutoWrapLabel::sizeHint() const
{
	QFontMetrics fm(font());
	// 单行文本宽度
	//int textWidth = fm.horizontalAdvance(text()); // Qt 5.11+；旧版用 fm.width(text())
	int textWidth = fm.width(text());
	// 限制在 [minimumWidth, maximumWidth] 之间
	int w = qBound(minimumWidth(), textWidth, maximumWidth());

	// 计算在宽度 w 下换行后的高度
	QRect rect = fm.boundingRect(QRect(0, 0, w, 0),
		Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
		text());
	int h = rect.height();

	// 加上 QLabel 的内边距（如果有）
	int margin = this->margin() * 2; // margin() 默认 0
	return QSize(w + margin, h + margin);
}

QSize AutoWrapLabel::minimumSizeHint() const
{
	QFontMetrics fm(font());
	int margin = this->margin() * 2;
	// 最小高度取单行高度即可。绝不能按最小宽度折行计算，
	// 否则长文本会得到巨大的最小高度，把外层布局撑爆，导致整片消息不可见。
	return QSize(minimumWidth() + margin, fm.lineSpacing() + margin);
}
