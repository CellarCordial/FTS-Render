#include "../include/ShadowMap.h"
#include "../../../Scene/include/Light.h"
#include "../../../Shader/ShaderCompiler.h"

namespace FTS
{
#define SHADOW_MAP_RESOLUTION 2048

	BOOL FShadowMapPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(1);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::ShadowMapPassConstant));
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Input Layout.
		{
			FVertexAttributeDescArray VertexAttriDescs(4);
			VertexAttriDescs[0].strName = "POSITION";
			VertexAttriDescs[0].Format = EFormat::RGB32_FLOAT;
			VertexAttriDescs[0].dwOffset = offsetof(FVertex, Position);
			VertexAttriDescs[0].dwElementStride = sizeof(FVertex);
			VertexAttriDescs[1].strName = "NORMAL";
			VertexAttriDescs[1].Format = EFormat::RGB32_FLOAT;
			VertexAttriDescs[1].dwOffset = offsetof(FVertex, Normal);
			VertexAttriDescs[1].dwElementStride = sizeof(FVertex);
			VertexAttriDescs[2].strName = "TANGENT";
			VertexAttriDescs[2].Format = EFormat::RGB32_FLOAT;
			VertexAttriDescs[2].dwOffset = offsetof(FVertex, Tangent);
			VertexAttriDescs[2].dwElementStride = sizeof(FVertex);
			VertexAttriDescs[3].strName = "TEXCOORD";
			VertexAttriDescs[3].Format = EFormat::RG32_FLOAT;
			VertexAttriDescs[3].dwOffset = offsetof(FVertex, UV);
			VertexAttriDescs[3].dwElementStride = sizeof(FVertex);
			ReturnIfFalse(pDevice->CreateInputLayout(
				VertexAttriDescs.data(), 
				VertexAttriDescs.Size(), 
				nullptr, 
				IID_IInputLayout, 
				PPV_ARG(m_pInputLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc VSCompileDesc;
			VSCompileDesc.strShaderName = "Atmosphere/ShadowMap.hlsl";
			VSCompileDesc.strEntryPoint = "VS";
			VSCompileDesc.Target = EShaderTarget::Vertex;
			FShaderData VSData = ShaderCompile::CompileShader(VSCompileDesc);

			FShaderDesc VSDesc;
			VSDesc.strEntryName = "VS";
			VSDesc.ShaderType = EShaderType::Vertex;
			ReturnIfFalse(pDevice->CreateShader(VSDesc, VSData.Data(), VSData.Size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));
		}

		// Load Vertices.
		{
			ReturnIfFalse(pCache->GetWorld()->Each<FMesh>(
				[this](FEntity* pEntity, FMesh* pMesh) -> BOOL
				{
					UINT64 stOldSize = m_DrawArguments.size();
					m_DrawArguments.resize(stOldSize + pMesh->Submeshes.size());
					m_DrawArguments[stOldSize].dwIndexOrVertexCount = static_cast<UINT32>(pMesh->Submeshes[0].Indices.size());

					if (stOldSize > 0)
					{
						m_DrawArguments[stOldSize].dwStartIndexLocation = m_Indices.size();
						m_DrawArguments[stOldSize].dwStartVertexLocation = m_Vertices.size();
					}

					for (UINT64 ix = 0; ix < pMesh->Submeshes.size(); ++ix)
					{
						const auto& crSubmesh = pMesh->Submeshes[ix];
						m_Vertices.insert(m_Vertices.end(), crSubmesh.Vertices.begin(), crSubmesh.Vertices.end());
						m_Indices.insert(m_Indices.end(), crSubmesh.Indices.begin(), crSubmesh.Indices.end());

						if (ix != 0)
						{
							m_DrawArguments[ix].dwIndexOrVertexCount = crSubmesh.Indices.size();
							m_DrawArguments[ix].dwStartIndexLocation = m_DrawArguments[ix - 1].dwStartIndexLocation + pMesh->Submeshes[ix - 1].Indices.size();
							m_DrawArguments[ix].dwStartVertexLocation = m_DrawArguments[ix - 1].dwStartVertexLocation + pMesh->Submeshes[ix - 1].Vertices.size();
						}
					}

					return true;
				}
			));
		}

		// Buffer.
		{
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateVertex(sizeof(FVertex) * m_Vertices.size(), "GeometryVertexBuffer"),
				IID_IBuffer,
				PPV_ARG(m_pVertexBuffer.GetAddressOf())
			));

			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateIndex(sizeof(UINT32) * m_Indices.size(), "GeometryIndexBuffer"),
				IID_IBuffer,
				PPV_ARG(m_pIndexBuffer.GetAddressOf())
			));

			ReturnIfFalse(pCache->Collect(m_pVertexBuffer.Get()));
			ReturnIfFalse(pCache->Collect(m_pIndexBuffer.Get()));
		}

		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateDepth(CLIENT_WIDTH, CLIENT_HEIGHT, EFormat::D32, "ShadowMapTexture"),
				IID_ITexture,
				PPV_ARG(m_pShadowMapTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pShadowMapTexture.Get()));
		}

		// Frame Buffer.
		{
			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(m_pShadowMapTexture.Get());
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.pInputLayout = m_pInputLayout.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			PipelineDesc.RenderState.DepthStencilState.bDepthTestEnable = true;
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(PipelineDesc, m_pFrameBuffer.Get(), IID_IGraphicsPipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}

		// Graphics State.
		{
			m_GraphicsState.pPipeline = m_pPipeline.Get();
			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
			m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = m_pVertexBuffer.Get()});
			m_GraphicsState.IndexBufferBinding = FIndexBufferBinding{ .pBuffer = m_pIndexBuffer.Get(), .Format = EFormat::R32_UINT };
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		ReturnIfFalse(pCache->CollectConstants("GeometryDrawArguments", m_DrawArguments.data(), m_DrawArguments.size()));

		return true;
	}

	BOOL FShadowMapPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		ClearDepthStencilAttachment(pCmdList, m_pFrameBuffer.Get());

		if (!m_bResourceWrited)
		{
			ReturnIfFalse(pCmdList->WriteBuffer(m_pVertexBuffer.Get(), m_Vertices.data(), m_Vertices.size() * sizeof(FVertex)));
			ReturnIfFalse(pCmdList->WriteBuffer(m_pIndexBuffer.Get(), m_Indices.data(), m_Indices.size() * sizeof(UINT32)));
			m_bResourceWrited = true;
		}

		// Update Constant.
		{
			ReturnIfFalse(pCache->GetWorld()->Each<FDirectionalLight>(
				[this](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstant.DirectionalLightViewProj = pLight->ViewProj;
					return true;
				}
			));
		}

		UINT64 stSubmeshIndex = 0;
		ReturnIfFalse(pCache->GetWorld()->Each<FMesh>(
			[this, pCmdList, &stSubmeshIndex](FEntity* pEntity, FMesh* pMesh) -> BOOL
			{
				for (UINT64 ix = 0; ix < pMesh->Submeshes.size(); ++ix)
				{
					m_PassConstant.WorldMatrix = pMesh->Submeshes[ix].WorldMatrix;

					ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
					ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstant, sizeof(Constant::ShadowMapPassConstant)));

					ReturnIfFalse(pCmdList->DrawIndexed(m_DrawArguments[stSubmeshIndex++]));
				}
				return true;
			}
		));
		ReturnIfFalse(stSubmeshIndex == m_DrawArguments.size());

		ReturnIfFalse(pCmdList->SetTextureState(m_pShadowMapTexture.Get(), FTextureSubresourceSet{}, EResourceStates::NonPixelShaderResource));

		ReturnIfFalse(pCmdList->Close());

		return true;
	}

	BOOL FShadowMapPass::FinishPass()
	{
		m_Vertices.clear();
		m_Indices.clear();
		return true;
	}

}