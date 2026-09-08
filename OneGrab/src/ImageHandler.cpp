#include "ImageHandler.h"
#include "HDCore/DGlobal.h"
#include "opencv2/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>


cv::Mat ImageHandler::fitImage(const cv::Mat& image, int width, int height)
{
	// 将mat裁剪
	int widthByHeight = image.cols * height / width;
	double scale = 1.0;
	if (image.rows > widthByHeight)  // 竖直长条形
	{
		// 根据宽度缩放图像到指定大小
		scale = static_cast<double>(width) / image.cols;
	}
	else  // 横向长条形
	{
		// 根据高度缩放图像到指定大小
		scale = static_cast<double>(height) / image.rows;
	}
	cv::Mat resizedImage;
	cv::resize(image, resizedImage, cv::Size(), scale, scale, cv::INTER_AREA);

	// 裁剪到与label相同大小
	QRect roi((resizedImage.cols - width) / 2, (resizedImage.rows - height) / 2, width, height);
	return cutImage(resizedImage, roi);
}

cv::Mat ImageHandler::blurImage(const cv::Mat& image, int blurRadius)
{
	cv::Mat blurredMat;
	//QElapsedTimer time;
	//time.start();
	//cv::GaussianBlur(resizedImage, blurredMat, cv::Size(101, 101), 18);  // 31 8
	//cv::blur(resizedImage, blurredMat, cv::Size(31, 31));
	//cv::stackBlur(croppedImage, blurredMat, cv::Size(61, 61));
	cv::stackBlur(image, blurredMat, cv::Size(blurRadius * 2 + 1, blurRadius * 2 + 1));
	//qDebug() << "GaussianBlur:" << time.elapsed();
	return blurredMat;
}

cv::Mat ImageHandler::lightImage(const cv::Mat& image, double brightness)
{
	cv::Mat trasMat;
	if (image.type() == CV_8UC4)
		cv::cvtColor(image, trasMat, cv::COLOR_RGBA2RGB);
	return brightness * trasMat;
}

cv::Mat ImageHandler::cutImage(const cv::Mat& image, const QRect& rect)
{
	if (rect.x() + rect.width() > image.cols
		|| rect.y() + rect.height() > image.rows)
		return image;

	cv::Rect roi(rect.x(), rect.y(), rect.width(), rect.height());
	return image(roi);
}

QRgb ImageHandler::getMainColor(const cv::Mat& image, const QRect& rect, int step)
{
	int fromCol = 0, toCol = 0, fromRow = 0, toRow = 0;
	if (rect == QRect(0, 0, 0, 0))
	{
		toCol = image.cols;
		toRow = image.rows;
	}
	else
	{
		fromCol = dBigger(rect.left(), 0);
		fromRow = dBigger(rect.top(), 0);
		toCol = dSmaller(rect.right(), image.cols);
		toRow = dSmaller(rect.bottom(), image.rows);
	}
	if (step < 1)
		step = 1;

	unsigned long long r = 0, g = 0, b = 0, a = 0;
	unsigned long long cnt = 0;
	for (int i = fromCol; i < toCol; i += step)
	{
		// 原来只采样图片下半部分
		//for (int j = image.height() / 2; j < image.height(); j += 4)
		for (int j = fromRow; j < toRow; j += step)
		{
			cv::Vec3b pixel = image.at<cv::Vec3b>(j, i);

			// 分别输出RGB值
			b += pixel[0];
			g += pixel[1];
			r += pixel[2];


			//QRgb pix = image.pixel(i, j);
			//r += qRed(pix);
			//g += qGreen(pix);
			//b += qBlue(pix);
			//a += qAlpha(pix);
			++cnt;
		}
	}
	return qRgba(r / cnt, g / cnt, b / cnt, a / cnt);
}

QString ImageHandler::getTextColor(const cv::Mat& image)
{
	QRgb c3 = getMainColor(image);

	return QString("%1, %2, %3").arg((int)((qRed(c3) << 2) + 255) / 5)
		.arg((int)((qGreen(c3) << 2) + 255) / 5)
		.arg((int)((qBlue(c3) << 2) + 255) / 5);

	//return QString("#%1%2%3").arg((int)qMin(qRed(c3) * 1.3, 255.0), 2, 16, QChar('0'))
	//	.arg((int)qMin(qGreen(c3) * 1.3, 255.0), 2, 16, QChar('0'))
	//	.arg((int)qMin(qBlue(c3) * 1.3, 255.0), 2, 16, QChar('0'));

	/*if (qRed(c3) + qGreen(c3) + qBlue(c3) < 384)
		return "#b6d1c8";
	else
		return "#5c5c66";*/
}

cv::Mat ImageHandler::QImageToCvMat(const QImage& inImage,
	bool inCloneImageData)
{
	switch (inImage.format())
	{
		// 8-bit, 4 channel
	case QImage::Format_RGBA8888:
	{
		QImage   swapped = inImage.rgbSwapped();

		cv::Mat  mat(swapped.height(), swapped.width(),
			CV_8UC4, const_cast<uchar*>(swapped.bits()),
			swapped.bytesPerLine());

		return (inCloneImageData ? mat.clone() : mat);
	}
	case QImage::Format_ARGB32:
	case QImage::Format_ARGB32_Premultiplied:
	{
		cv::Mat  mat(inImage.height(), inImage.width(),
			CV_8UC4, const_cast<uchar*>(inImage.bits()),
			inImage.bytesPerLine());

		return (inCloneImageData ? mat.clone() : mat);
	}
	case QImage::Format_RGB32:
	{
		cv::Mat  mat(inImage.height(), inImage.width(),
			CV_8UC4, const_cast<uchar*>(inImage.bits()),
			inImage.bytesPerLine());

		return (inCloneImageData ? mat.clone() : mat);
	}

	// 8-bit, 3 channel
	case QImage::Format_RGB888:
	{
		if (!inCloneImageData)
			qWarning() << "ASM::QImageToCvMat() - \
								  						Conversion requires cloning since we use \
																				a temporary QImage";

		//QImage   swapped = inImage.rgbSwapped();
		cv::Mat mat = cv::Mat(inImage.height(), inImage.width(),
			CV_8UC3, const_cast<uchar*>(inImage.bits()),
			inImage.bytesPerLine()).clone();
		cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);  // 快
		return mat;
	}

	// 8-bit, 1 channel
	case QImage::Format_Indexed8:
	case QImage::Format_Grayscale8:
	{
		cv::Mat  mat(inImage.height(), inImage.width(),
			CV_8UC1, const_cast<uchar*>(inImage.bits()),
			inImage.bytesPerLine());

		return (inCloneImageData ? mat.clone() : mat);
	}

	default:
		qWarning() << "ASM::QImageToCvMat() - QImage format \
							  					not handled in switch:" << inImage.format();
		break;
	}

	return cv::Mat();
}

QImage ImageHandler::cvMatToQImage(const cv::Mat& inMat)
{
	switch (inMat.type())
	{
		// 8-bit, 4 channel
	case CV_8UC4:
	{
		QImage image(inMat.data,
			inMat.cols, inMat.rows,
			static_cast<int>(inMat.step),
			QImage::Format_ARGB32);

		return image.copy();
	}

	// 8-bit, 3 channel
	case CV_8UC3:
	{
		cv::cvtColor(inMat, inMat, cv::COLOR_BGR2RGB);  // 快

		QImage image(inMat.data,
			inMat.cols, inMat.rows,
			static_cast<int>(inMat.step),
			QImage::Format_RGB888);

		//return image.rgbSwapped();
		return image.copy();
	}

	// 8-bit, 1 channel
	case CV_8UC1:
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)

		QImage image(inMat.data,
			inMat.cols, inMat.rows,
			static_cast<int>(inMat.step),
			QImage::Format_Grayscale8);

#else
		static QVector<QRgb>  sColorTable;

		// only create our color table the first time
		if (sColorTable.isEmpty())
		{
			sColorTable.resize(256);

			for (int i = 0; i < 256; ++i)
			{
				sColorTable[i] = qRgb(i, i, i);
			}
		}

		QImage image(inMat.data,
			inMat.cols, inMat.rows,
			static_cast<int>(inMat.step),
			QImage::Format_Indexed8);

		image.setColorTable(sColorTable);
#endif

		return image.copy();
	}

	default:
		qWarning() << "ASM::cvMatToQImage() - cv::Mat image type not handled in switch:" << inMat.type();
		break;
	}

	return QImage();
}

QImage ImageHandler::roundImage(const QImage& image, int radius, bool onlyTop)
{
	QImage roundedImage(image.size(), QImage::Format_ARGB32);
	roundedImage.fill(Qt::transparent);

	QPainter painter(&roundedImage);
	painter.setRenderHint(QPainter::Antialiasing);

	QPainterPath path;
	
	if (onlyTop)
		path.addRoundedRect(0, 0, image.width(), image.height() + radius, radius, radius);
	else
		path.addRoundedRect(roundedImage.rect(), radius, radius);

	//path.addRect(0, radiusTL, image.width(), image.height() - radiusTL);

	// 四个角
	//path.addRoundedRect(0, 0, radiusTL * 2, radiusTL * 2, radiusTL, radiusTL);
	//path.addRoundedRect(image.width() - 2 * radiusTR, 0, radiusTR * 2, radiusTR * 2, radiusTR, radiusTR);
	//path.addRoundedRect(0, image.height() - 2 * radiusBL, radiusBL * 2, radiusBL * 2, radiusBL, radiusBL);
	//path.addRoundedRect(image.width() - 2 * radiusBR, image.height() - 2 * radiusBR, radiusBR * 2, radiusBR * 2, radiusBR, radiusBR);
	
	//path.addRoundedRect(roundedImage.rect(), radiusTL, radiusTL);
	//path.addRoundedRect(roundedImage.rect(), radiusTL, radiusTL, Qt::TopLeftCorner);
	//path.addRoundedRect(roundedImage.rect(), radiusTR, radiusTR, Qt::TopRightCorner);
	//path.addRoundedRect(roundedImage.rect(), radiusBL, radiusBL, Qt::BottomLeftCorner);
	//path.addRoundedRect(roundedImage.rect(), radiusBR, radiusBR, Qt::BottomRightCorner);

	painter.setClipPath(path);
	painter.drawImage(roundedImage.rect(), image);

	return roundedImage;
}

cv::Mat ImageHandler::roundCVMat(const cv::Mat& image, int radiusTL, int radiusTR
	, int radiusBL, int radiusBR)
{
	// 创建遮罩层
	cv::Mat mask(image.size(), CV_8UC1, cv::Scalar(0));
	cv::Rect rectTopLeft(0, 0, 2 * radiusTL, 2 * radiusTL);
	cv::Rect rectTopRight(image.cols - 2 * radiusTR, 0, 2 * radiusTR, 2 * radiusTR);
	cv::Rect rectBottomLeft(0, image.rows - 2 * radiusBL, 2 * radiusBL, 2 * radiusBL);
	cv::Rect rectBottomRight(image.cols - 2 * radiusBR, image.rows - 2 * radiusBR, 2 * radiusBR, 2 * radiusBR);

	cv::circle(mask, rectTopLeft.tl() + cv::Point(radiusTL, radiusTL), radiusTL, cv::Scalar(255), -1, cv::LINE_AA);
	cv::circle(mask, rectTopRight.tl() + cv::Point(-radiusTR, radiusTR), radiusTR, cv::Scalar(255), -1, cv::LINE_AA);
	cv::circle(mask, rectBottomLeft.tl() + cv::Point(radiusBL, -radiusBL), radiusBL, cv::Scalar(255), -1, cv::LINE_AA);
	cv::circle(mask, rectBottomRight.tl() + cv::Point(-radiusBR, -radiusBR), radiusBR, cv::Scalar(255), -1, cv::LINE_AA);

	// 应用遮罩层
	cv::Mat result;
	image.copyTo(result, mask);
	return result;
}

void ImageHandler::downloadImage(const QString& url, ImageDownloadCallBack* callBack)
{
	if (nullptr == callBack)
	{
		qWarning() << __FUNCTION__ << "Image download call back is nullptr!";
		return;
	}

	QNetworkAccessManager* manager = new QNetworkAccessManager;
	QNetworkRequest request(url);
	QNetworkReply* reply = manager->get(request);

	QObject::connect(reply, &QNetworkReply::finished, reply, [manager, reply, callBack]()
	{
		if (reply->error() == QNetworkReply::NoError)
		{
			QByteArray imageData = reply->readAll();
			SharedImage image(new QImage);
			image->loadFromData(imageData);
			(*callBack)(image);
		}
		else
		{
			qWarning() << __FUNCTION__ << "Failed to download image:" << reply->errorString();
		}

		reply->deleteLater();
		manager->deleteLater();
	});
}

SharedImage ImageHandler::downloadImage(const QString& url)
{
	SharedImage imageRet(new QImage);
	
	QNetworkAccessManager* manager = new QNetworkAccessManager;
	QNetworkRequest request(url);
	QNetworkReply* reply = manager->get(request);

	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() == QNetworkReply::NoError)
	{
		QByteArray imageData = reply->readAll();
		imageRet->loadFromData(imageData);
	}
	else
	{
		qWarning() << __FUNCTION__ << "Failed to download image:" << reply->errorString();
	}

	reply->deleteLater();
	manager->deleteLater();

	return imageRet;
}

ImageDownloadCallBack::ImageDownloadCallBack(QObject *parent /*= nullptr*/)
	: QObject(parent)
{

}

ImageDownloadCallBack::~ImageDownloadCallBack()
{

}

void ImageDownloadCallBack::operator()(SharedImage image)
{
	emit sigImageSet(image);
}
