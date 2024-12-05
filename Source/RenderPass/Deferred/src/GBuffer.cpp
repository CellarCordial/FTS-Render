// #include "../include/GBuffer.h"
// #include "../../../Shader/ShaderCompiler.h"

// namespace FTS
// {
// 	BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache)
// 	{
// 		// Binding Layout.
// 		{
// 			FBindingLayoutItemArray BindingLayoutItems(N);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreatePushConstants(Slot, sizeof(Constant));
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateConstantBuffer(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_SRV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateStructuredBuffer_UAV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_SRV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateRawBuffer_UAV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_SRV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTypedBuffer_UAV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_SRV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateTexture_UAV(Slot);
// 			BindingLayoutItems[Index] = FBindingLayoutItem::CreateSampler(Slot);
// 			ReturnIfFalse(pDevice->CreateBindingLayout(
// 				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
// 				IID_IBindingLayout,
// 				PPV_ARG(m_pBindingLayout.GetAddressOf())
// 			));
// 		}

// 		// Input Layout.
// 		{
// 			FVertexAttributeDescArray VertexAttriDescs(4);
// 			VertexAttriDescs[0].strName = "POSITION";
// 			VertexAttriDescs[0].Format = EFormat::RGB32_FLOAT;
// 			VertexAttriDescs[0].dwOffset = offsetof(FVertex, Position);
// 			VertexAttriDescs[0].dwElementStride = sizeof(FVertex);
// 			VertexAttriDescs[1].strName = "NORMAL";
// 			VertexAttriDescs[1].Format = EFormat::RGB32_FLOAT;
// 			VertexAttriDescs[1].dwOffset = offsetof(FVertex, Normal);
// 			VertexAttriDescs[1].dwElementStride = sizeof(FVertex);
// 			VertexAttriDescs[2].strName = "TANGENT";
// 			VertexAttriDescs[2].Format = EFormat::RGB32_FLOAT;
// 			VertexAttriDescs[2].dwOffset = offsetof(FVertex, Tangent);
// 			VertexAttriDescs[2].dwElementStride = sizeof(FVertex);
// 			VertexAttriDescs[3].strName = "TEXCOORD";
// 			VertexAttriDescs[3].Format = EFormat::RG32_FLOAT;
// 			VertexAttriDescs[3].dwOffset = offsetof(FVertex, UV);
// 			VertexAttriDescs[3].dwElementStride = sizeof(FVertex);
// 			ReturnIfFalse(pDevice->CreateInputLayout(
// 				VertexAttriDescs.data(), 
// 				VertexAttriDescs.Size(), 
// 				nullptr, 
// 				IID_IInputLayout, 
// 				PPV_ARG(m_pInputLayout.GetAddressOf())
// 			));
// 		}


// 		// Shader.
// 		{
// 			FShaderCompileDesc ShaderCompileDesc;
// 			ShaderCompileDesc.strShaderName = ".hlsl";
// 			ShaderCompileDesc.strEntryPoint = "VS";
// 			ShaderCompileDesc.Target = EShaderTarget::Vertex;
// 			ShaderCompileDesc.strDefines.push_back("Define=" + std::to_string(Define));
// 			FShaderData VSData = ShaderCompile::CompileShader(ShaderCompileDesc);
// 			ShaderCompileDesc.strEntryPoint = "PS";
// 			ShaderCompileDesc.Target = EShaderTarget::Pixel;
// 			ShaderCompileDesc.strDefines.push_back("Define=" + std::to_string(Define));
// 			FShaderData PSData = ShaderCompile::CompileShader(ShaderCompileDesc);

// 			FShaderDesc VSDesc;
// 			VSDesc.strEntryName = "VS";
// 			VSDesc.ShaderType = EShaderType::Vertex;
// 			ReturnIfFalse(pDevice->CreateShader(VSDesc, VSData.Data(), VSData.Size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));

// 			FShaderDesc PSDesc;
// 			PSDesc.ShaderType = EShaderType::Pixel;
// 			PSDesc.strEntryName = "PS";
// 			ReturnIfFalse(pDevice->CreateShader(PSDesc, PSData.Data(), PSData.Size(), IID_IShader, PPV_ARG(m_pPS.GetAddressOf())));
// 		}

// 		// Buffer.
// 		{
// 			ReturnIfFalse(pDevice->CreateBuffer(
// 				FBufferDesc::CreateConstant(sizeof(Constant)),
// 				IID_IBuffer,
// 				PPV_ARG(pPassConstantBuffer.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateBuffer(
// 				FBufferDesc::CreateVertex(stByteSize, "VertexBuffer"),
// 				IID_IBuffer,
// 				PPV_ARG(pVertexBuffer.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateBuffer(
// 				FBufferDesc::CreateIndex(stByteSize, "IndexBuffer"),
// 				IID_IBuffer,
// 				PPV_ARG(pIndexBuffer.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateBuffer(
// 				FBufferDesc::CreateStructured(stByteSize, dwStride, "StructuredBuffer"),
// 				IID_IBuffer,
// 				PPV_ARG(pStructuredBuffer.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateBuffer(
// 				FBufferDesc::CreateReadBack(stByteSize, "ReadBackBuffer"),
// 				IID_IBuffer,
// 				PPV_ARG(pReadBackBuffer.GetAddressOf())
// 			));
// 		}

// 		// Texture.
// 		{
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, EFormat, "ShaderResourceTexture"),
// 				IID_ITexture,
// 				PPV_ARG(pShaderResourceTexture.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateShaderResource(dwWidth, dwHeight, dwDepth, EFormat, "ShaderResourceTexture3D"),
// 				IID_ITexture,
// 				PPV_ARG(pShaderResourceTexture3D.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateRenderTarget(dwWidth, dwHeight, EFormat, "RenderTargetTexture"),
// 				IID_ITexture,
// 				PPV_ARG(pRenderTargetTexture.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateDepth(dwWidth, dwHeight, EFormat, "DepthTexture"),
// 				IID_ITexture,
// 				PPV_ARG(pDepthTexture.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateReadWrite(dwWidth, dwHeight, dwDepth, EFormat, "ReadWriteTexture3D"),
// 				IID_ITexture,
// 				PPV_ARG(pReadWriteTexture3D.GetAddressOf())
// 			));
// 			ReturnIfFalse(pDevice->CreateTexture(
// 				FTextureDesc::CreateReadBack(dwWidth, dwHeight, EFormat, "ReadBackTexture"),
// 				IID_ITexture,
// 				PPV_ARG(pReadBackTexture.GetAddressOf())
// 			));
// 		}

// 		// Frame Buffer.
// 		{
// 			FFrameBufferDesc FrameBufferDesc;
// 			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture));
// 			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture));
// 			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pRenderTargetTexture));
// 			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(pDepthStencilTexture);
// 			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
// 		}

// 		// Pipeline.
// 		{
// 			FGraphicsPipelineDesc PipelineDesc;
// 			PipelineDesc.VS = m_pVS.Get();
// 			PipelineDesc.PS = m_pPS.Get();
// 			PipelineDesc.pInputLayout = m_pInputLayout.Get();
// 			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
// 			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
// 				PipelineDesc,
// 				m_pFrameBuffer.Get(),
// 				IID_IGraphicsPipeline,
// 				PPV_ARG(m_pPipeline.GetAddressOf())
// 			));
// 		}

// 		// Binding Set.
// 		{
// 			ISampler* pLinearClampSampler, * pPointClampSampler, * pLinearWarpSampler, * pPointWrapSampler;
// 			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
// 			ReturnIfFalse(pCache->Require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));
// 			ReturnIfFalse(pCache->Require("LinearWarpSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearWarpSampler)));
// 			ReturnIfFalse(pCache->Require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));

// 			FBindingSetItemArray BindingSetItems(N);
// 			BindingSetItems[Index] = FBindingSetItem::CreatePushConstants(Slot, sizeof(Constant));
// 			BindingSetItems[Index] = FBindingSetItem::CreateConstantBuffer(Slot, pPassConstantBuffer);
// 			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_SRV(Slot, pShaderResourceBuffer);
// 			BindingSetItems[Index] = FBindingSetItem::CreateStructuredBuffer_UAV(Slot, pUnorderedAccessBuffer);
// 			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_SRV(Slot, pShaderResourceRawBuffer);
// 			BindingSetItems[Index] = FBindingSetItem::CreateRawBuffer_UAV(Slot, pUnorderedAccessRawBuffer);
// 			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_SRV(Slot, pShaderResourceTypedBuffer, pShaderResourceTypedBuffer->GetDesc().Format);
// 			BindingSetItems[Index] = FBindingSetItem::CreateTypedBuffer_UAV(Slot, pUnorderedAccessTypedBuffer, pUnorderedAccessTypedBuffer->GetDesc().Format);
// 			BindingSetItems[Index] = FBindingSetItem::CreateTexture_SRV(Slot, pShaderResourceTexture, pShaderResourceTexture->GetDesc().Format);
// 			BindingSetItems[Index] = FBindingSetItem::CreateTexture_UAV(Slot, pUnorderedAccessTexture, pUnorderedAccessTexture->GetDesc().Format);
// 			BindingSetItems[Index] = FBindingSetItem::CreateSampler(Slot, pSampler.Get());
// 			ReturnIfFalse(pDevice->CreateBindingSet(
// 				FBindingSetDesc{ .BindingItems = BindingSetItems },
// 				pBindingLayout.Get(),
// 				IID_IBindingSet,
// 				PPV_ARG(m_pBindingSet.GetAddressOf())
// 			));
// 		}

// 		// Graphics State.
// 		{
// 			m_GraphicsState.pPipeline = m_pPipeline.Get();
// 			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
// 			m_GraphicsState.pBindingSets.PushBack(m_pBindingSet.Get());
// 			m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = pVertexBuffer });
// 			m_GraphicsState.IndexBufferBinding = FIndexBufferBinding{ .pBuffer = pIndexBuffer, .Format = EFormat::R32_UINT };
// 			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
// 		}


// 		Gui::Add(
// 			[this]()
// 			{
// 				BOOL bDirty = false;

// 				ImGui::SeparatorText("Render Pass");

// 				if (bDirty)
// 				{
// 					m_bResourceWrited = false;
// 				}
// 			}
// 		);
// 		return true;
// 	}

// 	BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
// 	{
// 		ReturnIfFalse(pCmdList->Open());

// 		// Update Constant.
// 		{
// 			m_PassConstant;
// 			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstant, sizeof(Constant::AerialLUTPassConstant)));
// 		}

// 		if (!m_bResourceWrited)
// 		{
// 			ReturnIfFalse(pCmdList->WriteBuffer(pVertexBuffer.Get(), Vertices.data(), sizeof(FVertex) * Vertices.size()));
// 			ReturnIfFalse(pCmdList->WriteBuffer(pIndexBuffer.Get(), Indices.data(), sizeof(EFormat::R16_UINT) * Indices.size()));
// 			m_bResourceWrited = true;
// 		}

// 		ClearColorAttachment(pCmdList, m_pFrameBuffer.Get(), 0);
// 		ReturnIfFalse(pCmdList->SetGraphicsState(GraphicsState));
// 		ReturnIfFalse(pCmdList->SetPushConstants(&PassConstant, sizeof(Constant)));
// 		ReturnIfFalse(pCmdList->DrawIndexed(DrawArgument));

// 		ReturnIfFalse(pCmdList->Close());
// 		return true;
// 	}
// }

