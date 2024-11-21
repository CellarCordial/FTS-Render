#ifndef RENDER_PASS_GLOBAL_SDF_H
#define RENDER_PASS_GLOBAL_SDF_H
#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include "../../../Math/include/Bounds.h"
#include <memory>

namespace FTS
{ 
	namespace Constant
	{
		struct GlobalSdfConstants
		{
			FMatrix4x4 VoxelWorldMatrix;
			
			FVector3I VoxelOffset;  
			FLOAT fGIMaxDistance = 500;
			
			UINT32 dwMeshSdfBegin = 0;
			UINT32 dwMeshSdfEnd = 0;
			UINT32 dwVoxelNumPerAxis = 0;
			UINT32 PAD = 0;
		};
		
		struct ModelSdfData
		{
			FMatrix4x4 CoordMatrix;

			FVector3F SdfLower;
			FVector3F SdfUpper;

			UINT32 dwMeshSdfIndex = 0;
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
		BOOL PipelineSetup(IDevice* pDevice);
		BOOL ComputeStateSetup(IDevice* pDevice, IRenderResourceCache* pCache);

	private:
		FBounds3F m_GlobalBox;
		UINT32 m_dwModelSdfDataDefaultCount = 32;
		std::vector<Constant::GlobalSdfConstants> m_PassConstants;
		std::vector<Constant::ModelSdfData> m_ModelSdfDatas;

		TComPtr<IBuffer> m_pModelSdfDataBuffer;
		TComPtr<ITexture> m_pGlobalSdfTexture;
		std::vector<ITexture*> m_pMeshSdfTextures;

		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IBindingLayout> m_pDynamicBindingLayout;
		TComPtr<IBindingLayout> m_pBindlessLayout;

		TComPtr<IBindingLayout> m_pClearPassBindingLayout;

		TComPtr<IShader> m_pCS;
		TComPtr<IComputePipeline> m_pPipeline;

		TComPtr<IShader> m_pClearPassCS;
		TComPtr<IComputePipeline> m_pClearPassPipeline;

		TComPtr<IBindingSet> m_pBindingSet;
		TComPtr<IBindingSet> m_pDynamicBindingSet;
		TComPtr<IBindlessSet> m_pBindlessSet;
		FComputeState m_ComputeState;

		TComPtr<IBindingSet> m_pClearPassBindingSet;
		FComputeState m_ClearPassComputeState;
	};
}





#endif