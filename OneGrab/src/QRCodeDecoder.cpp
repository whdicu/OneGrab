#include "QRCodeDecoder.h"
#include <QDebug>
#include <QPoint>
#include <QVector>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include "ReadBarcode.h"


namespace
{
	// 只认二维码：截图里不会有条码需求，限制格式能明显提速
	ZXing::DecodeHints makeHints()
	{
		ZXing::DecodeHints hints;
		hints.setFormats(ZXing::BarcodeFormat::QRCode);
		hints.setTryHarder(true);
		hints.setTryRotate(true);
		hints.setTryDownscale(true);
		return hints;
	}

	// 通道数映射到 ZXing 的像素格式：QImage 的 ARGB32 在内存里就是 B,G,R,A
	bool zxingFormatFromChannels(int channels, ZXing::ImageFormat& format)
	{
		switch (channels)
		{
		case 1:
			format = ZXing::ImageFormat::Lum;
			return true;
		case 3:
			format = ZXing::ImageFormat::BGR;
			return true;
		case 4:
			format = ZXing::ImageFormat::BGRX;
			return true;
		default:
			return false;
		}
	}

	// 统一成 8 位深的 1/3/4 通道数据，其余通道数取第一个通道
	cv::Mat normalizeImage(const cv::Mat& image)
	{
		if (image.empty())
			return cv::Mat();

		cv::Mat mat = image;
		if (mat.depth() != CV_8U)
		{
			const double scale = mat.depth() == CV_16U ? 1.0 / 256.0 : 1.0;
			cv::Mat converted;
			mat.convertTo(converted, CV_MAKETYPE(CV_8U, mat.channels()), scale);
			mat = converted;
		}

		if (mat.channels() != 1 && mat.channels() != 3 && mat.channels() != 4)
		{
			cv::Mat firstChannel;
			cv::extractChannel(mat, firstChannel, 0);
			mat = firstChannel;
		}

		return mat;
	}

	struct FoundQRCode
	{
		QString text;
		QPoint center;
	};

	cv::Mat toGray(const cv::Mat& image)
	{
		if (image.channels() == 1)
			return image;

		cv::Mat gray;
		cv::cvtColor(image, gray, image.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
		return gray;
	}

	// 扫描一次，把结果按位置合并进 found（同一个码在不同预处理图里位置会有几像素偏差，不能按文本去重）
	void scanOnce(const cv::Mat& image, double scale, const ZXing::DecodeHints& hints, QVector<FoundQRCode>& found)
	{
		ZXing::ImageFormat format;
		if (!zxingFormatFromChannels(image.channels(), format))
			return;

		const ZXing::ImageView view(image.data, image.cols, image.rows, format, static_cast<int>(image.step));
		const ZXing::Results results = ZXing::ReadBarcodes(view, hints);

		for (const ZXing::Result& result : results)
		{
			if (!result.isValid())
				continue;

			const std::string text = result.text();
			if (text.empty())
				continue;

			const ZXing::PointI position = ZXing::Center(result.position());
			const QPoint center(static_cast<int>(position.x / scale), static_cast<int>(position.y / scale));

			bool merged = false;
			for (const FoundQRCode& item : found)
			{
				if ((item.center - center).manhattanLength() <= 12)
				{
					merged = true;
					break;
				}
			}

			if (!merged)
				found.push_back({ QString::fromStdString(text), center });
		}
	}
}


QStringList QRCodeDecoder::decodeQRCodes(const cv::Mat& image)
{
	QStringList texts;

	cv::Mat mat = normalizeImage(image);
	if (mat.empty())
	{
		qDebug() << __FUNCTION__ << "empty or unsupported image";
		return texts;
	}

	QVector<FoundQRCode> found;
	const ZXing::DecodeHints hints = makeHints();

	scanOnce(mat, 1.0, hints, found);

	// 原图扫不出来时才补扫，而且只对"本身就不大"的图做：缩小过的截图需要放大才有足够码点，
	// 码点特别密（带中心 logo 的收款码）时还要配合 OTSU 二值化，实测这两种补扫能覆盖常见失败情况
	if (found.isEmpty() && std::min(mat.cols, mat.rows) <= 720)
	{
		cv::Mat enlarged;
		cv::resize(mat, enlarged, cv::Size(), 6.0, 6.0, cv::INTER_LANCZOS4);
		scanOnce(enlarged, 6.0, hints, found);

		if (found.isEmpty())
		{
			cv::Mat bigger, binary;
			cv::resize(mat, bigger, cv::Size(), 8.0, 8.0, cv::INTER_LANCZOS4);
			cv::threshold(toGray(bigger), binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
			scanOnce(binary, 8.0, hints, found);
		}
	}

	for (const FoundQRCode& item : found)
		texts << item.text;

	qDebug() << __FUNCTION__ << "image:" << mat.cols << "x" << mat.rows << "found:" << texts.size();
	return texts;
}
