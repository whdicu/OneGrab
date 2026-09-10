#pragma once
#include <QLabel>


class AutoWrapLabel : public QLabel
{
public:
	AutoWrapLabel(QWidget* parent = nullptr);

	// 以 Markdown 文本设置内容（内部转成 HTML，交给 QLabel 富文本渲染）
	void setMarkdown(const QString& markdown);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	// 内容与控件边框之间的边距
	int contentMargin() const;
	// 当前内容不折行时需要的宽度
	int idealContentWidth() const;
	// 当前内容在给定内容宽度下折行后的高度
	int contentHeightForWidth(int contentWidth) const;
};
