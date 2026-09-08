#include "OneGrab.h"
#include "BtnBar.h"
#include "HDCore/DBoolSetter.hpp"
#include "HDCore/DSys.hpp"
#include "DMessageBox.h"
#include "DProgressBox.h"
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
#include <QKeyEvent>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <vector>
#include "SettingDialog.h"
#include "SettingHandler.h"
#include "version.h"


const static int MARGIN = 5;
const static int COPY_TEMP_SIZE = 64;  // 复制图片到文件的最大图片保存数量


namespace
{
	inline QString generateImageId()
	{
		return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
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
}


OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, btnBar_(new BtnBar)
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

	connect(btnBar_, &BtnBar::sigDrawing, this, [this](int isDrawing)
	{
		ignoreKeyPress_ = isDrawing == DrawWordS;  // 只有绘制文字时屏蔽按键
		ui.view->setDrawingState(isDrawing);
	});
	connect(btnBar_, &BtnBar::sigClose, this, &OneGrab::finishGrab);
	connect(btnBar_, &BtnBar::sigFixed, this, &OneGrab::slotFixed);
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
	connect(mouseWindow_, &MouseWindow::sigMousePress, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseMove, this, &OneGrab::slotMouseEventInWindow);
	connect(mouseWindow_, &MouseWindow::sigMouseRelease, this, &OneGrab::slotMouseEventInWindow);
	connect(ui.view, &DGrabView::sigMousePressed, this, [this]()
	{
		mouseWindow_->hide();
		mouseWindow_->show();
		btnBar_->hide();
		btnBar_->show();
	});
	connect(ui.view, &DGrabView::sigMouseReleased, this, [this]()
	{
		mouseWindow_->hide();
		mouseWindow_->show();
		btnBar_->hide();
		btnBar_->show();
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
			slotFixed();  // 这个函数里已调 finishGrab
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

void OneGrab::slotFixed()
{
	QRect croppedRect;
	QPixmap croppedPixmap = ui.view->getSelectionPixmap(croppedRect);

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

	// 从剪贴板提取 URL 字符串（本地路径或远程链接）
	auto extractUrl = [clipboard]() -> QString
	{
		const QMimeData* mimeData = clipboard->mimeData();
		if (mimeData && mimeData->hasUrls())
		{
			QList<QUrl> urls = mimeData->urls();
			if (!urls.isEmpty())
			{
				QString filePath = urls.first().toLocalFile();
				if (!filePath.isEmpty())
					return filePath;
				return urls.first().toString();
			}
		}

		if (mimeData && mimeData->hasText())
			return mimeData->text();

		return QString();
	};

	// 尝试直接转成图像
	QPixmap pixmap = clipboard->pixmap();
	if (!pixmap.isNull())
	{
		slotFixedImageDownloadFinished(pixmap);
		return;
	}
	
	QString urlStr = extractUrl();
	if (urlStr.isEmpty())
		return;

	// 尝试从本地文件读取
	pixmap = QPixmap(urlStr);
	if (!pixmap.isNull())
	{
		slotFixedImageDownloadFinished(pixmap);
		return;
	}

	// 尝试从url链接下载图片
	if (urlStr.startsWith(tr("http://")) || urlStr.startsWith(tr("https://")))
		DownloadImage(urlStr);
}

void OneGrab::DownloadImage(const QString& url)
{
	QNetworkAccessManager* manager = new QNetworkAccessManager(this);
	QNetworkReply* reply = manager->get(QNetworkRequest(QUrl(url)));

	connect(reply, &QNetworkReply::finished, this, [this, reply, manager]()
	{
		reply->deleteLater();
		manager->deleteLater();

		if (reply->error() == QNetworkReply::NoError)
		{
			QPixmap pixmap;
			pixmap.loadFromData(reply->readAll());
			if (!pixmap.isNull())
				emit sigFixedImageDownloadFinished(pixmap);
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
	int toX = rect.right() - btnBar_->width();
	int toY = rect.bottom() + MARGIN;

	if (toX < x() + MARGIN)
		toX = x() + MARGIN;
	else if (toX > x() + width() - btnBar_->width() - MARGIN)
		toX = x() + width() - btnBar_->width() - MARGIN;

	if (toY < y() + MARGIN)
		toY = y() + MARGIN;
	else if (toY > y() + height() - btnBar_->height() - MARGIN)
		toY = y() + height() - btnBar_->height() - MARGIN;

	btnBar_->move(toX, toY);
	btnBar_->setSizeLabelText(rect.size());
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

	int ret = DMessageBox::information(this, tr("好消息"), tr("检测到新版本，是否立即更新？"), ALL_BTN);

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
		QPixmap pixmap = screen->grabWindow(0);
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

void OneGrab::finishGrab()
{
	qDebug() << __FUNCTION__;

	ignoreKeyPress_ = false;
	ui.view->onFinishGrab();
	mouseWindow_->hide();
	btnBar_->onFinishGrab();
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

void OneGrab::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}
