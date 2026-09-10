#pragma once

#include <QString>


// 轻量 Markdown -> HTML 转换。
// Qt 5.14 以下没有 QTextDocument::setMarkdown()，所以这里把常用的 Markdown
// 语法转成 Qt 富文本（QLabel / QTextDocument）支持的 HTML 子集后再显示。
class MarkdownHelper
{
public:
	// 支持的语法：标题、粗体、斜体、删除线、行内代码、代码块、
	// 有序/无序列表、引用、链接、水平线。
	static QString toHtml(const QString& markdown);
};
