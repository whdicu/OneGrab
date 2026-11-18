#include "WindowsOcr.h"

#include <QDebug>
#include <QPointer>
#include <QPixmap>
#include <QRunnable>
#include <QThread>

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <asyncinfo.h>
#include <MemoryBuffer.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.globalization.h>
#include <windows.graphics.imaging.h>
#include <windows.media.ocr.h>

#include <cstring>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;
using Microsoft::WRL::Wrappers::HStringReference;

namespace Ocr = ABI::Windows::Media::Ocr;
namespace Imaging = ABI::Windows::Graphics::Imaging;
namespace Foundation = ABI::Windows::Foundation;
namespace Collections = ABI::Windows::Foundation::Collections;
namespace Globalization = ABI::Windows::Globalization;

namespace
{
	// 一轮识别最多等多久（毫秒）；超时说明引擎卡住了，直接报错
	const static int C_OCR_TIMEOUT_MS = 15000;
	const static int C_OCR_POLL_INTERVAL_MS = 10;

	QString hstringToQString(HSTRING value)
	{
		if (!value)
			return QString();

		UINT32 length = 0;
		const wchar_t* raw = WindowsGetStringRawBuffer(value, &length);
		return QString::fromWCharArray(raw, static_cast<int>(length));
	}

	QString hrText(HRESULT hr)
	{
		return QString("0x%1").arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
	}

	// WinRT 每个线程要各自初始化；needUninitialize 告诉调用方要不要配平 RoUninitialize
	bool ensureWinrtInitialized(QString& error, bool& needUninitialize)
	{
		needUninitialize = false;

		const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
		if (hr == S_OK)
		{
			needUninitialize = true;
			return true;
		}

		// S_FALSE：本线程已经初始化过；RPC_E_CHANGED_MODE：已经被 Qt 初始化成 STA 了，也能用
		if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE)
			return true;

		error = WindowsOcr::tr("WinRT 初始化失败: %1").arg(hrText(hr));
		return false;
	}

	// 调用前必须已经在本线程做过 WinRT 初始化
	ComPtr<Ocr::IOcrEngine> createOcrEngine(QString& error)
	{
		ComPtr<Ocr::IOcrEngine> engine;

		ComPtr<Ocr::IOcrEngineStatics> statics;
		HRESULT hr = RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Media_Ocr_OcrEngine).Get()
			, IID_PPV_ARGS(statics.GetAddressOf()));
		if (FAILED(hr) || !statics)
		{
			error = WindowsOcr::tr("当前系统不支持 Windows.Media.Ocr");
			return engine;
		}

		// 先按用户配置的语言创建，失败再挨个试系统里装了 OCR 的语言包
		hr = statics->TryCreateFromUserProfileLanguages(engine.GetAddressOf());
		if (FAILED(hr) || !engine)
		{
			ComPtr<Collections::IVectorView<Globalization::Language*>> languages;
			if (SUCCEEDED(statics->get_AvailableRecognizerLanguages(languages.GetAddressOf())) && languages)
			{
				UINT32 count = 0;
				languages->get_Size(&count);
				for (UINT32 i = 0; i < count && !engine; ++i)
				{
					ComPtr<Globalization::ILanguage> language;
					if (FAILED(languages->GetAt(i, language.GetAddressOf())) || !language)
						continue;

					statics->TryCreateFromLanguage(language.Get(), engine.GetAddressOf());
				}
			}
		}

		if (!engine)
			error = WindowsOcr::tr("系统里没有可用的 OCR 语言包（设置 → 时间和语言 → 语言 里安装语言包）");

		return engine;
	}

	// 把 QImage 填进 SoftwareBitmap。小端下 QImage::Format_ARGB32 的内存排列就是 BGRA，
	// 和 BitmapPixelFormat_Bgra8 一致
	bool fillSoftwareBitmap(const QImage& image, Imaging::ISoftwareBitmap* bitmap, QString& error)
	{
		ComPtr<Imaging::IBitmapBuffer> buffer;
		HRESULT hr = bitmap->LockBuffer(Imaging::BitmapBufferAccessMode_Write, buffer.GetAddressOf());
		if (FAILED(hr) || !buffer)
		{
			error = WindowsOcr::tr("锁定 OCR 图片缓冲区失败: %1").arg(hrText(hr));
			return false;
		}

		// BitmapBuffer 本身不暴露 IMemoryBufferByteAccess，得先拿到 IMemoryBuffer 再 CreateReference，
		// 真正的字节访问接口在 reference 上（拿 buffer 直接 QI 会 E_NOINTERFACE）
		ComPtr<Foundation::IMemoryBuffer> memoryBuffer;
		ComPtr<Foundation::IMemoryBufferReference> reference;

		bool ok = false;
		do
		{
			hr = buffer.As(&memoryBuffer);
			if (FAILED(hr) || !memoryBuffer)
			{
				error = WindowsOcr::tr("访问 OCR 图片缓冲区失败: %1").arg(hrText(hr));
				break;
			}

			hr = memoryBuffer->CreateReference(reference.GetAddressOf());
			if (FAILED(hr) || !reference)
			{
				error = WindowsOcr::tr("创建 OCR 图片缓冲区引用失败: %1").arg(hrText(hr));
				break;
			}

			ComPtr<Windows::Foundation::IMemoryBufferByteAccess> access;
			hr = reference.As(&access);
			if (FAILED(hr) || !access)
			{
				error = WindowsOcr::tr("访问 OCR 图片缓冲区失败: %1").arg(hrText(hr));
				break;
			}

			BYTE* dest = nullptr;
			UINT32 capacity = 0;
			hr = access->GetBuffer(&dest, &capacity);
			if (FAILED(hr) || !dest)
			{
				error = WindowsOcr::tr("获取 OCR 图片缓冲区失败: %1").arg(hrText(hr));
				break;
			}

			Imaging::BitmapPlaneDescription plane = {};
			hr = buffer->GetPlaneDescription(0, &plane);
			if (FAILED(hr))
			{
				error = WindowsOcr::tr("获取 OCR 图片布局失败: %1").arg(hrText(hr));
				break;
			}

			const int rowBytes = image.width() * 4;
			if (plane.Stride < rowBytes
				|| static_cast<qint64>(plane.StartIndex) + static_cast<qint64>(plane.Stride) * image.height() > capacity)
			{
				error = WindowsOcr::tr("OCR 图片缓冲区尺寸不匹配");
				break;
			}

			for (int y = 0; y < image.height(); ++y)
				memcpy(dest + plane.StartIndex + static_cast<qint64>(y) * plane.Stride, image.constScanLine(y), rowBytes);

			ok = true;
		} while (false);

		// 引用和锁都要关掉，否则 SoftwareBitmap 后面没法用
		ComPtr<Foundation::IClosable> referenceClosable;
		if (reference && SUCCEEDED(reference.As(&referenceClosable)) && referenceClosable)
			referenceClosable->Close();

		ComPtr<Foundation::IClosable> bufferClosable;
		if (SUCCEEDED(buffer.As(&bufferClosable)) && bufferClosable)
			bufferClosable->Close();

		return ok;
	}

	// 前提：本线程已经做过 WinRT 初始化
	bool recognizeImpl(const QImage& source, WindowsOcr::Result& result, QString& error)
	{
		if (source.isNull())
		{
			error = WindowsOcr::tr("图片为空");
			return false;
		}

		ComPtr<Ocr::IOcrEngineStatics> statics;
		HRESULT hr = RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Media_Ocr_OcrEngine).Get()
			, IID_PPV_ARGS(statics.GetAddressOf()));
		if (FAILED(hr) || !statics)
		{
			error = WindowsOcr::tr("当前系统不支持 Windows.Media.Ocr");
			return false;
		}

		UINT32 maxDimension = 0;
		statics->get_MaxImageDimension(&maxDimension);

		// 超过引擎上限就等比缩小；坐标以缩放后的图为准
		QImage image = source;
		if (maxDimension > 0
			&& (image.width() > static_cast<int>(maxDimension) || image.height() > static_cast<int>(maxDimension)))
		{
			image = image.scaled(static_cast<int>(maxDimension), static_cast<int>(maxDimension)
				, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}

		if (image.format() != QImage::Format_ARGB32)
			image = image.convertToFormat(QImage::Format_ARGB32);

		ComPtr<Ocr::IOcrEngine> engine = createOcrEngine(error);
		if (!engine)
			return false;

		ComPtr<Imaging::ISoftwareBitmapFactory> bitmapFactory;
		hr = RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Graphics_Imaging_SoftwareBitmap).Get()
			, IID_PPV_ARGS(bitmapFactory.GetAddressOf()));
		if (FAILED(hr) || !bitmapFactory)
		{
			error = WindowsOcr::tr("创建 OCR 图片对象失败");
			return false;
		}

		ComPtr<Imaging::ISoftwareBitmap> bitmap;
		hr = bitmapFactory->Create(Imaging::BitmapPixelFormat_Bgra8
			, image.width(), image.height(), bitmap.GetAddressOf());
		if (FAILED(hr) || !bitmap)
		{
			error = WindowsOcr::tr("创建 OCR 图片对象失败");
			return false;
		}

		if (!fillSoftwareBitmap(image, bitmap.Get(), error))
			return false;

		ComPtr<ABI::Windows::Foundation::__FIAsyncOperation_1_Windows__CMedia__COcr__COcrResult_t> operation;
		hr = engine->RecognizeAsync(bitmap.Get(), operation.GetAddressOf());
		if (FAILED(hr) || !operation)
		{
			error = WindowsOcr::tr("启动 OCR 识别失败: %1").arg(hrText(hr));
			return false;
		}

		ComPtr<Foundation::IAsyncInfo> asyncInfo;
		hr = operation.As(&asyncInfo);
		if (FAILED(hr) || !asyncInfo)
		{
			error = WindowsOcr::tr("获取 OCR 任务状态失败");
			return false;
		}

		// 这里本来就在工作线程里，用轮询等结果就行；超时兜底，避免永远卡住
		Foundation::AsyncStatus status = Foundation::AsyncStatus::Started;
		const int maxLoops = C_OCR_TIMEOUT_MS / C_OCR_POLL_INTERVAL_MS;
		for (int i = 0; i < maxLoops; ++i)
		{
			asyncInfo->get_Status(&status);
			if (status != Foundation::AsyncStatus::Started)
				break;
			QThread::msleep(C_OCR_POLL_INTERVAL_MS);
		}

		if (status != Foundation::AsyncStatus::Completed)
		{
			if (status == Foundation::AsyncStatus::Error)
			{
				HRESULT errorCode = S_OK;
				asyncInfo->get_ErrorCode(&errorCode);
				error = WindowsOcr::tr("OCR 识别失败: %1").arg(hrText(errorCode));
			}
			else
			{
				error = WindowsOcr::tr("OCR 识别超时");
			}
			return false;
		}

		ComPtr<Ocr::IOcrResult> ocrResult;
		hr = operation->GetResults(ocrResult.GetAddressOf());
		if (FAILED(hr) || !ocrResult)
		{
			error = WindowsOcr::tr("获取 OCR 结果失败: %1").arg(hrText(hr));
			return false;
		}

		result = WindowsOcr::Result();
		result.imageWidth = image.width();
		result.imageHeight = image.height();

		ComPtr<Collections::IVectorView<Ocr::OcrLine*>> lines;
		hr = ocrResult->get_Lines(lines.GetAddressOf());
		if (FAILED(hr) || !lines)
		{
			error = WindowsOcr::tr("OCR 结果里没有文本行");
			return false;
		}

		UINT32 lineCount = 0;
		lines->get_Size(&lineCount);
		for (UINT32 i = 0; i < lineCount; ++i)
		{
			ComPtr<Ocr::IOcrLine> ocrLine;
			if (FAILED(lines->GetAt(i, ocrLine.GetAddressOf())) || !ocrLine)
				continue;

			WindowsOcr::Line line;
			HString lineText;
			ocrLine->get_Text(lineText.GetAddressOf());
			line.text = hstringToQString(lineText.Get());

			ComPtr<Collections::IVectorView<Ocr::OcrWord*>> words;
			if (SUCCEEDED(ocrLine->get_Words(words.GetAddressOf())) && words)
			{
				UINT32 wordCount = 0;
				words->get_Size(&wordCount);
				for (UINT32 w = 0; w < wordCount; ++w)
				{
					ComPtr<Ocr::IOcrWord> ocrWord;
					if (FAILED(words->GetAt(w, ocrWord.GetAddressOf())) || !ocrWord)
						continue;

					HString wordText;
					Foundation::Rect rect = {};
					ocrWord->get_Text(wordText.GetAddressOf());
					ocrWord->get_BoundingRect(&rect);

					WindowsOcr::Word word;
					word.text = hstringToQString(wordText.Get());
					word.rect = QRectF(rect.X, rect.Y, rect.Width, rect.Height);

					// IOcrLine 本身没有 BoundingRect，行框取所有词框的并集
					line.rect = line.words.isEmpty() ? word.rect : line.rect.united(word.rect);
					line.words.append(word);
				}
			}

			// 个别语言下 get_Text 可能为空，用词拼一个兜底
			if (line.text.isEmpty() && !line.words.isEmpty())
			{
				QStringList parts;
				for (const WindowsOcr::Word& word : line.words)
					parts.append(word.text);
				line.text = parts.join(' ');
			}

			result.lines.append(line);
		}

		return true;
	}

	bool recognizeWithWinrt(const QImage& source, WindowsOcr::Result& result, QString& error)
	{
		bool needUninitialize = false;
		QString initError;
		if (!ensureWinrtInitialized(initError, needUninitialize))
		{
			error = initError;
			return false;
		}

		const bool ok = recognizeImpl(source, result, error);

		if (needUninitialize)
			RoUninitialize();

		return ok;
	}

	// 后台任务：识别完把结果丢回对象所在线程
	class OcrTask : public QRunnable
	{
	public:
		OcrTask(const QImage& image, WindowsOcr* owner)
			: image_(image)
			, owner_(owner)
		{
			setAutoDelete(true);
		}

		void run() override
		{
			WindowsOcr::Result result;
			QString error;
			const bool ok = recognizeWithWinrt(image_, result, error);

			if (!owner_)
				return;

			if (ok)
			{
				QMetaObject::invokeMethod(owner_, "handleFinished", Qt::QueuedConnection
					, Q_ARG(WindowsOcr::Result, result));
			}
			else
			{
				QMetaObject::invokeMethod(owner_, "handleFailed", Qt::QueuedConnection
					, Q_ARG(QString, error));
			}
		}

	private:
		QImage image_;
		QPointer<WindowsOcr> owner_;
	};
}


QString WindowsOcr::Result::allText() const
{
	QStringList texts;
	for (const Line& line : lines)
	{
		if (!line.text.isEmpty())
			texts.append(line.text);
	}
	return texts.join('\n');
}

WindowsOcr::WindowsOcr(QObject* parent)
	: QObject(parent)
{
	qRegisterMetaType<WindowsOcr::Result>("WindowsOcr::Result");

	// OCR 单线程跑就够了，连续调用时按顺序执行，也不会把 CPU 打满
	pool_.setMaxThreadCount(1);
}

WindowsOcr::~WindowsOcr()
{
	// 等后台识别收尾，避免线程里还拿着 this
	pool_.waitForDone();
}

bool WindowsOcr::isAvailable(QString* error)
{
	bool needUninitialize = false;
	QString initError;
	if (!ensureWinrtInitialized(initError, needUninitialize))
	{
		if (error)
			*error = initError;
		return false;
	}

	QString engineError;
	bool available = false;
	{
		// engine 必须在 RoUninitialize 之前析构
		const ComPtr<Ocr::IOcrEngine> engine = createOcrEngine(engineError);
		available = engine != nullptr;
	}

	if (!available && error)
		*error = engineError;

	if (needUninitialize)
		RoUninitialize();

	return available;
}

void WindowsOcr::recognize(const QPixmap& pixmap)
{
	recognize(pixmap.toImage());
}

void WindowsOcr::recognize(const QImage& image)
{
	if (image.isNull())
	{
		emit failed(tr("图片为空"));
		return;
	}

	// 深拷贝一份给后台线程，调用方的图可以随便丢
	pool_.start(new OcrTask(image.copy(), this));
}

void WindowsOcr::handleFinished(WindowsOcr::Result result)
{
	emit finished(result);
}

void WindowsOcr::handleFailed(const QString& error)
{
	emit failed(error);
}
