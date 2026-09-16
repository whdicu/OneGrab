#pragma once
#pragma execution_character_set("utf-8")
#include <QColor>
#include <QString>

class QWidget;


// Windows 原生毛玻璃封装
// 仅适用于「独立顶层窗口」（有自己的 HWND），普通子控件请使用 GlassBlurBackground
// 所有 Win32 代码与头文件都放在 .cpp 里，API 全部运行时动态查找，失败时安全降级
class WindowsGlassEffect
{
public:
	// 玻璃模式
	enum Mode
	{
		ModeNone,         // 未启用
		ModeDwmBackdrop,  // Windows 11：DWM System Backdrop（Mica / Acrylic）
		ModeAcrylic,      // Windows 10 1803+：SetWindowCompositionAttribute + ACCENT_ENABLE_ACRYLICBLURBEHIND
		ModeBlurBehind,   // Win10：ACCENT_ENABLE_BLURBEHIND（平滑跟手，但模糊按窗口矩形铺）
	};

	// 调用结果
	enum Result
	{
		ResultOk,           // 成功，实际模式见 mode()
		ResultDisabled,     // 参数里把所有方案都关掉了
		ResultNotTopLevel,  // 不是顶层窗口，窗口级毛玻璃不适用
		ResultUnsupported,  // 当前系统/API 不支持任何原生方案
		ResultFailed,       // API 存在但调用失败
	};

	// 模糊档位
	// Windows 没有对外暴露 blur 半径参数，只能靠切换玻璃材质来近似强弱
	enum BlurLevel
	{
		// Win11: Mica(2)  Win10: ACCENT_ENABLE_BLURBEHIND
		// 最弱但最跟手：Win10 的 Acrylic 会让窗口移动时明显卡顿，想要丝的拖动感就用这档
		BlurLight,
		BlurMedium,  // Win11: TabbedWindow(4)  Win10: Acrylic
		BlurStrong,  // Win11: Acrylic(3)       Win10: Acrylic
	};

	struct Params
	{
		// Win11：是否允许使用 DWM System Backdrop
		bool allowDwmBackdrop = true;
		// Win10：是否允许使用 Acrylic（比 BlurBehind 好看，但拖动窗口可能有延迟）
		bool allowAcrylic = true;
		// 是否允许退回更老的 BlurBehind
		bool allowBlurBehind = true;
		// 模糊档位
		BlurLevel blurLevel = BlurStrong;
		// 玻璃着色：RGB = 颜色，alpha = 深浅（越小越透、模糊越明显）
		// Win11 系统 backdrop 不给自定义 tint，这层由窗口自己画；Win10 则由系统着色
		QColor tint = QColor(24, 26, 32, 130);
		// 系统已经着了色的模式（**只有 Win10 Acrylic**）里，窗口自绘那层只取这个比例，
		// 免得两层叠加之后几乎不透明、把模糊糊没了；想完全交给系统着色就设 0。
		// 实测（贴纯不透明红做对照）：BLURBEHIND 完全无视 GradientColor，Acrylic 才吃 —— 所以
		// BlurBehind 模式下自绘层不打折，否则这个窗口的"颜色深浅"就没任何地方受控了
		qreal appTintRatio = 0.3;
		// DWM backdrop 需要把客户区并入边框区域才能铺满整个窗口
		bool extendFrameIntoClientArea = true;
		// 窗口圆角半径（Qt 逻辑像素）；0 = 直角
		// 只画圆角的自绘层是不行的：系统模糊不跟着自绘层走，必须把窗口本身也裁成圆角（SetWindowRgn）。
		// Win10 的坑（已实测）：accent 模糊按窗口「矩形」铺、不认窗口区域（回读确认区域确实生效），
		// 所以圆角外面那四个小三角里会残留模糊 —— 半径越大越明显，这是系统限制。试过三条路都无效：
		//   ① DwmEnableBlurBehindWindow + DWM_BB_BLURREGION：Win10 上这个老 API 完全不出模糊
		//   ② ACCENT_ENABLE_HOSTBACKDROP：需要 WS_EX_NOREDIRECTIONBITMAP，普通窗口无效果
		//   ③ DwmExtendFrameIntoClientArea({-1,-1,-1,-1}) 把客户区并入边框区域：无改善
		// Win11 走官方 DWMWCP_ROUND，圆角和材质天生是齐的，不存在这些问题
		int cornerRadius = 0;
		// Win11 21H2+ 是否优先用 DWM 官方圆角：
		// 抗锯齿最自然、跟系统素材融合得最好，但半径由系统决定（约 8px，改不了）；
		// 想要精确半径（比如 20px）就设 false，退回自己 SetWindowRgn 裁（半径精确，边缘有轻微锯齿）
		bool preferDwmCorners = true;
		// SetWindowCompositionAttribute 的 AccentFlags：0x20 左 / 0x40 上 / 0x80 右 / 0x100 下，
		// 少写哪个方向，那条边就不会被画上 accent，表现出来就是"某一边没有边框和阴影"。
		// 四个方向都要就写 0x20|0x40|0x80|0x100（= 0x1E0）；也可以试 0（完全不画边）
		//quint32 accentFlags = 0x20 | 0x40 | 0x80 | 0x100;
		quint32 accentFlags = 0;
		// 是否让窗口标题栏跟随深色模式（无边框窗口看不到标题栏，浅色玻璃建议设 false）
		bool darkTitleBar = true;
	};

	// 给顶层窗口（重新）启用毛玻璃，返回实际结果
	static Result enable(QWidget* widget, const Params& params = Params());
	// 关闭毛玻璃并恢复窗口原有样式
	static void disable(QWidget* widget);

	// 已经生效的玻璃模式
	static Mode mode(const QWidget* widget);
	static QString modeText(Mode mode);
	static QString resultText(Result result);

	// 当前生效的参数（窗口自己画 tint 时要用它取颜色）；没启用过就返回默认参数
	static Params params(const QWidget* widget);
	// 窗口 paintEvent 里铺底色应该用的颜色：已按当前模式把深浅调好，直接用就行。
	// 深色/浅色、透明度全都由 Params::tint 决定，各窗口的调参常量最终都汇到这里
	static QColor appTint(const QWidget* widget);
	// 运行时热改，方便一边看一边调
	static bool setTint(QWidget* widget, const QColor& tint);
	static bool setBlurLevel(QWidget* widget, BlurLevel level);
	static bool setCornerRadius(QWidget* widget, int radius);

	// 当前系统是否具备对应能力（只判断能力，不代表调用一定成功）
	static bool isDwmBackdropSupported();
	// Win11 21H2+ 才有官方窗口圆角；Win10 上圆角只能自己裁、且模糊不会跟着圆角走
	static bool isDwmCornersSupported();
	// 当前窗口的圆角是否由 DWM 官方在处理（半径由系统定）。
	// 为 true 时窗口形状由系统裁，窗口自己的自绘层必须铺满整块，不要再画圆角
	static bool isDwmCornersActive(const QWidget* widget);
	static bool isAcrylicSupported();
	static bool isBlurBehindSupported();
};
