#include "DScreenCapture.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QScreen>
#include <QThread>

#include <activation.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <roapi.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

namespace Capture = ABI::Windows::Graphics::Capture;
namespace DirectX = ABI::Windows::Graphics::DirectX;
namespace Direct3D11 = ABI::Windows::Graphics::DirectX::Direct3D11;
namespace Foundation = ABI::Windows::Foundation;


namespace
{
	const static int CAPTURE_TIMEOUT_MS = 2000;  // 等首帧的超时时间
	const static int FRAME_POOL_BUFFER_COUNT = 2;
	const static int FRAME_POLL_INTERVAL_MS = 5;

	// HRESULT 打日志用
	inline QString hrText(HRESULT hr)
	{
		return QString("0x%1").arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
	}

	// tone map：源是 scRGB 线性的 FP16（scRGB 里 1.0 = 80 nits），目标是 8bit sRGB。
	// 捕获到的是显示器最终输出的内容，里面的 SDR 白对应 scRGB 的 (SDR 白电平 / 80nits)，
	// 先按这个电平归一化到 "SDR 白 = 1.0"，再编码成 sRGB，得到的就是人眼看到的那张 SDR 图。
	// 超过 SDR 白的高光会贴到白色，但不会像直接 8bit 抓屏那样把整个画面拉亮。
	const static char* TONE_MAP_SHADER = R"(
struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
	VSOutput output;
	float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
	output.uv = uv;
	output.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return output;
}

cbuffer ToneMapParams : register(b0)
{
	float sdrWhiteLevel;  // SDR 白在 scRGB 中的值
	float3 padding;
};

Texture2D frameTexture : register(t0);
SamplerState frameSampler : register(s0);

float3 linearToSrgb(float3 c)
{
	return (c <= 0.0031308f) ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

float4 PSMain(VSOutput input) : SV_Target
{
	float3 color = frameTexture.Sample(frameSampler, input.uv).rgb;
	color = saturate(color / sdrWhiteLevel);
	return float4(linearToSrgb(color), 1.0f);
}
)";

	// 进程内复用的 D3D11 设备与 tone map 用到的管线资源
	struct SharedDevice
	{
		ComPtr<ID3D11Device> device_;
		ComPtr<ID3D11DeviceContext> context_;
		ComPtr<ID3D11VertexShader> vertexShader_;
		ComPtr<ID3D11PixelShader> toneMapShader_;
		ComPtr<ID3D11SamplerState> sampler_;
		ComPtr<ID3D11Buffer> toneMapParams_;
		ComPtr<ID3D11Texture2D> sdrTexture_;
		ComPtr<ID3D11RenderTargetView> sdrRtv_;
		ComPtr<ID3D11Texture2D> sdrStaging_;
		QSize sdrSize_;
		bool valid_ = false;
	};

	SharedDevice& sharedDevice()
	{
		static SharedDevice device;
		return device;
	}

	bool ensureWinrtInitialized()
	{
		static bool initialized = false;
		if (initialized)
			return true;

		// Qt 的 GUI 线程通常是 STA，这里只是确保 WinRT 已经初始化
		HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
		if (FAILED(hr) && RPC_E_CHANGED_MODE != hr)
		{
			qWarning() << __FUNCTION__ << "RoInitialize failed:" << hrText(hr);
			return false;
		}

		initialized = true;
		return true;
	}

	bool createShaders(SharedDevice& shared)
	{
		UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

		ComPtr<ID3DBlob> vsBlob;
		ComPtr<ID3DBlob> psBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DCompile(TONE_MAP_SHADER, strlen(TONE_MAP_SHADER), "DScreenCapture"
			, nullptr, nullptr, "VSMain", "vs_4_0", compileFlags, 0
			, vsBlob.GetAddressOf(), errorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "compile vertex shader failed:";
			return false;
		}

		errorBlob.Reset();
		hr = D3DCompile(TONE_MAP_SHADER, strlen(TONE_MAP_SHADER), "DScreenCapture"
			, nullptr, nullptr, "PSMain", "ps_4_0", compileFlags, 0
			, psBlob.GetAddressOf(), errorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "compile pixel shader failed:";
			return false;
		}

		hr = shared.device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()
			, nullptr, shared.vertexShader_.GetAddressOf());
		if (FAILED(hr))
			return false;

		hr = shared.device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize()
			, nullptr, shared.toneMapShader_.GetAddressOf());
		if (FAILED(hr))
			return false;

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = shared.device_->CreateSamplerState(&samplerDesc, shared.sampler_.GetAddressOf());
		if (FAILED(hr))
			return false;

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = 16;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = shared.device_->CreateBuffer(&bufferDesc, nullptr, shared.toneMapParams_.GetAddressOf());

		return SUCCEEDED(hr);
	}

	// 查询显示器当前的 SDR 白电平，返回它在 scRGB 空间中对应的值（scRGB 里 1.0 = 80nits）。
	// 查询不到时按 80nits 处理。
	float querySdrWhiteLevel(HMONITOR monitor)
	{
		const float defaultLevel = 1.0f;

		MONITORINFOEXW monitorInfo = {};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (!GetMonitorInfoW(monitor, &monitorInfo))
			return defaultLevel;

		UINT32 pathCount = 0;
		UINT32 modeCount = 0;
		if (ERROR_SUCCESS != GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount))
			return defaultLevel;

		std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
		if (ERROR_SUCCESS != QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data()
			, &modeCount, modes.data(), nullptr))
			return defaultLevel;

		for (UINT32 i = 0; i < pathCount; ++i)
		{
			DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
			sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			sourceName.header.size = sizeof(sourceName);
			sourceName.header.adapterId = paths[i].sourceInfo.adapterId;
			sourceName.header.id = paths[i].sourceInfo.id;
			if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&sourceName.header))
				continue;
			if (0 != wcscmp(sourceName.viewGdiDeviceName, monitorInfo.szDevice))
				continue;

			DISPLAYCONFIG_SDR_WHITE_LEVEL whiteLevel = {};
			whiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
			whiteLevel.header.size = sizeof(whiteLevel);
			whiteLevel.header.adapterId = paths[i].targetInfo.adapterId;
			whiteLevel.header.id = paths[i].targetInfo.id;
			if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&whiteLevel.header))
				return defaultLevel;
			if (0 == whiteLevel.SDRWhiteLevel)
				return defaultLevel;

			// SDRWhiteLevel 的单位是 80nits 的千分之一
			return static_cast<float>(whiteLevel.SDRWhiteLevel) / 1000.0f;
		}

		return defaultLevel;
	}

	bool ensureSharedDevice()
	{
		SharedDevice& shared = sharedDevice();
		if (shared.valid_)
			return true;
		if (!ensureWinrtInitialized())
			return false;

		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

		HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags
			, levels, _countof(levels), D3D11_SDK_VERSION
			, shared.device_.GetAddressOf(), &level, shared.context_.GetAddressOf());
		if (E_INVALIDARG == hr)  // 系统不支持 11_1 时降级重试
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags
				, &levels[1], 1, D3D11_SDK_VERSION
				, shared.device_.GetAddressOf(), &level, shared.context_.GetAddressOf());
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "D3D11CreateDevice failed:" << hrText(hr);
			return false;
		}

		if (!createShaders(shared))
		{
			qWarning() << __FUNCTION__ << "create shaders failed";
			return false;
		}

		shared.valid_ = true;
		qDebug() << __FUNCTION__ << "d3d11 device ready";
		return true;
	}

	// 抓取会话，析构时按 session -> framePool -> item 的顺序释放
	struct CaptureSession
	{
		ComPtr<Capture::IGraphicsCaptureItem> item_;
		ComPtr<Capture::IDirect3D11CaptureFramePool> framePool_;
		ComPtr<Capture::IGraphicsCaptureSession> session_;

		~CaptureSession()
		{
			if (session_)
			{
				ComPtr<Foundation::IClosable> closable;
				if (SUCCEEDED(session_.As(&closable)))
					closable->Close();
			}
			session_.Reset();
			framePool_.Reset();
			item_.Reset();
		}
	};

	bool createWinrtDevice(ComPtr<Direct3D11::IDirect3DDevice>& outDevice)
	{
		ComPtr<IDXGIDevice> dxgiDevice;
		HRESULT hr = sharedDevice().device_.As(&dxgiDevice);
		if (FAILED(hr))
			return false;

		ComPtr<IInspectable> inspectableDevice;
		hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectableDevice.GetAddressOf());
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "CreateDirect3D11DeviceFromDXGIDevice failed:" << hrText(hr);
			return false;
		}

		hr = inspectableDevice.As(&outDevice);
		if (FAILED(hr))
			qWarning() << __FUNCTION__ << "query IDirect3DDevice failed:" << hrText(hr);

		return SUCCEEDED(hr);
	}

	// 等一帧过来，拿到最新的那张 FP16 纹理
	bool waitForFrame(Capture::IDirect3D11CaptureFramePool* framePool, ComPtr<ID3D11Texture2D>& outTexture)
	{
		QElapsedTimer timer;
		timer.start();

		while (timer.elapsed() < CAPTURE_TIMEOUT_MS)
		{
			ComPtr<Capture::IDirect3D11CaptureFrame> frame;
			if (SUCCEEDED(framePool->TryGetNextFrame(frame.GetAddressOf())) && frame)
			{
				ComPtr<Direct3D11::IDirect3DSurface> surface;
				if (SUCCEEDED(frame->get_Surface(surface.GetAddressOf())) && surface)
				{
					ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> access;
					if (SUCCEEDED(surface.As(&access)))
					{
						ComPtr<ID3D11Texture2D> texture;
						if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(texture.GetAddressOf()))) && texture)
						{
							outTexture = texture;
							return true;
						}
					}
				}
			}
			QThread::msleep(FRAME_POLL_INTERVAL_MS);
		}

		return false;
	}

	bool ensureSdrTarget(const QSize& size)
	{
		SharedDevice& shared = sharedDevice();
		if (shared.sdrTexture_ && shared.sdrRtv_ && shared.sdrStaging_ && shared.sdrSize_ == size)
			return true;

		shared.sdrTexture_.Reset();
		shared.sdrRtv_.Reset();
		shared.sdrStaging_.Reset();
		shared.sdrSize_ = QSize();

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = static_cast<UINT>(size.width());
		desc.Height = static_cast<UINT>(size.height());
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET;

		HRESULT hr = shared.device_->CreateTexture2D(&desc, nullptr, shared.sdrTexture_.GetAddressOf());
		if (FAILED(hr))
			return false;

		hr = shared.device_->CreateRenderTargetView(shared.sdrTexture_.Get(), nullptr
			, shared.sdrRtv_.GetAddressOf());
		if (FAILED(hr))
			return false;

		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		hr = shared.device_->CreateTexture2D(&desc, nullptr, shared.sdrStaging_.GetAddressOf());
		if (FAILED(hr))
			return false;

		shared.sdrSize_ = size;
		return true;
	}

	// 把 FP16 的一帧在 GPU 上转成 BGRA8，再读回 QImage
	QImage toneMapToImage(ID3D11Texture2D* texture, float sdrWhiteLevel)
	{
		if (nullptr == texture)
			return QImage();

		D3D11_TEXTURE2D_DESC desc = {};
		texture->GetDesc(&desc);
		QSize size(static_cast<int>(desc.Width), static_cast<int>(desc.Height));
		if (size.isEmpty())
			return QImage();
		if (!ensureSdrTarget(size))
		{
			qWarning() << __FUNCTION__ << "create sdr target failed";
			return QImage();
		}

		SharedDevice& shared = sharedDevice();
		ComPtr<ID3D11ShaderResourceView> frameView;
		HRESULT hr = shared.device_->CreateShaderResourceView(texture, nullptr, frameView.GetAddressOf());
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "CreateShaderResourceView failed:" << hrText(hr);
			return QImage();
		}

		ID3D11DeviceContext* context = shared.context_.Get();

		D3D11_MAPPED_SUBRESOURCE params = {};
		if (FAILED(context->Map(shared.toneMapParams_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &params)))
		{
			qWarning() << __FUNCTION__ << "map tone map params failed";
			return QImage();
		}
		static_cast<float*>(params.pData)[0] = sdrWhiteLevel;
		context->Unmap(shared.toneMapParams_.Get(), 0);
		context->PSSetConstantBuffers(0, 1, shared.toneMapParams_.GetAddressOf());

		context->OMSetRenderTargets(1, shared.sdrRtv_.GetAddressOf(), nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = static_cast<FLOAT>(size.width());
		viewport.Height = static_cast<FLOAT>(size.height());
		viewport.MaxDepth = 1.0f;
		context->RSSetViewports(1, &viewport);

		ID3D11ShaderResourceView* views[] = { frameView.Get() };
		context->PSSetShaderResources(0, 1, views);
		context->PSSetSamplers(0, 1, shared.sampler_.GetAddressOf());
		context->PSSetShader(shared.toneMapShader_.Get(), nullptr, 0);
		context->VSSetShader(shared.vertexShader_.Get(), nullptr, 0);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->Draw(3, 0);

		context->CopyResource(shared.sdrStaging_.Get(), shared.sdrTexture_.Get());

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		hr = context->Map(shared.sdrStaging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr))
		{
			qWarning() << __FUNCTION__ << "Map staging texture failed:" << hrText(hr);
			return QImage();
		}

		QImage image(size, QImage::Format_ARGB32);
		if (!image.isNull())
		{
			const int bytesPerLine = size.width() * 4;
			const unsigned char* src = static_cast<const unsigned char*>(mapped.pData);
			for (int y = 0; y < size.height(); ++y)
				memcpy(image.scanLine(y), src + static_cast<size_t>(y) * mapped.RowPitch, bytesPerLine);
		}
		context->Unmap(shared.sdrStaging_.Get(), 0);

		// 释放对源纹理的引用，不然下一次抓帧时它没法被覆写
		ID3D11ShaderResourceView* nullViews[] = { nullptr };
		context->PSSetShaderResources(0, 1, nullViews);

		return image;
	}

	// 该输出是否处于 HDR / Advanced Color 状态
	bool isHdrOutput(HMONITOR monitor)
	{
		ComPtr<IDXGIFactory1> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
			return false;

		for (UINT i = 0; ; ++i)
		{
			ComPtr<IDXGIAdapter1> adapter;
			if (FAILED(factory->EnumAdapters1(i, adapter.GetAddressOf())) || nullptr == adapter)
				break;

			for (UINT j = 0; ; ++j)
			{
				ComPtr<IDXGIOutput> output;
				if (FAILED(adapter->EnumOutputs(j, output.GetAddressOf())) || nullptr == output)
					break;

				DXGI_OUTPUT_DESC desc = {};
				if (FAILED(output->GetDesc(&desc)) || desc.Monitor != monitor)
					continue;

				ComPtr<IDXGIOutput6> output6;
				if (FAILED(output.As(&output6)))
					return false;

				DXGI_OUTPUT_DESC1 desc1 = {};
				if (FAILED(output6->GetDesc1(&desc1)))
					return false;

				return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 == desc1.ColorSpace;
			}
		}

		return false;
	}

	HMONITOR monitorOfScreen(QScreen* screen)
	{
		if (nullptr == screen)
			return nullptr;

		QPoint center = screen->geometry().center();
		POINT point = { center.x(), center.y() };
		return MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
	}
}


bool DScreenCapture::isSupported()
{
	if (!ensureWinrtInitialized())
		return false;

	ComPtr<IActivationFactory> factory;
	HRESULT hr = RoGetActivationFactory(
		HStringReference(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession).Get()
		, IID_PPV_ARGS(factory.GetAddressOf()));
	if (FAILED(hr))
		return false;

	ComPtr<Capture::IGraphicsCaptureSessionStatics> statics;
	if (FAILED(factory.As(&statics)))
		return false;

	boolean supported = FALSE;
	if (FAILED(statics->IsSupported(&supported)))
		return false;

	return supported != FALSE;
}

QImage DScreenCapture::grabMonitor(HMONITOR monitor)
{
	if (nullptr == monitor)
		return QImage();

	// 只处理 HDR 输出：SDR 输出下 QScreen::grabWindow 本身就是准确的，
	// 绕 WGC 反而要额外处理 DWM 的 SDR 白电平缩放
	if (!isHdrOutput(monitor))
		return QImage();

	if (!ensureSharedDevice())
		return QImage();
	if (!isSupported())
	{
		qWarning() << __FUNCTION__ << "Windows Graphics Capture is not supported";
		return QImage();
	}

	qDebug() << __FUNCTION__ << "capture HDR output via Windows Graphics Capture";

	ComPtr<IGraphicsCaptureItemInterop> interop;
	HRESULT hr = RoGetActivationFactory(
		HStringReference(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem).Get()
		, IID_PPV_ARGS(interop.GetAddressOf()));
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "get GraphicsCaptureItem interop failed:" << hrText(hr);
		return QImage();
	}

	CaptureSession capture;
	hr = interop->CreateForMonitor(monitor, IID_PPV_ARGS(capture.item_.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "CreateForMonitor failed:" << hrText(hr);
		return QImage();
	}

	ComPtr<Direct3D11::IDirect3DDevice> winrtDevice;
	if (!createWinrtDevice(winrtDevice))
		return QImage();

	ComPtr<IActivationFactory> poolFactory;
	hr = RoGetActivationFactory(
		HStringReference(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool).Get()
		, IID_PPV_ARGS(poolFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "get frame pool factory failed:" << hrText(hr);
		return QImage();
	}

	ComPtr<Capture::IDirect3D11CaptureFramePoolStatics2> poolStatics;
	if (FAILED(poolFactory.As(&poolStatics)))
	{
		qWarning() << __FUNCTION__ << "query IDirect3D11CaptureFramePoolStatics2 failed";
		return QImage();
	}

	ABI::Windows::Graphics::SizeInt32 contentSize = {};
	hr = capture.item_->get_Size(&contentSize);
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "get capture item size failed:" << hrText(hr);
		return QImage();
	}

	// 必须用 FP16，这样 HDR 内容才不会被裁成过曝的 8bit
	hr = poolStatics->CreateFreeThreaded(winrtDevice.Get()
		, DirectX::DirectXPixelFormat_R16G16B16A16Float
		, FRAME_POOL_BUFFER_COUNT
		, contentSize
		, capture.framePool_.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "create frame pool failed:" << hrText(hr);
		return QImage();
	}

	hr = capture.framePool_->CreateCaptureSession(capture.item_.Get()
		, capture.session_.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "CreateCaptureSession failed:" << hrText(hr);
		return QImage();
	}

	// 鼠标由截图工具自己画，这里不重复抓光标
	ComPtr<Capture::IGraphicsCaptureSession2> session2;
	if (SUCCEEDED(capture.session_.As(&session2)))
		session2->put_IsCursorCaptureEnabled(FALSE);

	hr = capture.session_->StartCapture();
	if (FAILED(hr))
	{
		qWarning() << __FUNCTION__ << "StartCapture failed:" << hrText(hr);
		return QImage();
	}

	ComPtr<ID3D11Texture2D> frameTexture;
	if (!waitForFrame(capture.framePool_.Get(), frameTexture))
	{
		qWarning() << __FUNCTION__ << "wait first frame timeout";
		return QImage();
	}

	// scRGB 里 1.0 = 80nits，先把 SDR 白归一化成 1.0，再编码成 sRGB
	float sdrWhiteLevel = querySdrWhiteLevel(monitor);
	QImage image = toneMapToImage(frameTexture.Get(), sdrWhiteLevel);
	if (image.isNull())
		qWarning() << __FUNCTION__ << "tone map failed";
	else
		qDebug() << __FUNCTION__ << "captured" << image.size() << "sdrWhiteLevel:" << sdrWhiteLevel;

	return image;
}

QPixmap DScreenCapture::grabScreen(QScreen* screen)
{
	if (nullptr == screen)
		return QPixmap();

	HMONITOR monitor = monitorOfScreen(screen);
	if (isHdrOutput(monitor))
	{
		QImage image = grabMonitor(monitor);
		if (!image.isNull())
			return QPixmap::fromImage(image);

		qWarning() << __FUNCTION__ << "HDR capture failed, fallback to QScreen::grabWindow";
	}

	// SDR 输出下 Qt 自带的截屏就是准确的
	return screen->grabWindow(0);
}
