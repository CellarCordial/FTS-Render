#include "../include/GBuffer.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Shader/ShaderCompiler.h"
#include "../../../Scene/include/Geometry.h"

#include <vector>

namespace FTS
{
#define TAA_MAX_SAMPLE_COUNT 8

    static const FLOAT gfHalton2[TAA_MAX_SAMPLE_COUNT] =
    {
        0.0,
        -1.0 / 2.0,
        1.0 / 2.0,
        -3.0 / 4.0,
        1.0 / 4.0,
        -1.0 / 4.0,
        3.0 / 4.0,
        -7.0 / 8.0
    };

    static const FLOAT gfHalton3[TAA_MAX_SAMPLE_COUNT] =
    {
        -1.0 / 3.0,
        1.0 / 3.0,
        -7.0 / 9.0,
        -1.0 / 9.0,
        5.0 / 9.0,
        -5.0 / 9.0,
        1.0 / 9.0,
        7.0 / 9.0
    };


    BOOL FGBufferPass::Compile(IDevice* pDevice, IRenderResourceCache* pCache)
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
        VertexAttriDescs[2].Format = EFormat::RGBA32_FLOAT;
        VertexAttriDescs[2].dwOffset = offsetof(FVertex, Tangent);
        VertexAttriDescs[2].dwElementStride = sizeof(FVertex);
        VertexAttriDescs[3].strName = "TEXCOORD";
        VertexAttriDescs[3].Format = EFormat::RG32_FLOAT;
        VertexAttriDescs[3].dwOffset = offsetof(FVertex, UV);
        VertexAttriDescs[3].dwElementStride = sizeof(FVertex);

        ReturnIfFalse(pDevice->CreateInputLayout(VertexAttriDescs.data(), VertexAttriDescs.Size(), nullptr, IID_IInputLayout, PPV_ARG(m_pInputLayout.GetAddressOf())));


        FShaderCompileDesc ShaderCompileDesc;
        ShaderCompileDesc.strShaderName = "Deferred/GBuffer_vs.hlsl";
        ShaderCompileDesc.strEntryPoint = "VS";
        ShaderCompileDesc.Target = EShaderTarget::Vertex;
        FShaderData VSData = ShaderCompile::CompileShader(ShaderCompileDesc);
        ShaderCompileDesc.strShaderName = "Deferred/GBuffer_ps.hlsl";
        ShaderCompileDesc.strEntryPoint = "PS";
        ShaderCompileDesc.Target = EShaderTarget::Pixel;
        FShaderData PSData = ShaderCompile::CompileShader(ShaderCompileDesc);

        FShaderDesc VSDesc;
        VSDesc.ShaderType = EShaderType::Vertex;
        VSDesc.strEntryName = "VS";
        ReturnIfFalse(pDevice->CreateShader(VSDesc, VSData.pData.data(), VSData.pData.size(), IID_IShader, PPV_ARG(m_pVS.GetAddressOf())));

        FShaderDesc PSDesc;
        PSDesc.ShaderType = EShaderType::Pixel;
        PSDesc.strEntryName = "PS";
        ReturnIfFalse(pDevice->CreateShader(PSDesc, PSData.pData.data(), PSData.pData.size(), IID_IShader, PPV_ARG(m_pPS.GetAddressOf())));


        FBindingLayoutItemArray BindingLayoutItems(4);
        BindingLayoutItems[0].Type = EResourceType::PushConstants;
        BindingLayoutItems[0].wSize = sizeof(Constant::GBufferPassConstant);
        BindingLayoutItems[0].dwSlot = 0;
        BindingLayoutItems[1].Type = EResourceType::VolatileConstantBuffer;
        BindingLayoutItems[1].dwSlot = 1;
        BindingLayoutItems[2].Type = EResourceType::StructuredBuffer_SRV;
        BindingLayoutItems[2].dwSlot = 5;
        BindingLayoutItems[3].Type = EResourceType::Sampler;
        BindingLayoutItems[3].dwSlot = 0;

        static_assert(FMaterial::TextureType_Num == 5);

        FBindingLayoutItemArray DynamicBindingLayoutItems(FMaterial::TextureType_Num);
        for (UINT32 ix = 0; ix < FMaterial::TextureType_Num; ++ix)
        {
            DynamicBindingLayoutItems[ix] = FBindingLayoutItem{
                .dwSlot = ix, 
                .Type = EResourceType::TypedBuffer_SRV
            };
        }

        FBindingLayoutDesc BindingLayoutDesc;
        BindingLayoutDesc.BindingLayoutItems = BindingLayoutItems;
        ReturnIfFalse(pDevice->CreateBindingLayout(BindingLayoutDesc, IID_IBindingLayout, PPV_ARG(m_pBindingLayout.GetAddressOf())));
        BindingLayoutDesc.BindingLayoutItems = DynamicBindingLayoutItems;
        ReturnIfFalse(pDevice->CreateBindingLayout(BindingLayoutDesc, IID_IBindingLayout, PPV_ARG(m_pDynamicBindingLayout.GetAddressOf())));

        // FTextureDesc RenderTargetDesc = FTextureDesc::CreateRenderTarget(CLIENT_WIDTH, CLIENT_HEIGHT, EFormat::RGBA32_FLOAT, "DiffuseTexture");
        // ReturnIfFalse(pDevice->CreateTexture(RenderTargetDesc, IID_ITexture, PPV_ARG(m_pDiffuse.GetAddressOf())));
        // RenderTargetDesc.strName = "MetallicRoughnessOcclusionDepthTexture";
        // ReturnIfFalse(pDevice->CreateTexture(RenderTargetDesc, IID_ITexture, PPV_ARG(m_pMetallicRoughnessOcclusionDepth.GetAddressOf())));
        // RenderTargetDesc.strName = "EmmisiveTexture";
        // ReturnIfFalse(pDevice->CreateTexture(RenderTargetDesc, IID_ITexture, PPV_ARG(m_pEmmisive.GetAddressOf())));
        // RenderTargetDesc.strName = "NormalTexture";
        // ReturnIfFalse(pDevice->CreateTexture(RenderTargetDesc, IID_ITexture, PPV_ARG(m_pNormal.GetAddressOf())));
        // RenderTargetDesc.strName = "PositionTexture";
        // ReturnIfFalse(pDevice->CreateTexture(RenderTargetDesc, IID_ITexture, PPV_ARG(m_pPosition.GetAddressOf())));

        // ReturnIfFalse(pCache->Collect(m_pDiffuse.Get()));
        // ReturnIfFalse(pCache->Collect(m_pMetallicRoughnessOcclusionDepth.Get()));
        // ReturnIfFalse(pCache->Collect(m_pEmmisive.Get()));
        // ReturnIfFalse(pCache->Collect(m_pNormal.Get()));
        // ReturnIfFalse(pCache->Collect(m_pPosition.Get()));


        FFrameBufferAttachmentArray FrameBufferAttachments(6);
        FrameBufferAttachments[4].pTexture = m_pPosDepthTexture.Get();
        FrameBufferAttachments[4].Format = EFormat::RGB32_FLOAT;
        FrameBufferAttachments[3].pTexture = m_pNormalTexture.Get();
        FrameBufferAttachments[3].Format = EFormat::RGB32_FLOAT;
        FrameBufferAttachments[0].pTexture = m_pBaseColorTexture.Get();
        FrameBufferAttachments[0].Format = EFormat::RGBA32_FLOAT;
        FrameBufferAttachments[2].pTexture = m_pPBRTexture.Get();
        FrameBufferAttachments[2].Format = EFormat::RGBA32_FLOAT;
        FrameBufferAttachments[1].pTexture = m_pEmissiveTexture.Get();
        FrameBufferAttachments[1].Format = EFormat::RGBA32_FLOAT;
        FrameBufferAttachments[1].pTexture = m_pVelocityVTexture.Get();
        FrameBufferAttachments[1].Format = EFormat::RGB32_FLOAT;

        ITexture* pDepthStencil;
        ReturnIfFalse(pCache->Require("DepthStencilTexture")->QueryInterface(IID_ITexture, PPV_ARG(&pDepthStencil)));
        
        FFrameBufferDesc FrameBufferDesc;
        FrameBufferDesc.ColorAttachments = FrameBufferAttachments;
        FrameBufferDesc.DepthStencilAttachment.Format = EFormat::D24S8;
        FrameBufferDesc.DepthStencilAttachment.pTexture = pDepthStencil;
        ReturnIfFalse(pDevice->CreateFrameBuffer(FrameBufferDesc, IID_IFrameBuffer, PPV_ARG(m_pFrameBuffer.GetAddressOf())));


        FGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VS = m_pVS.Get();
        PipelineDesc.PS = m_pPS.Get();
        PipelineDesc.pInputLayout = m_pInputLayout.Get();
        PipelineDesc.pBindingLayouts.PushBack(m_pBindingLayout.Get());
        PipelineDesc.pBindingLayouts.PushBack(m_pDynamicBindingLayout.Get());
        ReturnIfFalse(pDevice->CreateGraphicsPipeline(PipelineDesc, m_pFrameBuffer.Get(), IID_IGraphicsPipeline, PPV_ARG(m_pPipeline.GetAddressOf())));



        // FSamplerDesc AnisotropicSamplerDesc;
        // AnisotropicSamplerDesc.fMaxAnisotropy = 8.0f;
        // ReturnIfFalse(pDevice->CreateSampler(AnisotropicSamplerDesc, IID_ISampler, PPV_ARG(m_pAnisotropicSampler.GetAddressOf())));

        // const auto& crImage = m_pModelLoader->m_Images[0];
        // m_BlankData.resize(crImage.Height * crImage.Width * GetFormatInfo(crImage.Format).btBytesPerBlock, 0);
        // ReturnIfFalse(pDevice->CreateTexture(FTextureDesc::CreateShaderResource(crImage.Width, crImage.Height, crImage.Format), IID_ITexture, PPV_ARG(m_pBlankTexture.GetAddressOf())));


        // UINT64 stModelVerticesNum = 0;
        // UINT64 stModelIndicesNum = 0;
        // for (const auto& crMesh : m_crModel.Meshes)
        // {
        //     stModelIndicesNum += crMesh.Indices.size();
        //     stModelVerticesNum += crMesh.Vertices.size();
        // }

        // ReturnIfFalse(pDevice->CreateBuffer(FBufferDesc::CreateVertex(stModelVerticesNum * sizeof(FVertex), "ModelVertexBuffer"), IID_IBuffer, PPV_ARG(m_pVertexBuffer.GetAddressOf())));
        // ReturnIfFalse(pDevice->CreateBuffer(FBufferDesc::CreateIndex(stModelIndicesNum * sizeof(UINT16), "ModelIndexBuffer"), IID_IBuffer, PPV_ARG(m_pIndexBuffer.GetAddressOf())));
        

        // std::string strTextureNames[FMaterial::TextureType_Num] = {
        //     "DiffuseTexture",
        //     "NormalTexture",
        //     "EmissiveTexture",
        //     "OcclusionTexture",
        //     "MetallicRoughnessTexture"
        // };
        // for (UINT32 ix = m_crModel.dwImageOffset; ix < m_crModel.dwImageOffset + m_crModel.dwImageNum; ++ix)
        // {
        //     auto& rpTexture = m_pGeometryTextures.emplace_back();
        //     const auto& crImage = m_pModelLoader->m_Images[ix];
        //     ReturnIfFalse(pDevice->CreateTexture(FTextureDesc::CreateShaderResource(crImage.Width, crImage.Height, crImage.Format, strTextureNames[ix % FMaterial::TextureType_Num]), IID_ITexture, PPV_ARG(rpTexture.GetAddressOf())));
        // }

        // ReturnIfFalse(pDevice->CreateBuffer(FBufferDesc::CreateConstant(sizeof(GeometryConstants), true), IID_IBuffer, PPV_ARG(m_pGeometryCB.GetAddressOf())));
        // ReturnIfFalse(pDevice->CreateBuffer(FBufferDesc::CreateStructured(sizeof(FMaterial) * m_crModel.dwMaterialNum, sizeof(FMaterial)), IID_IBuffer, PPV_ARG(m_pMaterialCB.GetAddressOf())));


        // FBindingSetDesc BindingSetDesc;
        // BindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreatePushConstants(0, sizeof(PassConstants)));
        // BindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreateConstantBuffer(2, m_pGeometryCB.Get()));
        // BindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreateStructuredBuffer_SRV(5, m_pMaterialCB.Get(), FBufferRange{ 0, sizeof(FMaterial) * m_crModel.dwMaterialNum}));
        // BindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreateSampler(0, m_pAnisotropicSampler.Get()));
        // ReturnIfFalse(pDevice->CreateBindingSet(BindingSetDesc, m_pBindingLayout.Get(), IID_IBindingSet, PPV_ARG(m_pBindingSet.GetAddressOf())));
        
        // for (UINT32 ix = 0; ix < m_pModelLoader->m_Models[m_dwModelIndex].Meshes.size(); ++ix)
        // {
        //     const auto& crMesh = m_pModelLoader->m_Models[m_dwModelIndex].Meshes[ix];
        //     const auto& crMaterialData = m_pModelLoader->m_MaterialDatas[crMesh.dwMaterialIndex];

        //     FBindingSetDesc DynamicBindingSetDesc;
        //     const UINT32* pTextureIndex = &crMaterialData.dwDiffuseIndex;
        //     for (UINT32 ix = 0; ix < FMaterial::TextureType_Num; ++ix)
        //     {
        //         UINT32 dwIndex = *(pTextureIndex + ix);
        //         if (dwIndex != INVALID_SIZE_32)
        //             DynamicBindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreateTexture_SRV(ix, m_pGeometryTextures[dwIndex].Get()));
        //         else
        //             DynamicBindingSetDesc.BindingItems.PushBack(FBindingSetItem::CreateTexture_SRV(ix, m_pBlankTexture.Get()));
        //     }

        //     IBindingSet* pBindingSet;
        //     ReturnIfFalse(pDevice->CreateBindingSet(DynamicBindingSetDesc, m_pDynamicBindingLayout.Get(), IID_IBindingSet, PPV_ARG(&pBindingSet)));
        //     m_pDynamicBindingSets.emplace_back(pBindingSet);
        // }


        // m_GraphicsState.VertexBufferBindings.PushBack(FVertexBufferBinding{ .pBuffer = m_pVertexBuffer.Get() });
        // m_GraphicsState.IndexBufferBinding.pBuffer = m_pIndexBuffer.Get();
        // m_GraphicsState.IndexBufferBinding.Format = EFormat::R16_UINT;
        // m_GraphicsState.pFramebuffer = m_pFrameBuffer.Get();
        // m_GraphicsState.ViewportState = FViewportState::CreateSingleViewport(CLIENT_WIDTH, CLIENT_HEIGHT);
        // m_GraphicsState.pPipeline = m_pPipeline.Get();
        // m_GraphicsState.pBindingSets.Resize(2);
        // m_GraphicsState.pBindingSets[0] = m_pBindingSet.Get();

        return true;
    }

    BOOL FGBufferPass::Execute(ICommandList* pCmdList, IRenderResourceCache* pCache)
    {
        ReturnIfFalse(pCmdList->Open());

        // if (!m_bModelLoaded)
        // {
        //     const auto& crModel = m_pModelLoader->m_Models[m_dwModelIndex];
        //     const auto& crImage = m_pModelLoader->m_Images[0];
        //     ReturnIfFalse(pCmdList->WriteTexture(m_pBlankTexture.Get(), 0, 0, m_BlankData.data(), crImage.Width * GetFormatInfo(crImage.Format).btBytesPerBlock));
            

        //     m_DrawArguments.resize(crModel.Meshes.size());
        //     m_DrawArguments[0].dwIndexOrVertexCount = static_cast<UINT32>(crModel.Meshes[0].Indices.size());

        //     std::vector<FVertex> Vertices;
        //     std::vector<UINT16> Indices;
        //     for (UINT32 ix = 0; ix < crModel.Meshes.size(); ++ix)
        //     {
        //         const auto& crMesh = crModel.Meshes[ix];
        //         Vertices.insert(Vertices.end(), crMesh.Vertices.begin(), crMesh.Vertices.end());
        //         Indices.insert(Indices.end(), crMesh.Indices.begin(), crMesh.Indices.end());

        //         if (ix != 0)
        //         {
        //             m_DrawArguments[ix].dwIndexOrVertexCount = crMesh.Indices.size();
        //             m_DrawArguments[ix].dwStartIndexLocation = m_DrawArguments[ix - 1].dwStartIndexLocation + crModel.Meshes[ix - 1].Indices.size();
        //             m_DrawArguments[ix].dwStartVertexLocation = m_DrawArguments[ix - 1].dwStartVertexLocation + crModel.Meshes[ix - 1].Vertices.size();
        //         }
        //     }
            
        //     ReturnIfFalse(pCmdList->WriteBuffer(m_pVertexBuffer.Get(), Vertices.data(), Vertices.size() * sizeof(FVertex)));
        //     ReturnIfFalse(pCmdList->WriteBuffer(m_pIndexBuffer.Get(), Indices.data(), Indices.size() * sizeof(UINT16)));
        //     for (UINT32 ix = crModel.dwImageOffset; ix < crModel.dwImageOffset + crModel.dwImageNum; ++ix)
        //     {
        //         const auto& crImage = m_pModelLoader->m_Images[ix];
        //         ReturnIfFalse(pCmdList->WriteTexture(
        //             m_pGeometryTextures[ix].Get(), 
        //             0, 
        //             0, 
        //             crImage.Data.get(), 
        //             crImage.Width * GetFormatInfo(crImage.Format).btBytesPerBlock
        //         ));
        //     }

        //     std::vector<FModelLoader::MaterialData> CurrentMaterialDatas;
        //     for (const auto& crMesh : m_pModelLoader->m_Models[m_dwModelIndex].Meshes)
        //     {
        //         CurrentMaterialDatas.emplace_back(m_pModelLoader->m_MaterialDatas[crMesh.dwMaterialIndex]);
        //     }
        //     ReturnIfFalse(pCmdList->WriteBuffer(m_pMaterialCB.Get(), CurrentMaterialDatas.data(), sizeof(FMaterial) * CurrentMaterialDatas.size()));

        //     m_bModelLoaded = true;
        // }

        // 需要提前 set 一下, 让 PushConstants 能正确设置.
        // ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));

        // m_PassConstants.ViewProj = m_pCamera->GetViewProj();
        // m_PassConstants.ProjConstants = m_pCamera->GetProjectConstantsAB();
        // ReturnIfFalse(pCmdList->SetPushConstants(&m_PassConstants, sizeof(PassConstants)));



        // const auto& crModel = m_pModelLoader->m_Models[m_dwModelIndex];
        // for (UINT32 ix = 0; ix < crModel.Meshes.size(); ++ix)
        // {
        //     const auto& crMesh = crModel.Meshes[ix];

        //     m_GeometryConstants.WorldMatrix = Mul(crMesh.WorldMatrix, crModel.WorldMatrix);
        //     m_GeometryConstants.InvTransWorld = Inverse(Transpose(m_GeometryConstants.WorldMatrix));
        //     m_GeometryConstants.dwMaterialIndex = crMesh.dwMaterialIndex;
        //     ReturnIfFalse(pCmdList->WriteBuffer(m_pGeometryCB.Get(), &m_GeometryConstants, sizeof(GeometryConstants)));

        //     // Change geometry textures.
        //     m_GraphicsState.pBindingSets[1] = m_pDynamicBindingSets[ix].Get();
        //     ReturnIfFalse(pCmdList->SetGraphicsState(m_GraphicsState));

        //     ReturnIfFalse(pCmdList->DrawIndexed(m_DrawArguments[ix]));
        // }

        // ReturnIfFalse(pCmdList->Close());
        return true;
    }
}