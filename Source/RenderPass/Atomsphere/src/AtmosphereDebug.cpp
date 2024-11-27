#include "../include/AtmosphereDebug.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Scene/include/Scene.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Camera.h"
#include "../../../Scene/include/Light.h"

namespace FTS
{
	BOOL FAtmosphereDebugPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		// Binding Layout.
		{
			FBindingLayoutItemArray BindingLayoutItems(10);
			BindingLayoutItems[0] = FBindingLayoutItem::CreatePushConstants(0, sizeof(Constant::AtmosphereDebugPassConstant0));
			BindingLayoutItems[1] = FBindingLayoutItem::CreateConstantBuffer(1, false);
			BindingLayoutItems[2] = FBindingLayoutItem::CreateConstantBuffer(2);
			BindingLayoutItems[3] = FBindingLayoutItem::CreateTexture_SRV(0);
			BindingLayoutItems[4] = FBindingLayoutItem::CreateTexture_SRV(1);
			BindingLayoutItems[5] = FBindingLayoutItem::CreateTexture_SRV(2);
			BindingLayoutItems[6] = FBindingLayoutItem::CreateTexture_SRV(3);
			BindingLayoutItems[7] = FBindingLayoutItem::CreateSampler(0);
			BindingLayoutItems[8] = FBindingLayoutItem::CreateSampler(1);
			BindingLayoutItems[9] = FBindingLayoutItem::CreateSampler(2);
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
			ShaderCompileDesc.strShaderName = "Atmosphere/AtmosphereDebug.hlsl";
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

		// Frame Buffer.
		{
			ITexture* pDepthTexture;
			ReturnIfFalse(pCache->Require("FinalTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pFinalTexture)));
			ReturnIfFalse(pCache->Require("DepthTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pDepthTexture)));
			FFrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.ColorAttachments.PushBack(FFrameBufferAttachment::CreateAttachment(m_pFinalTexture));
			FrameBufferDesc.DepthStencilAttachment = FFrameBufferAttachment::CreateAttachment(pDepthTexture);
			ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));
		}

		// Pipeline.
		{
			FGraphicsPipelineDesc PipelineDesc;
			PipelineDesc.VS = m_pVS.Get();
			PipelineDesc.PS = m_pPS.Get();
			PipelineDesc.pInputLayout = m_pInputLayout.Get();
			PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
			PipelineDesc.RenderState.DepthStencilState.bDepthTestEnable = true;
			ReturnIfFalse(pDevice->CreateGraphicsPipeline(
				PipelineDesc,
				m_pFrameBuffer.Get(),
				IID_IGraphicsPipeline,
				PPV_ARG(m_pPipeline.GetAddressOf())
			));
		}

		// Buffer.
		{
			ReturnIfFalse(pDevice->CreateBuffer(
				FBufferDesc::CreateConstant(sizeof(Constant::AtmosphereDebugPassConstant1)),
				IID_IBuffer,
				PPV_ARG(m_pPassConstant1Buffer.GetAddressOf())
			));
		}

		// Texture.
		{
			std::string strImagePath = std::string(PROJ_DIR) + "Asset/Images/BlueNoise.png";
			m_BlueNoiseImage = Image::LoadImageFromFile(strImagePath.c_str());
			ReturnIfFalse(pDevice->CreateTexture(
				FTextureDesc::CreateShaderResource(
					m_BlueNoiseImage.Width,
					m_BlueNoiseImage.Height,
					m_BlueNoiseImage.Format
				),
				IID_ITexture,
				PPV_ARG(m_pBlueNoiseTexture.GetAddressOf())
			));
		}

		// Binding Set.
		{
			IBuffer* pAtmospherePropertiesBuffer;
			ITexture* pShadowMapTexture;
			ISampler* pLinearClampSampler, * pPointClampSampler, * pPointWrapSampler;
			ReturnIfFalse(pCache->Require("AtmospherePropertiesBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pAtmospherePropertiesBuffer)));
			ReturnIfFalse(pCache->Require("TransmittanceTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pTransmittanceTexture)));
			ReturnIfFalse(pCache->Require("MultiScatteringTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pMultiScatteringTexture)));
			ReturnIfFalse(pCache->Require("ShadowMapTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pShadowMapTexture)));
			ReturnIfFalse(pCache->Require("AerialLUTTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pAerialLUTTexture)));
			ReturnIfFalse(pCache->Require("LinearClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pLinearClampSampler)));
			ReturnIfFalse(pCache->Require("PointWrapSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointWrapSampler)));
			ReturnIfFalse(pCache->Require("PointClampSampler")->QueryInterface(IID_ISampler, PPV_ARG(&pPointClampSampler)));

			FBindingSetItemArray BindingSetItems(10);
			BindingSetItems[0] = FBindingSetItem::CreatePushConstants(0, sizeof(Constant::AtmosphereDebugPassConstant0));
			BindingSetItems[1] = FBindingSetItem::CreateConstantBuffer(1, pAtmospherePropertiesBuffer);
			BindingSetItems[2] = FBindingSetItem::CreateConstantBuffer(2, m_pPassConstant1Buffer.Get());
			BindingSetItems[3] = FBindingSetItem::CreateTexture_SRV(0, m_pTransmittanceTexture);
			BindingSetItems[4] = FBindingSetItem::CreateTexture_SRV(1, m_pAerialLUTTexture);
			BindingSetItems[5] = FBindingSetItem::CreateTexture_SRV(2, pShadowMapTexture);
			BindingSetItems[6] = FBindingSetItem::CreateTexture_SRV(3, m_pBlueNoiseTexture.Get());
			BindingSetItems[7] = FBindingSetItem::CreateSampler(0, pLinearClampSampler);
			BindingSetItems[8] = FBindingSetItem::CreateSampler(1, pPointClampSampler);
			BindingSetItems[9] = FBindingSetItem::CreateSampler(2, pPointWrapSampler);
			ReturnIfFalse(pDevice->CreateBindingSet(
				FBindingSetDesc{ .BindingItems = BindingSetItems },
				m_pBindingLayout.Get(),
				IID_IBindingSet,
				PPV_ARG(m_pBindingSet.GetAddressOf())
			));
		}

		// Graphics State.
		{
			IBuffer* pGeometryVertexBuffer, * pGeometryIndexBuffer;
			ReturnIfFalse(pCache->Require("GeometryVertexBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pGeometryVertexBuffer)));
			ReturnIfFalse(pCache->Require("GeometryIndexBuffer")->QueryInterface(IID_IBuffer, PPV_ARG(&pGeometryIndexBuffer)));

			m_GraphicsState.pPipeline = m_pPipeline.Get();
			m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
			m_GraphicsState.pBindingSets.PushBack(m_pBindingSet.Get());
			m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = pGeometryVertexBuffer });
			m_GraphicsState.IndexBufferBinding = FIndexBufferBinding{ .pBuffer = pGeometryIndexBuffer, .Format = EFormat::R32_UINT };
			m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		ReturnIfFalse(pCache->RequireConstants("GeometryDrawArguments", PPV_ARG(&m_DrawArguments), &m_stDrawArgumentsSize));


		return true;
	}

	BOOL FAtmosphereDebugPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		// Update Constant.
		{
			FLOAT* pfWorldScale;
			FVector3F* pGroundAlbedo;
			ReturnIfFalse(pCache->RequireConstants("WorldScale", PPV_ARG(&pfWorldScale)));
			ReturnIfFalse(pCache->RequireConstants("GroundAlbedo", PPV_ARG(&pGroundAlbedo)));
			m_PassConstant1.fWorldScale = *pfWorldScale;
			m_PassConstant1.GroundAlbedo = *pGroundAlbedo;

			FTextureDesc AerialLUTDesc = m_pAerialLUTTexture->GetDesc();
			m_PassConstant1.JitterFactor = {
				m_fJitterRadius / AerialLUTDesc.dwWidth,
				m_fJitterRadius / AerialLUTDesc.dwHeight
			};
			m_PassConstant1.BlueNoiseUVFactor = {
				(1.0f * CLIENT_WIDTH) / m_BlueNoiseImage.Width,
				(1.0f * CLIENT_HEIGHT) / m_BlueNoiseImage.Height
			};

			ReturnIfFalse(pCache->GetWorld()->Each<FDirectionalLight>(
				[this](FEntity* pEntity, FDirectionalLight* pLight) -> BOOL
				{
					m_PassConstant1.SunDirection = pLight->Direction;
					m_PassConstant1.fSunTheta = std::asin(-pLight->Direction.y);
					m_PassConstant1.SunRadiance = FVector3F(pLight->fIntensity * pLight->Color);
					m_PassConstant1.ShadowViewProj = pLight->ViewProj;
					return true;
				}
			));


			ReturnIfFalse(pCache->GetWorld()->Each<FCamera>(
				[this](FEntity* pEntity, FCamera* pCamera) -> BOOL
				{
					m_PassConstant0.ViewProj = pCamera->GetViewProj();
					m_PassConstant1.CameraPos = pCamera->Position;
					return true;
				}
			));

			ReturnIfFalse(pCmdList->WriteBuffer(m_pPassConstant1Buffer.Get(), &m_PassConstant1, sizeof(Constant::AtmosphereDebugPassConstant1)));

			UINT64 stSubmeshIndex = 0;
			ReturnIfFalse(pCache->GetWorld()->Each<FMesh>(
				[this, pCmdList, &stSubmeshIndex](FEntity* pEntity, FMesh* pMesh) -> BOOL
				{
					for (UINT64 ix = 0; ix < pMesh->Submeshes.size(); ++ix)
					{
						m_PassConstant0.WorldMatrix = pMesh->Submeshes[ix].WorldMatrix;

						ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));
						ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstant0, sizeof(Constant::AtmosphereDebugPassConstant0)));

						ReturnIfFalse(pCmdList->DrawIndexed(m_DrawArguments[stSubmeshIndex++]));
					}
					return true;
				}
			));
			ReturnIfFalse(stSubmeshIndex == m_stDrawArgumentsSize);
		}

		if (!m_bWritedResource)
		{
			ReturnIfFalse(pCmdList->WriteTexture(m_pBlueNoiseTexture.Get(), 0, 0, m_BlueNoiseImage.Data.get(), m_BlueNoiseImage.stSize / m_BlueNoiseImage.Height));
			m_bWritedResource = true;
		}

		ReturnIfFalse(pCmdList->SetTextureState(m_pAerialLUTTexture, FTextureSubresourceSet{}, EResourceStates::Common));
		ReturnIfFalse(pCmdList->SetTextureState(m_pTransmittanceTexture, FTextureSubresourceSet{}, EResourceStates::Common));
		ReturnIfFalse(pCmdList->SetTextureState(m_pMultiScatteringTexture, FTextureSubresourceSet{}, EResourceStates::UnorderedAccess));

		ReturnIfFalse(pCmdList->Close());
		return true;
	}


	BOOL FAtmosphereDebugRender::Setup(IRenderGraph* pRenderGraph)
	{
		ReturnIfFalse(pRenderGraph != nullptr);

		pRenderGraph->AddPass(&m_TransmittanceLUTPass);
		pRenderGraph->AddPass(&m_MultiScatteringLUTPass);
		pRenderGraph->AddPass(&m_ShadowMapPass);
		pRenderGraph->AddPass(&m_SkyLUTPass);
		pRenderGraph->AddPass(&m_AerialLUTPass);
		pRenderGraph->AddPass(&m_SkyPass);
		pRenderGraph->AddPass(&m_SunDiskPass);
		pRenderGraph->AddPass(&m_AtmosphereDebugPass);

		m_TransmittanceLUTPass.Precede(&m_MultiScatteringLUTPass);


		m_ShadowMapPass.Precede(&m_AerialLUTPass);
		m_SkyLUTPass.Precede(&m_AerialLUTPass);
		m_SkyLUTPass.Precede(&m_SkyPass);
		m_AerialLUTPass.Precede(&m_SunDiskPass);
		m_SkyPass.Precede(&m_SunDiskPass);
		m_SunDiskPass.Precede(&m_AtmosphereDebugPass);


		IRenderResourceCache* pCache = pRenderGraph->GetResourceCache();
		pCache->CollectConstants("WorldScale", &m_fWorldScale);
		pCache->CollectConstants("GroundAlbedo", &m_GroundAlbedo);


		FWorld* pWorld = pCache->GetWorld();
		FEntity* pEntity = pWorld->CreateEntity();
		Constant::AtmosphereProperties* pProperties = pEntity->Assign<Constant::AtmosphereProperties>();


		ReturnIfFalse(pWorld->Boardcast(Event::OnModelLoad{ 
			.pEntity = pWorld->CreateEntity(),
			.strModelPath = "Asset/Model/Mountain/terrain.gltf" 
		}));


		FDirectionalLight Light;
		FLOAT X = Radians(Light.Angle.x);
		FLOAT Y = Radians(-Light.Angle.y);
		Light.Direction = Normalize(FVector3F(
			std::cos(X) * std::cos(Y),
			std::sin(Y),
			std::sin(X) * std::cos(Y)
		));

		Light.ViewProj = Mul(
			LookAtLeftHand(-Light.Direction * 20.0f, FVector3F{}, FVector3F(0.0f, 1.0f, 0.0f)),
			OrthographicLeftHand(20.0f, 20.0f, 0.1f, 80.0f)
		);

		FEntity* pLightEntity = pWorld->CreateEntity();
		FDirectionalLight* pLight = pLightEntity->Assign<FDirectionalLight>(Light);


		Gui::Add(
			[this, pProperties, pLight]()
			{
				if (ImGui::CollapsingHeader("Atmosphere Debug Render"))
				{
					BOOL bDirty = false;
					if (ImGui::TreeNode("Atmosphere Properties"))
					{
						bDirty |= ImGui::SliderFloat("Planet Radius         (km)   ", &pProperties->fPlanetRadius, 0.0f, 10000.0f);
						bDirty |= ImGui::SliderFloat("Atmosphere Radius     (km)   ", &pProperties->fAtmosphereRadius, 0.0f, 10000.0f);
						bDirty |= ImGui::SliderFloat3("Rayleight Scattering  (um^-1)", &pProperties->RayleighScatter.x, 0.0f, 100.0f);
						bDirty |= ImGui::SliderFloat("Rayleight Density H   (km)   ", &pProperties->fRayleighDensity, 0.0f, 30.0f);
						bDirty |= ImGui::SliderFloat("Mie Scatter           (um^-1)", &pProperties->fMieScatter, 0.0f, 10.0f);
						bDirty |= ImGui::SliderFloat("Mie Absorb            (um^-1)", &pProperties->fMieAbsorb, 0.0f, 10.0f);
						bDirty |= ImGui::SliderFloat("Mie Density           (km)   ", &pProperties->fMieDensity, 0.0f, 10.0f);
						bDirty |= ImGui::SliderFloat("Mie Scatter Asymmetry        ", &pProperties->fMieAsymmetry, 0.0f, 1.0f);
						bDirty |= ImGui::SliderFloat3("Ozone Absorb          (um^-1)", &pProperties->OzoneAbsorb.x, 0.0f, 5.0f);
						bDirty |= ImGui::SliderFloat("Ozone Center Height   (km)   ", &pProperties->fOzoneCenterHeight, 0.0f, 100.0f);
						bDirty |= ImGui::SliderFloat("Ozone Thickness       (km)   ", &pProperties->fOzoneThickness, 0.0f, 100.0f);

						if (ImGui::Button("Reset"))
						{
							bDirty = true;
							*pProperties = Constant::AtmosphereProperties{};
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Multi Scatter Pass"))
					{
						bDirty |= ImGui::SliderInt("Ray March Step Count", &m_MultiScatteringLUTPass.m_PassConstants.dwRayMarchStepCount, 10, 500);
						bDirty |= ImGui::ColorEdit3("Ground Albedo", &m_GroundAlbedo.x);

						if (ImGui::Button("Reset"))
						{
							bDirty = true;
							m_MultiScatteringLUTPass.m_PassConstants.dwRayMarchStepCount = 256;
							m_GroundAlbedo = { 0.3f, 0.3f, 0.3f };
						}

						ImGui::TreePop();
					}

					static BOOL bEnableMultiScattering = true;
					static BOOL bEnableShadow = true;

					if (ImGui::TreeNode("Sky LUT Pass"))
					{
						ImGui::SliderInt("Ray March Step Count", &m_SkyLUTPass.m_PassConstant.dwMarchStepCount, 10, 100);

						if (ImGui::Button("Reset"))
						{
							m_SkyLUTPass.m_PassConstant.dwMarchStepCount = 40;
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Aerial LUT Pass"))
					{
						ImGui::SliderFloat("Max Aerial Distance", &m_AerialLUTPass.m_PassConstant.fMaxAerialDistance, 100.0f, 5000.0f);
						ImGui::SliderInt("Per Slice March Step Count", &m_AerialLUTPass.m_PassConstant.dwPerSliceMarchStepCount, 1, 100);

						m_AtmosphereDebugPass.m_PassConstant1.fMaxAerialDistance = m_AerialLUTPass.m_PassConstant.fMaxAerialDistance;

						if (ImGui::Button("Reset"))
						{
							m_AerialLUTPass.m_PassConstant.fMaxAerialDistance = 2000.0f;
							m_AerialLUTPass.m_PassConstant.dwPerSliceMarchStepCount = 1;
						}
						ImGui::TreePop();
					}


					if (ImGui::TreeNode("Sun"))
					{
						bDirty |= ImGui::SliderFloat("Intensity", &pLight->fIntensity, 0.0f, 20.0f);
						ImGui::ColorEdit3("Color", &pLight->Color.x);

						BOOL bAngleChanged = false;
						if (ImGui::SliderFloat("Angle Vert", &pLight->Angle.x, 0.0f, 360.0f)) bAngleChanged = true;
						if (ImGui::SliderFloat("Angle Horz", &pLight->Angle.y, 0.0f, 180.0f)) bAngleChanged = true;

						if (bAngleChanged)
						{
							FLOAT X = Radians(pLight->Angle.x);
							FLOAT Y = Radians(-pLight->Angle.y);
							pLight->Direction = Normalize(FVector3F(
								std::cos(X) * std::cos(Y),
								std::sin(Y),
								std::sin(X) * std::cos(Y)
							));

							pLight->ViewProj = Mul(
								LookAtLeftHand(-pLight->Direction * 20.0f, FVector3F{}, FVector3F(0.0f, 1.0f, 0.0f)),
								OrthographicLeftHand(20.0f, 20.0f, 0.1f, 80.0f)
							);
						}

						if (ImGui::Button("Reset"))
						{
							bDirty = true;

							*pLight = FDirectionalLight{};

							FLOAT X = Radians(pLight->Angle.x);
							FLOAT Y = Radians(-pLight->Angle.y);
							pLight->Direction = Normalize(FVector3F(
								std::cos(X) * std::cos(Y),
								std::sin(Y),
								std::sin(X) * std::cos(Y)
							));

							pLight->ViewProj = Mul(
								LookAtLeftHand(-pLight->Direction * 20.0f, FVector3F{}, FVector3F(0.0f, 1.0f, 0.0f)),
								OrthographicLeftHand(20.0f, 20.0f, 0.1f, 80.0f)
							);
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Misc"))
					{
						ImGui::SliderFloat("World Scale", &m_fWorldScale, 10.0f, 500.0f);
						ImGui::Checkbox("Enable Shadow", &bEnableShadow);
						ImGui::Checkbox("Enable Multi Scattering", &bEnableMultiScattering);
						m_AerialLUTPass.m_PassConstant.bEnableShadow = static_cast<UINT32>(bEnableShadow);
						m_AerialLUTPass.m_PassConstant.bEnableMultiScattering = static_cast<UINT32>(bEnableMultiScattering);
						m_SkyLUTPass.m_PassConstant.bEnableMultiScattering = static_cast<UINT32>(bEnableMultiScattering);

						if (ImGui::Button("Reset"))
						{
							m_fWorldScale = 200.0f;
							bEnableShadow = true;
							bEnableMultiScattering = true;
						}
						ImGui::TreePop();
					}

					if (bDirty)
					{
						m_TransmittanceLUTPass.Regenerate();
						m_MultiScatteringLUTPass.Regenerate();
					}
				}
			}
		);
		return true;
	}
}