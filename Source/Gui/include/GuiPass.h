#ifndef RENDER_PASS_GUI_H
#define RENDER_PASS_GUI_H

#include "../../RenderGraph/include/RenderGraph.h"
#include "../../Core/include/ComCli.h"
#include "../../Gui/include/GuiPanel.h"

namespace FTS
{
	namespace Constant
	{
		struct GuiPassConstant
		{

		};
	}

	class FGuiPass : public IRenderPass
	{
	public:
		FGuiPass() { Type = ERenderPassType::Graphics; }
		~FGuiPass() { Gui::Destroy(); }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache);
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache);

		void Init(GLFWwindow* pWindow, IDevice* pDevice) { Gui::Initialize(pWindow, pDevice); }

	private:
		TComPtr<IFrameBuffer> m_pFrameBuffer;
		ITexture* m_pFinalTexture;
	};

}

#endif