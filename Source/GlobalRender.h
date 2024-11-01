#ifndef GLOBAL_RENDER_H
#define GLOBAL_RENDER_H


#include <dxgi.h>
#include "Core/include/Timer.h"
#include "Core/include/Entity.h"
#include "Core/include/ComCli.h"
#include "Gui/include/GuiPass.h"
#include "RenderGraph/include/RenderGraph.h"
#include "RenderPass/Atomsphere/include/AtmosphereRender.h"
#include "Scene/include/Camera.h"

namespace FTS
{
	class FGlobalRender
	{
	public:
		~FGlobalRender();

		BOOL Init();
		BOOL Run();

	private:
		BOOL D3D12Init();
		BOOL VulkanInit();

		BOOL CreateSamplers();
		BOOL CreateCamera();

	private:
		FTimer m_Timer;
		FWorld m_World;
		GLFWwindow* m_pWindow = nullptr;
		FCamera* pCamera = nullptr;

		// D3D12.
		TComPtr<IDXGISwapChain> m_pSwapChain;
		UINT32 m_dwCurrBackBufferIndex = 0;

		// ±£³ÖÎö¹¹Ë³Ðò.
		TComPtr<IDevice> m_pDevice;
		TComPtr<ITexture> m_pBackBuffers[NUM_FRAMES_IN_FLIGHT];
		TComPtr<ITexture> m_pFinalTexture;

		FGuiPass m_GuiPass;
		FAtmosphereRender m_AtmosphereRender;
		TComPtr<IRenderGraph> m_pRenderGraph;
	};
}





#endif