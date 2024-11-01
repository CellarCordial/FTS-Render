#ifndef RHI_DEVICE_H
#define RHI_DEVICE_H

#include "CommandList.h"
#include "DynamicRHI.h"

namespace FTS
{
    
    extern const IID IID_IDevice;

    struct IDevice : public IUnknown
    {
    public:
        virtual BOOL CreateHeap(const FHeapDesc& crDesc, CREFIID criid, void** ppvHeap) = 0;

        virtual BOOL CreateTexture(const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture) = 0;
        
        virtual BOOL CreateStagingTexture(
            const FTextureDesc& crDesc,
            ECpuAccessMode CpuAccess,
            CREFIID criid,
            void** ppvStagingTexture
        ) = 0;

        virtual BOOL CreateBuffer(const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer) = 0;

        virtual BOOL CreateTextureForNative(void* pNativeTexture, const FTextureDesc& crDesc, CREFIID criid, void** ppvTexture) = 0;

        virtual BOOL CreateBufferForNative(void* pNativeBuffer, const FBufferDesc& crDesc, CREFIID criid, void** ppvBuffer) = 0;

        virtual BOOL CreateShader(
            const FShaderDesc& crDesc,
            const void* cpvBinary,
            UINT64 stBinarySize,
            CREFIID criid,
            void** ppvShader
        ) = 0;
        
        virtual BOOL CreateShaderLibrary(
            const void* cpvBinary,
            UINT64 stBinarySize,
            CREFIID criid,
            void** ppvShaderLibrary
        ) = 0;
        
        virtual BOOL CreateSampler(const FSamplerDesc& crDesc, CREFIID criid, void** ppvSampler) = 0;

        virtual BOOL CreateInputLayout(
            const FVertexAttributeDesc* cpDesc,
            UINT32 dwAttributeNum,
            IShader* pVertexShader,
            CREFIID criid,
            void** ppvInputLayout
        ) = 0;
        
        virtual BOOL CreateEventQuery(CREFIID criid, void** ppvEventQuery) = 0;
        
        virtual BOOL SetEventQuery(IEventQuery* pQuery, ECommandQueueType QueueType) = 0;
        
        virtual BOOL PollEventQuery(IEventQuery* pQuery, BOOL* pbResult) = 0;
        
        virtual BOOL WaitEventQuery(IEventQuery* pQuery) = 0;
        
        virtual BOOL ResetEventQuery(IEventQuery* pQuery) = 0;

        virtual BOOL CreateTimerQuery(CREFIID criid, void** ppvTimeQuery) = 0;
        
        virtual BOOL PollTimerQuery(ITimerQuery* pQuery) = 0;
        
        virtual BOOL GetTimerQueryTime(ITimerQuery* pQuery, FLOAT* pfTimeInSeconds) = 0;
        
        virtual BOOL ResetTimerQuery(ITimerQuery* pQuery) = 0;

        virtual EGraphicsAPI GetGraphicsAPI() const = 0;

        virtual void* GetNativeDescriptorHeap(EDescriptorHeapType Type) const = 0;

        virtual void* GetNativeObject() const = 0;

        virtual BOOL CreateFrameBuffer(const FFrameBufferDesc& crDesc, CREFIID criid, void** ppvFrameBuffer) = 0;
        
        virtual BOOL CreateGraphicsPipeline(
            const FGraphicsPipelineDesc& crDesc,
            IFrameBuffer* pFrameBuffer,
            CREFIID criid,
            void** ppvGraphicsPipeline
        ) = 0;
        
        virtual BOOL CreateComputePipeline(const FComputePipelineDesc& crDesc, CREFIID criid, void** ppvComputePipeline) = 0;

        virtual BOOL CreateBindingLayout(const FBindingLayoutDesc& crDesc, CREFIID criid, void** ppvBindingLayout) = 0;
        
        virtual BOOL CreateBindlessLayout(const FBindlessLayoutDesc& crDesc, CREFIID criid, void** ppvBindlessLayout) = 0;

        virtual BOOL CreateBindingSet(
            const FBindingSetDesc& crDesc,
            IBindingLayout* pLayout,
            CREFIID criid,
            void** ppvBindingSet
        ) = 0;
        
        virtual BOOL CreateDescriptorTable(IBindingLayout* pLayout, CREFIID criid, void** ppvDescriptorTable) = 0;

        virtual BOOL ResizeDescriptorTable(IDescriptorTable* pDescriptorTable, UINT32 dwNewSize, BOOL bKeepContents = true) = 0;
        
        virtual BOOL WriteDescriptorTable(IDescriptorTable* pDescriptorTable, const FBindingSetItem& crBindingItem) = 0;
    
        virtual BOOL CreateCommandList(const FCommandListDesc& crDesc, CREFIID criid, void** ppvCmdList) = 0;
        
        virtual UINT64 ExecuteCommandLists(
            ICommandList* const* pcpCommandLists,
            UINT64 stCommandListsNum = 1,
            ECommandQueueType ExecutionQueueType = ECommandQueueType::Graphics
        ) = 0;
        
        virtual BOOL QueueWaitForCommandList(ECommandQueueType WaitQueueType, ECommandQueueType ExecutionQueueType, UINT64 stInstance) = 0;

        virtual void WaitForIdle() = 0;

        virtual void RunGarbageCollection() = 0;



		virtual ~IDevice() = default;
    };
}






























#endif