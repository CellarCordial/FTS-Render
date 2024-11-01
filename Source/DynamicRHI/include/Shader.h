#ifndef RHI_SHADER_H
#define RHI_SHADER_H

#include "../../Core/include/ComIntf.h"

#include <string>


namespace FTS
{
    enum class EShaderType : UINT16
    {
        None            = 0x0000,

        Compute         = 0x0020,

        Vertex          = 0x0001,
        Hull            = 0x0002,
        Domain          = 0x0004,
        Geometry        = 0x0008,
        Pixel           = 0x0010,

        All             = 0x3FFF,
    };

    struct FCustomSemantic
    {
        enum class EType
        {
            Undefined       = 0,
            XRight          = 1,
            ViewportMask    = 2
        };

        EType Type;

        CHAR* pcName;
        UINT32 dwNameSize;
    };

    struct FShaderDesc
    {
        std::string strDebugName;

        EShaderType ShaderType = EShaderType::None;
        
        std::string strEntryName;
        
        INT32 dwHlslExtensionsUAV = -1;
        
        BOOL bUseSpecificShaderExt = false;
        UINT32 dwNumCustomSemantics = 0;
        FCustomSemantic* pCustomSemantics = nullptr;

        UINT32* pCoordinateSwizzling = nullptr;
    };

    struct FShaderSpecialization
    {
        UINT32 dwConstantID = 0;
        union
        {
            UINT32 dwu = 0;
            INT32 dwi;
            FLOAT f;
        } Value;
    };


    extern const IID IID_IShader;

    struct IShader : public IUnknown
    {
        virtual FShaderDesc GetDesc() const = 0;
        virtual BOOL GetBytecode(const void** cppvBytecode, UINT64* pstSize) const = 0;
        
		virtual ~IShader() = default;
    };


    extern const IID IID_IShaderLibrary;

    struct IShaderLibrary : public IUnknown
    {
        virtual BOOL GetShader(
            const CHAR* strEntryName,
            EShaderType ShaderType,
            CREFIID criid,
            void** ppShader
        ) = 0;
        virtual BOOL GetBytecode(const void** cppvBytecode, UINT64* pstSize) const = 0;

        
		virtual ~IShaderLibrary() = default;
    };
}


#endif
