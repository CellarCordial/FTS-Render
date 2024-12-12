/**
 * *****************************************************************************
 * @file        DX12CommandList.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-06-03
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_COMMANDLIST_H
 #define RHI_DX12_COMMANDLIST_H

#include "../../include/DynamicRHI.h"
#include "DX12Forward.h"
#include "DX12Descriptor.h"
#include "../../include/CommandList.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Core/include/ComCli.h"
#include "../StateTrack.h"
#include "DX12RayTracing.h"
#include <atomic>
#include <combaseapi.h>
#include <d3d12.h>
#include <deque>
#include <list>
#include <memory>
#include <vector>

namespace FTS 
{
    struct FDX12InternalCommandList
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCmdAllocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pD3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pD3D12CommandList4;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> pD3D12CommandList6;
        UINT64 stLastSubmittedValue = 0;
    };

    struct FDX12CommandListInstance
    {
        Microsoft::WRL::ComPtr<ID3D12CommandList> pD3D12CommandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator;
        ECommandQueueType CommandQueueType = ECommandQueueType::Graphics;
        
        Microsoft::WRL::ComPtr<ID3D12Fence> pFence;
        UINT64 stSubmittedValue = 0;
        
        std::vector<TComPtr<IResource>> pReferencedResources;
        std::vector<TComPtr<IStagingTexture>> pReferencedStagingTextures;
        std::vector<TComPtr<IBuffer>> pReferencedStagingBuffers;
        std::vector<TComPtr<ITimerQuery>> pReferencedTimerQueries;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Object>> pReferencedNativeResources;
    };

    struct FDX12CommandQueue
    {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> pD3D12CommandQueue;
        Microsoft::WRL::ComPtr<ID3D12Fence> pD3D12Fence;

        UINT64 stLastSubmittedValue = 0;
        UINT64 stLastCompletedValue = 0;

        std::atomic<UINT64> stRecordingVersion = 1;
        std::deque<std::shared_ptr<FDX12CommandListInstance>> pCommandListsInFlight;
        

        explicit FDX12CommandQueue(const FDX12Context& crContext, ID3D12CommandQueue* pD3D12Queue) : pD3D12CommandQueue(pD3D12Queue)
        {
            if (FAILED(crContext.pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(pD3D12Fence.GetAddressOf()))))
            {
                LOG_ERROR("Call to FDX12CommandQueue constructor failed because create ID3D12Fence failed.");
                assert(pD3D12Queue != nullptr);
            }
        }

        UINT64 UpdateLastCompletedValue()
        {
            if (stLastCompletedValue < stLastSubmittedValue)
            {
                stLastCompletedValue = pD3D12Fence->GetCompletedValue();
            }
            return stLastCompletedValue;
        }
    };

    struct FDX12BufferChunk
    {
        static const UINT64 cstSizeAlignment = 4096;     /**< GPU page size. */

        Microsoft::WRL::ComPtr<ID3D12Resource> pD3D12Buffer;
        UINT64 stVersion = 0;
        UINT64 stBufferSize = 0;
        UINT64 stWriteEndPosition = 0;
        void* pvCpuAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
        UINT32 dwIndexInPool = 0;

        ~FDX12BufferChunk()
        {
            if (pD3D12Buffer != nullptr && pvCpuAddress != nullptr)
            {
                pD3D12Buffer->Unmap(0, nullptr);
                pvCpuAddress = nullptr;
            }
        }
    };

    class FDX12UploadManager
    {
    public:
        FDX12UploadManager(
            const FDX12Context* cpContext, 
            FDX12CommandQueue* pCmdQueue, 
            UINT64 stDefaultChunkSize, 
            UINT64 stMemoryLimit,
            BOOL bDxrScratch = false
        );

        BOOL SuballocateBuffer(
            UINT64 stSize, 
            ID3D12Resource** ppD3D12Buffer, 
            UINT64* pstOffset, 
            UINT8** ppCpuAddress, 
            D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress, 
            UINT64 stCurrentVersion, 
            UINT32 dwAligment = 256,
            ID3D12GraphicsCommandList* pD3D12CmdList = nullptr
        );

        void SubmitChunks(UINT64 stCurrentVersion, UINT64 stSubmittedVersion);

    private:
        std::shared_ptr<FDX12BufferChunk> CreateBufferChunk(UINT64 stSize) const;

    private:
        const FDX12Context* m_cpContext;
        FDX12CommandQueue* m_pCmdQueue;

        UINT64 m_stDefaultChunkSize;
        UINT64 m_stMaxMemorySize;
        UINT64 m_stAllocatedMemorySize;

        std::list<std::shared_ptr<FDX12BufferChunk>> m_pChunkPool;
        std::shared_ptr<FDX12BufferChunk> m_pCurrentChunk;

        BOOL m_bDxrScratch;
    };


    class FDX12CommandList :
        public TComObjectRoot<FComMultiThreadModel>,
        public ICommandList
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12CommandList)
            INTERFACE_ENTRY(IID_ICommandList, ICommandList)
        END_INTERFACE_MAP

        FDX12CommandList(
            const FDX12Context* cpContext,
            FDX12DescriptorHeaps* pDescriptorHeaps,
            IDevice* pDevice,
            FDX12CommandQueue* pDX12CmdQueue,
            const FCommandListDesc& crDesc
        );

        BOOL Initialize();

        // ICommandList
        BOOL Open() override;
        BOOL Close() override;

        BOOL ClearState() override;

        BOOL ClearTextureFloat(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, const FColor& crClearColor) override;
        BOOL ClearTextureUInt(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, UINT32 dwClearColor) override;
        BOOL ClearDepthStencilTexture(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, BOOL bClearDepth, FLOAT fDepth, BOOL bClearStencil, UINT8 btStencil) override;
        
        BOOL WriteTexture(ITexture* pDst, UINT32 dwArraySlice, UINT32 dwMipLevel, const UINT8* cpData, UINT64 stRowPitch, UINT64 stDepthPitch) override;
        BOOL ResolveTexture( ITexture* pDst, const FTextureSubresourceSet& crDstSubresourceSet, ITexture* pSrc, const FTextureSubresourceSet& crSrcSubresourceSet) override;
        BOOL CopyTexture(ITexture* pDst, const FTextureSlice& crDstSlice, ITexture* pSrc, const FTextureSlice& crSrcSlice) override;
        BOOL CopyTexture(IStagingTexture* pDst, const FTextureSlice& crDstSlice, ITexture* pSrc, const FTextureSlice& crSrcSlice) override;
        BOOL CopyTexture(ITexture* pDst, const FTextureSlice& crDstSlice, IStagingTexture* pSrc, const FTextureSlice& crSrcSlice) override;
        
        BOOL WriteBuffer(IBuffer* pBuffer, const void* cpvData, UINT64 stDataSize, UINT64 stDstOffsetBytes) override;
        BOOL ClearBufferUInt(IBuffer* pBuffer, UINT32 dwClearValue) override;
        BOOL CopyBuffer(IBuffer* pDst, UINT64 stDstOffsetBytes, IBuffer* pSrc, UINT64 stSrcOffsetBytes, UINT64 stDataSizeBytes) override;
        
        BOOL SetPushConstants(const void* cpvData, UINT64 stByteSize) override;
        BOOL SetGraphicsState(const FGraphicsState& crState) override;
        BOOL SetComputeState(const FComputeState& crState) override;
        
        BOOL Draw(const FDrawArguments& crArgs) override;
        BOOL DrawIndexed(const FDrawArguments& crArgs) override;
        BOOL Dispatch(UINT32 dwGroupsX, UINT32 dwGroupsY = 1, UINT32 dwGroupsZ = 1) override;
        
        BOOL BeginTimerQuery(ITimerQuery* pQuery) override;
        BOOL EndTimerQuery(ITimerQuery* pQuery) override;
        BOOL BeginMarker(const CHAR* cpcName) override;
        BOOL EndMarker() override;
        
        BOOL SetEnableUavBarriersForTexture(ITexture* pTexture, BOOL bEnableBarriers) override;
        BOOL SetEnableUavBarriersForBuffer(IBuffer* pBuffer, BOOL bEnableBarriers) override;
        BOOL SetTextureState(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSet, EResourceStates States) override;
        BOOL SetBufferState(IBuffer* pBuffer, EResourceStates States) override;
       
        void CommitBarriers() override;
		BOOL BindFrameBuffer(IFrameBuffer* pFrameBuffer) override;
		BOOL CommitDescriptorHeaps() override;

        BOOL GetTextureSubresourceState(ITexture* pTexture, UINT32 dwArraySlice, UINT32 dwMipLevel, EResourceStates* pResourceStates) override;
        BOOL GetBufferState(IBuffer* pBuffer, EResourceStates* pResourceStates) override;
		IDevice* GetDevice() override;
		FCommandListDesc GetDesc() override;
        void* GetNativeObject() override;

#ifdef RAY_TRACING
        BOOL SetRayTracingState(const RayTracing::FPipelineState& crState) override;
		BOOL DispatchRays(const RayTracing::FDispatchRaysArguments& crArguments) override;

		BOOL BuildBottomLevelAccelStruct(
            RayTracing::IAccelStruct* pAccelStruct,
            const RayTracing::FGeometryDesc* pGeometryDescs,
            UINT32 dwGeometryDescNum
        ) override;
		BOOL BuildTopLevelAccelStruct(
            RayTracing::IAccelStruct* pAccelStruct, 
            const RayTracing::FInstanceDesc* cpInstanceDescs, 
            UINT32 dwInstanceNum
        ) override;

		BOOL SetAccelStructState(RayTracing::IAccelStruct* pAccelStruct, EResourceStates State) override;


        RayTracing::FDX12ShaderTableState* GetShaderTableState(RayTracing::IShaderTable* pShaderTable);
#endif


        BOOL AllocateUploadBuffer(UINT64 stSize, UINT8** ppCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress);


        BOOL GetBufferGpuVA(IBuffer* pBuffer, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress);
        BOOL UpdateGraphicsVolatileBuffers();
        BOOL UpdateComputeVolatileBuffers();
        std::shared_ptr<FDX12CommandListInstance> Executed(ID3D12Fence* pD3D12Fence, UINT64 stLastSubmittedValue);


		BOOL SetResourceStatesForBindingSet(IBindingSet* pBindingSet);
		BOOL SetResourceStatesForFramebuffer(IFrameBuffer* pFrameBuffer);
        BOOL RequireTextureState(ITexture* pTexture, const FTextureSubresourceSet& crSubresourceSetSet, EResourceStates State);
        BOOL RequireStagingTextureState(IStagingTexture* pStagingTexture, EResourceStates State);
        BOOL RequireBufferState(IBuffer* pBuffer, EResourceStates State);

        BOOL SetGraphicsBindings(
            const FPipelineStateBindingSetArray& crBindingSets, 
            UINT32 dwBindingUpdateMask, 
            IDX12RootSignature* pRootSignature
        );
        BOOL SetComputeBindings(
            const FPipelineStateBindingSetArray& crBindingSets, 
            UINT32 dwBindingUpdateMask, 
            IDX12RootSignature* pRootSignature
        );
        
    private:
        void ClearStateCache();
        BOOL BindGraphicsPipeline(IGraphicsPipeline* pGraphicsPipeline, BOOL bUpdateRootSignature) const;
        std::shared_ptr<FDX12InternalCommandList> CreateInternalCmdList() const;

        struct VolatileConstantBufferBinding
        {
            UINT32 dwBindingPoint;
            IBuffer* pBuffer;
            D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
        };

        
    public:
        std::shared_ptr<FDX12InternalCommandList> m_pActiveCmdList;

    private:
        const FDX12Context* m_cpContext;
        FDX12DescriptorHeaps* m_pDescriptorHeaps;

        IDevice* m_pDevice;
        FDX12CommandQueue* m_pCmdQueue;
        FDX12UploadManager m_UploadManager;

        inline static FResourceStateTracker m_ResourceStateTracker;

        FCommandListDesc m_Desc;

        std::list<std::shared_ptr<FDX12InternalCommandList>> m_pCmdListPool;    // 容纳提交给 CmdQueue 的 CmdList.

        std::shared_ptr<FDX12CommandListInstance> m_pInstance;
        UINT64 m_stRecordingVersion = 0;

    
        // Cache

        FGraphicsState m_CurrGraphicsState;
        FComputeState m_CurrComputeState;
        BOOL m_bCurrGraphicsStateValid = false;
        BOOL m_bCurrComputeStateValid = false;

#ifdef RAY_TRACING
        FDX12UploadManager m_DxrScratchManager;

        RayTracing::FPipelineState m_CurrRayTracingState;
        BOOL m_bCurrRayTracingStateValid = false;

        std::unordered_map<RayTracing::IShaderTable*, std::unique_ptr<RayTracing::FDX12ShaderTableState>> m_ShaderTableStatesMap;
#endif

        ID3D12DescriptorHeap* m_pCurrSRVetcHeap = nullptr;
        ID3D12DescriptorHeap* m_pCurrSamplerHeap = nullptr;

        ID3D12Resource* m_pCurrentUploadBuffer = nullptr;

        std::unordered_map<IBuffer*, D3D12_GPU_VIRTUAL_ADDRESS> m_VolatileCBAddressMap;
        BOOL m_bAnyVolatileCBWrites = false;

        std::vector<D3D12_RESOURCE_BARRIER> m_D3D12Barriers;

        std::vector<VolatileConstantBufferBinding> m_CurrGraphicsVolatileCBs;
        std::vector<VolatileConstantBufferBinding> m_CurrComputeVolatileCBs;
    };

}







 #endif