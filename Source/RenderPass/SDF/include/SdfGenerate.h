#ifndef RENDER_PASS_SDF_GENERATE_H
#define RENDER_PASS_SDF_GENERATE_H
#include "../../../DynamicRHI/include/CommandList.h"
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Core/include/File.h"
#include "../../../Math/include/Vector.h"
#include "../../../Scene/include/Scene.h"
#include "../../../Math/include/Bvh.h"
#include <memory>

namespace FTS 
{
    namespace Constant
    {
		struct SdfGeneratePassConstants
		{
			FVector3F SdfLower;
			UINT32 dwTriangleNum = 0;

			FVector3F SdfUpper;
			UINT32 dwSignRayNum = 3;

			FVector3F SdfExtent;
			UINT32 dwXBegin = 0;

			UINT32 dwXEnd = 0;
			FVector3I Pad;
		};
    }

    class FSdfGeneratePass : public IRenderPass
    {
    public:
		FSdfGeneratePass() { Type = ERenderPassType::Precompute | ERenderPassType::Exclude; }

        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

        BOOL FinishPass() override;

    private:
        BOOL BuildBvh();

    private:
        UINT32 m_dwBeginX = 0;
        BOOL m_bResourceWrited = false;
        UINT32 m_dwCurrMeshSdfIndex = 0;
        FDistanceField* m_pDistanceField = nullptr;
        std::unique_ptr<Serialization::BinaryOutput> pBinaryOutput;
		Constant::SdfGeneratePassConstants m_PassConstants;

        TComPtr<IBuffer> m_pBvhNodeBuffer;
        TComPtr<IBuffer> m_pBvhVertexBuffer;
        TComPtr<ITexture> m_pSdfOutputTexture;
        TComPtr<IStagingTexture> m_pReadBackTexture;

        TComPtr<IBindingLayout> m_pBindingLayout;
        
        TComPtr<IShader> m_pCS;
        TComPtr<IComputePipeline> m_pPipeline;

        TComPtr<IBindingSet> m_pBindingSet;
        FComputeState m_ComputeState;
    };
}





#endif