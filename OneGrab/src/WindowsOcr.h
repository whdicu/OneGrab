#pragma once

#include <QObject>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QList>
#include <QThreadPool>

class QPixmap;

// 本地 OCR：Windows.Media.Ocr（Win10 自带，不联网、不计费、延迟低）。
// 只处理图片；PDF / Word 这类文档仍然走 AIHandler::callOcr（DeepSeek 接口）。
// 结果给到逐行 / 逐词的像素坐标，适合框选、复制指定行这类操作。
class WindowsOcr : public QObject
{
	Q_OBJECT

public:
	struct Word
	{
		QString text;
		QRectF rect;   // 像素坐标，相对实际送去识别的图
	};

	struct Line
	{
		QString text;
		QRectF rect;   // 这一行所有词的并集
		QList<Word> words;
	};

	struct Result
	{
		QList<Line> lines;
		int imageWidth = 0;    // 实际送去识别的图尺寸（超过引擎上限时会等比缩小）
		int imageHeight = 0;

		// 按行拼成纯文本
		QString allText() const;
	};

	explicit WindowsOcr(QObject* parent=nullptr);
	~WindowsOcr();

	// 系统里有没有可用的 OCR 语言包；没有时 error 给出原因
	static bool isAvailable(QString* error=nullptr);

	// 异步识别：内部深拷贝图片，结果统一回到对象所在线程。
	// 超大图超过引擎上限时会自动等比缩小，坐标以 Result::imageWidth/Height 为准。
	void recognize(const QPixmap& pixmap);
	void recognize(const QImage& image);

signals:
	void finished(const WindowsOcr::Result& result);
	void failed(const QString& error);

private slots:
	void handleFinished(WindowsOcr::Result result);
	void handleFailed(const QString& error);

private:
	// 单线程跑识别，避免同时开多个识别把 CPU/内存打满；析构时等它收尾
	QThreadPool pool_;
};

Q_DECLARE_METATYPE(WindowsOcr::Result)
