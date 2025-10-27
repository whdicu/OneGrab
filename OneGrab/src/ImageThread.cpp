#include "ImageThread.h"
#include <mutex>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

const static int SLEEP_TIME = 20;


static std::once_flag onceFlag;
static ImageThread* image_save_thread = nullptr;
ImageThread* ImageThread::getInstance()
{
	std::call_once(onceFlag, [] { image_save_thread = new ImageThread; });
	return image_save_thread;
}

ImageThread::ImageThread(QObject *parent)
	: QThread(parent)
	, running_(true)
	, mutex_(QMutex::Recursive)
{
}

ImageThread::~ImageThread()
{
	stop();
	wait();
}

bool ImageThread::doSave(const ImageInfo& info)
{
	if (info.pixmap.isNull())
	{
		qWarning() << __FUNCTION__ << "pixmap is null!";
		return false;
	}

	if (info.abPath.isEmpty())
	{
		qWarning() << __FUNCTION__ << "abPath is empty!";
		return false;

	}

	bool ret = info.pixmap.save(info.abPath);
	if (!ret)
		qWarning() << __FUNCTION__ << "save" << info.abPath << "faild!";
	return ret;
}

bool ImageThread::doDelete(const ImageInfo& info)
{
	if (info.abPath.isEmpty())
	{
		qWarning() << __FUNCTION__ << "abPath is empty!";
		return false;

	}

	bool ret;
	QFileInfo entry(info.abPath);
	if (entry.isDir() && !entry.isSymLink())
	{
		// µÝ¹éÉ¾³ý×ÓÄ¿Â¼
		QDir subDir(info.abPath);
		ret = subDir.removeRecursively();
	}
	else
	{
		// É¾³ýÎÄ¼þ»ò·ûºÅÁ´½Ó
		ret = QFile::remove(info.abPath);
	}

	if (!ret)
		qWarning() << "delete file" << info.abPath << "faild!";
	return ret;
}

void ImageThread::addImage(const ImageInfo& info)
{
	QMutexLocker locker(&mutex_);
	imgQueue_.enqueue(info);
}

void ImageThread::run()
{
	while (running_)
	{
		QThread::msleep(SLEEP_TIME);

		if (imgQueue_.isEmpty())
			continue;

		ImageInfo info;
		{
			QMutexLocker locker(&mutex_);
			info = imgQueue_.dequeue();
		}

		if (info.isSave)
			doSave(info);
		else
			doDelete(info);
	}
}
