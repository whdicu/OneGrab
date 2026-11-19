#include "OneGrab.h"
#include "AIHandler.h"
#include "AITalkWidget.h"
#include "BtnBar.h"
#include "HDCore/DBoolSetter.hpp"
#include "HDCore/DSys.hpp"
#include "HDQt/include/DToast.h"
#include "DMessageBox.h"
#include "DProgressBox.h"
#include "DScreenCapture.h"
#include "DUpdateHandler.h"
#include "ImageHandler.h"
#include "ImageThread.h"
#include <opencv2/imgproc.hpp>
#include "LabelIsland1.h"
#include "LabelIsland2.h"
#include "LabelIsland3.h"
#include "LabelIsland4.h"
#include "LabelIsland5.h"
#include "MouseWindow.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMimeData>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include "BarcodeDecoder.h"
#include "QRCodeBar.h"
#include <QRegularExpression>
#include <QScreen>
#include <QSharedPointer>
#include <QTextBlock>
#include <QTextDocument>
#include <vector>
#include "SettingDialog.h"
#include "SettingHandler.h"
#include "version.h"
#include "WindowsOcr.h"


const static int MARGIN = 5;
const static int COPY_TEMP_SIZE = 64;  // 复制图片到文件的最大图片保存数量
// 翻译用的系统提示词：直接把图片交给模型做 OCR + 翻译 + 排版，输出严格 JSON
const static QString SYSTEM_STR =
	"你是一名 OCR + 翻译 + 排版助手。用户会给你一张截图和图片的像素尺寸。\n"
	"任务：自己找出图中所有需要翻译的文字块，翻译成简体中文，并给出重新绘制所需的信息。\n"
	"规则：\n"
	"1. 已经是中文的文字不要返回；纯数字、纯符号、代码、URL、邮箱、版本号、文件路径也不要返回；\n"
	"2. 同一句话在视觉上被拆成多段（同一行相邻，或换行连续）要合并成一个文字块，一个块只给一条译文；\n"
	"   不要把不同控件、不同句子、不同列的内容合在一起；\n"
	"3. 每个文字块输出：\n"
	"   text：识别出的原文，按原样输出，不要翻译；\n"
	"   rect：[x, y, w, h]，该块所有文字在图片中的像素矩形；\n"
	"   bg_color：[r, g, b]，该块的背景色（0~255）；\n"
	"   text_color：[r, g, b]，该块的文字颜色（0~255）；\n"
	"   font：原文字体族名称的字符串（例如 \"Microsoft YaHei UI\"、\"SimSun\"、\"HeiTi\"），无法判断返回空字符串；\n"
	"   draw_rect：[x, y, w, h]，译文最终绘制的区域，默认等于 rect；译文比原文短时不要缩小，\n"
	"     需要更多空间时可以向右、向下扩展，但不能压到其他文字块，也不能超出图片；\n"
	"   translation：中文译文。\n"
	"4. 排版约束（重要）：\n"
	"   - 译文字数要接近原区域能放下的字数（中文字宽≈字高）：明显超出会溢出，明显偏少说明过度压缩；\n"
	"   - 空间不够用更简洁的表达（意译、省略语气助词、必要时用通行缩写），空间富余不要注水；\n"
	"   - 同一句话只补一次标点，不要每段都加句号或大写开头；\n"
	"   - 专有名词、品牌名、缩写保留原文或使用通行译名。\n"
	"5. 只返回真正翻译成中文的块：保留原文、译文与原文相同（忽略大小写和标点）、\n"
	"   只是优化排版格式、或只是把标点换成中文标点的，都不要返回；没有可翻译内容返回 []。\n"
	"6. 坐标以图片左上角为原点、单位像素，必须是整数；颜色必须是图中真实取到的颜色，不要臆造。\n"
	"7. 只输出 JSON 数组，不要 markdown 代码块、不要解释或多余文字。\n"
	"格式示例：\n"
	"{\"text\":\"Hello world\",\"rect\":[10,20,142,30],\"bg_color\":[255,255,255],"
	"\"text_color\":[30,30,30],\"font\":\"Microsoft YaHei UI\",\"draw_rect\":[10,20,142,30],\"translation\":\"你好，世界\"}\n"
	"多个块就是这样的对象组成的数组。";

// Windows OCR 路径：本地先识别文字和矩形，模型只负责翻译和排版
const static QString SYSTEM_STR_WINOCR =
	"你是一名专业翻译兼排版助手。译文之后会被重新绘制回原图，所以既要准确，也要考虑版面。\n"
	"输入是一个 JSON 对象：\n"
	"- image：原图的 {width, height}（像素）\n"
	"- lines：数组，每项 {id, text, rect:[x,y,w,h], max_chars}\n"
	"  rect 以原图左上角为原点、单位像素；max_chars 是该矩形大致能放下的中文字符数。\n"
	"规则：\n"
	"1. 已经是中文的条目不要输出；纯数字、纯符号、代码、URL、邮箱、版本号、文件路径不翻译也不输出；\n"
	"2. OCR 可能把一句话、一个词组或一个单词拆成多个相邻条目（rect 在同一行或紧挨着），id 按阅读顺序排列。\n"
	"   先把确实相邻且语义连续的碎片合并成一组，不要把不同控件、不同句子、不同列的内容硬凑在一起。\n"
	"3. 每组只输出一条：{\"ids\":[该组所有 id，按阅读顺序], \"rect\":[x,y,w,h], \"translation\":\"整组译文\"}。\n"
	"   没有需要翻译的内容就输出 []。\n"
	"4. rect 是这条译文最终绘制的区域：\n"
	"   - 默认取该组所有输入 rect 的并集；\n"
	"   - 译文比原文短时仍保留原区域，不要缩到只剩文字大小；\n"
	"   - 译文需要更多空间时可以适当向右、向下扩展，但不能压到其他条目的 rect，也不能超出 image 范围；\n"
	"   - 宽高必须是正数。\n"
	"5. 排版约束（重要）：\n"
	"   - 译文字数要接近该组 max_chars 之和：明显超出会溢出原区域，明显偏少说明过度压缩；\n"
	"   - 空间不够用更简洁的表达（意译、省略语气助词、必要时用通行缩写），空间富余不要注水；\n"
	"   - 同一句话合并后只补一次标点，不要每段都加句号或大写开头；\n"
	"   - 专有名词、品牌名、缩写保留原文或使用通行译名。\n"
	"6. 只返回真正翻译成中文的组：保留原文、译文与原文相同（忽略大小写和标点）、\n"
	"   只是优化排版格式、或只是把标点换成中文标点的，都不要返回。\n"
	"7. 只输出 JSON 数组，不要 markdown 代码块、不要解释或多余文字。";


// 不改变可见性地把窗口顶到最前。
// 原来靠 hide()+show() 顶 Z 序：hide 会让窗口真的消失，show 之前一直不显示；
// 而全屏截图窗成为前台窗后能把 topmost 的工具窗压下去，只能靠"再 show 一次"兜回来，
// 一旦漏掉一次（比如画图时的那次按下）就会看到按钮栏和放大窗消失。
static void bringWindowToFront(QWidget* widget)
{
	if (nullptr == widget)
		return;

	if (!widget->isVisible())
		widget->show();

	::SetWindowPos(reinterpret_cast<HWND>(widget->winId()), HWND_TOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	// 被盖住再顶上来后重贴一遍内容，避免留下空白窗口
	widget->update();
}


namespace
{
	inline QString generateImageId()
	{
		return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	}

	// 已经是中文、或者压根没有字母（纯数字/符号）的行不需要翻译
	bool needsTranslation(const QString& text)
	{
		bool hasLetter = false;
		for (const QChar ch : text)
		{
			const ushort code = ch.unicode();
			// CJK 统一表意文字、扩展 A、兼容表意文字
			if ((code >= 0x3400 && code <= 0x4DBF)
				|| (code >= 0x4E00 && code <= 0x9FFF)
				|| (code >= 0xF900 && code <= 0xFAFF))
				return false;

			if (ch.isLetter())
				hasLetter = true;
		}

		return hasLetter;
	}

	// 对每个像素的前 3 个颜色通道做 3×3 均值滤波：取周围 8 个像素（不含自身）该通道的平均值。
	// 4 通道时 alpha 保持不变。kernel 中心为 0、周围 8 个为 1，再除以 8。
	void apply33MeanFilter(cv::Mat& mat)
	{
		static const cv::Mat kernel = []()
		{
			cv::Mat k = cv::Mat::ones(3, 3, CV_32F);
			k.at<float>(1, 1) = 0.0f;  // 中心为 0，表示不含自身
			return k / 8.0f;
		}();

		const int channels = mat.channels();
		const int colorChannels = (channels >= 3) ? 3 : channels;

		std::vector<cv::Mat> planes;
		cv::split(mat, planes);
		for (int c = 0; c < colorChannels; ++c)
		{
			cv::Mat blurred;
			cv::filter2D(planes[c], blurred, -1, kernel, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
			planes[c] = blurred;
		}
		cv::merge(planes, mat);
	}

	// Qt 渲染剪贴板 HTML 时空行会异常变高：块级元素后的 <br> 会渲染成 2 行高的空块，
	// 块级标签之间的源码缩进会渲染成 1 行高的空白块。这里先把 HTML 规范化。
	QString normalizeHtml(const QString& html)
	{
		// 注意只处理块级标签周边：行内标签（span 等）之间的空白是正文内容
		// （如 "except<span> </span>Exception"），删了会把单词粘在一起
		static const QRegularExpression blankAfterBlock(
			"(</(?:html|body|div|p|li|tr|h[1-6]|blockquote|table|ul|ol)>"
			"|<(?:html|body|div|p|li|tr|h[1-6]|blockquote|table|ul|ol)\\b[^>]*>"
			"|<br\\s*/?>)\\s+(?=<)");
		static const QRegularExpression blankBeforeBlockEnd(
			"\\s+(?=</(?:html|body|div|p|li|tr|h[1-6]|blockquote|table|ul|ol)>)");
		static const QRegularExpression blockBr(
			"(</(?:html|body|div|p|li|tr|h[1-6]|blockquote|table|ul|ol)>)<br\\s*/?>");

		QString result = html;

		// 块级标签之间的纯空白文本节点（源码换行/缩进）不会显示，去掉避免多余空白块
		result.replace(blankAfterBlock, "\\1");
		result.replace(blankBeforeBlockEnd, "");

		// 块级元素后的 <br> 在 Qt 里算 2 行，换成 1 行高的空 div
		result.replace(blockBr, "\\1<div>&nbsp;</div>");

		return result;
	}

	QImage htmlToImage(const QString& html)
	{
		QTextDocument document;
		document.setHtml(normalizeHtml(html));
		document.setDocumentMargin(0);
		document.setTextWidth(document.idealWidth());

		// Qt 富文本引擎不会把外层 div 的 background-color 继承到内部子块，
		// 导致只有外层 div 前导空白行有背景、其余行透明，这里取出来铺满整图
		QBrush bgBrush;
		QColor listTextColor;
		for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
		{
			if (Qt::NoBrush == bgBrush.style() && Qt::NoBrush != block.blockFormat().background().style())
				bgBrush = block.blockFormat().background();

			if (!listTextColor.isValid() && block.textList())
				listTextColor = block.charFormat().foreground().color();
		}

		// Qt 的列表 marker 颜色取自 QPalette::Text，与 CSS 和字符格式都无关，
		// 深色背景下会被画成黑色，这里临时把 palette 文本色调成列表文字颜色
		QPalette oldPalette = QApplication::palette();
		bool paletteChanged = false;
		if (listTextColor.isValid() && listTextColor != oldPalette.color(QPalette::Text))
		{
			QPalette palette = oldPalette;
			palette.setColor(QPalette::Text, listTextColor);
			QApplication::setPalette(palette);
			paletteChanged = true;
		}

		//document.adjustSize();

		QSize size = document.size().toSize();

		QImage image(size, QImage::Format_ARGB32);
		image.fill(bgBrush.style() != Qt::NoBrush ? bgBrush.color() : Qt::transparent);

		QPainter painter(&image);
		document.drawContents(&painter);

		if (paletteChanged)
			QApplication::setPalette(oldPalette);

		return image;
	}

	QImage textToImage(const QString& text)
	{
		QTextDocument document;
		QFont font("Consolas", 20);
		font.setWeight(QFont::Medium);  // 笔画稍微加粗
		document.setDefaultFont(font);
		document.setPlainText(text);
		document.setDocumentMargin(0);
		document.setTextWidth(document.idealWidth());

		QSize size = document.size().toSize();

		QImage image(size, QImage::Format_ARGB32);
		image.fill(Qt::white);

		QPainter painter(&image);
		document.drawContents(&painter);

		return image;
	}
}


OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, btnBar_(new BtnBar)
	, qrCodeBar_(new QRCodeBar)
	, mouseWindow_(new MouseWindow)
	, ignoreKeyPress_(false)
	, updateHelper_(new DUpdateHandler(this))
	, progressBox_(new DProgressBox(this, "下载更新", "正在下载更新文件，请稍候..."))
{
    ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);
	QGraphicsScene* lpScene = new QGraphicsScene;
	ui.view->setScene(lpScene);

	connect(btnBar_, &BtnBar::sigAskAI, this, [this]()
	{
		slotFixed(true);
	});
	connect(btnBar_, &BtnBar::sigTranslate, this, &OneGrab::slotTranslate);
	connect(btnBar_, &BtnBar::sigDrawing, this, [this](int isDrawing)
	{
		ignoreKeyPress_ = isDrawing == DrawWordS;  // 只有绘制文字时屏蔽按键
		ui.view->setDrawingState(isDrawing);
	});
	connect(btnBar_, &BtnBar::sigClose, this, &OneGrab::finishGrab);
	connect(btnBar_, &BtnBar::sigFixed, this, [this]()
	{
		slotFixed(false);
	});
	connect(btnBar_, &BtnBar::sigSave, this, &OneGrab::slotSave);
	connect(this, &OneGrab::sigFixedImageDownloadFinished, this, &OneGrab::slotFixedImageDownloadFinished, Qt::QueuedConnection);
	connect(btnBar_, &BtnBar::sigCopy, this, &OneGrab::slotCopy);
	connect(btnBar_, &BtnBar::sigMouseEnter, ui.view, &DGrabView::removeBorderBright);
	connect(btnBar_, &BtnBar::sigMouseMoveGlobal, this, [this](const QPoint& screenPos)
	{
		QPoint localPos = screenPos - pos();
		slotPosChanged(localPos);
	});
	connect(btnBar_, &BtnBar::sigSetIgnoreKey, ui.view, [this](bool ignore)
	{
		ignoreKeyPress_ = ignore;
	}, Qt::DirectConnection);
	connect(qrCodeBar_, &QRCodeBar::sigNeedFinishGrab, this, &OneGrab::finishGrab);
	connect(qrCodeBar_, &QRCodeBar::sigMouseEnter, ui.view, &DGrabView::removeBorderBright);
	connect(qrCodeBar_, &QRCodeBar::sigMouseMoveGlobal, this, [this](const QPoint& screenPos)
	{
		QPoint localPos = screenPos - pos();
		slotPosChanged(localPos);
	});
	connect(mouseWindow_, &MouseWindow::sigMousePress, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseMove, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseRelease, this, &OneGrab::slotMouseEventInWindow);
	connect(ui.view, &DGrabView::sigMousePressed, this, [this]()
	{
		// 按下时重新顶一次：截图窗是全屏窗，抢回前台时会把两个工具窗压到下面
		bringWindowToFront(mouseWindow_);
		bringWindowToFront(btnBar_);
	});
	connect(ui.view, &DGrabView::sigMouseReleased, this, [this](MouseState state)
	{
		// 自动二维码识别
		if (SETTING_HANDLER->getAutoQRCode())
			doAutoQRCode();

		bringWindowToFront(mouseWindow_);
		bringWindowToFront(btnBar_);
	});
	connect(ui.view, &DGrabView::sigPosChanged, this, &OneGrab::slotPosChanged);
	connect(ui.view, &DGrabView::sigSelectionChanged, this, &OneGrab::slotSelectionChanged);
	connect(ui.view, &DGrabView::sigResetDrawingState, this, [this]()
	{
		ui.view->setDrawingState(btnBar_->getDrawingType());
	});

	// 检查更新相关
	connect(updateHelper_, &DUpdateHandler::sigNewVersionAvailable, this, &OneGrab::slotNewVersionAvailable, Qt::QueuedConnection);

	connect(updateHelper_, &DUpdateHandler::sigAlreadyLatest,
		this, [this](const QString &version)
	{
		qDebug() << "Already the latest version:" << version;
		if (showLatestDialog_)
		{
			showLatestDialog_ = false;
			DMessageBox::information(this, tr("好消息"), tr("当前已经是最新版本"), ACCEPT_BTN);
		}
	}, Qt::QueuedConnection);

	connect(updateHelper_, &DUpdateHandler::sigCheckFailed,
		this, [](const QString &error)
	{
		qDebug() << "Update check failed:" << error;
	}, Qt::QueuedConnection);

	// 下载进度
	connect(updateHelper_, &DUpdateHandler::sigDownloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
	{
		if (progressBox_ && bytesTotal > 0)
			progressBox_->setProgress(bytesReceived * 100.0 / bytesTotal);
	});

	// 下载完成
	connect(updateHelper_, &DUpdateHandler::sigDownloadFinished, this, [](const QString& filePath)
	{
		QString dir = QApplication::applicationDirPath();  // 找到应用的目录

		QString exePath = QApplication::applicationDirPath() + "/updater/OneUpdater.exe";
		if (!QFile::exists(exePath))
		{
			DMessageBox::warning(nullptr, tr("有些事情好像不太行"), tr("文件缺失：\n%1").arg(exePath));
			return;
		}

		QStringList args = { "zip=" + filePath, "dir=" + dir };
		bool ret = QProcess::startDetached(exePath, args);
		QApplication::quit();
	});

	// 下载失败
	connect(updateHelper_, &DUpdateHandler::sigDownloadFailed, this, [](const QString& errMsg)
	{
		DMessageBox::warning(nullptr, tr("出问题了"), tr("下载失败：\n%1").arg(errMsg));
	});

	progressBox_->setAutoCloseOnComplete(false);

	updateHelper_->setInfos(GITEE_NAME, PROJECT_NAME, APP_VERSION_STR);
	updateHelper_->setSkipPrerelease(false);
	if (SETTING_HANDLER->getCheckUpdateOnStart())
		checkUpdate();

	// 程序启动时清空 temp 文件夹中的所有缓存图片
	QString tempDir = QCoreApplication::applicationDirPath() + "/temp";
	QDir dir(tempDir);
	if (dir.exists())
	{
		QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files);
		for (const QFileInfo& entry : entries)
		{
			QFile::remove(entry.absoluteFilePath());
		}
	}

}

OneGrab::~OneGrab()
{
	if (nullptr != btnBar_)
	{
		btnBar_->deleteLater();
		btnBar_ = nullptr;
	}
	if (nullptr != qrCodeBar_)
	{
		qrCodeBar_->deleteLater();
		qrCodeBar_ = nullptr;
	}
	if (nullptr != mouseWindow_)
	{
		mouseWindow_->deleteLater();
		mouseWindow_ = nullptr;
	}
	if (nullptr != updateHelper_)
	{
		updateHelper_->deleteLater();
		updateHelper_ = nullptr;
	}
	if (nullptr != progressBox_)
	{
		progressBox_->deleteLater();
		progressBox_ = nullptr;
	}
}

void OneGrab::doGrab()
{
	if (!isHidden())
		return;

	// x y 可以是负数
	QRect screenRect(0, 0, 0, 0);
	DSharedPointer<QPixmap> fp = getFullPixmap(screenRect);
	cv::Mat fullMat = ImageHandler::QImageToCvMat(fp->toImage());
	doFSY(fullMat);
	QImage fullImage = ImageHandler::cvMatToQImage(fullMat);
	fullPixmap_.reset(new QPixmap(QPixmap::fromImage(fullImage)));

	// 将所有可见窗口矩形画到截图上，并传入 DGrabView 用于吸附选择
	if (SETTING_HANDLER->getAutoGrabWindow())
	{
		auto rectInfos = DSys::GetAllVisibleWindowRects();
		DList<QRect> qRects;
		QPoint offset = -screenRect.topLeft();
		//QPainter painter(&fullPixmap_);
		//painter.setPen(QPen(Qt::red, 2));

		// 过滤掉等于显示器大小的矩形（桌面窗口等）
		auto screens = QGuiApplication::screens();
		for (const WindowRectInfo& rectInfo : rectInfos)
		{
			const RECT& wr = rectInfo.rect;
			//bool isScreen = false;
			//for (auto* screen : screens)
			//{
			//	QRect sr = screen->geometry();
			//	int srr = sr.left() + sr.width();
			//	int srb = sr.top() + sr.height();
			//	if (wr.left == sr.left() && wr.top == sr.top()
			//		&& wr.right == srr && wr.bottom == srb)
			//	{
			//		isScreen = true;
			//		break;
			//	}
			//}
				//qDebug() << wr.left << "    " << wr.top
				//<< "    " << wr.right << "    " << wr.bottom;
			//if (!isScreen)
			{
				QRect qr(wr.left + offset.x(), wr.top + offset.y(),
					wr.right - wr.left, wr.bottom - wr.top);
				//painter.drawRect(qr);
				qRects.append(qr);
			}
		}

		auto islandRects = getLabelIslandRects(true);
		for (QRect& r : islandRects)
		{
			r.translate(offset);
			qRects.pushFront(r);
		}

		ui.view->setWindowRects(qRects);
	}
	else
		ui.view->setWindowRects(DList<QRect>());

	ui.view->setImg(*fullPixmap_);
	setGeometry(screenRect);
	show();

	QPoint pixPoint = QCursor::pos() - pos();
	mouseWindow_->moveAndRefresh(pixPoint, geometry());
	mouseWindow_->show();

	ui.view->setFocus();

	slotRefreshPixelInfo(pixPoint);
}

void OneGrab::checkUpdate(bool showDialogOnLatest)
{
	showLatestDialog_ = showDialogOnLatest;
	updateHelper_->doCheck();
}

QColor OneGrab::getPixelColor(const QPoint& pos)
{
	return fullPixmap_->toImage().pixelColor(pos);
}

void OneGrab::slotKeyPressed(const KeyInfo& info)
{
	if (isHidden())
	{
		switch (info.key)
		{
		case 112ul:  // F1
			if (info.ctrlPressed)
				slotFixedOldOne();
			else if (info.shiftPressed)
				slotFixedCopyOne();
			else
				doGrab();
			break;
		}
		return;
	}

	if (ignoreKeyPress_)
	{
		qDebug() << __FUNCTION__ << "ignoreKeyPress. key:" << info.key;
		return;
	}

	switch (info.key)
	{
	case 27ul:  // ESC
		// 有顶层dialog时，按esc只是隐藏顶层dialog
		// 不然整个程序会退出，因为dialog推出后已经做一遍finishGrab了
		if (!isHidden() && (QApplication::activeModalWidget() == nullptr))
			finishGrab();
		break;
	case 37ul:  // left
		QCursor::setPos(QCursor::pos() + QPoint(-1, 0));
		break;
	case 38ul:  // up
		QCursor::setPos(QCursor::pos() + QPoint(0, -1));
		break;
	case 39ul:  // right
		QCursor::setPos(QCursor::pos() + QPoint(1, 0));
		break;
	case 40ul:  // down
		QCursor::setPos(QCursor::pos() + QPoint(0, 1));
		break;
	case 46ul:  // delete
		ui.view->deleteHoverItem();
		break;
	case 48ul:  // 0
		if (DrawRectS == btnBar_->getDrawingType())
			btnBar_->setLineWidth(Line0);
		break;
	case 49ul:  // 1
		if (btnBar_->getDrawingType())
			btnBar_->setLineWidth(Line1);
		break;
	case 50ul:  // 2
		if (btnBar_->getDrawingType())
			btnBar_->setLineWidth(Line2);
		break;
	case 51ul:  // 3
		if (btnBar_->getDrawingType())
			btnBar_->setLineWidth(Line3);
		break;
	case 52ul:  // 4
		if (btnBar_->getDrawingType())
			btnBar_->setLineWidth(Line4);
		break;
	case 'C':
		if (info.ctrlPressed)
		{
			QApplication::clipboard()->
				setText(mouseWindow_->getCurrentColorStr());
			finishGrab();
		}
		else
			slotCopy();  // 这个函数里已调 finishGrab
		break;
	case 'A':  // 画箭头
		btnBar_->setDrawMode(DrawArrowS);
		break;
	case 'L':  // 画直线
		btnBar_->setDrawMode(DrawLineS);
		break;
	case 'P':  // 随便画
		btnBar_->setDrawMode(DrawPenS);
		break;
	case 'Q':
		if (!info.ctrlPressed)
			finishGrab();
		break;
	case 'R':  // 画矩形
		btnBar_->setDrawMode(DrawRectS);
		break;
	case 'S':
		if (!info.ctrlPressed)
			slotSave();  // 这个函数里已调 finishGrab
		break;
	case 'T':
		if (!info.ctrlPressed)
			slotFixed(false);  // 这个函数里已调 finishGrab
		break;
	case 'W':  // 画文字
		btnBar_->setDrawMode(DrawWordS);
		break;
	case 'Z':
		if (info.ctrlPressed)
		{
			ui.view->zItem(info.shiftPressed);
		}
		break;
	case 160ul:  // SHIFT
		mouseWindow_->switchColorStrMode();
		break;
	}

	Hook::getInstance()->blockOnce();
}

void OneGrab::slotTranslate()
{
	QString apiKey = SETTING_HANDLER->getAIApiKey();
	if (apiKey.isEmpty())
	{
		DToast::toast(tr("请先设置Api Key"), DToast::FromMouse, DToast::Slow);
		SettingDialog::getInstance()->show();
		SettingDialog::getInstance()->gotoSetApiKey();
		finishGrab();
		return;
	}

	QRect croppedRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(croppedRect);
	if (croppedPixmap.isNull())
		return;

	// AIOCR 打开：直接把图片交给多模态模型；关闭：先跑本地 Windows OCR，再让模型只做翻译和排版
	if (SETTING_HANDLER->getAIOCR())
		translateByAI(croppedPixmap, croppedRect);
	else
		translateByWindowsOcr(croppedPixmap, croppedRect);
}

void OneGrab::translateByAI(const QPixmap& croppedPixmap, const QRect& croppedRect)
{
	const QString apiKey = SETTING_HANDLER->getAIApiKey();
	const int imageWidth = croppedPixmap.width();
	const int imageHeight = croppedPixmap.height();

	// 直接把截图交给多模态模型：让它自己找文字、定矩形、取背景色/文字色、翻译并做排版
	const QString imageDataUrl = AIHandler::imageFileToDataUrl(croppedPixmap);
	if (imageDataUrl.isEmpty())
	{
		qWarning() << "[translate] convert image to data url failed";
		return;
	}

	AIHandler::Params aiParams;
	aiParams.model = "deepseek-flash";
	aiParams.stream = false;      // 要一次性拿到完整 JSON，流式没法直接解析
	aiParams.baseUrl = "https://api.deepseek.com/v1";
	aiParams.webSearch = false;
	aiParams.temperature = 0.3;   // 结构化输出，温度低一点更稳

	AIHandler::ChatMessage systemMsg;
	systemMsg.role = AIHandler::System;
	systemMsg.content = SYSTEM_STR;
	aiParams.msgs.append(systemMsg);

	AIHandler::ChatMessage userMsg;
	userMsg.role = AIHandler::User;
	userMsg.content = QString("图片尺寸：%1x%2。请按系统要求只返回 JSON 数组。")
		.arg(imageWidth).arg(imageHeight);
	userMsg.images.append(imageDataUrl);
	aiParams.msgs.append(userMsg);

	// 每次翻译用独立的 handler，回复/报错后自己回收
	auto* translator = new AIHandler(apiKey);
	translator->setParent(this);

	connect(translator, &AIHandler::replyReady, this
		, [translator, croppedRect, imageWidth, imageHeight, this](const QString& reply, const QString&)
	{
		translator->deleteLater();

		// 等待期间用户可能已经退出截图，结果直接丢掉
		if (!isVisible())
		{
			qWarning() << "[translate] grab already finished, drop result";
			return;
		}

		// 模型有时候会拿 ```json 包一层，这里截出第一个 [ 到最后一个 ]
		const int begin = reply.indexOf('[');
		const int end = reply.lastIndexOf(']');
		if (begin < 0 || end <= begin)
		{
			qWarning() << "[translate] parse failed, raw reply:" << reply;
			return;
		}

		QJsonParseError err;
		const QJsonDocument doc = QJsonDocument::fromJson(reply.mid(begin, end - begin + 1).toUtf8(), &err);
		if (err.error != QJsonParseError::NoError || !doc.isArray())
		{
			qWarning() << "[translate] invalid json:" << err.errorString() << "raw:" << reply;
			return;
		}

		auto parseRect = [imageWidth, imageHeight](const QJsonValue& value) -> QRectF
		{
			const QJsonArray array = value.toArray();
			if (array.size() != 4)
				return QRectF();

			const QRectF rect(array.at(0).toDouble(), array.at(1).toDouble()
				, array.at(2).toDouble(), array.at(3).toDouble());
			return rect.intersected(QRectF(0, 0, imageWidth, imageHeight));
		};

		auto parseColor = [](const QJsonValue& value) -> QColor
		{
			const QJsonArray array = value.toArray();
			if (array.size() < 3)
				return QColor();

			return QColor(qBound(0, array.at(0).toInt(), 255)
				, qBound(0, array.at(1).toInt(), 255)
				, qBound(0, array.at(2).toInt(), 255));
		};

		const QJsonArray blocks = doc.array();
		for (const QJsonValue& value : blocks)
		{
			const QJsonObject one = value.toObject();
			const QString translation = one.value("translation").toString().trimmed();
			if (translation.isEmpty())
				continue;

			const QString sourceText = one.value("text").toString().trimmed();
			if (!sourceText.isEmpty())
			{
				// 模型偶尔会把中文/数字也带回来，再挡一道
				if (!needsTranslation(sourceText))
					continue;

				if (translation.compare(sourceText, Qt::CaseInsensitive) == 0)
					continue;
			}

			QRectF rect = parseRect(one.value("rect"));
			QRectF drawRect = parseRect(one.value("draw_rect"));

			if (drawRect.width() <= 0 || drawRect.height() <= 0)
				drawRect = rect;
			if (rect.width() <= 0 || rect.height() <= 0)
				rect = drawRect;
			if (rect.width() <= 0 || rect.height() <= 0)
				continue;

			// 绘制区域至少要把原文盖住
			drawRect = drawRect.united(rect);

			QColor bgColor = parseColor(one.value("bg_color"));
			QColor textColor = parseColor(one.value("text_color"));
			if (!bgColor.isValid())
				bgColor = Qt::white;
			if (!textColor.isValid())
				textColor = Qt::black;

			// drawRect 是图片内坐标，换算成场景坐标
			const QPoint viewTopLeft = croppedRect.topLeft() + drawRect.topLeft().toPoint();
			const QRect sceneRect(ui.view->mapToScene(viewTopLeft).toPoint(), drawRect.size().toSize());

			const QString fontFamily = one.value("font").toString().trimmed();

			qDebug() << "[translate]" << rect << "draw:" << drawRect << fontFamily
				<< sourceText << "->" << translation;
			ui.view->coverText(sceneRect, translation, bgColor, textColor, fontFamily);
		}
	});

	connect(translator, &AIHandler::errorOccured, this, [translator](const QString& error, const QString&)
	{
		translator->deleteLater();
		qWarning() << "[translate] request failed:" << error;
	});

	translator->callDeepSeek(aiParams);
}

void OneGrab::translateByWindowsOcr(const QPixmap& croppedPixmap, const QRect& croppedRect)
{
	const QString apiKey = SETTING_HANDLER->getAIApiKey();
	const int imageWidth = croppedPixmap.width();
	const int imageHeight = croppedPixmap.height();

	// 颜色估算用原图
	const cv::Mat baseMat = ImageHandler::QImageToCvMat(croppedPixmap.toImage());

	auto* ocr = new WindowsOcr(this);
	connect(ocr, &WindowsOcr::finished, this
		, [ocr, apiKey, croppedRect, imageWidth, imageHeight, baseMat, this](const WindowsOcr::Result& r)
	{
		ocr->deleteLater();

		// 等待期间用户可能已经退出截图，没必要再发翻译请求
		if (!isVisible())
			return;

		QJsonArray linesJson;
		DList<QRectF> rects;
		DList<QRectF> globalRects;
		QStringList texts;
		for (const WindowsOcr::Line& line : r.lines)
		{
			const QString text = line.text.trimmed();
			if (text.isEmpty() || !needsTranslation(text))
				continue;

			QJsonObject item;
			item["id"] = linesJson.size();
			item["text"] = text;

			QJsonArray rect;
			rect.append(qRound(line.rect.x()));
			rect.append(qRound(line.rect.y()));
			rect.append(qRound(line.rect.width()));
			rect.append(qRound(line.rect.height()));
			item["rect"] = rect;

			// 单行文字，中文字宽大约等于行高的 0.9
			const double charWidth = qMax(1.0, line.rect.height() * 0.9);
			item["max_chars"] = qMax(1, qRound(line.rect.width() / charWidth));

			linesJson.append(item);
			rects.append(line.rect);
			globalRects.append(QRectF(line.rect.topLeft() +  croppedRect.topLeft()
				, line.rect.bottomRight() + croppedRect.topLeft()));
			texts.append(text);

			qDebug() << "windwos ORC" << line.rect << line.text;
		}

		// OCR识别完了先把文字rect显示上去
		ui.view->addTextRects(globalRects, SETTING_HANDLER->getMainColor());

		if (linesJson.isEmpty())
		{
			qDebug() << "[translate-ocr] nothing to translate";
			return;
		}

		AIHandler::Params aiParams;
		aiParams.model = "deepseek-flash";
		aiParams.stream = false;      // 要一次性拿到完整 JSON
		aiParams.baseUrl = "https://api.deepseek.com/v1";
		aiParams.webSearch = false;
		aiParams.temperature = 1.3;   // 官方推荐翻译时使用1.3

		AIHandler::ChatMessage systemMsg;
		systemMsg.role = AIHandler::System;
		systemMsg.content = SYSTEM_STR_WINOCR;
		aiParams.msgs.append(systemMsg);

		QJsonObject imageInfo;
		imageInfo["width"] = imageWidth;
		imageInfo["height"] = imageHeight;

		QJsonObject payload;
		payload["image"] = imageInfo;
		payload["lines"] = linesJson;

		AIHandler::ChatMessage userMsg;
		userMsg.role = AIHandler::User;
		userMsg.content = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
		aiParams.msgs.append(userMsg);

		auto* translator = new AIHandler(apiKey);
		translator->setParent(this);

		connect(translator, &AIHandler::replyReady, this
			, [translator, rects, texts, imageWidth, imageHeight, baseMat, croppedRect, this](const QString& reply, const QString&)
		{
			translator->deleteLater();
			ui.view->clearTextRects();

			// 等待期间用户可能已经退出截图，结果直接丢掉
			if (!isVisible())
			{
				qWarning() << "[translate-ocr] grab already finished, drop result";
				return;
			}

			const int begin = reply.indexOf('[');
			const int end = reply.lastIndexOf(']');
			if (begin < 0 || end <= begin)
			{
				qWarning() << "[translate-ocr] parse failed, raw reply:" << reply;
				return;
			}

			QJsonParseError err;
			const QJsonDocument doc = QJsonDocument::fromJson(reply.mid(begin, end - begin + 1).toUtf8(), &err);
			if (err.error != QJsonParseError::NoError || !doc.isArray())
			{
				qWarning() << "[translate-ocr] invalid json:" << err.errorString() << "raw:" << reply;
				return;
			}

			const QJsonArray translations = doc.array();
			for (const QJsonValue& value : translations)
			{
				const QJsonObject one = value.toObject();
				const QString translation = one.value("translation").toString().trimmed();
				if (translation.isEmpty())
					continue;

				// 优先用 ids（同一句被拆成多个 rect 的情况），兼容只返回 id 的老格式
				QList<int> ids;
				if (one.value("ids").isArray())
				{
					for (const QJsonValue& idValue : one.value("ids").toArray())
					{
						if (idValue.isDouble())
							ids.append(idValue.toInt());
					}
				}
				else if (one.value("id").isDouble())
				{
					ids.append(one.value("id").toInt());
				}

				QRectF unionRect;
				bool hasRect = false;
				QStringList sources;
				for (int id : ids)
				{
					if (id < 0 || id >= rects.size())
						continue;

					unionRect = hasRect ? unionRect.united(rects.at(id)) : rects.at(id);
					hasRect = true;
					sources.append(texts.at(id));
				}

				if (!hasRect)
					continue;

				const QString sourceText = sources.join(' ');
				if (translation.compare(sourceText, Qt::CaseInsensitive) == 0)
					continue;

				// 模型给的绘制区域先校验，越界/明显不合理就退回组内并集
				QRectF drawRect = unionRect;
				const QJsonArray rectArray = one.value("rect").toArray();
				if (rectArray.size() == 4)
				{
					QRectF candidate(rectArray.at(0).toDouble(), rectArray.at(1).toDouble()
						, rectArray.at(2).toDouble(), rectArray.at(3).toDouble());
					candidate = candidate.intersected(QRectF(0, 0, imageWidth, imageHeight));

					const QRectF overlap = candidate.intersected(unionRect);
					const qreal unionArea = unionRect.width() * unionRect.height();
					const qreal candidateArea = candidate.width() * candidate.height();
					if (candidate.width() > 0 && candidate.height() > 0
						&& overlap.width() * overlap.height() >= qMin(unionArea, candidateArea) * 0.5
						&& candidateArea <= qMax(unionArea, qreal(1.0)) * 4)
					{
						drawRect = candidate;
					}
				}

				QColor bgColor(255, 255, 255);
				QColor textColor(0, 0, 0);
				if (!baseMat.empty())
				{
					const QPair<cv::Scalar, cv::Scalar> colors
						= ImageHandler::guessBgAndTextColor(baseMat, drawRect);
					bgColor = QColor(int(colors.first[2]), int(colors.first[1]), int(colors.first[0]));
					textColor = QColor(int(colors.second[2]), int(colors.second[1]), int(colors.second[0]));
				}

				const QPoint viewTopLeft = croppedRect.topLeft() + drawRect.topLeft().toPoint();
				const QRect sceneRect(ui.view->mapToScene(viewTopLeft).toPoint(), drawRect.size().toSize());

				qDebug() << "[translate-ocr]" << ids << "draw:" << drawRect
					<< "src:" << unionRect << sourceText << "->" << translation;
				// Windows OCR 不识别字体，统一用 Microsoft YaHei
				ui.view->coverText(sceneRect, translation, bgColor, textColor, QString("Microsoft YaHei"));
			}
		});

		connect(translator, &AIHandler::errorOccured, this, [translator](const QString& error, const QString&)
		{
			translator->deleteLater();
			qWarning() << "[translate-ocr] request failed:" << error;
		});

		translator->callDeepSeek(aiParams);
	});

	connect(ocr, &WindowsOcr::failed, this, [ocr](const QString& error)
	{
		ocr->deleteLater();
		qWarning() << "[translate-ocr] windows ocr failed:" << error;
	});

	ocr->recognize(croppedPixmap);
}

void OneGrab::slotFixed(bool showAI)
{
	QRect croppedRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(croppedRect);
	fixedLabelIsland(croppedPixmap, croppedRect, showAI);
	finishGrab();
}

void OneGrab::slotFixedOldOne()
{
	// 找到最新的未显示的island
	LabelIsland* lastUnshowIsland = nullptr;
	for (auto island : islandBuffer_)
	{
		if (island->isHidden())
			lastUnshowIsland = island;
	}
	if (nullptr != lastUnshowIsland)
	{
		lastUnshowIsland->show();
	}
}

void OneGrab::slotFixedCopyOne()
{
	QClipboard* clipboard = QApplication::clipboard();

	// 从剪贴板提取字符串，和类型。-1 有问题  0 是本地文件  1 是HTML文本  2 是url链接  3 是普通文本
	auto extractUrl = [clipboard]() -> QPair<QString, int>
	{
		const QMimeData* mimeData = clipboard->mimeData();
		if (mimeData && mimeData->hasUrls())
		{
			if (mimeData->hasUrls())
			{
				QList<QUrl> urls = mimeData->urls();
				if (!urls.isEmpty())
				{
					QString filePath = urls.first().toLocalFile();
					if (!filePath.isEmpty())
						return qMakePair(filePath, 0);
					return qMakePair(urls.first().toString(), 0);
				}
			}
		}

		if (mimeData && mimeData->hasHtml())
			return qMakePair(mimeData->html(), 1);  // 是HTML文本

		if (mimeData && mimeData->hasText())
		{
			QString str = mimeData->text();
			if (str.startsWith(tr("http://")) || str.startsWith(tr("https://")))
				return qMakePair(str, 2);  // 是url
			else
				return qMakePair(str, 3);  // 就是普通文本
		}

		return qMakePair(QString(), -1);
	};

	// 尝试直接转成图像
	QPixmap pixmap = clipboard->pixmap();
	if (!pixmap.isNull())
	{
		slotFixedImageDownloadFinished(pixmap);
		return;
	}
	
	QPair<QString, int> pair = extractUrl();
	if (-1 == pair.second || pair.first.isEmpty())
		return;

	switch (pair.second)
	{
	case 0:  // 本地文件
	{
		pixmap = QPixmap(pair.first);
		if (!pixmap.isNull())
		{
			slotFixedImageDownloadFinished(pixmap);
			return;
		}
		break;
	}
	case 1:  // HTML文本
	{
		pixmap = QPixmap::fromImage(htmlToImage(pair.first));
		if (!pixmap.isNull())
		{
			slotFixedImageDownloadFinished(pixmap);
			return;
		}
		break;
	}
	case 2:  // url链接
	{
		DownloadImage(pair.first);
		break;
	}
	default:  // 普通文本
	{
		pixmap = QPixmap::fromImage(textToImage(pair.first));
		if (!pixmap.isNull())
		{
			slotFixedImageDownloadFinished(pixmap);
			return;
		}
		break;
	}
	}
}

void OneGrab::DownloadImage(const QString& url)
{
	QNetworkAccessManager* manager = new QNetworkAccessManager(this);
	QNetworkReply* reply = manager->get(QNetworkRequest(QUrl(url)));

	connect(reply, &QNetworkReply::finished, this, [this, reply, manager, url]()
	{
		reply->deleteLater();
		manager->deleteLater();

		if (reply->error() == QNetworkReply::NoError)
		{
			QPixmap pixmap;
			pixmap.loadFromData(reply->readAll());
			if (!pixmap.isNull())
			{
				emit sigFixedImageDownloadFinished(pixmap);
				return;
			}
		}

		// 失败了，还是按照文本来解析
		QPixmap pixmap = QPixmap::fromImage(textToImage(url));
		if (!pixmap.isNull())
		{
			slotFixedImageDownloadFinished(pixmap);
			return;
		}
	});
}

void OneGrab::slotFixedImageDownloadFinished(QPixmap pixmap)
{
	if (pixmap.isNull())
		return;

	QPoint pos = QCursor::pos() - QPoint(pixmap.width() / 2, pixmap.height() / 2);
	LabelIsland* island = new LabelIsland(pixmap, pos);
	connect(SettingDialog::getInstance(), &SettingDialog::sigRefreshSetting, island, &LabelIsland::onRefreshSetting);
	connect(island, &LabelIsland::sigHide, this, [this, island]()
	{
		islandBuffer_.removeFirst(island);
		islandBuffer_.enqueue(island);
	});
	island->show();
	islandBuffer_.enqueue(island);
	while (islandBuffer_.size() > SETTING_HANDLER->getIslandNum())
		islandBuffer_.dequeue()->deleteLater();

	// AI聊天Widget
	AITalkWidget* talkWidget = new AITalkWidget(island);
	talkWidget->move(island->x() + island->width() + 10
		, island->y() + island->height() - talkWidget->height());
}

void OneGrab::slotSave()
{
	DBoolSetter setter(ignoreKeyPress_, true);
	QRect uselessRect;
	// 保存截图到文件
	QString timestamp = generateImageId();
	QString filename = QString("OneGrab_%1.%2").arg(timestamp).arg(SETTING_HANDLER->getImgTypeStr());

	SettingStruct settingStruct = SETTING_HANDLER->getSettingStruct();
	QString fileurl;
	if (settingStruct.UseDefaultSavePath && !settingStruct.DefaultSavePath.isEmpty())
	{
		fileurl = settingStruct.DefaultSavePath + '/' + filename;
	}
	else
	{
		QString imgType = SETTING_HANDLER->getImgTypeStr();
		DList<QString> imgTypeList = SETTING_HANDLER->getImgTypeStrs().values();
		QStringList filters;
		for (const QString& ext : imgTypeList)
		{
			filters << tr("%1 文件 (*.%1)").arg(ext);
		}
		QString filter = filters.join(";;");
		QString selectedFilter = tr("%1 文件 (*.%1)").arg(imgType);

		fileurl = QFileDialog::getSaveFileName(this, tr("保存文件"),
			settingStruct.LastSavePath + '/' + filename, filter, &selectedFilter);
		if (!fileurl.isEmpty())
		{
			fileurl = fileurl.replace('\\', '/');
			int i = fileurl.lastIndexOf('/');
			settingStruct.LastSavePath = fileurl.mid(0, i);
			SETTING_HANDLER->setSettingStruct(settingStruct);
		}
	}

	ImageInfo info;
	info.pixmap = ui.view->getSelectionPixmap(uselessRect);
	info.abPath = fileurl;
	IMAGE_THREAD->addImage(info);
	finishGrab();
}

void OneGrab::slotCopy()
{
	QRect uselessRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(uselessRect);
	if (SETTING_HANDLER->getCopy2File())
	{
		QString timestamp = generateImageId();
		QString abPath = save2Buffer(timestamp, croppedPixmap);

		QMimeData* mimeData = new QMimeData;
		QList<QUrl> urls;
		urls << QUrl::fromLocalFile(abPath);
		mimeData->setUrls(urls);
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setMimeData(mimeData);
	}
	else
	{
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setPixmap(croppedPixmap);
	}
	
	finishGrab();
}

void OneGrab::slotSelectionChanged(const QRectF& rectf)
{
	QRect rect = rectf.toRect();
	rect.moveLeft(rect.left() + x());
	rect.moveTop(rect.top() + y());
	int toXBtnBar = rect.right() - btnBar_->width();
	int toYBtnBar = rect.bottom() + MARGIN;

	// 边界检查
	if (toXBtnBar < x() + MARGIN)
		toXBtnBar = x() + MARGIN;
	else if (toXBtnBar > x() + width() - btnBar_->width() - MARGIN)
		toXBtnBar = x() + width() - btnBar_->width() - MARGIN;

	if (toYBtnBar < y() + MARGIN)
		toYBtnBar = y() + MARGIN;
	else if (toYBtnBar > y() + height() - btnBar_->height() - MARGIN)
		toYBtnBar = y() + height() - btnBar_->height() - MARGIN;

	btnBar_->move(toXBtnBar, toYBtnBar);
	btnBar_->setSizeLabelText(rect.size());

	updateQRCodeBarPos(rect);
}

void OneGrab::updateQRCodeBarPos(const QRect& selectionRect)
{
	// 注意:必须保证 qrCodeBar_ 的尺寸已经是最新的（内容变更后先 adjustSize），否则会按旧尺寸算出错误位置
	int toXQRCodeBar = selectionRect.right() - qrCodeBar_->width();
	int toYQRCodeBar = selectionRect.top() - MARGIN - qrCodeBar_->height();

	if (toXQRCodeBar < x() + MARGIN)
		toXQRCodeBar = x() + MARGIN;
	else if (toXQRCodeBar > x() + width() - qrCodeBar_->width() - MARGIN)
		toXQRCodeBar = x() + width() - qrCodeBar_->width() - MARGIN;

	if (toYQRCodeBar < y() + MARGIN)
		toYQRCodeBar = y() + MARGIN;
	else if (toYQRCodeBar > y() + height() - qrCodeBar_->height() - MARGIN)
		toYQRCodeBar = y() + height() - qrCodeBar_->height() - MARGIN;

	qrCodeBar_->move(toXQRCodeBar, toYQRCodeBar);
}

void OneGrab::slotRefreshPixelInfo(const QPoint& mousePos)
{
	int sc = SETTING_HANDLER->getMouseScaleNum();
	double scaleNum = pow(1.2, sc);
	QColor color = getPixelColor(mousePos);
	const QSize windowSize = mouseWindow_->getWindowSize();
	const QSize windowSizeInFull = windowSize / scaleNum;

	QRect baseTargetRect = QRect(mousePos - QPoint(windowSizeInFull.width() / 2, windowSizeInFull.height() / 2)
		, windowSizeInFull + QSize(1, 1));
	// 计算原图中可以截取的有效区域（和src交集）
	QRect baseSrcRect = baseTargetRect & QRect(0, 0, fullPixmap_->width(), fullPixmap_->height());
	// 裁剪
	QPixmap basePixmap(baseTargetRect.size());
	basePixmap.fill(Qt::black);
	if (!baseSrcRect.isEmpty())
	{
		// 从原图中截取有效部分
		QPixmap cropped = fullPixmap_->copy(baseSrcRect);
		// 计算将cropped粘贴到result中的位置（相对位置）
		QPoint destTopLeft = baseSrcRect.topLeft() - baseTargetRect.topLeft();
		QPainter painter(&basePixmap);
		painter.drawPixmap(destTopLeft, cropped);
		painter.end();
	}

	// 画鼠标所在像素的矩形框
	QColor mainColor = SETTING_HANDLER->getMainColor();
	QPainter painter(&basePixmap);
	painter.setPen(QPen(mainColor, 1));
	painter.drawRect(QRect(mousePos - baseTargetRect.topLeft() - QPoint(1, 1), QSize(2, 2)));

	// 放大
	QPixmap scaledBasePixmap = basePixmap.scaled(basePixmap.size() * scaleNum
		, Qt::KeepAspectRatio, Qt::FastTransformation);

	// 获取实际要裁剪的图像，在scaledBasePixmap中的rect
	QPoint pointInScaledBase = (mousePos - baseTargetRect.topLeft()) * scaleNum
		+ QPoint(0.5 * scaleNum, 0.5 * scaleNum)
		- QPoint(windowSize.width() / 2, windowSize.height() / 2);
	QRect rectInScaledBase = QRect(pointInScaledBase, windowSize);

	QPixmap windowPixmap = scaledBasePixmap.copy(rectInScaledBase);
	mouseWindow_->refreshInfo(mousePos, color, windowPixmap);
}

void OneGrab::slotMouseEventInWindow(QMouseEvent* event)
{
	QMouseEvent newEvent(event->type(), event->localPos() + mouseWindow_->pos() - pos(), event->screenPos(),
		event->button(), event->buttons(), event->modifiers());
	
	switch (event->type())
	{
	case QMouseEvent::MouseButtonPress:
		ui.view->mousePressEvent(&newEvent);
		break;
	case QMouseEvent::MouseMove:
	{
		QPoint toPos = mouseWindow_->pos() + event->pos() - pos();
		//qDebug() << 11111 << pos() << mouseWindow_->pos() << event->pos() << toPos;
		slotPosChanged(toPos);
		ui.view->mouseMoveEvent(&newEvent);
		break;
	}
	case QMouseEvent::MouseButtonRelease:
		ui.view->mouseReleaseEvent(&newEvent);
		break;
	}
}

void OneGrab::slotPosChanged(const QPoint& pos)
{
	mouseWindow_->moveAndRefresh(pos, geometry());
	slotRefreshPixelInfo(pos);
}

void OneGrab::slotNewVersionAvailable(const QString& version, const QString& url, const QString& notes, const QString& download)
{
	qDebug() << "New version available:" << version;
	qDebug() << "Release URL:" << url;
	qDebug() << "Release notes:" << notes;
	qDebug() << "Download URL:" << download;

	if (download.isEmpty())
	{
		DMessageBox::warning(this, tr("警告"), tr("开发者好像忘记上传更新文件了。"));
		return;
	}

	int ret = DMessageBox::information(this, tr("好消息"), tr("检测到新版本\n%1\n是否立即更新？").arg(version), ALL_BTN);

	if (ret == QDialog::Accepted)
	{
		progressBox_->setProgress(0);
		progressBox_->show();

		QString saveDir = QCoreApplication::applicationDirPath() + "/update";
		updateHelper_->downloadFile(download, saveDir);
	}
}

DSharedPointer<QPixmap> OneGrab::getFullPixmap(QRect& screenRect)
{
	QList<QScreen*> screens = QGuiApplication::screens();

	// 获取容纳所有显示器图片的矩形
	for (QScreen *screen : screens)
	{
		QRect scRect = screen->geometry();
		if (scRect.x() < screenRect.x())
			screenRect.setX(scRect.x());
		if (scRect.y() < screenRect.y())
			screenRect.setY(scRect.y());
		screenRect = screenRect.united(scRect);
	}
	DSharedPointer<QPixmap> combinedPixmap(new QPixmap(screenRect.size()));
	combinedPixmap->fill(Qt::transparent);

	// 将每个屏幕的内容绘制到 combinedPixmap
	QPainter painter(combinedPixmap.getPtr());
	for (QScreen* screen : screens)
	{
		// 走 HDR 抓屏（Windows Graphics Capture + FP16），内部不成功会回退 screen->grabWindow(0)
		QPixmap pixmap = DScreenCapture::grabScreen(screen);
		painter.drawPixmap(screen->geometry().topLeft() - screenRect.topLeft(), pixmap);
	}
	painter.end();
	return combinedPixmap;
}

void OneGrab::doFSY(cv::Mat& mat)
{
	if (mat.empty())
	{
		qDebug() << __FUNCTION__ << "mat is empty";
		return;
	}

	qint64 i0 = QDateTime::currentMSecsSinceEpoch();
	const qint64 i00 = i0;

	// 图像轻微旋转
	if (SETTING_HANDLER->getFSYRotateEnable())
	{
		// 绕图像中心旋转一个很小角度，破坏隐水印的像素对齐。
		// 随机选 -1° 或 +1°；BORDER_REPLICATE 用边缘像素填充旋转后露出的空白
		cv::RNG rng(cv::getTickCount());
		const double angle = (rng.uniform(0, 2) == 0) ? -1.0 : 1.0;
		cv::Point2f center(mat.cols / 2.0f, mat.rows / 2.0f);
		cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);

		cv::Mat rotated;
		cv::warpAffine(mat, rotated, rot, mat.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
		mat = rotated;

		qint64 i1 = QDateTime::currentMSecsSinceEpoch();
		qDebug() << __FUNCTION__ << "rotate used time:" << (i1 - i0);
		i0 = i1;
	}

	// 3×3邻域平均滤波
	if (SETTING_HANDLER->getFSY33LBEnable())
	{
		for (int i = 0; i < SETTING_HANDLER->getFSY33LBRound(); ++i)
		{
			apply33MeanFilter(mat);
		}

		qint64 i1 = QDateTime::currentMSecsSinceEpoch();
		qDebug() << __FUNCTION__ << "3*3 filter used time:" << (i1 - i0);
		i0 = i1;
	}

	// 随机噪声
	if (SETTING_HANDLER->getFSYRandomEnable())
	{
		const int maxNum = SETTING_HANDLER->getFSYRandomMaxNum();
		if (maxNum <= 0)
			return;

		// 转到 16 位有符号，避免加噪声时溢出；alpha 通道保持原值不变
		cv::Mat temp;
		mat.convertTo(temp, CV_16SC4);

		cv::Mat noise(temp.size(), temp.type());
		cv::RNG rng(cv::getTickCount());
		rng.fill(noise, cv::RNG::UNIFORM,
			cv::Scalar(-maxNum, -maxNum, -maxNum, 0),
			cv::Scalar(maxNum + 1, maxNum + 1, maxNum + 1, 1));

		temp += noise;

		// 转回原类型，saturate_cast 自动把越界值夹紧到 0~255
		cv::Mat dst;
		temp.convertTo(dst, mat.type());
		mat = dst;

		qint64 i1 = QDateTime::currentMSecsSinceEpoch();
		qDebug() << __FUNCTION__ << "random salt used time:" << (i1 - i0);
		i0 = i1;
	}

	qint64 i10000 = QDateTime::currentMSecsSinceEpoch();
	qDebug() << __FUNCTION__ << "total used time:" << (i10000 - i00);
}

void OneGrab::doAutoQRCode()
{
	QRect uselessRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(uselessRect);
	cv::Mat croppedMat = ImageHandler::QImageToCvMat(croppedPixmap.toImage());

	const QStringList texts = BarcodeDecoder::decodeBarcodes(croppedMat);
	if (texts.isEmpty())
	{
		qDebug() << __FUNCTION__ << "QR code not found.";
		qrCodeBar_->clearQRCodeTexts();
		return;
	}
	
	qrCodeBar_->setQRCodeTexts(texts);
	// 内容变化后先把尺寸算出来：二维码条是首次显示时才由布局决定真实尺寸的，
	// 若沿用 .ui 里的旧尺寸定位，首次松手时会出现位置偏移（压住选区）
	qrCodeBar_->adjustSize();

	QRect selectionRect = ui.view->getSelectionRect();
	selectionRect.moveLeft(selectionRect.left() + x());
	selectionRect.moveTop(selectionRect.top() + y());
	updateQRCodeBarPos(selectionRect);

	bringWindowToFront(qrCodeBar_);
}

void OneGrab::finishGrab()
{
	qDebug() << __FUNCTION__;

	ignoreKeyPress_ = false;
	ui.view->onFinishGrab();
	mouseWindow_->hide();
	btnBar_->onFinishGrab();
	qrCodeBar_->onFinishGrab();
	hide();
}

QString OneGrab::save2Buffer(const QString& timestamp, const QPixmap& pixmap)
{
	QString strFile = QCoreApplication::applicationDirPath();
	strFile += QString("/temp/OneGrab_%1.png").arg(timestamp);

	QFileInfo fileInfo(strFile);
	QDir().mkpath(fileInfo.absolutePath());

	// 如果图片数量超出了COPY_TEMP_SIZE，删除最老的缓存图片
	QFileInfoList entries = fileInfo.absoluteDir().entryInfoList(QDir::NoDotAndDotDot | QDir::Files
		, QDir::Name | QDir::IgnoreCase);

	if (entries.size() > COPY_TEMP_SIZE)
	{
		for (int i = 0; i < entries.size() - COPY_TEMP_SIZE; ++i)
		{
			const QFileInfo& entry = entries[i];
			ImageInfo deleteInfo;
			deleteInfo.abPath = entry.absoluteFilePath();
			deleteInfo.isSave = false;
			IMAGE_THREAD->addImage(deleteInfo);
		}
	}

	ImageInfo info;
	info.pixmap = pixmap;
	info.abPath = strFile;
	IMAGE_THREAD->addImage(info);

	return strFile;
}

DList<QRect> OneGrab::getLabelIslandRects(bool onlyShowing)
{
	DList<QRect> ret;
	for (LabelIsland* li : islandBuffer_)
	{
		bool ok = onlyShowing ? !li->isHidden() : true;

		qDebug() << __FUNCTION__ << ok << li->geometry();

		if (ok)
			ret.pushBack(li->geometry());
	}
	return ret;
}

void OneGrab::fixedLabelIsland(const QPixmap& croppedPixmap, const QRect& croppedRect, bool showAI)
{
	LabelIsland* island = new LabelIsland(croppedPixmap, croppedRect.topLeft() + pos());
	connect(SettingDialog::getInstance(), &SettingDialog::sigRefreshSetting, island, &LabelIsland::onRefreshSetting);
	connect(island, &LabelIsland::sigHide, this, [this, island]()
	{
		islandBuffer_.removeFirst(island);
		islandBuffer_.enqueue(island);
	});
	island->show();
	islandBuffer_.enqueue(island);
	while (islandBuffer_.size() > SETTING_HANDLER->getIslandNum())
		islandBuffer_.dequeue()->deleteLater();

	// AI聊天Widget
	AITalkWidget* talkWidget = new AITalkWidget(island);
	talkWidget->move(island->x() + island->width() + 10
		, island->y() + island->height() - talkWidget->height());
	if (showAI)
		talkWidget->show();
}

void OneGrab::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}
