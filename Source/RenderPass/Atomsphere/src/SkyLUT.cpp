#include "../include/SkyLUT.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Light.h"
#include "../../../Scene/include/Camera.h"


namespace FTS
{
#define SKY_LUT_RES	64

	BOOL FSkyLUTPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(5);
			BindingLayoutItems[0] = FBindingLayoutItem::CreateConstantBuffer(0, false);
			BindingLayoutItems[1] = FBindingLayoutItem::CreateConstantBuffer(1);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateTexture_SRV(1);
			BindingLayoutItems[4] = FBindingLayoutItem::CreateSampler(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.strShaderName = "Atmosphere/SkyLUT.hlsl";
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
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateConstant(sizeof(Constant::SkyLUTPassConstant)),
				IID_IBuffer,
				PPV_ARG(m_pPassConstantBuffer.GetAddressOf())
			));
		}
 
		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateRenderTarget(
					SKY_LUT_RES,
					SKY_LUT_RES,
					EFormat::RGBA32_FLOAT,
					"SkyLUTTexture"
				),
				IID_ITexture,
				PPV_ARG(m_pSkyLUTTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pSkyLUTTexture.Get()));
		}

		// Frame Buffer.
		{
			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(m_pSkyLUTTexture.Get()));
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.PS = m_pPS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(PipelineDesc, m_pFrameBuffer.Get(), IID_IGraphicsPipeline, PPV_ARG(m_pPipeline.GetAddressOf())));
		}

		// Binding Set.
		{
			IBuffer* pAtmospherePropertiesBuffer;
			ISampler* pLinearClampSampler;
			ReturnIfFalse(pCache->Require("AtmospherePropertiesBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pAtmospherePropertiesBuffer)));
			ReturnIfFalse(pCache->Require("TransmittanceTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pTransmittanceTexture)));
			ReturnIfFalse(pCache->Require("MultiScatteringTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pMultiScatteringTexture)));
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));

			FBindingSetItemArray BindingSetItems(5);
			BindingSetItems[0] = FBindingSetItem::CreateConstantBuffer(0, pAtmospherePropertiesBuffer);
			BindingSetItems[1] = FBindingSetItem::CreateConstantBuffer(1, m_pPassConstantBuffer.Get());
			BindingSetItems[2] = FBindingSetItem::CreateTexture_SRV(0, pMultiScatteringTexture);
			BindingSetItems[3] = FBindingSetItem::CreateTexture_SRV(1, pTransmittanceTexture);
			BindingSetItems[4] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
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
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(SKY_LUT_RES, SKY_LUT_RES);
		}

		return true;
	}

	BOOL FSkyLUTPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		// Update Constant.
		{
			FLOAT* pfWorldScale;
			ReturnIfFalse(pCache->RequireConstants("WorldScale", PPV_ARG(&pfWorldScale)));

			pCache->GetWorld()->Each<FCamera>(
				[this, pfWorldScale](FEntity* pEntity, FCamera* pCamera) -> BOOL
				{
					m_PassConstant.CameraPosition = pCamera->Position * (*pfWorldScale);
					return true;
				}
			);
			pCache->GetWorld()->Each<FDirectionalLight>(
				[this](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstant.SunDir = pLight->Direction;
					m_PassConstant.SunIntensity = FVector3F(pLight->fIntensity * pLight->Color);
					return true;
				}
			);
			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstantBuffer.Get(), &m_PassConstant, sizeof(Constant::SkyLUTPassConstant)));
		}

		ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
		ReturnIfFalse(pCmdList->Draw(FDrawArguments{ .dwIndexOrVertexCount = 6 }));

		ReturnIfFalse(pCmdList->SetTextureState(pTransmittanceTexture, FTextureSubresourceSet{}, EResourceStates::NonPixelShaderResource));
		ReturnIfFalse(pCmdList->SetTextureState(pMultiScatteringTexture, FTextureSubresourceSet{}, EResourceStates::NonPixelShaderResource));

		ReturnIfFalse(pCmdList->Close());

		return true;
	}

}