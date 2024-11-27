#ifndef RENDER_PASS_SURFACE_CACHE_CAPTURE_H
#define RENDER_PASS_SURFACE_CACHE_CAPTURE_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Scene/include/Scene.h"
#include "../../../Core/include/File.h"

namespace FTS
{
	namespace Constant
	{
		struct SurfaceCapturePassConstant
		{

		};
	}

	class FSurfaceCapturePass : public IRenderPass
	{
	public:
		FSurfaceCapturePass() { Type = ERenderPassType::Precompute; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;
        BOOL FinishPass() override;

	private:
		BOOL SetupPipeline(ICommandList* pCmdList, IRenderResourceCache* pCache, FSurfaceCache* pSurfaceCache);

	private:
		FEntity* m_pModelEntity = nullptr;
		UINT32 m_dwCurrMeshSurfaceCacheIndex = 0;
        std::unique_ptr<Serialization::BinaryOutput> pBinaryOutput;
		Constant::SurfaceCapturePassConstant m_PassConstant;

		ISampler* m_pLinearClampSampler;

		TComPtr<IBuffer> m_pVertexBuffer;
		TComPtr<IBuffer> m_pIndexBuffer;
		TComPtr<ITexture> m_pTexture;

		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IInputLayout> m_pInputLayout;

		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;

		TComPtr<IFrameBuffer> m_pFrameBuffer;
		TComPtr<IGraphicsPipeline> m_pPipeline;

		TComPtr<IBindingSet> m_pBindingSet;
		FGraphicsState m_GraphicsState;
		FDrawArguments m_DrawArguments;
	};

}







#endif