#include "../include/SurfaceCapture.h"
#include "../../../Shader/ShaderCompiler.h"
#include <string>
#include <wincrypt.h>

namespace FTS
{
	BOOL FSurfaceCapturePass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		pCache->GetWorld()->GetGlobalEntity()->GetComponent<Event::GenerateSurfaceCache>()->AddEvent(
			[this](FEntity* pEntity) -> BOOL
			{
				ContinuePrecompute();
				m_pModelEntity = pEntity;
				ReturnIfFalse(m_pModelEntity != nullptr);

				FSurfaceCache* pSurfaceCache = m_pModelEntity->GetComponent<FSurfaceCache>();
				if (!pSurfaceCache->CheckSurfaceCacheExist())
				{
					const auto& crMeshDF = pSurfaceCache->MeshSurfaceCaches[0];
					std::string strSurfaceCacheName = *m_pModelEntity->GetComponent<std::string>() + ".sc";
					pBinaryOutput = std::make_unique<Serialization::BinaryOutput>(std::string(PROJ_DIR) + "Asset/SurfaceCache/" + strSurfaceCacheName);
				}
				return true;
			}
		);

		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(7);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::SurfaceCapturePassConstant));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateTexture_SRV(1);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateTexture_SRV(2);
			BindingLayoutItems[4] = FBindingLayoutItem::CreateTexture_SRV(3);
			BindingLayoutItems[5] = FBindingLayoutItem::CreateTexture_SRV(4);
			BindingLayoutItems[6] = FBindingLayoutItem::CreateSampler(0);
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
			FShaderCompileDesc ShaderCompileDesc;
			ShaderCompileDesc.strShaderName = "SurfaceCache/SurfaceCapture.hlsl";
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

		
		ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&m_pLinearClampSampler)));

		// Graphics State.
		{
			m_GraphicsState.pBindingSets.Resize(1);
			m_GraphicsState.VertexBufferBindings.Resize(1);
		}

		return true;
	}

	BOOL FSurfaceCapturePass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFailed(pCmdList->Open());

		FSurfaceCache* pSurfaceCache = m_pModelEntity->GetComponent<FSurfaceCache>();
		if (pSurfaceCache->CheckSurfaceCacheExist())
		{
			IDevice* pDevice = pCmdList->GetDevice();
			for (UINT32 ix = 0; ix < pSurfaceCache->MeshSurfaceCaches.size(); ++ix)
			{
				for (const auto& crSurface : pSurfaceCache->MeshSurfaceCaches[ix].Surfaces)
				{
					ITexture* pSurfaceTexture;
					ReturnIfFalse(pDevice->CreateTexture(
						FTextureDesc::CreateShaderResource(
							gdwSurfaceResolution, 
							gdwSurfaceResolution, 
							pSurfaceCache->Format, 
							crSurface.strSurfaceTextureName.c_str()
						),
						IID_ITexture,
						PPV_ARG(&pSurfaceTexture)
					));
					ReturnIfFalse(pCache->Collect(pSurfaceTexture));

					UINT32 dwPixelSize = GetFormatInfo(pSurfaceCache->Format).btBytesPerBlock;
					ReturnIfFalse(pCmdList->WriteTexture(pSurfaceTexture, 0, 0, crSurface.Data.data(), gdwSurfaceResolution * dwPixelSize));
				}
			}
		}
		else 
		{
			ReturnIfFailed(SetupPipeline(pCmdList, pCache, pSurfaceCache));
			ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
			ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstant, sizeof(Constant::SurfaceCapturePassConstant)));
			ReturnIfFalse(pCmdList->DrawIndexed(m_DrawArguments));
		}
		ReturnIfFailed(pCmdList->Close());
		return true;
	}

	BOOL FSurfaceCapturePass::FinishPass()
	{
		FSurfaceCache* pSurfaceCache = m_pModelEntity->GetComponent<FSurfaceCache>();
		if (++m_dwCurrMeshSurfaceCacheIndex == static_cast<UINT32>(pSurfaceCache->MeshSurfaceCaches.size()))
		{

			m_dwCurrMeshSurfaceCacheIndex = 0;
			m_pModelEntity = nullptr;
		}
		else 
		{
			ContinuePrecompute();
		}
		return true;
	}


	BOOL FSurfaceCapturePass::SetupPipeline(ICommandList* pCmdList, IRenderResourceCache* pCache, FSurfaceCache* pSurfaceCache)
	{
		IDevice* pDevice = pCmdList->GetDevice();

		// Frame Buffer.
		{
			m_pFrameBuffer.Reset();

			FFrameBufferDesc FrameBufferDesc;
			for (const auto& crSurface : pSurfaceCache->MeshSurfaceCaches[m_dwCurrMeshSurfaceCacheIndex].Surfaces)
			{
				ITexture* pSurfaceTexture;
				ReturnIfFalse(pDevice->CreateTexture(
					FTextureDesc::CreateShaderResource(
						gdwSurfaceResolution, 
						gdwSurfaceResolution, 
						pSurfaceCache->Format, 
						crSurface.strSurfaceTextureName.c_str()
					),
					IID_ITexture,
					PPV_ARG(&pSurfaceTexture)
				));
				ReturnIfFalse(pCache->Collect(pSurfaceTexture));
				FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(pSurfaceTexture));
			}
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			m_pPipeline.Reset();
			
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.PS = m_pPS.Get();
			PipelineDesc.pInputLayout = m_pInputLayout.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
				PipelineDesc,
				m_pFrameBuffer.Get(),
				IID_IGraphicsPipeline,
				PPV_ARG(m_pPipeline.GetAddressOf())
			));
		}


		const auto& crModelName = *m_pModelEntity->GetComponent<std::string>();
		const auto& crSubmesh = m_pModelEntity->GetComponent<FMesh>()->Submeshes[m_dwCurrMeshSurfaceCacheIndex];
		const auto& crMaterial = m_pModelEntity->GetComponent<FMaterial>()->SubMaterials[crSubmesh.dwMaterialIndex];

		// Buffer.
		{
			m_pVertexBuffer.Reset();
			m_pIndexBuffer.Reset();

			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateVertex(crSubmesh.Vertices.size() * sizeof(FVertex)), 
				IID_IBuffer, 
				PPV_ARG(m_pVertexBuffer.GetAddressOf())
			));
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateIndex(crSubmesh.Indices.size() * sizeof(UINT32)), 
				IID_IBuffer, 
				PPV_ARG(m_pIndexBuffer.GetAddressOf())
			));

			ReturnIfFalse(pCmdList->WriteBuffer(m_pVertexBuffer.Get(), crSubmesh.Vertices.data(), crSubmesh.Vertices.size() * sizeof(FVertex)));
			ReturnIfFalse(pCmdList->WriteBuffer(m_pIndexBuffer.Get(), crSubmesh.Indices.data(), crSubmesh.Indices.size() * sizeof(UINT32)));
		}



		ITexture* pMaterialTextures[FMaterial::TextureType_Num];

		// Texture.
		{
			static_assert(FMaterial::TextureType_Num == 5);
			const CHAR* strMaterialTextureNames[FMaterial::TextureType_Num] = 
			{ 
				"Diffuse",
				"Normal",
				"Emissive",
				"Occlusion",
				"MetallicRoughness" 
			};
			
			for (UINT32 ix = 0; ix < FMaterial::TextureType_Num; ++ix)
			{
				std::string strTextureName = crModelName + strMaterialTextureNames[ix] + std::to_string(m_dwCurrMeshSurfaceCacheIndex);
				ReturnIfFalse(pDevice->CreateTexture(
					FTextureDesc::CreateShaderResource(
						crMaterial.Images[ix].Width, 
						crMaterial.Images[ix].Height, 
						0, 
						crMaterial.Images[ix].Format,
						strTextureName.c_str()
					), 
					IID_ITexture, 
					PPV_ARG(&pMaterialTextures[ix])
				));
				ReturnIfFalse(pCmdList->WriteTexture(
					pMaterialTextures[ix], 
					0, 
					0, 
					crMaterial.Images[ix].Data.get(), 
					crMaterial.Images[ix].stSize / crMaterial.Images[ix].Height)
				);

				ReturnIfFalse(pCache->Collect(pMaterialTextures[ix]));
			}
		}

		// Binding Set.
		{
			m_pBindingSet.Reset();

			FBindingSetItemArray BindingSetItems(7);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::SurfaceCapturePassConstant));
			BindingSetItems[1] = FBindingSetItem::CreateTexture_SRV(0, pMaterialTextures[0]);
			BindingSetItems[2] = FBindingSetItem::CreateTexture_SRV(1, pMaterialTextures[1]);
			BindingSetItems[3] = FBindingSetItem::CreateTexture_SRV(2, pMaterialTextures[2]);
			BindingSetItems[4] = FBindingSetItem::CreateTexture_SRV(3, pMaterialTextures[3]);
			BindingSetItems[5] = FBindingSetItem::CreateTexture_SRV(4, pMaterialTextures[4]);
			BindingSetItems[6] = FBindingSetItem::CreateSampler(0, m_pLinearClampSampler);
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
			m_GraphicsState.pBindingSets[0] = m_pBindingSet.Get();
			m_GraphicsState.VertexBufferBindings[0] = FVertexBufferBinding{ .pBuffer = m_pVertexBuffer.Get() };
			m_GraphicsState.IndexBufferBinding = FIndexBufferBinding{ .pBuffer = m_pIndexBuffer.Get(), .Format = EFormat::R32_UINT };
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		// Draw Arguments.
		{
			m_DrawArguments.dwIndexOrVertexCount = static_cast<UINT32>(crSubmesh.Indices.size());
		}
		return true;
	}

}

