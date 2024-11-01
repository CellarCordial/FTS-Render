#ifndef RENDER_PASS_SUN_DISK_H
#define RENDER_PASS_SUN_DISK_H

#include "../../../RenderGraph/include/RenderGraph.h"
#include "../../../Core/include/ComCli.h"
#include "../../../Math/include/Vector.h"
#include "../../../Math/include/Matrix.h"
#include "AtmosphereProperties.h"


namespace FTS
{
	namespace Constant
	{
		struct SunDiskPassConstant
		{
			FMatrix4x4 WorldViewProj;
			
			FVector3F SunRadius;
			FLOAT fSunTheta = 0.0f;

			FLOAT fCameraHeight = 0.0f;
			FVector3F PAD;
		};
	}

	class FSunDiskPass : public IRenderPass
	{
	public:
		FSunDiskPass() { Type = ERenderPassType::Graphics; }

		BOOL Compile(IDevice* pDevice, IRenderResourceCache* pCache) override;
		BOOL Execute(ICommandList* pCmdList, IRenderResourceCache* pCache) override;

		friend class FAtmosphereRender;

	private:
		void GenerateSunDiskVertices();

	private:
		struct Vertex
		{
			FVector2F Position;
		};

		BOOL m_bWritedBuffer = false;
		FLOAT m_fSunDiskSize = 0.01f;
		std::vector<Vertex> m_SunDiskVertices;
		Constant::SunDiskPassConstant m_PassConstant;

		TComPtr<IBuffer> m_pPassConstantBuffer;
		TComPtr<IBuffer> m_pVertexBuffer;
		
		TComPtr<IBindingLayout> m_pBindingLayout;
		TComPtr<IInputLayout> m_pInputLayout;
		
		TComPtr<IShader> m_pVS;
		TComPtr<IShader> m_pPS;
		
		TComPtr<IFrameBuffer> m_pFrameBuffer;
		TComPtr<IGraphicsPipeline> m_pPipeline;
		
		TComPtr<IBindingSet> m_pBindingSet;
		FGraphicsState m_GraphicsState;
	};
}





#endif