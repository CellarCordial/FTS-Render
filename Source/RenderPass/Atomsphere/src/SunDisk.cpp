#include "../include/SunDisk.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Scene/include/Camera.h"
#include "../../../Scene/include/Light.h"


namespace FTS
{
#define SUN_DISK_SEGMENT_NUM 32

	void FSunDiskPass::GenerateSunDiskVertices()
	{
		for (UINT32 ix = 0; ix < SUN_DISK_SEGMENT_NUM; ++ix)
		{
			const FLOAT cfPhiBegin = Lerp(0.0f, 2.0f * PI, static_cast<FLOAT>(ix) / SUN_DISK_SEGMENT_NUM);
			const FLOAT cfPhiEnd = Lerp(0.0f, 2.0f * PI, static_cast<FLOAT>(ix + 1) / SUN_DISK_SEGMENT_NUM);

			const FVector2F A = {};
			const FVector2F B = { std::cos(cfPhiBegin), std::sin(cfPhiBegin) };
			const FVector2F C = { std::cos(cfPhiEnd), std::sin(cfPhiEnd) };

			m_SunDiskVertices.push_back(Vertex{ .Position = A });
			m_SunDiskVertices.push_back(Vertex{ .Position = B });
			m_SunDiskVertices.push_back(Vertex{ .Position = C });
		}
	}


	BOOL FSunDiskPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{	
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(4);
			BindingLayoutItems[0] = FBindingLayoutItem::CreateConstantBuffer(0, false);
			BindingLayoutItems[1] = FBindingLayoutItem::CreatePushConstants(1, sizeof(Constant::SunDiskPassConstant));
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateSampler(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Input Layout.
		{
			FVertexAttributeDescArray VertexAttributesDesc(1);
			VertexAttributesDesc[0].strName = "POSITION";
			VertexAttributesDesc[0].Format = EFormat::RG32_FLOAT;
			VertexAttributesDesc[0].dwElementStride = sizeof(Vertex);
			VertexAttributesDesc[0].dwOffset = offsetof(Vertex, Position);
			ReturnIfFalse(pDevice->CreateInputLayout(
				VertexAttributesDesc.data(),
				VertexAttributesDesc.Size(),
				nullptr,
				IID_IInputLayout,
				PPV_ARG(m_pInputLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.strShaderName = "Atmosphere/SunDisk.hlsl";
			ShaderCompileDesc.strEntryPoint = "VS";
			ShaderCompileDesc.Target = EShaderTarget::Vertex;
			FShaderData VSData = ShaderCompile::CompileShader(ShaderCompileDesc);
			ShaderCompileDesc.strEntryPoint = "PS";
			ShaderCompileDesc.Target = EShaderTarget::Pixel;
			FShaderData PSData = ShaderCompile::CompileShader(ShaderCompileDesc);

			FShaderDesc VSDesc;
			VSDesc.strEntryName = "VS";
			VSDesc.ShaderType = EShaderType::Vertex;
			ReturnIfFalse(pDevice->CreateShader(VSDesc, VSData.Data(), VSData.Size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));

			FShaderDesc PSDesc;
			PSDesc.ShaderType = EShaderType::Pixel;
			PSDesc.strEntryName = "PS";
			ReturnIfFalse(pDevice->CreateShader(PSDesc, PSData.Data(), PSData.Size(), IID_IShader, PPV_ARG(m_pPS.GetAddressOf())));
		}

		// Buffer.
		{
			GenerateSunDiskVertices();
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateVertex(sizeof(Vertex) * SUN_DISK_SEGMENT_NUM * 3, "SunDiskVertexBuffer"),
				IID_IBuffer,
				PPV_ARG(m_pVertexBuffer.GetAddressOf())
			));
		}

		// Frame Buffer.
		{
			ITexture* pFinalTexture, * pDepthTexture;
			ReturnIfFalse(pCache->Require("FinalTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pFinalTexture)));
			ReturnIfFalse(pCache->Require("DepthTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pDepthTexture)));

			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pFinalTexture));
			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(pDepthTexture);
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.RenderState.DepthStencilState.bDepthTestEnable = true;
			PipelineDesc.RenderState.DepthStencilState.bDepthWriteEnable = false;
			PipelineDesc.RenderState.DepthStencilState.DepthFunc = EComparisonFunc::LessOrEqual;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.PS = m_pPS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			PipelineDesc.pInputLayout = m_pInputLayout.Get();
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
				PipelineDesc,
				m_pFrameBuffer.Get(),
				IID_IGraphicsPipeline,
				PPV_ARG(m_pPipeline.GetAddressOf())
			));
		}

		// Binding Set.
		{
			IBuffer* pAtmospherePropertiesBuffer;
			ITexture* pTransmittanceTexture;
			ISampler* pLinearClampSampler;
			ReturnIfFalse(pCache->Require("AtmospherePropertiesBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pAtmospherePropertiesBuffer)));
			ReturnIfFalse(pCache->Require("TransmittanceTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pTransmittanceTexture)));
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));


			FBindingSetItemArray BindingSetItems(4);
			BindingSetItems[0] = FBindingSetItem::CreateConstantBuffer(0, pAtmospherePropertiesBuffer);
			BindingSetItems[1] = FBindingSetItem::CreatePushConstants(1, sizeof(Constant::SunDiskPassConstant));
			BindingSetItems[2] = FBindingSetItem::CreateTexture_SRV(0, pTransmittanceTexture);
			BindingSetItems[3] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = BindingSetItems },
				m_pBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pBindingSet.GetAddressOf())
			));
		}

		// Graphics State.
		{
			m_GraphicsState.pPipeline = m_pPipeline.Get();
			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
			m_GraphicsState.pBindingSets.PushBack(m_pBindingSet.Get());
			m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = m_pVertexBuffer.Get() });
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	BOOL FSunDiskPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		if (!m_bResourceWrited)
		{
			ReturnIfFalse(pCmdList->WriteBuffer(m_pVertexBuffer.Get(), m_SunDiskVertices.data(), m_SunDiskVertices.size() * sizeof(Vertex)));
			m_bResourceWrited = true;
		}

		{
			FLOAT* pfWorldScale;
			ReturnIfFalse(pCache->RequireConstants("WorldScale", PPV_ARG(&pfWorldScale)));

			FVector3F LightDirection;
			pCache->GetWorld()->Each<FDirectionalLight>(
				[this, &LightDirection](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstant.fSunTheta = std::asin(-pLight->Direction.y);
					m_PassConstant.SunRadius = FVector3F(pLight->fIntensity * pLight->Color);

					LightDirection = pLight->Direction;
					return true;
				}
			);

			pCache->GetWorld()->Each<FCamera>(
				[&](FEntity* pEntity, FCamera* pCamera) -> BOOL
				{
					m_PassConstant.fCameraHeight = pCamera->Position.y * (*pfWorldScale);

					FMatrix3x3 OrthogonalBasis = CreateOrthogonalBasisFromZ(LightDirection);
					FMatrix4x4 WorldMatrix = Mul(
						Scale(FVector3F(m_fSunDiskSize)),
						Mul(
							FMatrix4x4(OrthogonalBasis),
							Mul(Translate(pCamera->Position), Translate(-LightDirection))
						)
					);
					m_PassConstant.WorldViewProj = Mul(WorldMatrix, pCamera->GetViewProj());

					return true;
				}
			);
		}

		ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
		ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstant, sizeof(Constant::SunDiskPassConstant)));
		ReturnIfFalse(pCmdList->Draw(FDrawArguments{ .dwIndexOrVertexCount = SUN_DISK_SEGMENT_NUM * 3 }));

		ReturnIfFalse(pCmdList->Close());

		return true;
	}

	BOOL FSunDiskPass::FinishPass()
	{
		if (!m_SunDiskVertices.empty()) m_SunDiskVertices.resize(0);
		return true;
	}

}