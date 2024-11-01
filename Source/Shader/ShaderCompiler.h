#ifndef SHADER_COMPILER_H
#define SHADER_COMPILER_H


#include <basetsd.h>
#include <string>
#include <vector>
#include <windows.h>
#include <dxcapi.h>

namespace FTS 
{
    enum class EShaderTarget : UINT16
    {
        None            = 0x0000,

        Compute         = 0x0020,

        Vertex          = 0x0001,
        Hull            = 0x0002,
        Domain          = 0x0004,
        Geometry        = 0x0008,
        Pixel           = 0x0010,

        Num             = 0x3FFF,
    };
    
    struct FShaderCompileDesc
    {
        std::string strShaderName;     // Need the file extension.
        std::string strEntryPoint;
        EShaderTarget Target = EShaderTarget::None;
        std::vector<std::string> strDefines;
    };

    struct FShaderData
    {
        std::vector<UINT8> pData;
        std::vector<std::string> strIncludeShaderFiles;

        UINT64 Size() const { return pData.size(); }
        UINT8* Data() { return pData.data(); }

        void SetByteCode(const void* cpvData, UINT64 stSize)
        {
            if (cpvData)
            {
                pData.resize(stSize);
                memcpy(pData.data(), cpvData, stSize);
            }
        }

        BOOL Invalid() const { return pData.empty() || strIncludeShaderFiles.empty(); }
    };

    namespace ShaderCompile
    {
        void Initialize();
        void Destroy();
        FShaderData CompileShader(const FShaderCompileDesc& InDesc);
    };

}















#endif







