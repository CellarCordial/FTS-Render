#ifndef RENDER_PASS_SHADOW_MAP_H
#define RENDER_PASS_SHADOW_MAP_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Matrix.h"
#include "../../../Scene/include/Geometry.h"

namespace FTS 
{
    namespace Constant
    {
        struct ShadowMapPassConstant
        {
            FMatrix4x4 WorldMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
            FMatrix4x4 DirectionalLightViewProj;
        };
    }


    class FShadowMapPass : public IRenderPass
    {
    public:
        FShadowMapPass() { Type = ERenderPassType::Graphics; }
        
        BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache);
        BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache);

		friend class FAtmosphereRender;

    private:
        BOOL m_bWritedBuffer = false;
        std::vector<FVertex> m_Vertices;
        std::vector<UINT32> m_Indices;
        Constant::ShadowMapPassConstant m_PassConstant;

		TComPtr<IBuffer> m_pVertexBuffer;
		TComPtr<IBuffer> m_pIndexBuffer;
        TComPtr<ITexture> m_pShadowMapTexture;

        TComPtr<IBindingLayout> m_pBindingLayout;
        TComPtr<IInputLayout> m_pInputLayout;

		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;

        TComPtr<IFrameBuffer> m_pFrameBuffer;
        TComPtr<IGraphicsPipeline> m_pPipeline;
        
        FGraphicsState m_GraphicsState;

        std::vector<FDrawArguments> m_DrawArguments;
    };

}












#endif