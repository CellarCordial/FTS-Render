#include "GlobalRender.h"
#include <d3d12.h>
#include "Shader/ShaderCompiler.h"
#include "Scene/include/Scene.h"
#include "Parallel/include/Parallel.h"
#include <glfw3.h>
#include <wrl.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>

namespace FTS
{
	FGlobalRender::~FGlobalRender()
	{
		ShaderCompile::Destroy();
		Parallel::Destroy();
		glfwDestroyWindow(m_pWindow);
		glfwTerminate();
	}

	BOOL FGlobalRender::Init()
	{
		ReturnIfFalse(glfwInit());
		m_pWindow = glfwCreateWindow(CLIENT_WIDTH, CLIENT_HEIGHT, "FTS-Render", nullptr, nullptr);
		if (!m_pWindow)
		{
			glfwTerminate();
			return false;
		}
		ShaderCompile::Initialize();
		Parallel::Initialize();

		// Entity System.
		{
			m_World.RegisterSystem(new FSceneSystem());

			FEntity* pEntity = m_World.CreateEntity();
			pCamera = pEntity->Assign<FCamera>(m_pWindow);
		}

		ReturnIfFalse(D3D12Init());
		ReturnIfFalse(CreateSamplers());
		m_AtmosphereDebugRender.Setup(m_pRenderGraph.Get());
		//m_SdfDebugRender.Setup(m_pRenderGraph.Get());

		m_GuiPass.Init(m_pWindow, m_pDevice.Get());
		m_pRenderGraph->AddPass(&m_GuiPass);

		m_AtmosphereDebugRender.GetLastPass()->Precede(&m_GuiPass);
		//m_SdfDebugRender.GetLastPass()->Precede(&m_GuiPass);



		return true;
	}

	BOOL FGlobalRender::Run()
	{
		ReturnIfFalse(m_pRenderGraph->Compile());
		while (!glfwWindowShouldClose(m_pWindow))
		{
			glfwPollEvents();
			m_World.Tick(m_Timer.Tick());
			ReturnIfFalse(m_pRenderGraph->Execute());
		}
		return true;
	}

	BOOL FGlobalRender::D3D12Init()
	{
#ifdef DEBUG
		// Enable the D3D12 debug layer.
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> pDebugController;
			D3D12GetDebugInterface(IID_PPV_ARGS(pDebugController.GetAddressOf()));
			pDebugController->EnableDebugLayer();
		}
#endif
		ID3D12Device* pD3D12Device;
		ReturnIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pD3D12Device)));

		ID3D12CommandQueue* pD3D12GraphicsCmdQueue;
		ID3D12CommandQueue* pD3D12ComputeCmdQueue;
		D3D12_COMMAND_QUEUE_DESC D3D12QueueDesc{};
		D3D12QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ReturnIfFailed(pD3D12Device->CreateCommandQueue(&D3D12QueueDesc, IID_PPV_ARGS(&pD3D12GraphicsCmdQueue)));
		D3D12QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ReturnIfFailed(pD3D12Device->CreateCommandQueue(&D3D12QueueDesc, IID_PPV_ARGS(&pD3D12ComputeCmdQueue)));

		TComPtr<IDXGIFactory> pDxgiFactory;
		ReturnIfFailed(CreateDXGIFactory(IID_PPV_ARGS(pDxgiFactory.GetAddressOf())));

		DXGI_SWAP_CHAIN_DESC SwapChainDesc;
		SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SwapChainDesc.BufferDesc.Width = CLIENT_WIDTH;
		SwapChainDesc.BufferDesc.Height = CLIENT_HEIGHT;
		SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
		SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		SwapChainDesc.BufferCount = NUM_FRAMES_IN_FLIGHT;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.OutputWindow = glfwGetWin32Window(m_pWindow);
		SwapChainDesc.SampleDesc = { 1, 0 };
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Windowed = true;
		SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ReturnIfFailed(pDxgiFactory->CreateSwapChain(pD3D12GraphicsCmdQueue, &SwapChainDesc, m_pSwapChain.GetAddressOf()));

		ID3D12Resource* pSwapChainBuffers[NUM_FRAMES_IN_FLIGHT];
		for (UINT32 ix = 0; ix < NUM_FRAMES_IN_FLIGHT; ++ix)
		{
			m_pSwapChain->GetBuffer(ix, IID_PPV_ARGS(&pSwapChainBuffers[ix]));
		}


		FDX12DeviceDesc DeviceDesc;
		DeviceDesc.pD3D12Device = pD3D12Device;
		DeviceDesc.pD3D12GraphicsCommandQueue = pD3D12GraphicsCmdQueue;
		DeviceDesc.pD3D12ComputeCommandQueue = pD3D12ComputeCmdQueue;
		ReturnIfFalse(CreateDevice(DeviceDesc, IID_IDevice, PPV_ARG(m_pDevice.GetAddressOf())));

		ReturnIfFalse(CreateRenderGraph(
			m_pDevice.Get(),
			[this]() { m_pSwapChain->Present(0, 0); m_dwCurrBackBufferIndex = (m_dwCurrBackBufferIndex + 1) % NUM_FRAMES_IN_FLIGHT; },
			&m_World,
			IID_IRenderGraph,
			PPV_ARG(m_pRenderGraph.GetAddressOf())
		));

		IRenderResourceCache* pCache = m_pRenderGraph->GetResourceCache();
		pCache->CollectConstants("BackBufferIndex", &m_dwCurrBackBufferIndex);

		FTextureDesc TextureDesc;
		TextureDesc.dwWidth = CLIENT_WIDTH;
		TextureDesc.dwHeight = CLIENT_HEIGHT;
		TextureDesc.InitialState = EResourceStates::Present;
		TextureDesc.Format = EFormat::RGBA8_UNORM;
		TextureDesc.bUseClearValue = true;
		TextureDesc.ClearValue = FColor(0.0f, 0.0f, 0.0f, 1.0f);

		for (UINT32 ix = 0; ix < NUM_FRAMES_IN_FLIGHT; ++ix)
		{
			TextureDesc.strName = "BackBuffer" + std::to_string(ix);
			ReturnIfFalse(m_pDevice->CreateTextureForNative(pSwapChainBuffers[ix], TextureDesc, IID_ITexture, PPV_ARG(m_pBackBuffers[ix].GetAddressOf())));
			pCache->Collect(m_pBackBuffers[ix].Get());
		}

		ReturnIfFalse(m_pDevice->CreateTexture(
			FTextureDesc::CreateRenderTarget(CLIENT_WIDTH, CLIENT_HEIGHT, EFormat::RGBA8_UNORM, "FinalTexture"),
			IID_ITexture,
			PPV_ARG(m_pFinalTexture.GetAddressOf())
		));
		ReturnIfFalse(pCache->Collect(m_pFinalTexture.Get()));



		return true;
	}

	BOOL FGlobalRender::VulkanInit()
	{

		return false;
	}

	BOOL FGlobalRender::CreateSamplers()
	{
		ISampler* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;

		FSamplerDesc SamplerDesc;
		SamplerDesc.strName = "LinearWarpSampler";
		ReturnIfFalse(m_pDevice->CreateSampler(SamplerDesc, IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
		SamplerDesc.SetFilter(false);
		SamplerDesc.strName = "PointWrapSampler";
		ReturnIfFalse(m_pDevice->CreateSampler(SamplerDesc, IID_ISampler, PPV_ARG(&pPointWrapSampler)));
		SamplerDesc.SetAddressMode(ESamplerAddressMode::Clamp);
		SamplerDesc.strName = "PointClampSampler";
		ReturnIfFalse(m_pDevice->CreateSampler(SamplerDesc, IID_ISampler, PPV_ARG(&pPointClampSampler)));
		SamplerDesc.SetFilter(true);
		SamplerDesc.strName = "LinearClampSampler";
		ReturnIfFalse(m_pDevice->CreateSampler(SamplerDesc, IID_ISampler, PPV_ARG(&pLinearClampSampler)));

		IRenderResourceCache* pCache = m_pRenderGraph->GetResourceCache();
		ReturnIfFalse(pCache->Collect(pLinearClampSampler));
		ReturnIfFalse(pCache->Collect(pPointClampSampler));
		ReturnIfFalse(pCache->Collect(pLinearWarpSampler));
		ReturnIfFalse(pCache->Collect(pPointWrapSampler));

		return true;
	}
}