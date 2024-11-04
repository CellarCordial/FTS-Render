#include "../include/SdfDebug.h"
#include "../../../Gui/include/GuiPass.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Shader/ShaderCompiler.h"
#include <fstream>

namespace FTS
{
#define SDF_RESOLUTION 128

	BOOL FSdfDebugPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(3);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::SdfDebugPassConstants));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateSampler(0);
			ReturnIfFalse(pDevice->CreateBindingLayout(
				FBindingLayoutDesc{ .BindingLayoutItems = BindingLayoutItems }, 
				IID_IBindingLayout, 
				PPV_ARG(m_pBindingLayout.GetAddressOf())
			));
		}

		// Shader.
		{
			FShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.strShaderName = "SDF/SdfDebug.hlsl";
			ShaderCompileDesc.strEntryPoint = "VS";
			ShaderCompileDesc.Target = EShaderTarget::Vertex;
			FShaderData VSData = ShaderCompile::CompileShader(ShaderCompileDesc);
			ShaderCompileDesc.strEntryPoint = "PS";
			ShaderCompileDesc.Target = EShaderTarget::Pixel;
			FShaderData PSData = ShaderCompile::CompileShader(ShaderCompileDesc);

			FShaderDesc ShaderDesc;
			ShaderDesc.ShaderType = EShaderType::Vertex;
			ShaderDesc.strEntryName = "VS";
			ReturnIfFalse(pDevice->CreateShader(ShaderDesc, VSData.Data(), VSData.Size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));

			ShaderDesc.ShaderType = EShaderType::Pixel;
			ShaderDesc.strEntryName = "PS";
			ReturnIfFalse(pDevice->CreateShader(ShaderDesc, PSData.Data(), PSData.Size(), IID_IShader, PPV_ARG(m_pPS.GetAddressOf())));
		}

		// Texture.
		{
			//ReturnIfFalse(pDevice->CreateTexture(
			//	FTextureDesc::CreateShaderResource(
			//		SDF_RESOLUTION,
			//		SDF_RESOLUTION,
			//		SDF_RESOLUTION,
			//		EFormat::R32_FLOAT
			//	),
			//	IID_ITexture,
			//	PPV_ARG(m_pSdfTexture.GetAddressOf())
			//));
		}

		// Frame Buffer.
		{
			ITexture* pFinalTexture;
			ReturnIfFalse(pCache->Require("FinalTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pFinalTexture)));

			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pFinalTexture));
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.PS = m_pPS.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
				PipelineDesc, 
				m_pFrameBuffer.Get(), 
				IID_IGraphicsPipeline, 
				PPV_ARG(m_pPipeline.GetAddressOf())
			));
		}

		// Binding Set.
		{
			ISampler* pLinearClampSampler;
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(pCache->Require("SdfTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pSdfTexture)));

			FBindingSetItemArray BindingSetItems(3);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::SdfDebugPassConstants));
			BindingSetItems[1] = FBindingSetItem::CreateTexture_SRV(0, m_pSdfTexture);
			BindingSetItems[2] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
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
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}



		m_PassConstants.SdfExtent = m_PassConstants.SdfUpper - m_PassConstants.SdfLower;

		return true;
	}
	BOOL FSdfDebugPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		// Update Constant.
		{
			pCache->GetWorld()->Each<FCamera>(
				[this](FEntity* pEntity, FCamera* pCamera) -> BOOL
				{
					FCamera::FrustumDirections Directions = pCamera->GetFrustumDirections();
					m_PassConstants.FrustumA = Directions.A;
					m_PassConstants.FrustumB = Directions.B;
					m_PassConstants.FrustumC = Directions.C;
					m_PassConstants.FrustumD = Directions.D;
					m_PassConstants.CameraPosition = pCamera->Position;
					return true;
				}
			);
		}
		
		ReturnIfFalse(pCmdList->Open());

		if (!m_bResourcesWrited)
		{
			//std::ifstream fInput("D:/Document/Code/FTSRender/Asset/Sdf/Model.sdf");

			//m_SdfData.resize(SDF_RESOLUTION * SDF_RESOLUTION * SDF_RESOLUTION);

			//UINT64 stIndex = 0;
			//for (UINT32 z = 0; z < SDF_RESOLUTION; ++z)
			//	for (UINT32 y = 0; y < SDF_RESOLUTION; ++y)
			//		for (UINT32 x = 0; x < SDF_RESOLUTION; ++x)
			//			fInput >> m_SdfData[stIndex++];

			//ReturnIfFalse(pCmdList->WriteTexture(
			//	m_pSdfTexture.Get(),
			//	0,
			//	0,
			//	reinterpret_cast<UINT8*>(m_SdfData.data()),
			//	sizeof(FLOAT) * SDF_RESOLUTION,
			//	sizeof(FLOAT) * SDF_RESOLUTION * SDF_RESOLUTION
			//));

			m_bResourcesWrited = true;
		}

		ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
		ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstants, sizeof(Constant::SdfDebugPassConstants)));

		ReturnIfFalse(pCmdList->Draw(FDrawArguments{ .dwIndexOrVertexCount = 6 }));

		ReturnIfFalse(pCmdList->SetTextureState(m_pSdfTexture, FTextureSubresourceSet{}, EResourceStates::UnorderedAccess));

		ReturnIfFalse(pCmdList->Close());
		return true;
	}

	BOOL FSdfDebugRender::Setup(IRenderGraph* pRenderGraph)
	{
		ReturnIfFalse(pRenderGraph != nullptr);

		pRenderGraph->AddPass(&m_SdfGeneratePass);
		pRenderGraph->AddPass(&m_SdfDebugPass);

		FWorld* pWorld = pRenderGraph->GetResourceCache()->GetWorld();
		FEntity* pBunnyEntity = pWorld->CreateEntity();
		pWorld->Boardcast(Event::OnGeometryLoad{ .pEntity = pBunnyEntity, .FilesDirectory = "Asset/Bunny" });

		m_SdfGeneratePass.GenerateSdf(pBunnyEntity->GetComponent<FMesh>());

		return true;
	}

}