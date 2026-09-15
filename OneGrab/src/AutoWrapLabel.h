#pragma once
#include <QLabel>


class AutoWrapLabel : public QLabel
{
public:
	AutoWrapLabel(QWidget* parent = nullptr);

	// 以 Markdown 文本设置内容（内部转成 HTML，交给 QLabel 富文本渲染）
	// 内部会记下这份 Markdown 源文本，供 appendMarkdown() 继续往后拼
	void setMarkdown(const QString& markdown);

	// 追加一段 Markdown 源文本并整体重新渲染 —— 给流式输出用：
	// 拼的是"源文本"，所以跨块的语法（```代码块、列表、表格）不会被截断
	void appendMarkdown(const QString& markdown);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	// 累积的 Markdown 源文本（注意：直接调 setText() 会绕过它，两者别混用）
	QString markdown_;
	// 内容与控件边框之间的边距
	int contentMargin() const;
	// 当前内容不折行时需要的宽度
	int idealContentWidth() const;
	// 当前内容在给定内容宽度下折行后的高度
	int contentHeightForWidth(int contentWidth) const;
};
