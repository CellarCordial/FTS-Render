#ifndef RENDER_PASS_ATOMSPHERE_MULTISCATTERING_LUT_H
#define RENDER_PASS_ATOMSPHERE_MULTISCATTERING_LUT_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "AtmosphereProperties.h"

namespace FTS 
{
    namespace Constant
    {
        struct MultiScatteringPassConstant
        {
            FVector3F SunIntensity = FVector3F(1.0f, 1.0f, 1.0f);
            INT32 dwRayMarchStepCount = 256;

            FVector3F GroundAlbedo;
            FLOAT PAD = 0.0f;
        };
    };


    class FMultiScatteringLUTPass : public IRenderPass
    {
    public:
        FMultiScatteringLUTPass()
        {
            Type = ERenderPassType::Precompute;
            CreatePoissonDiskSamples();
        }

        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache);
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache);

        BOOL FinishPass() override;

		void Regenerate() { Type &= ~ERenderPassType::Exclude; }

		friend class FAtmosphereDebugRender;

    private:
        void CreatePoissonDiskSamples();

    private:
        BOOL m_bResourceWrited = false;
        std::vector<FVector2F> m_DirSamples;
        Constant::MultiScatteringPassConstant m_PassConstants;

        TComPtr<IBuffer> m_pPassConstantBuffer;
        TComPtr<IBuffer> m_pDirScampleBuffer;
        TComPtr<ITexture> m_pMultiScatteringTexture;

        TComPtr<IBindingLayout> m_pBindingLayout;

        TComPtr<IShader> m_pCS;
        TComPtr<IComputePipeline> m_pPipeline;

        TComPtr<IBindingSet> m_pBindingSet;
        FComputeState m_ComputeState;
    };

}












#endif