#pragma once
#include <QImage>
#include <QPixmap>
#include <windows.h>

class QScreen;

// 截屏接口。
//
// Windows 上优先走 Windows Graphics Capture：用 DXGI_FORMAT_R16G16B16A16_FLOAT 抓一帧。
// HDR 显示器下拿到的就是 scRGB 线性的真实 HDR 数据，不会像 8bit 抓屏那样被裁成过曝的泛白画面；
// 随后在 GPU 上把这一帧 tone map 成 SDR，再读回一张 QImage。
//
// 系统不支持、抓取超时或失败时返回空结果，调用方按老办法回退即可。
class DScreenCapture
{
public:
	// 系统是否支持 Windows Graphics Capture
	static bool isSupported();

	// 抓取一个显示器的一帧，返回 SDR 转换后的图像；失败返回空 QImage
	static QImage grabMonitor(HMONITOR monitor);

	// 抓取一个屏幕对应的显示器，抓不到时自动回退 QScreen::grabWindow(0)
	static QPixmap grabScreen(QScreen* screen);
};
