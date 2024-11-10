#include "../include/Sky.h"
#include "../../../Scene/include/Camera.h"
#include "../../../Shader/ShaderCompiler.h"

namespace FTS
{
	BOOL FSkyPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(3);
			BindingLayoutItems[0].Type = EResourceType::PushConstants;
			BindingLayoutItems[0].wSize = sizeof(Constant::SkyPassConstant);
			BindingLayoutItems[0].dwSlot = 0;
			BindingLayoutItems[1].Type = EResourceType::Texture_SRV;
			BindingLayoutItems[1].dwSlot = 0;
			BindingLayoutItems[2].Type = EResourceType::Sampler;
			BindingLayoutItems[2].dwSlot = 0;
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems },
				IID_IBindingLayout,
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.strShaderName = "Atmosphere/Sky.hlsl";
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

		// Texture.
		{
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateDepth(CLIENT_WIDTH, CLIENT_HEIGHT, EFormat::D32, "DepthTexture"),
				IID_ITexture,
				PPV_ARG(m_pDepthTexture.GetAddressOf())
			));
			ReturnIfFalse(pCache->Collect(m_pDepthTexture.Get()));
		}

		// Sampler.
		{
			FSamplerDesc SamplerDesc;	// U_Wrap VW_Clamp Linear
			SamplerDesc.AddressV = ESamplerAddressMode::Clamp;
			SamplerDesc.AddressW = ESamplerAddressMode::Clamp;
			ReturnIfFalse(pDevice->CreateSampler(SamplerDesc, IID_ISampler, PPV_ARG(m_pSampler.GetAddressOf())));
		}

		// Frame Buffer.
		{
			ITexture* pFinalTexture;
			ReturnIfFalse(pCache->Require("FinalTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pFinalTexture)));

			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pFinalTexture));
			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(m_pDepthTexture.Get());
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
			ITexture* pSkyLUTTexture;
			ReturnIfFalse(pCache->Require("SkyLUTTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pSkyLUTTexture)));

			FBindingSetItemArray BindingSetItems(3);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::SkyPassConstant));
			BindingSetItems[1] = FBindingSetItem::CreateTexture_SRV(0, pSkyLUTTexture);
			BindingSetItems[2] = FBindingSetItem::CreateSampler(0, m_pSampler.Get());
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = BindingSetItems },
				m_pBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pBindingSet.GetAddressOf())
			));
		}

		// Graphics State.
		{
			m_GraphicsState.pBindingSets.PushBack(m_pBindingSet.Get());
			m_GraphicsState.pPipeline = m_pPipeline.Get();
			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	BOOL FSkyPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		ReturnIfFalse(pCache->GetWorld()->Each<FCamera>(
			[this](FEntity* pEntity, FCamera* pCamera) -> BOOL
			{
				auto Frustum = pCamera->GetFrustumDirections();
				m_PassConstant.FrustumA = Frustum.A;
				m_PassConstant.FrustumB = Frustum.B;
				m_PassConstant.FrustumC = Frustum.C;
				m_PassConstant.FrustumD = Frustum.D;
				return true;
			}
		));
		ClearColorAttachment(pCmdList, m_pFrameBuffer.Get(), 0);
		ClearDepthStencilAttachment(pCmdList, m_pFrameBuffer.Get());

		ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
		ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstant, sizeof(Constant::SkyPassConstant)));
		ReturnIfFalse(pCmdList->Draw(FDrawArguments{ .dwIndexOrVertexCount = 6 }));

		ReturnIfFalse(pCmdList->Close());

		return true;
	}

}