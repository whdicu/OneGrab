#include "OneGrab.h"
#include "BtnBar.h"
#include "HDCore/DBoolSetter.hpp"
#include "DMessageBox.h"
#include "DProgressBox.h"
#include "DUpdateHelper.h"
#include "ImageThread.h"
#include "LabelIsland.h"
#include "LabelIsland2.h"
#include "MouseWindow.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include "SettingDialog.h"
#include "SettingHandler.h"
#include "version.h"


const static int MARGIN = 5;
const static int COPY_TEMP_SIZE = 64;  // 复制图片到文件的最大图片保存数量


inline QString generateImageId()
{
	return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
}


OneGrab::OneGrab(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, btnBar_(new BtnBar)
	, mouseWindow_(new MouseWindow)
	, ignoreKeyPress_(false)
	, updateHelper_(new DUpdateHelper(this))
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
	connect(btnBar_, &BtnBar::sigCopy, this, &OneGrab::slotCopy);
	connect(btnBar_, &BtnBar::sigMouseEnter, ui.view, &DGrabView::removeBorderBright);
	connect(btnBar_, &BtnBar::sigSetIgnoreKey, ui.view, [this](bool ignore)
	{
		ignoreKeyPress_ = ignore;
	}, Qt::DirectConnection);
	connect(mouseWindow_, &MouseWindow::sigNeedRefresh, this, &OneGrab::slotRefreshPixelInfo);
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
	connect(updateHelper_, &DUpdateHelper::sigNewVersionAvailable, this, &OneGrab::slotNewVersionAvailable, Qt::QueuedConnection);

	connect(updateHelper_, &DUpdateHelper::sigAlreadyLatest,
		this, [](const QString &version)
	{
		qDebug() << "Already the latest version:" << version;
	}, Qt::QueuedConnection);

	connect(updateHelper_, &DUpdateHelper::sigCheckFailed,
		this, [](const QString &error)
	{
		qDebug() << "Update check failed:" << error;
	}, Qt::QueuedConnection);

	// 下载进度
	connect(updateHelper_, &DUpdateHelper::sigDownloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
	{
		if (progressBox_ && bytesTotal > 0)
			progressBox_->setProgress(bytesReceived * 100.0 / bytesTotal);
	});

	// 下载完成
	connect(updateHelper_, &DUpdateHelper::sigDownloadFinished, this, [](const QString& filePath)
	{
		
	});

	// 下载失败
	connect(updateHelper_, &DUpdateHelper::sigDownloadFailed, this, [](const QString& errMsg)
	{

	});

	progressBox_->setAutoCloseOnComplete(false);

	updateHelper_->setInfos(GITEE_NAME, PROJECT_NAME, APP_VERSION_STR);
	updateHelper_->setSkipPrerelease(false);
	if (SETTING_HANDLER->getCheckUpdateOnStart())
		checkUpdate();
}

void OneGrab::doGrab()
{
	if (!isHidden())
		return;

	// x y 可以是负数
	QRect screenRect(0, 0, 0, 0);
	fullPixmap_ = getFullPixmap(screenRect);
	ui.view->setImg(fullPixmap_);
	setGeometry(screenRect);
	show();

	mouseWindow_->moveAndRefresh(QCursor::pos() - pos(), geometry());
	mouseWindow_->show();

	ui.view->setFocus();
}

void OneGrab::checkUpdate()
{
	updateHelper_->doCheck();
}

QColor OneGrab::getPixelColor(const QPoint& pos)
{
	return fullPixmap_.toImage().pixelColor(pos);
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

void OneGrab::slotSave()
{
	DBoolSetter setter(ignoreKeyPress_, true);
	QRect uselessRect;
	// 保存截图到文件
	QString timestamp = generateImageId();
	QString filename = QString("OneGrab_%1.png").arg(timestamp);

	SettingStruct settingStruct = SETTING_HANDLER->getSettingStruct();
	QString fileurl;
	if (settingStruct.UseDefaultSavePath && !settingStruct.DefaultSavePath.isEmpty())
	{
		fileurl = settingStruct.DefaultSavePath + '/' + filename;
	}
	else
	{
		fileurl = QFileDialog::getSaveFileName(this, tr("保存文件"), settingStruct.LastSavePath + '/' + filename);
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
	QRect baseSrcRect = baseTargetRect & QRect(0, 0, fullPixmap_.width(), fullPixmap_.height());
	// 裁剪
	QPixmap basePixmap(baseTargetRect.size());
	basePixmap.fill(Qt::black);
	if (!baseSrcRect.isEmpty())
	{
		// 从原图中截取有效部分
		QPixmap cropped = fullPixmap_.copy(baseSrcRect);
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
	QMouseEvent* newEvent = new QMouseEvent(event->type(), event->localPos() + mouseWindow_->pos() - pos(), event->screenPos(),
		event->button(), event->buttons(), event->modifiers());
	
	switch (event->type())
	{
	case QMouseEvent::MouseButtonPress:
		ui.view->mousePressEvent(newEvent);
		break;
	case QMouseEvent::MouseMove:
		ui.view->mouseMoveEvent(newEvent);
		break;
	case QMouseEvent::MouseButtonRelease:
		ui.view->mouseReleaseEvent(newEvent);
		break;
	}
}

void OneGrab::slotPosChanged(const QPoint& pos)
{
	mouseWindow_->moveAndRefresh(pos, geometry());
}

void OneGrab::slotNewVersionAvailable(const QString& version, const QString& url, const QString& notes, const QString& download)
{
	qDebug() << "New version available:" << version;
	qDebug() << "Release URL:" << url;
	qDebug() << "Release notes:" << notes;
	qDebug() << "Download URL:" << download;

	int ret = DMessageBox::information(this, tr("好消息"), tr("检测到新版本，是否立即更新？"), ALL_BTN);

	if (ret == QDialog::Accepted)
	{
		progressBox_->setProgress(0);
		progressBox_->show();

		QString saveDir = QCoreApplication::applicationDirPath() + "/update";
		updateHelper_->downloadFile(download, saveDir);
	}
}

QPixmap OneGrab::getFullPixmap(QRect& screenRect)
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
	QPixmap combinedPixmap(screenRect.size());
	combinedPixmap.fill(Qt::transparent);

	// 将每个屏幕的内容绘制到 combinedPixmap
	QPainter painter(&combinedPixmap);
	for (QScreen* screen : screens)
	{
		QPixmap pixmap = screen->grabWindow(0);
		painter.drawPixmap(screen->geometry().topLeft() - screenRect.topLeft(), pixmap);
	}
	painter.end();
	return combinedPixmap;
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

void OneGrab::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

//void OneGrab::mousePressEvent(QMouseEvent* event)
//{
//	if (event->button() == Qt::LeftButton)
//	{
//		mouseWindow_->hide();
//		mouseWindow_->show();
//
//		selectionStart_ = event->pos();
//		selectionEnd_ = event->pos();
//		
//		QRect selectionRect = labelMask_->getSelectionRect();
//		mouseState_ = 0;
//
//		int isInBorder = 0;
//		if (dAbs(selectionRect.left() - selectionStart_.x()) < DRAG_SPACE)
//		{
//			isInBorder |= dragLeft;
//		}
//		else if (dAbs(selectionRect.right() - selectionStart_.x()) < DRAG_SPACE)
//		{
//			isInBorder |= dragRight;
//		}
//		if (dAbs(selectionRect.top() - selectionStart_.y()) < DRAG_SPACE)
//		{
//			isInBorder |= dragTop;
//		}
//		else if (dAbs(selectionRect.bottom() - selectionStart_.y()) < DRAG_SPACE)
//		{
//			isInBorder |= dragBottom;
//		}
//
//		if (isInBorder)
//		{
//			mouseState_ = isInBorder;
//		}
//		else
//		{
//			if (selectionRect.contains(selectionStart_))
//			{
//				mouseState_ = MoveState;
//			}
//			else
//			{
//				btnBar_->hide();
//				mouseState_ = SelectState;
//				labelMask_->setSelectionRect(QRect(selectionStart_, selectionEnd_));
//			}
//		}
//	}
//}

//void OneGrab::mouseMoveEvent(QMouseEvent* event)
//{
//	mouseWindow_->moveAndRefresh(event->globalPos(), geometry());
//
//	switch (mouseState_)
//	{
//	case FreeState:
//		break;
//	case SelectState:
//		labelMask_->setSelectionRect(QRect(selectionStart_, event->pos()));
//		break;
//	case MoveState:
//	{
//		int dX = event->pos().x() - selectionEnd_.x();
//		int dY = event->pos().y() - selectionEnd_.y();
//		labelMask_->moveSelectionRect(dX, dY);
//		break;
//	}
//	default:
//	{
//		int dX = event->pos().x() - selectionEnd_.x();
//		int dY = event->pos().y() - selectionEnd_.y();
//		if (mouseState_ & dragLeft)
//		{
//			if (labelMask_->moveSelectionRectLeft(dX))
//			{
//				mouseState_ = (mouseState_ | dragRight) & (~dragLeft);
//			}
//		}
//		else if (mouseState_ & dragRight)
//		{
//			if (labelMask_->moveSelectionRectRight(dX))
//			{
//				mouseState_ = (mouseState_ | dragLeft) & (~dragRight);
//			}
//		}
//		if (mouseState_ & dragTop)
//		{
//			if (labelMask_->moveSelectionRectTop(dY))
//			{
//				mouseState_ = (mouseState_ | dragBottom) & (~dragTop);
//			}
//		}
//		else if (mouseState_ & dragBottom)
//		{
//			if (labelMask_->moveSelectionRectBottom(dY))
//			{
//				mouseState_ = (mouseState_ | dragTop) & (~dragBottom);
//			}
//		}
//		break;
//	}
//	}
//	selectionEnd_ = event->pos();
//}

//void OneGrab::mouseReleaseEvent(QMouseEvent* event)
//{
//	if (event->button() == Qt::LeftButton)
//	{
//		mouseState_ = FreeState;
//		mouseWindow_->show();
//		btnBar_->show();
//	}
//}
