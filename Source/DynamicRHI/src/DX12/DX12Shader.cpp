#include "DX12Shader.h"
#include <cstring>

namespace FTS 
{
    FDX12Shader::FDX12Shader(const FDX12Context* cpContext, FShaderDesc ShaderDesc) :
        m_cpContext(cpContext), m_Desc(ShaderDesc)
    {
    }
    
    BOOL FDX12Shader::Initialize(const void* cpvBinaryCode, UINT64 stBinarySize)
    {
        if (m_Desc.dwNumCustomSemantics > 0 || 
            m_Desc.pCoordinateSwizzling != nullptr || 
            m_Desc.dwHlslExtensionsUAV >= 0
        )
        {
            LOG_ERROR("Not support now.");
            return false;
        }

        if (stBinarySize == 0 || cpvBinaryCode == nullptr)
        {
            LOG_ERROR("Create FDX12Shader failed for using empty binary data.");
            return false;
        }

        m_ByteCode.resize(stBinarySize);
        memcpy(m_ByteCode.data(), cpvBinaryCode, stBinarySize);

        return true;
    }


    BOOL FDX12Shader::GetBytecode(const void** cppvBytecode, UINT64* pstSize) const
    {
        if (cppvBytecode == nullptr || pstSize == nullptr) return false;
        
        *cppvBytecode = m_ByteCode.data();
        *pstSize = m_ByteCode.size();

        return true;
    }

    FDX12ShaderLibraryEntry::FDX12ShaderLibraryEntry(const FDX12Context* cpContext, IShaderLibrary* pLibrary) :
        m_cpContext(cpContext), m_pLibrary(pLibrary)
    {
    }

    BOOL FDX12ShaderLibraryEntry::Initialize(const std::string& crstrEntryName, EShaderType ShaderType)
    {
        m_Desc.ShaderType = ShaderType;
        m_Desc.strEntryName = crstrEntryName;

        if (m_pLibrary == nullptr)
        {
            LOG_ERROR("Create FDX12ShaderLibraryEntry failed for using nullptr IShaderLibrary.");
            return false;
        }
        return true;
    }


    BOOL FDX12ShaderLibraryEntry::GetBytecode(const void** cppvBytecode, UINT64* pstSize) const
    {
        return m_pLibrary->GetBytecode(cppvBytecode, pstSize);
    }

    FDX12ShaderLibrary::FDX12ShaderLibrary(const FDX12Context* cpContext) : 
        m_cpContext(cpContext)
    {
    }

    BOOL FDX12ShaderLibrary::Initialize(const void* cpvBinaryCode, UINT64 stBinarySize)
    {
        if (stBinarySize == 0 || cpvBinaryCode == nullptr)
        {
            LOG_ERROR("Create FDX12ShaderLibrary failed for using empty binary data.");
            return false;
        }
        else 
        {
            m_ByteCode.resize(stBinarySize);
            memcpy(m_ByteCode.data(), cpvBinaryCode, stBinarySize);
        }

        return true;
    }

    BOOL FDX12ShaderLibrary::GetShader(
        const CHAR* strEntryName,
        EShaderType ShaderType,
        CREFIID criid,
        void** ppShader
    )
    {
        if (ppShader == nullptr) return false;

        FDX12ShaderLibraryEntry* pEntry = new FDX12ShaderLibraryEntry(m_cpContext, this);
        if (!pEntry || !pEntry->Initialize(strEntryName, ShaderType)) return false;
        
        return pEntry->QueryInterface(criid, ppShader);
    }

    BOOL FDX12ShaderLibrary::GetBytecode(const void** cppvBytecode, UINT64* pstSize) const
    {
        if (cppvBytecode == nullptr || pstSize == nullptr) return false;

        *cppvBytecode = m_ByteCode.data();
        *pstSize = m_ByteCode.size();
        
        return true;
    }

}