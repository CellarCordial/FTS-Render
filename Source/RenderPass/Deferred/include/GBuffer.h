#ifndef RENDER_PASS_GBUFFER_H
#define RENDER_PASS_GBUFFER_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Matrix.h"

namespace FTS
{
	namespace Constant
	{
		struct GBufferPassConstant
		{
            FMatrix4x4 ViewProj;

            FMatrix4x4 View;
            FMatrix4x4 PrevView;
		};
	}

	class FGBufferPass : public IRenderPass
	{
	public:
		FGBufferPass() { Type = ERenderPassType::Graphics; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

	private:
		BOOL m_bResourceWrited = false;
		Constant::GBufferPassConstant m_PassConstant;

		TComPtr<ITexture> m_pPosDepthTexture;
		TComPtr<ITexture> m_pNormalTexture;
		TComPtr<ITexture> m_pPBRTexture;
		TComPtr<ITexture> m_pEmissiveTexture;
		TComPtr<ITexture> m_pBaseColorTexture;
		TComPtr<ITexture> m_pVelocityVTexture;

		TComPtr<IInputLayout> m_pInputLayout;
		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IBindingLayout> m_pDynamicBindingLayout;

		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;

		TComPtr<IFrameBuffer> m_pFrameBuffer;
		TComPtr<IGraphicsPipeline> m_pPipeline;

		TComPtr<IBindingSet> m_pBindingSet;
		FGraphicsState m_GraphicsState;
	};

}

#endif