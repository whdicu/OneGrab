#pragma once
#include <QStringList>
#include "opencv2/core.hpp"


namespace BarcodeDecoder
{
	// 用 ZXing-C++ 识别图片中的二维码/一维码，返回所有成功解码且内容非空的结果（UTF-8 已转成 QString）
	QStringList decodeBarcodes(const cv::Mat& image);
}
