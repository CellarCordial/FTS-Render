#ifndef RHI_RAY_TRACING
#define RHI_RAY_TRACING

#if RAY_TRACING
#include "Pipeline.h"
#include "Draw.h"
#include <vector>

namespace FTS
{
	namespace RayTracing
	{
		enum class EGeometryFlags : UINT8
		{
			None = 0,
			Opaque = 1,
			NoDuplicateAnyHitInvocation = 2
		};
		FTS_ENUM_CLASS_FLAG_OPERATORS(EGeometryFlags);

		enum class EGeometryType : UINT8
		{
			Triangle,
			BoundingBox
		};

		struct FGeometryTriangles
		{
			IBuffer* pIndexBuffer;
			IBuffer* pVertexBuffer;
			EFormat IndexFormat;
			EFormat VertexFormat;

			UINT64 stIndexOffset = 0;
			UINT64 stVertexOffset = 0;
			UINT32 dwIndexCount = 0;
			UINT32 dwVertexCount = 0;
			UINT32 dwVertexStride = 0;
		};

		struct FGeometryBoundingBoxes
		{
			IBuffer* pBuffer = nullptr;
			IBuffer* pUnusedBuffer = nullptr;
			UINT64 stOffset = 0;
			UINT32 dwCount = 0;
			UINT32 dwStride = 0;
		};

		typedef FLOAT FAffineMatrix[12];


		struct FGeometryDesc
		{
			EGeometryFlags Flags = EGeometryFlags::None;
			EGeometryType Type = EGeometryType::Triangle;

			union 
			{
				FGeometryTriangles Triangles;
				FGeometryBoundingBoxes AABBs;
			};

			BOOL bUseTransform = false;
			FAffineMatrix AffineTransform;
		};

		enum class EInstanceFlags : UINT8
		{
			None							= 0,
			TriangleCullDisable				= 1,
			TriangleFrontCounterclockwise	= 1 << 1,
			ForceOpaque						= 1 << 2,
			ForceNonOpaque					= 1 << 3,
		};
		FTS_ENUM_CLASS_FLAG_OPERATORS(EInstanceFlags);

		struct IAccelStruct;

		struct FInstanceDesc
		{
			FAffineMatrix AffineTransform;

			UINT32 dwInstanceID : 24 = 0;
			UINT32 dwInstanceMask : 8 = 0;

			UINT32 dwInstanceContributionToHitGroupIndex : 24 = 0;
			EInstanceFlags Flags : 8 = EInstanceFlags::None;
			
			union {
				IAccelStruct* pBottomLevelAS = nullptr;
				UINT64 stBiasDeviceAddress;
			};
		};

		enum class EAccelStructBuildFlags : UINT8
		{
			None				= 0,
			AllowUpdate			= 1,
			AllowCompaction		= 1 << 1,
			PreferFastTrace		= 1 << 2,
			PreferFastBuild		= 1 << 3,
			MinimizeMemory		= 1 << 4,
			PerformUpdate		= 1 << 5
		};
		FTS_ENUM_CLASS_FLAG_OPERATORS(EAccelStructBuildFlags);

		struct FAccelStructDesc
		{
			std::string strName;

			BOOL bIsVirtual = false;
			BOOL bIsTopLevel = false;
			UINT64 stTopLevelMaxInstantNum = 0;
			std::vector<FGeometryDesc> BottomLevelGeometryDescs;

			EAccelStructBuildFlags Flags = EAccelStructBuildFlags::None;
		};

		extern const IID IID_IAccelStruct;
		struct IAccelStruct : IResource
		{
			virtual const FAccelStructDesc& GetDesc() const = 0;
		};

		struct FShaderDesc
		{
			IShader* pShader = nullptr;
			IBindingLayout* pBindingLayout = nullptr;
		};

		struct FHitGroupDesc
		{
			std::string strExportName;
			IShader* pClosestHitShader = nullptr;
			IShader* pAnyHitShader = nullptr;
			IShader* pIntersectShader = nullptr;
			IBindingLayout* pBindingLayout = nullptr;
			
			BOOL bIsProceduralPrimitive = false;
		};

		struct FPipelineDesc
		{
			std::vector<FShaderDesc> Shaders;
			std::vector<FHitGroupDesc> HitGroups;

			FPipelineBindingLayoutArray pGlobalBindingLayouts;
			UINT32 dwMaxPayloadSize = 0;
			UINT32 dwMaxAttributeSize = sizeof(FLOAT) * 2;
			UINT32 dwMaxRecursionDepth = 1;
			INT32 dwHlslExtensionsUAV = -1;
		};

		extern const IID IID_IPipeline;
		struct IPipeline : public IResource
		{
			virtual const FPipelineDesc& GetDesc() const = 0;
			virtual BOOL CreateShaderTable(CREFIID criid, void** ppvShaderTable) = 0;
		};

		extern const IID IID_IShaderTable;
		struct IShaderTable : public IResource
		{
			virtual void SetRayGenShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) = 0;

			virtual INT32 AddMissShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) = 0;
			virtual INT32 AddHitGroup(const CHAR* strName, IBindingSet* pBindingSet = nullptr) = 0;
			virtual INT32 AddCallableShader(const CHAR* strName, IBindingSet* pBindingSet = nullptr) = 0;
			
			virtual void ClearMissShaders() = 0;
			virtual void ClearHitShaders() = 0;
			virtual void ClearCallableShaders() = 0;

			virtual IPipeline* GetPipeline() const = 0;
		};


		struct FPipelineState
		{
			IShaderTable* pShaderTable = nullptr;
			FPipelineStateBindingSetArray pBindingSets;
		};

		struct FDispatchRaysArguments
		{
			UINT32 dwWidth = 1;	
			UINT32 dwHeight = 1;
			UINT32 dwDepth = 1;
		};
	}
}



#endif









#endif