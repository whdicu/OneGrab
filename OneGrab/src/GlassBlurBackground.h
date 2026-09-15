#pragma once
#pragma execution_character_set("utf-8")
#include <QColor>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class GlassBlurLayer;


// 普通子控件（没有独立 HWND，用不了窗口级 DWM / Acrylic）的毛玻璃 fallback：
//   截取控件后方的父窗口背景 -> 高斯模糊 -> 叠一层 tint -> 画在控件背后
//
// 实现要点：
// - 复用一个铺在控件底层的透明子控件来绘制，不改业务控件的 paintEvent
// - 截取时只渲染父控件自身背景（不带 children），因此不会递归截到自己
// - 结果带缓存，只在控件尺寸/位置/显示状态变化时才重算，不在每次重绘里截屏
// 顶层窗口请用 WindowsGlassEffect，不要用这个类
class GlassBlurBackground : public QObject
{
	Q_OBJECT

public:
	struct Params
	{
		int blurRadius = 16;                       // 高斯模糊半径
		int downsample = 4;                        // 先降采样再模糊以降低开销，1 表示不降采样
		QColor tint = QColor(255, 255, 255, 40);   // 模糊后叠加的着色层
		int cornerRadius = 0;                      // > 0 时按圆角裁剪
	};

	// 给子控件挂上毛玻璃背景；返回是否成功（顶层窗口会返回 false）
	static bool attach(QWidget* widget, const Params& params = Params());
	static void detach(QWidget* widget);
	// 父窗口背景变化后手动刷新缓存
	static void refresh(QWidget* widget);
	static bool isAttached(const QWidget* widget);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	explicit GlassBlurBackground(QWidget* widget, const Params& params);

	void buildLayer();     // 创建底层绘制控件
	void invalidate();     // 缓存失效并重绘

	QPointer<QWidget> widget_;
	QPointer<GlassBlurLayer> layer_;
	Params params_;
};


// 铺在目标控件底层、负责画模糊背景的控件
class GlassBlurLayer : public QWidget
{
public:
	GlassBlurLayer(QWidget* source, const GlassBlurBackground::Params& params);

	// 重新截取 + 模糊 + 缓存，带重入保护
	void rebuild();

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QPointer<QWidget> source_;      // 需要毛玻璃的那个控件
	GlassBlurBackground::Params params_;
	QPixmap cache_;
	QPoint offset_;                 // 缓存图相对控件左上角的偏移（模糊时向外扩了一圈）
	bool rebuilding_ = false;       // 防止重入
};
