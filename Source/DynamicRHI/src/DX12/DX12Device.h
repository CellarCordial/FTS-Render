#ifndef RHI_DX12_DEVICE_H
#define RHI_DX12_DEVICE_H


#include "../../include/Device.h"
#include "../../include/Draw.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Core/include/ComCli.h"
#include "DX12CommandList.h"
#include "DX12Descriptor.h"
#include "DX12Forward.h"
#include <d3d12.h>
#include <mutex>

namespace FTS 
{
    class FDX12EventQuery :
        public TComObjectRoot<FComMultiThreadModel>,
        public IEventQuery
    {
    public:

        BEGIN_INTERFACE_MAP(FDX12EventQuery)
            INTERFACE_ENTRY(IID_IEventQuery, IEventQuery)
        END_INTERFACE_MAP

        BOOL Initialize() { return true; }

        BOOL Start(ID3D12Fence* pD3D12Fence, UINT64 stFenceCounter);
        BOOL Poll();
        void Wait(HANDLE WaitEvent);
        void Reset();

    private:

        Microsoft::WRL::ComPtr<ID3D12Fence> m_pD3D12Fence;
        UINT64 m_stFenceCounter = 0;
        BOOL m_bStarted = false;
        BOOL m_bResolved = false;
    };



    class FDX12TimerQuery :
        public TComObjectRoot<FComMultiThreadModel>,
        public ITimerQuery
    {
    public:

        BEGIN_INTERFACE_MAP(FDX12TimerQuery)
            INTERFACE_ENTRY(IID_ITimerQuery, ITimerQuery)
        END_INTERFACE_MAP

        FDX12TimerQuery(const FDX12Context* cpContext, FDX12DescriptorHeaps* pDescriptorHeaps, UINT32 dwQueryIndex);

        BOOL Initialize() { return true; }

        // ITimerQuery

        BOOL Poll();
        FLOAT GetTime(HANDLE WaitEvent, UINT64 stFrequency);
        void Reset();

    public:
        UINT32 m_dwBeginQueryIndex = 0;
        UINT32 m_dwEndQueryIndex = 0;
        BOOL m_bStarted = false;
        BOOL m_bResolved = false;
        
        Microsoft::WRL::ComPtr<ID3D12Fence> m_pD3D12Fence;
        UINT64 m_stFenceCounter = 0;
        
    private:
        const FDX12Context* m_cpContext = nullptr; 

        FLOAT m_fTime = 0.0f;

        FDX12DescriptorHeaps* m_pDescriptorHeaps = nullptr;
    };



    class FDX12Device :
        public TComObjectRoot<FComMultiThreadModel>,
        public IDevice
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Device)
            INTERFACE_ENTRY(IID_IDevice, IDevice)
        END_INTERFACE_MAP

        explicit FDX12Device(const FDX12DeviceDesc& crDesc);
        ~FDX12Device() noexcept;

        BOOL Initialize();

        
        // IDevice
        BOOL CreateHeap(const FHeapDesc& crDesc, CREFIID criid, void** ppvHeap) override;
        BOOL CreateTexture(const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture) override;
        BOOL CreateStagingTexture(const FTextureDesc& crDesc, ECpuAccessMode CpuAccess, CREFIID criid, void** ppvStagingTexture) override;
        BOOL CreateBuffer(const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer) override;

        BOOL CreateTextureForNative(void* pNativeTexture, const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture) override;
        BOOL CreateBufferForNative(void* pNativeBuffer, const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer) override;

        BOOL CreateShader(const FShaderDesc& crDesc, const void* cpvBinary, UINT64 stBinarySize, CREFIID criid, void** ppvShader) override;
        BOOL CreateShaderLibrary(const void* cpvBinary, UINT64 stBinarySize, CREFIID criid, void** ppvShaderLibrary) override;
        BOOL CreateSampler(const FSamplerDesc& crDesc, CREFIID criid, void** ppvSampler) override;
        BOOL CreateInputLayout(const FVertexAttributeDesc* cpDesc, UINT32 dwAttributeNum, IShader* pVertexShader, CREFIID criid, void** ppvInputLayout) override;
        
        BOOL CreateEventQuery(CREFIID criid, void** ppvEventQuery) override;
        BOOL SetEventQuery(IEventQuery* pQuery, ECommandQueueType QueueType) override;
        BOOL PollEventQuery(IEventQuery* pQuery, BOOL* pbResult) override;
        BOOL WaitEventQuery(IEventQuery* pQuery) override;
        BOOL ResetEventQuery(IEventQuery* pQuery) override;
        
        BOOL CreateTimerQuery(CREFIID criid, void** ppvTimeQuery) override;
        BOOL PollTimerQuery(ITimerQuery* pQuery) override;
        BOOL GetTimerQueryTime(ITimerQuery* pQuery, FLOAT* pfTimeInSeconds) override;
        BOOL ResetTimerQuery(ITimerQuery* pQuery) override;
        
        EGraphicsAPI GetGraphicsAPI() const override;
        void* GetNativeDescriptorHeap(EDescriptorHeapType Type) const override;
        void* GetNativeObject() const override;
        
        BOOL CreateFrameBuffer(const FFrameBufferDesc& crDesc, CREFIID criid, void** ppvFrameBuffer) override;
        BOOL CreateGraphicsPipeline(const FGraphicsPipelineDesc& crDesc, IFrameBuffer* pFrameBuffer, CREFIID criid, void** ppvGraphicsPipeline) override;
        BOOL CreateComputePipeline(const FComputePipelineDesc& crDesc, CREFIID criid, void** ppvComputePipeline) override;
        
        BOOL CreateBindingLayout(const FBindingLayoutDesc& crDesc, CREFIID criid, void** ppvBindingLayout) override;
        BOOL CreateBindlessLayout(const FBindlessLayoutDesc& crDesc, CREFIID criid, void** ppvBindlessLayout) override;
        
        BOOL CreateBindingSet(const FBindingSetDesc& crDesc, IBindingLayout* pLayout, CREFIID criid, void** ppvBindingSet) override;
        BOOL CreateBindlessSet(IBindingLayout* pLayout, CREFIID criid, void** ppvDescriptorTable) override;
        BOOL ResizeBindlessSet(IBindlessSet* pDescriptorTable, UINT32 dwNewSize, BOOL bKeepContents = true) override;
        BOOL WriteBindlessSet(IBindlessSet* pDescriptorTable, const FBindingSetItem& crBindingItem) override;
        
        BOOL CreateCommandList(const FCommandListDesc& crDesc, CREFIID criid, void** ppvCmdList) override;
        UINT64 ExecuteCommandLists(ICommandList* const* pcpCommandLists, UINT64 stCommandListsNum = 1, ECommandQueueType ExecutionQueueType = ECommandQueueType::Graphics) override;
        BOOL QueueWaitForCommandList(ECommandQueueType WaitQueueType, ECommandQueueType ExecutionQueueType, UINT64 stInstance) override;
        
        void WaitForIdle() override;
        void RunGarbageCollection() override;
        


        BOOL BuildRootSignature(const FPipelineBindingLayoutArray& crpBindingLayouts, BOOL bAllowInputLayout, CREFIID criid, void** ppvRootSignature) const;
        
    private:
        FDX12CommandQueue* GetQueue(ECommandQueueType Type) const;

    private:
        FDX12Context m_Context;
        std::unique_ptr<FDX12DescriptorHeaps> m_pDescriptorHeaps;

        FDX12DeviceDesc m_Desc;

        std::array<std::unique_ptr<FDX12CommandQueue>, static_cast<UINT8>(ECommandQueueType::Count)> m_pCmdQueues;
        HANDLE m_FenceEvent{};

        std::mutex m_Mutex;

        std::vector<ID3D12CommandList*> m_pD3D12CmdListsToExecute;
    };

}


#endif