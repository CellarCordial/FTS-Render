// #ifndef RENDER_PASS_GBUFFER_H
// #define RENDER_PASS_GBUFFER_H

// #include "../../../RenderGraph/include/RenderGraph.h"
// #include "../../../Core/include/ComCli.h"
// #include "../../../Gui/include/GuiPanel.h"

// namespace FTS
// {
// 	namespace Constant
// 	{
// 		struct PassConstant
// 		{

// 		};
// 	}

// 	class FPass : public IRenderPass
// 	{
// 	public:
// 		FPass() { Type = ERenderPassType::Graphics; }

// 		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
// 		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

// 	private:
// 		BOOL m_bResourceWrited = false;
// 		Constant::PassConstant m_PassConstant;

// 		TComPtr<IBuffer> m_pBuffer;
// 		TComPtr<ITexture> m_pTexture;
		
// 		TComPtr<IBindingLayout> m_pBindingLayout;
// 		TComPtr<IInputLayout> m_pInputLayout;

// 		TComPtr<IShader> m_pVS;
// 		TComPtr<IShader> m_pPS;

// 		TComPtr<IFrameBuffer> m_pFrameBuffer;
// 		TComPtr<IGraphicsPipeline> m_pPipeline;

// 		TComPtr<IBindingSet> m_pBindingSet;
// 		FGraphicsState m_GraphicsState;
// 	};

// }

// #endif