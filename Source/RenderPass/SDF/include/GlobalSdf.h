#ifndef RENDER_PASS_GLOBAL_SDF_H
#define RENDER_PASS_GLOBAL_SDF_H
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include <memory>

namespace FTS
{ 
	namespace Constant
	{
		struct GlobalSdfConstants
		{
			FMatrix4x4 VoxelWorldMatrix;
			FVector3I VoxelOffset;  FLOAT fGIMaxDistance = 500;
			UINT32 dwMeshSdfCount = 0;   FVector3I PAD;
		};
		
		struct ModelSdfData
		{
			FMatrix4x4 LocalMatrix;
			FMatrix4x4 WorldMatrix;
			FMatrix4x4 CoordMatrix;

			FVector3F SdfLower;
			FVector3F SdfUpper;
		};
	}

	class FGlobalSdfPass : public IRenderPass
	{
	public:
		FGlobalSdfPass() { Type = ERenderPassType::Precompute | ERenderPassType::Exclude; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		BOOL FinishPass() override;

	private:
		BOOL m_bResourceWrited = false;
		Constant::GlobalSdfConstants m_PassConstant;
		std::vector<Constant::ModelSdfData> m_ModelSdfDatas;

		TComPtr<IBuffer> m_pModelSdfDataBuffer;
		TComPtr<ITexture> m_pGlobalSdfTexture;
		std::vector<TComPtr<ITexture>> m_pMeshSdfTextures;

		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IBindingLayout> m_pBindlessLayout;

		TComPtr<IShader> m_pCS;
		TComPtr<IComputePipeline> m_pPipeline;

		TComPtr<IBindingSet> m_pBindingSet;
		TComPtr<IBindlessSet> m_pBindlessSet;
		FComputeState m_ComputeState;

	};
}





#endif