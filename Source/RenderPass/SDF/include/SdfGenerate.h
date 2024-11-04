#ifndef SAMPLE_SDF_GENERATE_H
#define SAMPLE_SDF_GENERATE_H
#include "../../../DynamicRHI/include/CommandList.h"
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Scene/include/Geometry.h"
#include "../../../Math/include/Bvh.h"
#include <memory>

namespace FTS 
{
    namespace Constant
    {
		struct SdfGeneratePassConstants
		{
			FVector3F SdfLower = FVector3F(-2.4f);
			UINT32 dwTriangleNum = 0;

			FVector3F SdfUpper = FVector3F(2.4f);
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
		FSdfGeneratePass() { Type = ERenderPassType::Precompute; }

        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

        BOOL FinishPass() override;

		void GenerateSdf(FMesh* pMesh) 
        {
            if (!pMesh) return;

			m_cpMesh = pMesh;
			m_bResourceWrited = false;
        }

    private:
        BOOL BuildBvh();

    private:
        FBvh m_Bvh;
		const FMesh* m_cpMesh = nullptr;
        BOOL m_bResourceWrited = false;
        UINT32 m_dwBeginX = 0;
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