/**
 * *****************************************************************************
 * @file        DX12Shader.h
 * @brief       
 * @author      CellarCordial (591885295@qq.com)
 * @date        2024-05-29
 * @copyright Copyright (c) 2024
 * *****************************************************************************
 */

 #ifndef RHI_DX12_SHADER_H
 #define RHI_DX12_SHADER_H

#include "../../include/Shader.h"
#include "../../../Core/include/ComRoot.h"
#include "../../../Core/include/ComCli.h"
#include "DX12Forward.h"
#include <vector>

namespace FTS
{
    class FDX12Shader :
        public TComObjectRoot<FComMultiThreadModel>,
        public IShader
    {
    public:
        BEGIN_INTERFACE_MAP(FDX12Shader)
            INTERFACE_ENTRY(IID_IShader, IShader)
        END_INTERFACE_MAP

        FDX12Shader(const FDX12Context* cpContext, FShaderDesc ShaderDesc);
        BOOL Initialize(const void* cpvBinaryCode, UINT64 stBinarySize);

        // IShader
        FShaderDesc GetDesc() const override { return m_Desc; }
        BOOL GetBytecode(const void** cppvBytecode, UINT64* pstSize) const override;

    private:
        const FDX12Context* m_cpContext;
        FShaderDesc m_Desc;
        std::vector<UINT8> m_ByteCode;
    };

    class FDX12ShaderLibraryEntry :
        public TComObjectRoot<FComMultiThreadModel>,
        public IShader
    {
    public:

        BEGIN_INTERFACE_MAP(FDX12ShaderLibraryEntry)
            INTERFACE_ENTRY(IID_IShader, IShader)
        END_INTERFACE_MAP

        FDX12ShaderLibraryEntry(const FDX12Context* cpContext, IShaderLibrary* pLibrary);
        BOOL Initialize(const std::string& crstrEntryName, EShaderType ShaderType);

        // IShader
        FShaderDesc GetDesc() const override { return m_Desc; }
        BOOL GetBytecode(const void** cppvBytecode, UINT64* pstSize) const override;

    
    private:
        const FDX12Context* m_cpContext;
        FShaderDesc m_Desc;
        TComPtr<IShaderLibrary> m_pLibrary;
    };


    class FDX12ShaderLibrary :
        public TComObjectRoot<FComMultiThreadModel>,
        public IShaderLibrary
    {
    public:

        BEGIN_INTERFACE_MAP(FDX12ShaderLibrary)
            INTERFACE_ENTRY(IID_IShaderLibrary, IShaderLibrary)
        END_INTERFACE_MAP

        FDX12ShaderLibrary(const FDX12Context* cpContext);
        BOOL Initialize(const void* cpvBinaryCode, UINT64 stBinarySize);

        // IShaderLibrary
        BOOL GetShader(
            const CHAR* strEntryName,
            EShaderType ShaderType,
            CREFIID criid,
            void** ppShader
        ) override;
        BOOL GetBytecode(const void** cppvBytecode, UINT64* pstSize) const override;

    private:
        const FDX12Context* m_cpContext;
        std::vector<UINT8> m_ByteCode;
    };


}








 #endif