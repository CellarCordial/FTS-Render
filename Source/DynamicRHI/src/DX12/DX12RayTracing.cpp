#include "DX12RayTracing.h"

#if USE_RAY_TRACING
#include "DX12Resource.h"

namespace FTS
{
	namespace RayTracing
	{
		struct FDX12GeometryDesc
		{

		};

		class FDX12AccelStructBuildInput
		{
		public:


		private:
			struct AccelStructBuildStructure
			{
				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE Type;
				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags;
				UINT32 dwNumDescs;
				D3D12_ELEMENTS_LAYOUT DescsLayout;

				union 
				{
					D3D12_GPU_VIRTUAL_ADDRESS InstanceAddress;
					const FDX12GeometryDesc** cppGeometryDesc;
				};
			};

			AccelStructBuildStructure m_Data;
			std::vector<FDX12GeometryDesc> m_GeometryDescs;
			std::vector<FDX12GeometryDesc*> m_pGeometryDescs;
		};

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO FDX12AccelStruct::GetAccelStructPrebuildInfo()
		{
			FDX12AccelStructBuildInput BuildInput;
			if (m_Desc.bIsTopLevel)
			{

			}
			else
			{

			}
		}


		void FDX12ShaderTable::SetRayGenerationShader(const CHAR* strName, IBindingSet* pBindingSet /*= nullptr*/)
		{
		}

		INT32 FDX12ShaderTable::AddMissShader(const CHAR* strName, IBindingSet* pBindingSet /*= nullptr*/)
		{
		}

		INT32 FDX12ShaderTable::AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet /*= nullptr*/)
		{

		}

		INT32 FDX12ShaderTable::AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet /*= nullptr*/)
		{

		}

		void FDX12ShaderTable::ClearMissShaders()
		{

		}

		void FDX12ShaderTable::ClearHitShaders()
		{

		}

		void FDX12ShaderTable::ClearCallableShaders()
		{

		}

		IPipeline* FDX12ShaderTable::GetPipeline() const
		{

		}

		const FAccelStructDesc& FDX12AccelStruct::GetDesc() const
		{

		}

		BOOL FDX12AccelStruct::IsCompacted() const
		{

		}

		FDX12AccelStruct::FDX12AccelStruct(const FDX12Context* cpContext, const FAccelStructDesc& crDesc) :
			m_cpContext(cpContext), m_Desc(crDesc)
		{
		}

		BOOL FDX12AccelStruct::Initialize()
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};

		}

		UINT64 FDX12AccelStruct::GetDeviceAddress() const
		{
			return CheckedCast<FDX12Buffer*>(m_pDataBuffer)->m_GpuAddress;
		}

	}

}


#endif
