#include "../include/GuiPass.h"
#include "../../Shader/ShaderCompiler.h"

namespace FTS
{
	BOOL FGuiPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCache->Require("FinalTexture")->QueryInterface(IID_ITexture, PPV_ARG(&m_pFinalTexture)));

		FFrameBufferAttachmentArray ColorAttachment(1);
		ColorAttachment[0].Format = EFormat::RGBA8_UNORM;
		ColorAttachment[0].pTexture = m_pFinalTexture;
		ReturnIfFalse(pDevice->CreateFrameBuffer(FFrameBufferDesc{ .ColorAttachments = ColorAttachment }, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));

		return true;
	}

	BOOL FGuiPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
	{
		ReturnIfFalse(pCmdList->Open());

		ReturnIfFalse(pCmdList->BindFrameBuffer(m_pFrameBuffer.Get()));
		ReturnIfFalse(pCmdList->CommitDescriptorHeaps());
		Gui::Execution(pCmdList);

		UINT32* pdwBackBufferIndex;
		ReturnIfFalse(pCache->RequireConstants("BackBufferIndex", PPV_ARG(&pdwBackBufferIndex)));
		std::string strBackBufferName = "BackBuffer" + std::to_string(*pdwBackBufferIndex);

		ITexture* pBackBuffer;
		ReturnIfFalse(pCache->Require(strBackBufferName.c_str())->QueryInterface(IID_ITexture, PPV_ARG(&pBackBuffer)));

		ReturnIfFalse(pCmdList->CopyTexture(pBackBuffer, FTextureSlice{}, m_pFinalTexture, FTextureSlice{}));
		ReturnIfFalse(pCmdList->SetTextureState(pBackBuffer, FTextureSubresourceSet{}, EResourceStates::Present));

		ReturnIfFalse(pCmdList->Close());
		return true;
	}
}

