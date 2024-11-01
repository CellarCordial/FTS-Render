#include "../include/AtmosphereRender.h"
#include "../../../Gui/include/GuiPanel.h"
#include "../../../Scene/include/Light.h"

namespace FTS 
{

    BOOL FAtmosphereRender::SetupDebug(IRenderGraph* pRenderGraph)
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
        m_MultiScatteringLUTPass.Precede(&m_ShadowMapPass);
		m_MultiScatteringLUTPass.Precede(&m_SkyLUTPass);
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


		FEntity* pGeometryEntity = pWorld->CreateEntity();
		pWorld->Boardcast(Event::OnGeometryLoad{ .pEntity = pGeometryEntity, .FilesDirectory = "Asset/Mountain" });


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