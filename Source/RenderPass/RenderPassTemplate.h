/*--------------------------------------- Graphics Pass -----------------------------------------------*/

//#ifndef RENDER_PASS_H
//#define RENDER_PASS_H
// 
//#include "../../../RenderGraph/include/RenderGraph.h"
//#include "../../../Core/include/ComCli.h"
//#include "../../../Gui/include/GuiPanel.h"
// 
//namespace FTS
//{
//	namespace Constant
//	{
//		struct PassConstant
//		{
//
//		};
//	}
//
//	class FPass : public IRenderPass
//	{
//	public:
//		FPass() { Type = ERenderPassType::Graphics; }
//
//		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache);
//		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache);
//
//	private:
//		BOOL m_bResourceWrited = false;
//		Constant::PassConstant m_PassConstant;
//
//		TComPtr<IBuffer> m_pBuffer;
//		TComPtr<ITexture> m_pTexture;
//		
//		TComPtr<IBindingLayout> m_pBindingLayout;
//		TComPtr<IInputLayout> m_pInputLayout;
//
//		TComPtr<IShader> m_pVS;
//		TComPtr<IShader> m_pPS;
//
//		TComPtr<IFrameBuffer> m_pFrameBuffer;
//		TComPtr<IGraphicsPipeline> m_pPipeline;
//
//		TComPtr<IBindingSet> m_pBindingSet;
//		FGraphicsState m_GraphicsState;
//	};
//
//}
// 
//#endif



//#include "../include/###RenderPass.h"
//#include "../../../Shader/ShaderCompiler.h"
//
//namespace FTS
//{
//	BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache)
//	{
//		// Binding Layout.
//		{
//			FBindingLayoutItemArray BindingLayoutItems(N);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreatePushConstants(Slot, sizeof(Constant));
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateConstantBuffer(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateSampler(Slot);
//			ReturnIfFalse(pDevice->CreateBindingLayout(
//				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
//				IID_IBindingLayout,
//				PPV_ARG(m_pBindingLayout.GetAddressOf())
//			));
//		}
//
//		// Input Layout.
//		{
//			FVertexAttributeDescArray VertexAttriDescs(4);
//			VertexAttriDescs[0].strName = "POSITION";
//			VertexAttriDescs[0].Format = EFormat::RGB32_FLOAT;
//			VertexAttriDescs[0].dwOffset = offsetof(FVertex, Position);
//			VertexAttriDescs[0].dwElementStride = sizeof(FVertex);
//			VertexAttriDescs[1].strName = "NORMAL";
//			VertexAttriDescs[1].Format = EFormat::RGB32_FLOAT;
//			VertexAttriDescs[1].dwOffset = offsetof(FVertex, Normal);
//			VertexAttriDescs[1].dwElementStride = sizeof(FVertex);
//			VertexAttriDescs[2].strName = "TANGENT";
//			VertexAttriDescs[2].Format = EFormat::RGB32_FLOAT;
//			VertexAttriDescs[2].dwOffset = offsetof(FVertex, Tangent);
//			VertexAttriDescs[2].dwElementStride = sizeof(FVertex);
//			VertexAttriDescs[3].strName = "TEXCOORD";
//			VertexAttriDescs[3].Format = EFormat::RG32_FLOAT;
//			VertexAttriDescs[3].dwOffset = offsetof(FVertex, UV);
//			VertexAttriDescs[3].dwElementStride = sizeof(FVertex);
//			ReturnIfFalse(pDevice->CreateInputLayout(
//				VertexAttriDescs.data(), 
//				VertexAttriDescs.Size(), 
//				nullptr, 
//				IID_IInputLayout, 
//				PPV_ARG(m_pInputLayout.GetAddressOf())
//			));
//		}
//
//
//		// Shader.
//		{
//			FShaderCompileDesc ShaderCompileDesc;
//			ShaderCompileDesc.strShaderName = ".hlsl";
//			ShaderCompileDesc.strEntryPoint = "VS";
//			ShaderCompileDesc.Target = EShaderTarget::Vertex;
//			ShaderCompileDesc.strDefines.push_back("Define=" + std::to_string(Define));
//			FShaderData VSData = ShaderCompile::CompileShader(ShaderCompileDesc);
//			ShaderCompileDesc.strEntryPoint = "PS";
//			ShaderCompileDesc.Target = EShaderTarget::Pixel;
//			ShaderCompileDesc.strDefines.push_back("Define=" + std::to_string(Define));
//			FShaderData PSData = ShaderCompile::CompileShader(ShaderCompileDesc);
//
//			FShaderDesc VSDesc;
//			VSDesc.strEntryName = "VS";
//			VSDesc.ShaderType = EShaderType::Vertex;
//			ReturnIfFalse(pDevice->CreateShader(VSDesc, VSData.Data(), VSData.Size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));
//
//			FShaderDesc PSDesc;
//			PSDesc.ShaderType = EShaderType::Pixel;
//			PSDesc.strEntryName = "PS";
//			ReturnIfFalse(pDevice->CreateShader(PSDesc, PSData.Data(), PSData.Size(), IID_IShader, PPV_ARG(m_pPS.GetAddressOf())));
//		}
//
//		// Buffer.
//		{
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateConstant(sizeof(Constant)),
//				IID_IBuffer,
//				PPV_ARG(pPassConstantBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateVertex(stByteSize, "VertexBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pVertexBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateIndex(stByteSize, "IndexBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pIndexBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateStructured(stByteSize, dwStride, "StructuredBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pStructuredBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateReadBack(stByteSize, "ReadBackBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pReadBackBuffer.GetAddressOf())
//			));
//		}
//
//		// Texture.
//		{
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, EFormat, "ShaderResourceTexture"),
//				IID_ITexture,
//				PPV_ARG(pShaderResourceTexture.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, dwDepth, EFormat, "ShaderResourceTexture3D"),
//				IID_ITexture,
//				PPV_ARG(pShaderResourceTexture3D.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateRenderTarget(dwWidth, dwHeight, EFormat, "RenderTargetTexture"),
//				IID_ITexture,
//				PPV_ARG(pRenderTargetTexture.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateDepth(dwWidth, dwHeight, EFormat, "DepthTexture"),
//				IID_ITexture,
//				PPV_ARG(pDepthTexture.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateReadWrite(dwWidth, dwHeight, dwDepth, EFormat, "ReadWriteTexture3D"),
//				IID_ITexture,
//				PPV_ARG(pReadWriteTexture3D.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateReadBack(dwWidth, dwHeight, EFormat, "ReadBackTexture"),
//				IID_ITexture,
//				PPV_ARG(pReadBackTexture.GetAddressOf())
//			));
//		}
// 
//		// Frame Buffer.
//		{
//			FFrameBufferDesc FrameBufferDesc;
//			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture, EFormat));
//			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture, EFormat));
//			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture, EFormat));
//			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(pDepthStencilTexture, EFormat);
//			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
//		}
// 
//		// Pipeline.
//		{
//			FGraphicsPipelineDesc PipelineDesc;
//			PipelineDesc.VS = pVS.Get();
//			PipelineDesc.PS = pPS.Get();
//			PipelineDesc.pInputLayout = pInputLayout.Get();
//			PipelineDesc.pBindingLayouts.PushBack(pBindingLayout.Get());
//			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
//				PipelineDesc,
//				m_pFrameBuffer.Get(),
//				IID_IGraphicsPipeline,
//				PPV_ARG(m_pPipeline.GetAddressOf())
//			));
//		}
//
//		// Binding Set.
//		{
//			ISampler* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;
//			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
//			ReturnIfFalse(pCache->Require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));
//			ReturnIfFalse(pCache->Require("LinearWarpSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
//			ReturnIfFalse(pCache->Require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));
// 
//			FBindingSetItemArray BindingSetItems(N);
//			BindingSetItems[Index] = FBindingSetItem::CreatePushConstants(Slot, sizeof(Constant));
//			BindingSetItems[Index] = FBindingSetItem::CreateConstantBuffer(Slot, pPassConstantBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_SRV(Slot, pShaderResourceBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_UAV(Slot, pUnorderedAccessBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_SRV(Slot, pShaderResourceRawBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_UAV(Slot, pUnorderedAccessRawBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_SRV(Slot, pShaderResourceTypedBuffer, pShaderResourceTypedBuffer->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_UAV(Slot, pUnorderedAccessTypedBuffer, pUnorderedAccessTypedBuffer->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTexture_SRV(Slot, pShaderResourceTexture, pShaderResourceTexture->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTexture_UAV(Slot, pUnorderedAccessTexture, pUnorderedAccessTexture->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateSampler(Slot, pSampler.Get());
//			ReturnIfFalse(pDevice->CreateBindingSet(
//				FBindingSetDesc{ .BindingItems = BindingSetItems },
//				pBindingLayout.Get(),
//				IID_IBindingSet,
//				PPV_ARG(m_pBindingSet.GetAddressOf())
//			));
//		}
//
//		// Graphics State.
//		{
//			m_GraphicsState.pPipeline = m_pPipeline.Get();
//			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
//			m_GraphicsState.pBindingSets.PushBack(m_pBindingSet.Get());
//			m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = pVertexBuffer });
//			m_GraphicsState.IndexBufferBinding = FIndexBufferBinding{ .pBuffer = pIndexBuffer, .Format = EFormat::R32_UINT };
//			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
//		}
//
// 
// 		Gui::Add(
//			[this]()
//			{
//				BOOL bDirty = false;
//
//				ImGui::SeparatorText("Render Pass");
//
//				if (bDirty)
//				{
//					m_bResourceWrited = false;
//				}
//			}
//		);
//		return true;
//	}
//
//	BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
//	{
//		ReturnIfFalse(pCmdList->Open());
//
//		// Update Constant.
//		{
//			m_PassConstant;
//			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstant, sizeof(Constant::AerialLUTPassConstant)));
//		}
// 
//		if (!m_bResourceWrited)
//		{
//			ReturnIfFalse(pCmdList->WriteBuffer(pVertexBuffer.Get(), Vertices.data(), sizeof(FVertex) * Vertices.size()));
//			ReturnIfFalse(pCmdList->WriteBuffer(pIndexBuffer.Get(), Indices.data(), sizeof(EFormat::R16_UINT) * Indices.size()));
//			m_bResourceWrited = true;
//		}
//
//		ReturnIfFalse(pCmdList->SetGraphicsState(GraphicsState));
//		ReturnIfFalse(pCmdList->SetPushConstants(&PassConstant, sizeof(Constant)));
//		ReturnIfFalse(pCmdList->DrawIndexed(DrawArgument));
//
//		ReturnIfFalse(pCmdList->Close());
//		return true;
//	}
//}
//


/*--------------------------------------- Compute Pass -----------------------------------------------*/


//#ifndef RENDER_PASS_H
//#define RENDER_PASS_H
//
//#include "../../../RenderGraph/include/RenderGraph.h"
//#include "../../../Core/include/ComCli.h"
//
//namespace FTS
//{
//	namespace Constant
//	{
//		struct PassConstant
//		{
//
//		};
//	}
//
//
//	class FPass : public IRenderPass
//	{
//	public:
//		FPass() { Type = ERenderPassType::Compute; }
//
//		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
//		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;
//
//		void Regenerate() { Type &= ~ERenderPassType::OnceFinished; }
//
//	private:
//		BOOL m_bResourceWrited = false;
//		Constant::PassConstant m_PassConstant;
//
//		TComPtr<IBuffer> m_pBuffer;
//		TComPtr<ITexture> m_pTexture;
//
//		TComPtr<IBindingLayout> m_pBindingLayout;
//
//		TComPtr<IShader> m_pCS;
//		TComPtr<IComputePipeline> m_pPipeline;
//
//		TComPtr<IBindingSet> m_pBindingSet;
//		FComputeState m_ComputeState;
//	};
//}
//
//#endif


//#include "../include/###RenderPass.h"
//#include "../../../Shader/ShaderCompiler.h"
//#include "../../../Gui/include/GuiPanel.h"
//
//
//namespace FTS
//{
//#define THREAD_GROUP_SIZE_X 16 
//#define THREAD_GROUP_SIZE_Y 16 
// 
//	BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache)
//	{
//		// Binding Layout.
//		{
//			FBindingLayoutItemArray BindingLayoutItems(N);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreatePushConstants(Slot, sizeof(Constant));
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateConstantBuffer(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_SRV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_UAV(Slot);
//			BindingLayoutItems[Index] = FBindingLayoutItem::CreateSampler(Slot);
//			ReturnIfFalse(pDevice->CreateBindingLayout(
//				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
//				IID_IBindingLayout,
//				PPV_ARG(pBindingLayout.GetAddressOf())
//			));
//		}
//
//		// Shader.
//		{
//			FShaderCompileDesc CSCompileDesc;
//			CSCompileDesc.strShaderName = ".hlsl";
//			CSCompileDesc.strEntryPoint = "CS";
//			CSCompileDesc.Target = EShaderTarget::Compute;
//			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
//			CSCompileDesc.strDefines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
//			FShaderData CSData = ShaderCompile::CompileShader(CSCompileDesc);
//
//			FShaderDesc CSDesc;
//			CSDesc.strEntryName = "CS";
//			CSDesc.ShaderType = EShaderType::Compute;
//			ReturnIfFalse(pDevice->CreateShader(CSDesc, CSData.Data(), CSData.Size(), IID_IShader, PPV_ARG(m_pCS.GetAddressOf())));
//		}
//
//		// Pipeline.
//		{
//			FComputePipelineDesc PipelineDesc;
//			PipelineDesc.CS = pCS.Get();
//			PipelineDesc.pBindingLayouts.PushBack(pBindingLayout.Get());
//			ReturnIfFalse(pDevice->CreateComputePipeline(PipelineDesc, IID_IComputePipeline, PPV_ARG(pPipeline.GetAddressOf())));
//		}
//
//
//		// Buffer.
//		{
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateConstant(sizeof(Constant)),
//				IID_IBuffer,
//				PPV_ARG(pPassConstantBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateStructured(stByteSize, dwStride, "StructuredBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pStructuredBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateRWStructured(stByteSize, dwStride, "RWStructuredBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pRWStructuredBuffer.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateBuffer(
//				FBufferDesc::CreateReadBack(stByteSize, "ReadBackBuffer"),
//				IID_IBuffer,
//				PPV_ARG(pReadBackBuffer.GetAddressOf())
//			));
//		}
//
//		// Texture.
//		{
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, EFormat, "ShaderResourceTexture"),
//				IID_ITexture,
//				PPV_ARG(pShaderResourceTexture.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, dwDepth, EFormat, "ShaderResourceTexture3D"),
//				IID_ITexture,
//				PPV_ARG(pShaderResourceTexture3D.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateReadWrite(dwWidth, dwHeight, EFormat, "ReadWriteTexture"),
//				IID_ITexture,
//				PPV_ARG(pReadWriteTexture.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateReadWrite(dwWidth, dwHeight, dwDepth, EFormat, "ReadWriteTexture3D"),
//				IID_ITexture,
//				PPV_ARG(pReadWriteTexture3D.GetAddressOf())
//			));
//			ReturnIfFalse(pDevice->CreateTexture(
//				FTextureDesc::CreateReadBack(dwWidth, dwHeight, EFormat, "ReadBackTexture"),
//				IID_ITexture,
//				PPV_ARG(pReadBackTexture.GetAddressOf())
//			));
//		}
// 
//		// Binding Set.
//		{
//			ISampler* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;
//			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
//			ReturnIfFalse(pCache->Require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));
//			ReturnIfFalse(pCache->Require("LinearWarpSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
//			ReturnIfFalse(pCache->Require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));
// 
//			FBindingSetItemArray BindingSetItems(N);
//			BindingSetItems[Index] = FBindingSetItem::CreatePushConstants(Slot, sizeof(Constant));
//			BindingSetItems[Index] = FBindingSetItem::CreateConstantBuffer(Slot, pPassConstantBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_SRV(Slot, pShaderResourceBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_UAV(Slot, pUnorderedAccessBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_SRV(Slot, pShaderResourceRawBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_UAV(Slot, pUnorderedAccessRawBuffer);
//			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_SRV(Slot, pShaderResourceTypedBuffer, pShaderResourceTypedBuffer->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_UAV(Slot, pUnorderedAccessTypedBuffer, pUnorderedAccessTypedBuffer->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTexture_SRV(Slot, pShaderResourceTexture, pShaderResourceTexture->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateTexture_UAV(Slot, pUnorderedAccessTexture, pUnorderedAccessTexture->GetDesc().Format);
//			BindingSetItems[Index] = FBindingSetItem::CreateSampler(Slot, pSampler.Get());
//			ReturnIfFalse(pDevice->CreateBindingSet(
//				FBindingSetDesc{ .BindingItems = BindingSetItems },
//				m_pBindingLayout.Get(),
//				IID_IBindingSet,
//				PPV_ARG(m_pBindingSet.GetAddressOf())
//			));
//		}
//
//		// Compute State.
//		{
//			m_ComputeState.pBindingSets.PushBack(m_pBindingSet.Get());
//			m_ComputeState.pPipeline = m_pPipeline.Get();
//		}
// 
// 		Gui::Add(
//			[this]()
//			{
//				BOOL bDirty = false;
//
//				ImGui::SeparatorText("Render Pass");
//
//				if (bDirty)
//				{
//					m_bResourceWrited = false;
//				}
//			}
//		);
// 
//		return true;
//	}
//
//	BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
//	{
//      if ((Type & ERenderPassType::OnceFinished) != ERenderPassType::OnceFinished)
//		{
//			ReturnIfFalse(pCmdList->Open());
//
//			// Update Constant.
//			{
//				m_PassConstant;
//				ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstant, sizeof(Constant::AerialLUTPassConstant)));
//			}
// 
//			if (!m_bResourceWrited)
//			{
//				m_bResourceWrited = true;
//			}
//
//
//			FVector2I ThreadGroupNum = {
//				static_cast<UINT32>((Align(OutputTextureRes.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
//				static_cast<UINT32>((Align(OutputTextureRes.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
//					};
//
//			ReturnIfFalse(pCmdList->SetComputeState(ComputeState));
//			ReturnIfFalse(pCmdList->Dispatch(ThreadGroupNum.x, ThreadGroupNum.y));
//
//			ReturnIfFalse(pCmdList->Close());
//			return true;
//		}
//	}
//}






