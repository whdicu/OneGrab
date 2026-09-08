#pragma once
#include "HDMemory/DSharedPointer.hpp"
#include <QImage>
#include "opencv2/core.hpp"


using SharedImage = DSharedPointer<QImage>;


class ImageDownloadCallBack : public QObject
{
	Q_OBJECT
public:
	explicit ImageDownloadCallBack(QObject *parent = nullptr);
	~ImageDownloadCallBack();
	void operator()(SharedImage image);

signals:
	void sigImageSet(SharedImage image);
};


namespace ImageHandler
{
	// 将图片保持比例缩放到指定大小，会去除多余部分
	cv::Mat fitImage(const cv::Mat& image, int width, int height);

	// 对图片模糊处理
	cv::Mat blurImage(const cv::Mat& image, int blurRadius);

	// 调整图像亮度
	cv::Mat lightImage(const cv::Mat& image, double brightness);

	// 裁剪图片
	cv::Mat cutImage(const cv::Mat& image, const QRect& rect);

	QRgb getMainColor(const cv::Mat& image, const QRect& rect = QRect(0, 0, 0, 0), int step = 3);

	QString getTextColor(const cv::Mat& image);
	
	cv::Mat QImageToCvMat(const QImage& inImage, bool inCloneImageData = true);

	QImage cvMatToQImage(const cv::Mat& inMat);
	
	QImage roundImage(const QImage& image, int radius, bool onlyTop = false);
	
	cv::Mat roundCVMat(const cv::Mat& image, int radiusTL, int radiusTR
		, int radiusBL, int radiusBR);

	// 图片下载函数，阻塞执行，图片下载完了函数才会返回
	SharedImage downloadImage(const QString& url);

	// 图片下载函数，不阻塞执行，发送完下载请求就会返回，图片在下载完时通过信号发送
	void downloadImage(const QString& url, ImageDownloadCallBack* callBack);
}

