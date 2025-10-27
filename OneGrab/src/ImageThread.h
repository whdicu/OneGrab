#pragma once
#include <atomic>
#include "HDBase/DQueue.hpp"
#include <QMutex>
#include <QPixmap>
#include <QThread>

struct ImageInfo
{
	QPixmap pixmap;
	QString abPath;
	bool isSave{ true };
};

class ImageThread : public QThread
{
	Q_OBJECT

public:
	static ImageThread* getInstance();
	void stop() { running_ = false; }
	void addImage(const ImageInfo& info);

protected:
	void run() override;

private:
	ImageThread(QObject *parent = nullptr);
	~ImageThread();
	bool doSave(const ImageInfo& info);
	bool doDelete(const ImageInfo& info);

	std::atomic<bool> running_;
	QMutex mutex_;
	DQueue<ImageInfo> imgQueue_;
};

#ifndef IMAGE_THREAD
#define IMAGE_THREAD ImageThread::getInstance()
#endif
