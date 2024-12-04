#ifndef RHI_DX12_RAY_TRACING_H
#define RHI_DX12_RAY_TRACING_H

#if RAY_TRACING
#include "../../../Core/include/ComRoot.h"
#include "../../../Core/include/ComCli.h"
#include "../../include/RayTracing.h"
#include "DX12Forward.h"

namespace FTS
{
	namespace RayTracing
	{
		class FDX12AccelStruct :
			public TComObjectRoot<FComMultiThreadModel>,
			public IAccelStruct
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12AccelStruct)
				INTERFACE_ENTRY(IID_IAccelStruct, IAccelStruct)
			END_INTERFACE_MAP

			FDX12AccelStruct(const FDX12Context* cpContext, const FAccelStructDesc& crDesc);

			BOOL Initialize();

			// IAccelStruct.
			UINT64 GetDeviceAddress() const override;
			BOOL IsCompacted() const override { return m_bCompacted; }
			const FAccelStructDesc& GetDesc() const override { return m_Desc; }

		private:
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO GetAccelStructPrebuildInfo();

		private:
			const FDX12Context* m_cpContext;

			FAccelStructDesc m_Desc;
			std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_DxrInstanceDescs;
			std::vector<TComPtr<IAccelStruct>> m_pBottomLevelAccelStructs;
			TComPtr<IBuffer> m_pDataBuffer;

			BOOL m_bAllowUpdate = false;
			BOOL m_bCompacted = false;
		};


		class FDX12ShaderTable :
			public TComObjectRoot<FComMultiThreadModel>,
			public IShaderTable
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12ShaderTable)
				INTERFACE_ENTRY(IID_IShaderTable, IShaderTable)
			END_INTERFACE_MAP

			// IShaderTable.
			void SetRayGenerationShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddMissShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			INT32 AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) override;
			void ClearMissShaders() override;
			void ClearHitShaders() override;
			void ClearCallableShaders() override;
			IPipeline* GetPipeline() const override;

		private:

		};


		class FDX12Pipeline :
			public TComObjectRoot<FComMultiThreadModel>,
			public IPipeline
		{
		public:
			BEGIN_INTERFACE_MAP(FDX12Pipeline)
				INTERFACE_ENTRY(IID_IPipeline, IPipeline)
			END_INTERFACE_MAP

			// IPipeline.
			const FPipelineDesc& GetDesc() const override;
			BOOL CreateShaderTabel(CREFIID criid, void** ppvShaderTable) override;

		};
	}

}

#endif

#endif