#ifndef RENDER_PASS_ATOMSPHERE_RENDER_H
#define RENDER_PASS_ATOMSPHERE_RENDER_H

#include "../../../Core/include/Entity.h"
#include "TransmittanceLUT.h"
#include "MultiScatteringLUT.h"
#include "ShadowMap.h"
#include "SkyLUT.h"
#include "AerialLUT.h"
#include "Sky.h"
#include "SunDisk.h"
#include "AtmosphereDebug.h"

namespace FTS
{
	class FAtmosphereRender
	{
	public:
		BOOL SetupDebug(IRenderGraph* pRenderGraph);

		IRenderPass* GetFirstPass() { return &m_TransmittanceLUTPass; }
		IRenderPass* GetLastPass() { return &m_AtmosphereDebugPass; }

	private:
		FLOAT m_fWorldScale = 200.0f;
		FVector3F m_GroundAlbedo = { 0.3f, 0.3f, 0.3f };

		FTransmittanceLUTPass m_TransmittanceLUTPass;
		FMultiScatteringLUTPass m_MultiScatteringLUTPass;
		FShadowMapPass m_ShadowMapPass;
		FSkyLUTPass m_SkyLUTPass;
		FAerialLUTPass m_AerialLUTPass;
		FSkyPass m_SkyPass;
		FSunDiskPass m_SunDiskPass;
		FAtmosphereDebugPass m_AtmosphereDebugPass;
	};



}




















#endif