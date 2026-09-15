#include "WindowsGlassEffect.h"
#include <QDebug>
#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QVariant>

#ifdef Q_OS_WIN

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

// ---------------------------------------------------------------- 私有类型
// SetWindowCompositionAttribute 是非公开 API，SDK 里没有对应声明，这里自己定义
namespace
{
	enum AccentState
	{
		AccentDisabled = 0,
		AccentEnableGradient = 1,
		AccentEnableTransparentGradient = 2,
		AccentEnableBlurBehind = 3,
		AccentEnableAcrylicBlurBehind = 4,
		// 系统"宿主背景"材质（开始菜单那一套）。实测（Win10 19045 + Qt 半透明窗口）不出效果：
		// 官方前置条件是窗口带 WS_EX_NOREDIRECTIONBITMAP，普通窗口不满足，所以这里不作为方案
		AccentEnableHostBackdrop = 5,
		AccentInvalidState = 6
	};

	struct AccentPolicy
	{
		AccentState accentState;
		DWORD accentFlags;
		DWORD gradientColor;   // 注意是 ABGR：0xAABBGGRR
		DWORD animationId;
	};

	enum WindowCompositionAttribute
	{
		WcaAccentPolicy = 19
	};

	struct WindowCompositionAttributeData
	{
		WindowCompositionAttribute attribute;
		PVOID data;
		SIZE_T sizeOfData;
	};

	// DWM 属性，老 SDK 里没有定义，这里自己写
	const DWORD DwmwaUseImmersiveDarkMode = 20;
	const DWORD DwmwaSystemBackdropType = 38;

	// DWMSBT_*
	const int DwmsbtMainWindow = 2;       // Mica
	const int DwmsbtTransientWindow = 3;  // Acrylic，适合弹出窗口
	const int DwmsbtTabbedWindow = 4;

	// 模糊档位 -> Win11 backdrop 类型
	int blurLevelToBackdrop(WindowsGlassEffect::BlurLevel level)
	{
		switch (level)
		{
		case WindowsGlassEffect::BlurLight:
			return DwmsbtMainWindow;
		case WindowsGlassEffect::BlurMedium:
			return DwmsbtTabbedWindow;
		default:
			return DwmsbtTransientWindow;
		}
	}

	// 模糊档位 -> Win10 accent 状态（最弱那档用更平滑的老式模糊）
	AccentState blurLevelToAccent(WindowsGlassEffect::BlurLevel level)
	{
		return level == WindowsGlassEffect::BlurLight ? AccentEnableBlurBehind : AccentEnableAcrylicBlurBehind;
	}

	// RTL_OSVERSIONINFOW 的结构，自己定义避免引入 winternl.h
	struct OsVersionInfo
	{
		DWORD osVersionInfoSize;
		DWORD majorVersion;
		DWORD minorVersion;
		DWORD buildNumber;
		DWORD platformId;
		WCHAR csdVersion[128];
	};

	// Windows 11 22H2 起才支持 DWMWA_SYSTEMBACKDROP_TYPE
	const DWORD MinBuildForDwmBackdrop = 22621;
	// Windows 11 21H2 起才有官方窗口圆角
	const DWORD MinBuildForDwmCorners = 22000;
	// Windows 10 1803 起才支持 Acrylic
	const DWORD MinBuildForAcrylic = 17134;

	// DWMWA_WINDOW_CORNER_PREFERENCE 与 DWMWCP_*
	const DWORD DwmwaWindowCornerPreference = 33;
	const int DwmwcpDoNotRound = 1;
	const int DwmwcpRound = 2;

	typedef BOOL(WINAPI* PFN_SetWindowCompositionAttribute)(HWND, WindowCompositionAttributeData*);
	typedef HRESULT(WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
	typedef HRESULT(WINAPI* PFN_DwmGetWindowAttribute)(HWND, DWORD, PVOID, DWORD);
	typedef HRESULT(WINAPI* PFN_DwmExtendFrameIntoClientArea)(HWND, const MARGINS*);
	typedef LONG(WINAPI* PFN_RtlGetVersion)(OsVersionInfo*);

	// 只查一次，查不到就记住 nullptr
	template <typename T>
	T resolve(HMODULE module, const char* name)
	{
		return module ? reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(module, name))) : nullptr;
	}

	PFN_SetWindowCompositionAttribute resolveSetWindowCompositionAttribute()
	{
		static PFN_SetWindowCompositionAttribute pfn =
			resolve<PFN_SetWindowCompositionAttribute>(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
		return pfn;
	}

	PFN_DwmSetWindowAttribute resolveDwmSetWindowAttribute()
	{
		static PFN_DwmSetWindowAttribute pfn =
			resolve<PFN_DwmSetWindowAttribute>(LoadLibraryW(L"dwmapi.dll"), "DwmSetWindowAttribute");
		return pfn;
	}

	PFN_DwmGetWindowAttribute resolveDwmGetWindowAttribute()
	{
		static PFN_DwmGetWindowAttribute pfn =
			resolve<PFN_DwmGetWindowAttribute>(LoadLibraryW(L"dwmapi.dll"), "DwmGetWindowAttribute");
		return pfn;
	}

	PFN_DwmExtendFrameIntoClientArea resolveDwmExtendFrameIntoClientArea()
	{
		static PFN_DwmExtendFrameIntoClientArea pfn =
			resolve<PFN_DwmExtendFrameIntoClientArea>(LoadLibraryW(L"dwmapi.dll"), "DwmExtendFrameIntoClientArea");
		return pfn;
	}

	// 取真实系统版本号，GetVersionEx 在没写 manifest 时会说谎
	const OsVersionInfo& osVersion()
	{
		static OsVersionInfo info = []()
		{
			OsVersionInfo v = {};
			v.osVersionInfoSize = sizeof(OsVersionInfo);
			PFN_RtlGetVersion pfn = resolve<PFN_RtlGetVersion>(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
			if (pfn)
				pfn(&v);
			return v;
		}();
		return info;
	}

	// ABGR
	DWORD toAbgr(const QColor& color)
	{
		const QColor c = color.isValid() ? color : QColor(0, 0, 0, 0);
		return (static_cast<DWORD>(c.alpha()) << 24)
			| (static_cast<DWORD>(c.blue()) << 16)
			| (static_cast<DWORD>(c.green()) << 8)
			| static_cast<DWORD>(c.red());
	}

	HWND hwndOf(const QWidget* widget)
	{
		return reinterpret_cast<HWND>(widget->winId());
	}
}

// 执行 SetWindowCompositionAttribute，返回是否成功
static bool applyAccentPolicy(HWND hwnd, AccentState state, DWORD flags, const QColor& tint)
{
	PFN_SetWindowCompositionAttribute pfn = resolveSetWindowCompositionAttribute();
	if (!pfn)
		return false;

	AccentPolicy policy = {};
	policy.accentState = state;
	policy.accentFlags = flags;
	policy.gradientColor = toAbgr(tint);
	policy.animationId = 0;

	WindowCompositionAttributeData data = {};
	data.attribute = WcaAccentPolicy;
	data.data = &policy;
	data.sizeOfData = sizeof(policy);

	return FALSE != pfn(hwnd, &data);
}

// 生成圆角窗口区域（窗口物理像素）。调用方负责释放：SetWindowRgn 成功后归系统，
// 失败或者给 DwmEnableBlurBehindWindow 用的话要自己 DeleteObject
static HRGN createRoundedRegion(QWidget* widget, int radius)
{
	const HWND hwnd = hwndOf(widget);
	if (!hwnd || radius <= 0)
		return nullptr;

	RECT rect = {};
	if (!GetWindowRect(hwnd, &rect))
	{
		qWarning() << __FUNCTION__ << "GetWindowRect failed";
		return nullptr;
	}

	const int w = rect.right - rect.left;
	const int h = rect.bottom - rect.top;
	if (w <= 0 || h <= 0)
		return nullptr;

	const qreal dpr = widget->devicePixelRatioF() > 0 ? widget->devicePixelRatioF() : 1.0;
	const int r = qRound(radius * dpr);   // Qt 逻辑像素 -> 屏幕物理像素

	// CreateRoundRectRgn 收的是圆角椭圆的宽高，所以要乘 2
	return CreateRoundRectRgn(0, 0, w + 1, h + 1, r * 2, r * 2);
}

// 把窗口裁成圆角。只画圆角自绘层是不够的：系统模糊是按窗口形状来的，
// 不裁窗口的话圆角外面还会露着一块方形的模糊
static void applyCornerRadius(QWidget* widget, const WindowsGlassEffect::Params& params)
{
	const HWND hwnd = hwndOf(widget);
	if (!hwnd)
		return;

	// 先清掉可能残留的旧区域，避免和系统圆角打架
	SetWindowRgn(hwnd, nullptr, TRUE);

	// Win11 有官方圆角，抗锯齿比 SetWindowRgn 好，但半径由系统定
	if (params.cornerRadius > 0 && params.preferDwmCorners
		&& osVersion().buildNumber >= MinBuildForDwmCorners)
	{
		PFN_DwmSetWindowAttribute setAttr = resolveDwmSetWindowAttribute();
		if (setAttr)
		{
			const int pref = DwmwcpRound;
			if (SUCCEEDED(setAttr(hwnd, DwmwaWindowCornerPreference, &pref, sizeof(pref))))
			{
				// 圆角交给系统了，自绘层就别再画圆角：两边半径不一致的话，
				// 角上会留一条没着色的缝（系统半径约 8px，我们请求的可能是 20px）
				widget->setProperty("WindowsGlassEffect.dwmCorners", true);
				return;
			}
		}
	}

	widget->setProperty("WindowsGlassEffect.dwmCorners", false);

	if (params.cornerRadius <= 0)
		return;   // 上面已经清成直角了

	HRGN rgn = createRoundedRegion(widget, params.cornerRadius);
	if (!rgn)
		return;

	if (!SetWindowRgn(hwnd, rgn, TRUE))
	{
		DeleteObject(rgn);   // 失败时区域还归我们，得自己释放；成功的话归系统
		qWarning() << __FUNCTION__ << "SetWindowRgn failed";
		return;
	}

	qDebug() << __FUNCTION__ << "corner radius =" << params.cornerRadius;
}

// 监听窗口重建（改 flags / 重新 show 会让 Qt 换一个 HWND），换句柄后重新贴一遍效果
class WindowsGlassEffectFilter : public QObject
{
public:
	WindowsGlassEffectFilter(QWidget* widget, const WindowsGlassEffect::Params& params)
		: QObject(widget)
		, widget_(widget)
		, params_(params)
	{
	}

	const WindowsGlassEffect::Params& params() const { return params_; }
	void setParams(const WindowsGlassEffect::Params& params) { params_ = params; }

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (!widget_)
			return QObject::eventFilter(watched, event);

		switch (event->type())
		{
		case QEvent::WinIdChange:
		case QEvent::Show:
			// WinIdChange 期间 HWND 已经换成新的，但要等 Qt 把窗口属性都设置完再贴效果
			QTimer::singleShot(0, widget_, [this]()
			{
				if (widget_)
					WindowsGlassEffect::enable(widget_, params_);
			});
			break;
		case QEvent::Resize:
			// 尺寸变了要把窗口区域重新裁一遍（只动窗口形状，不动玻璃本身）
			applyCornerRadius(widget_, params_);
			break;
		default:
			break;
		}
		return QObject::eventFilter(watched, event);
	}

private:
	QPointer<QWidget> widget_;
	WindowsGlassEffect::Params params_;
};

// 一个窗口只需要一个 filter，用对象名查已有实例
const char* const FILTER_NAME = "WindowsGlassEffectFilter";

static WindowsGlassEffectFilter* findFilter(const QWidget* widget)
{
	return widget ? widget->findChild<WindowsGlassEffectFilter*>(FILTER_NAME) : nullptr;
}

static void installFilter(QWidget* widget, const WindowsGlassEffect::Params& params)
{
	if (findFilter(widget))
		return;

	WindowsGlassEffectFilter* filter = new WindowsGlassEffectFilter(widget, params);
	filter->setObjectName(FILTER_NAME);
	widget->installEventFilter(filter);
}

static void uninstallFilter(QWidget* widget)
{
	WindowsGlassEffectFilter* filter = findFilter(widget);
	if (!filter)
		return;

	widget->removeEventFilter(filter);
	filter->deleteLater();
}

WindowsGlassEffect::Result WindowsGlassEffect::enable(QWidget* widget, const Params& params)
{
	if (!widget)
	{
		qWarning() << __FUNCTION__ << "widget is null";
		return ResultFailed;
	}

	// 只有独立顶层窗口才有自己的 HWND，窗口级毛玻璃才成立
	if (!widget->isWindow())
	{
		qWarning() << __FUNCTION__ << "not a top level window, use GlassBlurBackground instead";
		return ResultNotTopLevel;
	}

	if (!params.allowDwmBackdrop && !params.allowAcrylic && !params.allowBlurBehind)
	{
		qWarning() << __FUNCTION__ << "all methods are disabled by params";
		return ResultDisabled;
	}

	const HWND hwnd = hwndOf(widget);
	if (!hwnd)
	{
		qWarning() << __FUNCTION__ << "hwnd is null";
		return ResultFailed;
	}

	// 先挂上监听：窗口重新 show 或 HWND 被 Qt 重建时要重新贴一遍效果，
	// 所以即使这次全部失败也留着它，下次 show 再试
	installFilter(widget, params);

	// 圆角是窗口形状，跟玻璃方案成败无关，先裁好（失败时自绘底色也是圆的）
	applyCornerRadius(widget, params);

	const DWORD build = osVersion().buildNumber;

	// 1. Windows 11 22H2+：官方 DWM System Backdrop
	if (params.allowDwmBackdrop && build >= MinBuildForDwmBackdrop)
	{
		PFN_DwmSetWindowAttribute setAttr = resolveDwmSetWindowAttribute();
		if (setAttr)
		{
			// 无边框窗口没有标题栏，但这个属性也会影响系统 backdrop 的明暗基调
			BOOL dark = params.darkTitleBar ? TRUE : FALSE;
			setAttr(hwnd, DwmwaUseImmersiveDarkMode, &dark, sizeof(dark));

			const int backdrop = blurLevelToBackdrop(params.blurLevel);
			HRESULT hr = setAttr(hwnd, DwmwaSystemBackdropType, &backdrop, sizeof(backdrop));

			// 光看返回值不够，再读回来确认一次
			int readBack = 0;
			bool accepted = false;
			PFN_DwmGetWindowAttribute getAttr = resolveDwmGetWindowAttribute();
			if (SUCCEEDED(hr) && getAttr)
				accepted = SUCCEEDED(getAttr(hwnd, DwmwaSystemBackdropType, &readBack, sizeof(readBack)))
				&& readBack == backdrop;

			if (accepted)
			{
				// 客户区并入边框区域，backdrop 才能铺满整个窗口
				if (params.extendFrameIntoClientArea)
				{
					PFN_DwmExtendFrameIntoClientArea extend = resolveDwmExtendFrameIntoClientArea();
					if (extend)
					{
						MARGINS margins = { -1, -1, -1, -1 };
						HRESULT hrExtend = extend(hwnd, &margins);
						if (FAILED(hrExtend))
							qWarning() << __FUNCTION__ << "DwmExtendFrameIntoClientArea failed" << (quint32)hrExtend;
					}
				}

				widget->setProperty("WindowsGlassEffect.mode", (int)ModeDwmBackdrop);
				qDebug() << __FUNCTION__ << "ok, mode = dwm backdrop, type =" << backdrop << "build =" << build;
				return ResultOk;
			}

			qWarning() << __FUNCTION__ << "DwmSetWindowAttribute(SystemBackdropType) rejected"
				<< (quint32)hr << "readBack =" << readBack;
		}
	}

	// 2. Windows 10 1803+：SetWindowCompositionAttribute，Acrylic 还是 BlurBehind 由模糊档位决定
	if (params.allowAcrylic && build >= MinBuildForAcrylic && isAcrylicSupported())
	{
		const AccentState state = blurLevelToAccent(params.blurLevel);
		if (applyAccentPolicy(hwnd, state, params.accentFlags, params.tint))
		{
			const Mode mode = (state == AccentEnableBlurBehind) ? ModeBlurBehind : ModeAcrylic;
			widget->setProperty("WindowsGlassEffect.mode", (int)mode);
			qDebug() << __FUNCTION__ << "ok, mode =" << (int)mode << "accent =" << (int)state << "build =" << build;
			return ResultOk;
		}

		qWarning() << __FUNCTION__ << "SetWindowCompositionAttribute failed, accent =" << (int)state;
	}

	// 3. 更老的系统：BlurBehind
	if (params.allowBlurBehind && isBlurBehindSupported())
	{
		if (applyAccentPolicy(hwnd, AccentEnableBlurBehind, params.accentFlags, params.tint))
		{
			widget->setProperty("WindowsGlassEffect.mode", (int)ModeBlurBehind);
			qDebug() << __FUNCTION__ << "ok, mode = blur behind, build =" << build;
			return ResultOk;
		}

		qWarning() << __FUNCTION__ << "SetWindowCompositionAttribute(blur behind) failed";
		return ResultFailed;
	}

	qWarning() << __FUNCTION__ << "no glass method available, build =" << build;
	return ResultUnsupported;
}

void WindowsGlassEffect::disable(QWidget* widget)
{
	if (!widget)
		return;

	const HWND hwnd = hwndOf(widget);
	if (hwnd)
	{
		// 兼容性最好的“关掉”方式：把 accent 置为 disabled，再把边框区域收回去
		applyAccentPolicy(hwnd, AccentDisabled, 0, QColor());

		PFN_DwmExtendFrameIntoClientArea extend = resolveDwmExtendFrameIntoClientArea();
		if (extend)
		{
			MARGINS margins = { 0, 0, 0, 0 };
			extend(hwnd, &margins);
		}
	}

	widget->setProperty("WindowsGlassEffect.mode", (int)ModeNone);
	uninstallFilter(widget);
	qDebug() << __FUNCTION__ << "done";
}

WindowsGlassEffect::Mode WindowsGlassEffect::mode(const QWidget* widget)
{
	if (!widget)
		return ModeNone;

	const QVariant value = widget->property("WindowsGlassEffect.mode");
	return value.isValid() ? (Mode)value.toInt() : ModeNone;
}

QString WindowsGlassEffect::modeText(Mode mode)
{
	switch (mode)
	{
	case ModeDwmBackdrop:
		return QObject::tr("DWM System Backdrop");
	case ModeAcrylic:
		return QObject::tr("Acrylic");
	case ModeBlurBehind:
		return QObject::tr("Blur Behind");
	default:
		return QObject::tr("None");
	}
}

QString WindowsGlassEffect::resultText(Result result)
{
	switch (result)
	{
	case ResultOk:
		return QObject::tr("成功");
	case ResultDisabled:
		return QObject::tr("参数中关闭了所有方案");
	case ResultNotTopLevel:
		return QObject::tr("不是顶层窗口");
	case ResultUnsupported:
		return QObject::tr("当前系统不支持");
	default:
		return QObject::tr("调用失败");
	}
}

WindowsGlassEffect::Params WindowsGlassEffect::params(const QWidget* widget)
{
	WindowsGlassEffectFilter* filter = findFilter(widget);
	return filter ? filter->params() : Params();
}


bool WindowsGlassEffect::setTint(QWidget* widget, const QColor& tint)
{
	WindowsGlassEffectFilter* filter = findFilter(widget);
	if (!filter || !tint.isValid())
	{
		qWarning() << __FUNCTION__ << "glass is not enabled on this widget";
		return false;
	}

	Params p = filter->params();
	p.tint = tint;
	filter->setParams(p);

	// Win10 的着色是系统层做的，得重新贴一次；Win11 backdrop 不吃系统 tint，重绘就够
	const Mode current = mode(widget);
	if (current == ModeAcrylic || current == ModeBlurBehind)
	{
		const AccentState state = (current == ModeBlurBehind)
			? AccentEnableBlurBehind
			: blurLevelToAccent(p.blurLevel);
		applyAccentPolicy(hwndOf(widget), state, p.accentFlags, p.tint);
	}

	widget->update();
	qDebug() << __FUNCTION__ << "tint =" << tint << "mode =" << (int)current;
	return true;
}

bool WindowsGlassEffect::setBlurLevel(QWidget* widget, BlurLevel level)
{
	WindowsGlassEffectFilter* filter = findFilter(widget);
	if (!filter)
	{
		qWarning() << __FUNCTION__ << "glass is not enabled on this widget";
		return false;
	}

	Params p = filter->params();
	p.blurLevel = level;
	filter->setParams(p);

	const Result result = enable(widget, p);
	widget->update();
	qDebug() << __FUNCTION__ << "blur level =" << (int)level << "result =" << (int)result;
	return result == ResultOk;
}

bool WindowsGlassEffect::setCornerRadius(QWidget* widget, int radius)
{
	WindowsGlassEffectFilter* filter = findFilter(widget);
	if (!filter || !widget)
	{
		qWarning() << __FUNCTION__ << "glass is not enabled on this widget";
		return false;
	}

	Params p = filter->params();
	p.cornerRadius = qMax(0, radius);
	filter->setParams(p);

	applyCornerRadius(widget, p);
	widget->update();   // 自绘底色也要跟着改成圆角
	qDebug() << __FUNCTION__ << "corner radius =" << p.cornerRadius;
	return true;
}

bool WindowsGlassEffect::isDwmBackdropSupported()
{
	return resolveDwmSetWindowAttribute() != nullptr
		&& osVersion().buildNumber >= MinBuildForDwmBackdrop;
}

bool WindowsGlassEffect::isDwmCornersSupported()
{
	return osVersion().buildNumber >= MinBuildForDwmCorners;
}

bool WindowsGlassEffect::isDwmCornersActive(const QWidget* widget)
{
	if (!widget)
		return false;

	const QVariant value = widget->property("WindowsGlassEffect.dwmCorners");
	return value.isValid() && value.toBool();
}

bool WindowsGlassEffect::isAcrylicSupported()
{
	return resolveSetWindowCompositionAttribute() != nullptr
		&& osVersion().buildNumber >= MinBuildForAcrylic;
}

bool WindowsGlassEffect::isBlurBehindSupported()
{
	return resolveSetWindowCompositionAttribute() != nullptr;
}

#else  // 非 Windows 平台：全部安全返回

WindowsGlassEffect::Result WindowsGlassEffect::enable(QWidget* widget, const Params& params)
{
	Q_UNUSED(widget)
	Q_UNUSED(params)
	qWarning() << __FUNCTION__ << "only supported on windows";
	return ResultUnsupported;
}

void WindowsGlassEffect::disable(QWidget* widget)
{
	Q_UNUSED(widget)
}

WindowsGlassEffect::Mode WindowsGlassEffect::mode(const QWidget* widget)
{
	Q_UNUSED(widget)
	return ModeNone;
}

WindowsGlassEffect::Params WindowsGlassEffect::params(const QWidget* widget)
{
	Q_UNUSED(widget)
	return Params();
}

bool WindowsGlassEffect::setTint(QWidget* widget, const QColor& tint)
{
	Q_UNUSED(widget)
	Q_UNUSED(tint)
	return false;
}

bool WindowsGlassEffect::setBlurLevel(QWidget* widget, BlurLevel level)
{
	Q_UNUSED(widget)
	Q_UNUSED(level)
	return false;
}

bool WindowsGlassEffect::setCornerRadius(QWidget* widget, int radius)
{
	Q_UNUSED(widget)
	Q_UNUSED(radius)
	return false;
}

QString WindowsGlassEffect::modeText(Mode mode)
{
	Q_UNUSED(mode)
	return tr("None");
}

QString WindowsGlassEffect::resultText(Result result)
{
	Q_UNUSED(result)
	return tr("当前系统不支持");
}

bool WindowsGlassEffect::isDwmBackdropSupported()
{
	return false;
}

bool WindowsGlassEffect::isDwmCornersSupported()
{
	return false;
}

bool WindowsGlassEffect::isDwmCornersActive(const QWidget* widget)
{
	Q_UNUSED(widget)
	return false;
}

bool WindowsGlassEffect::isAcrylicSupported()
{
	return false;
}

bool WindowsGlassEffect::isBlurBehindSupported()
{
	return false;
}

#endif
