#include "AutoWrapLabel.h"
#include "MarkdownHelper.h"

#include <QFontMetrics>
#include <QSizePolicy>
#include <QTextDocument>
#include <QtMath>


namespace
{
// QLabel 未显式设置 margin 时，其内部 QTextDocument 使用的默认文档边距
const static int DEFAULT_DOC_MARGIN = 4;
}


AutoWrapLabel::AutoWrapLabel(QWidget *parent /*= nullptr*/)
	: QLabel(parent)
{
	setWordWrap(true);

	// 水平 Preferred、垂直 Preferred：宽度受 min/max 约束，高度随内容自适应
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void AutoWrapLabel::setMarkdown(const QString& markdown)
{
	setText(MarkdownHelper::toHtml(markdown));
}

int AutoWrapLabel::contentMargin() const
{
	const int m = margin();
	return m > 0 ? m : DEFAULT_DOC_MARGIN;
}

int AutoWrapLabel::idealContentWidth() const
{
	QTextDocument doc;
	doc.setDefaultFont(font());
	doc.setDocumentMargin(0);
	doc.setHtml(text());
	return qCeil(doc.idealWidth());
}

int AutoWrapLabel::contentHeightForWidth(int contentWidth) const
{
	QTextDocument doc;
	doc.setDefaultFont(font());
	doc.setDocumentMargin(0);
	doc.setHtml(text());
	doc.setTextWidth(qMax(1, contentWidth));
	return qCeil(doc.size().height());
}

QSize AutoWrapLabel::sizeHint() const
{
	// 富文本（HTML）的高度必须用 QTextDocument 量，不能用 QFontMetrics——
	// text() 返回的是 HTML 源码，按纯文本量出来的宽高全是错的。
	const int m = contentMargin();
	const int ideal = idealContentWidth() + 2 * m;
	const int w = qBound(minimumWidth(), ideal, maximumWidth());
	const int h = contentHeightForWidth(w - 2 * m) + 2 * m;
	return QSize(w, h);
}

QSize AutoWrapLabel::minimumSizeHint() const
{
	const int m = contentMargin();
	// 最小高度取单行高度。绝不能按最小宽度折行计算，
	// 否则长文本会得到巨大的最小高度，把外层布局撑爆，导致整片消息不可见。
	const QFontMetrics fm(font());
	return QSize(minimumWidth(), fm.lineSpacing() + 2 * m);
}
