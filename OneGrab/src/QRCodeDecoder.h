#pragma once
#include <QStringList>
#include "opencv2/core.hpp"


namespace QRCodeDecoder
{
	// 用 ZXing-C++ 识别图片中的二维码，返回所有成功解码且内容非空的结果（UTF-8 已转成 QString）
	QStringList decodeQRCodes(const cv::Mat& image);
}
