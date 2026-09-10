#include "MarkdownHelper.h"

#include <QRegularExpression>
#include <QStringList>


namespace
{
const static QString INLINE_CODE_STYLE = "background-color:#F0F0F2;";
const static QString CODE_BLOCK_STYLE = "background-color:#F5F5F7;";
const static QString QUOTE_STYLE = "color:#6A6A72;";

QString escapeHtml(const QString& text)
{
	QString out;
	out.reserve(text.size());
	for (const QChar& c : text)
	{
		if (c == '&')
			out += "&amp;";
		else if (c == '<')
			out += "&lt;";
		else if (c == '>')
			out += "&gt;";
		else if (c == '"')
			out += "&quot;";
		else
			out += c;
	}
	return out;
}

// 处理一行内的行内标记：`code`、**粗体**、*斜体*、~~删除线~~、[文字](链接)
QString inlineToHtml(const QString& raw, int depth=0)
{
	if (depth > 4)  // 递归保护，避免极端嵌套把栈打穿
		return escapeHtml(raw);

	QString out;
	const int n = raw.size();
	int i = 0;

	while (i < n)
	{
		const QChar c = raw.at(i);

		// 行内代码
		if (c == '`')
		{
			const int end = raw.indexOf('`', i + 1);
			if (end > i + 1)
			{
				out += "<span style=\"" + INLINE_CODE_STYLE + "\">"
					+ escapeHtml(raw.mid(i + 1, end - i - 1)) + "</span>";
				i = end + 1;
				continue;
			}
		}

		// 链接 [文字](地址)
		if (c == '[')
		{
			const int rb = raw.indexOf(']', i + 1);
			if (rb > i && rb + 1 < n && raw.at(rb + 1) == '(')
			{
				const int rp = raw.indexOf(')', rb + 2);
				if (rp > rb)
				{
					const QString text = raw.mid(i + 1, rb - i - 1);
					const QString href = raw.mid(rb + 2, rp - rb - 2);
					out += "<a href=\"" + escapeHtml(href) + "\">"
						+ inlineToHtml(text, depth + 1) + "</a>";
					i = rp + 1;
					continue;
				}
			}
		}

		// 删除线 ~~文字~~
		if (c == '~' && i + 1 < n && raw.at(i + 1) == '~')
		{
			const int end = raw.indexOf("~~", i + 2);
			if (end > i + 1)
			{
				out += "<s>" + inlineToHtml(raw.mid(i + 2, end - i - 2), depth + 1) + "</s>";
				i = end + 2;
				continue;
			}
		}

		// 粗体 **文字** / __文字__
		if ((c == '*' || c == '_') && i + 1 < n && raw.at(i + 1) == c)
		{
			const QString marker = QString(c) + c;
			const int end = raw.indexOf(marker, i + 2);
			if (end > i + 1)
			{
				out += "<b>" + inlineToHtml(raw.mid(i + 2, end - i - 2), depth + 1) + "</b>";
				i = end + 2;
				continue;
			}
		}

		// 斜体 *文字* / _文字_
		if (c == '*' || c == '_')
		{
			const int end = raw.indexOf(c, i + 1);
			if (end > i)
			{
				out += "<i>" + inlineToHtml(raw.mid(i + 1, end - i - 1), depth + 1) + "</i>";
				i = end + 1;
				continue;
			}
		}

		// 普通字符
		if (c == '&')
			out += "&amp;";
		else if (c == '<')
			out += "&lt;";
		else if (c == '>')
			out += "&gt;";
		else if (c == '"')
			out += "&quot;";
		else
			out += c;
		++i;
	}

	return out;
}
}


QString MarkdownHelper::toHtml(const QString& markdown)
{
	QString normalized = markdown;
	normalized.replace("\r\n", "\n");
	normalized.replace('\r', '\n');

	const QStringList lines = normalized.split('\n');

	enum class ListType { None, Unordered, Ordered };

	QString html;
	ListType listType = ListType::None;
	bool inCodeBlock = false;
	QString codeBuffer;

	auto closeList = [&html, &listType]()
	{
		if (listType == ListType::Unordered)
			html += "</ul>";
		else if (listType == ListType::Ordered)
			html += "</ol>";
		listType = ListType::None;
	};

	for (const QString& rawLine : lines)
	{
		// 代码块围栏
		if (rawLine.trimmed().startsWith("```"))
		{
			if (!inCodeBlock)
			{
				closeList();
				inCodeBlock = true;
				codeBuffer.clear();
			}
			else
			{
				inCodeBlock = false;
				html += "<pre style=\"" + CODE_BLOCK_STYLE + "\">"
					+ escapeHtml(codeBuffer) + "</pre>";
			}
			continue;
		}

		// 代码块内部原样保留（含缩进）
		if (inCodeBlock)
		{
			if (!codeBuffer.isEmpty())
				codeBuffer += "\n";
			codeBuffer += rawLine;
			continue;
		}

		const QString trimmed = rawLine.trimmed();

		// 空行：结束当前列表
		if (trimmed.isEmpty())
		{
			closeList();
			continue;
		}

		// 水平线
		if (trimmed == "---" || trimmed == "***" || trimmed == "___")
		{
			closeList();
			html += "<hr>";
			continue;
		}

		// 标题
		if (trimmed.startsWith('#'))
		{
			int level = 0;
			while (level < trimmed.size() && level < 6 && trimmed.at(level) == '#')
				++level;

			if (level > 0 && level < trimmed.size() && trimmed.at(level) == ' ')
			{
				closeList();
				const QString text = trimmed.mid(level + 1);
				const QString tag = QString::number(level);
				html += "<h" + tag + ">" + inlineToHtml(text) + "</h" + tag + ">";
				continue;
			}
		}

		// 引用
		if (trimmed.startsWith("> "))
		{
			closeList();
			html += "<blockquote style=\"" + QUOTE_STYLE + "\">"
				+ inlineToHtml(trimmed.mid(2)) + "</blockquote>";
			continue;
		}

		// 无序列表
		if (trimmed.startsWith("- ") || trimmed.startsWith("* ") || trimmed.startsWith("+ "))
		{
			if (listType != ListType::Unordered)
			{
				closeList();
				html += "<ul>";
				listType = ListType::Unordered;
			}
			html += "<li>" + inlineToHtml(trimmed.mid(2)) + "</li>";
			continue;
		}

		// 有序列表
		{
			const static QRegularExpression ORDERED_LIST_RE("^\\d+\\.\\s+(.*)$");
			const QRegularExpressionMatch match = ORDERED_LIST_RE.match(trimmed);
			if (match.hasMatch())
			{
				if (listType != ListType::Ordered)
				{
					closeList();
					html += "<ol>";
					listType = ListType::Ordered;
				}
				html += "<li>" + inlineToHtml(match.captured(1)) + "</li>";
				continue;
			}
		}

		// 普通段落
		closeList();
		html += "<p>" + inlineToHtml(trimmed) + "</p>";
	}

	// 收尾：未闭合的代码块和列表
	if (inCodeBlock && !codeBuffer.isEmpty())
	{
		html += "<pre style=\"" + CODE_BLOCK_STYLE + "\">"
			+ escapeHtml(codeBuffer) + "</pre>";
	}
	closeList();

	return html;
}
