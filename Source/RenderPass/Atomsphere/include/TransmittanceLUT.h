#ifndef RENDER_PASS_ATOMSPHERE_TRANSMITTANCE_LUT_H
#define RENDER_PASS_ATOMSPHERE_TRANSMITTANCE_LUT_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "AtmosphereProperties.h"

namespace FTS 
{
    class FTransmittanceLUTPass : public IRenderPass
    {
    public:
        FTransmittanceLUTPass() { Type = ERenderPassType::Precompute; }

        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		void Regenerate() { Type = ERenderPassType::Precompute; }

		friend class FAtmosphereDebugRender;

    private:
        Constant::AtmosphereProperties m_StandardAtomsphereProperties;

		TComPtr<IBuffer> m_pAtomspherePropertiesBuffer;
        TComPtr<ITexture> m_pTransmittanceTexture;

        TComPtr<IBindingLayout> m_pBindingLayout;
        
        TComPtr<IShader> m_pCS;
        TComPtr<IComputePipeline> m_pPipeline;
        
        TComPtr<IBindingSet> m_pBindingSet;
        FComputeState m_ComputeState;
    };

}
















#endif