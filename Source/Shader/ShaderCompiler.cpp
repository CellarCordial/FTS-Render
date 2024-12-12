#include "ShaderCompiler.h"

#include "../Core/include/File.h"
#include <vector>
#include <winerror.h>
#include <winnt.h>
#include <winscard.h>

#ifdef SLANG_SHADER
#include "../Core/include/ComIntf.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#endif

#ifdef HLSL_SHADER
#include "../Core/include/ComCli.h"
#include <dxcapi.h>
#endif

namespace FTS 
{
    bool CheckCache(const char* InCachePath, const char* InShaderPath)
    {
        if (!IsFileExist(InCachePath)) return false;
        if (CompareFileWriteTime(InShaderPath, InCachePath)) return false;
        return true;
    }

    void SaveToCache(const char* InCachePath, const FShaderData& InData)
    {
        if (InData.Invalid())
        {
            LOG_ERROR("Call to FShaderCompiler::SaveToCache() failed for invalid Shader Data.");
            return;
        }

        Serialization::BinaryOutput Output(InCachePath);
        Output(InData.pData.size());
        Output.SaveBinaryData(InData.pData.data(), InData.pData.size());
    }

    FShaderData LoadFromCache(const char* InCachePath)
    {
        FShaderData ShaderData;
        
        Serialization::BinaryInput Input(InCachePath);

        UINT64 ByteCodeSize = 0;
        Input(ByteCodeSize);
        ShaderData.pData.resize(ByteCodeSize);
        Input.LoadBinaryData(ShaderData.pData.data(), ByteCodeSize);

        return ShaderData;
    }


#if defined(SLANG_SHADER)
namespace ShaderCompile
{
    namespace
    {
        Slang::ComPtr<slang::IGlobalSession> gpGlobalSession;
    }

    void Initialize()
    {
        SlangResult Res = createGlobalSession(gpGlobalSession.writeRef());
        assert(Res == SLANG_OK);
    }

    void Destroy()
    {
        gpGlobalSession->Release();
    }

    FShaderData CompileShader(const FShaderCompileDesc& crDesc)
    {
        if (gpGlobalSession == nullptr)
        {
            LOG_ERROR("Please call FTS::StaticShaderCompiler::Initialize() first.");
            return FShaderData{};
        }

        const std::string ProjPath = PROJ_DIR;
        const std::string CachePath = ProjPath + "Asset/ShaderCache/" + RemoveFileExtension(crDesc.strShaderName.c_str()) + "_" + crDesc.strEntryPoint + "_DEBUG.bin";
        const std::string ShaderPath = ProjPath + "Source/Shader/" + crDesc.strShaderName;

        if (CheckCache(CachePath.c_str(), ShaderPath.c_str()))
        {
            return LoadFromCache(CachePath.c_str());
        }

		SIZE_T pos = ShaderPath.find_last_of('/');
		if (pos == std::string::npos)
		{
			LOG_ERROR("Find hlsl file's Directory failed.");
			return FShaderData{};
		}
		const std::string strFileDirectory = ShaderPath.substr(0, pos);


        slang::SessionDesc SessionDesc{};

        slang::TargetDesc TargetDesc = {};
        TargetDesc.format = SLANG_DXIL;
        TargetDesc.profile = gpGlobalSession->findProfile("sm_6_5");

        SessionDesc.targets = &TargetDesc;
        SessionDesc.targetCount = 1;

        std::vector<slang::PreprocessorMacroDesc> PreprocessorMacroDesc;
        for (const auto& crDefine : crDesc.strDefines)
        {
            auto EqualPos = crDefine.find_first_of('=');
            PreprocessorMacroDesc.push_back(slang::PreprocessorMacroDesc{
                .name = crDefine.substr(0, EqualPos).c_str(),
                .value = crDefine.substr(EqualPos + 1).c_str()
            });
        }

        SessionDesc.preprocessorMacros = PreprocessorMacroDesc.data();
        SessionDesc.preprocessorMacroCount = PreprocessorMacroDesc.size();

        std::array<slang::CompilerOptionEntry, 1> options = 
        {
            { 
                slang::CompilerOptionName::DebugInformation,
                { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr }
            }
        };
        SessionDesc.compilerOptionEntries = options.data();
        SessionDesc.compilerOptionEntryCount = options.size();

        const char* searchPaths[] = { strFileDirectory.c_str() };
        SessionDesc.searchPaths = searchPaths;
        SessionDesc.searchPathCount = 1;

        Slang::ComPtr<slang::ISession> pSession;
        gpGlobalSession->createSession(SessionDesc, pSession.writeRef());

        Slang::ComPtr<slang::IBlob> pDiagnostics;
        Slang::ComPtr<slang::IModule> pModule(pSession->loadModule("MyShaders", pDiagnostics.writeRef()));
        if (pDiagnostics)
        {
            LOG_ERROR((const char*) pDiagnostics->getBufferPointer());
            return FShaderData{};
        }
        pDiagnostics.setNull();

        Slang::ComPtr<slang::IEntryPoint> pEntryPoint;
        pModule->getDefinedEntryPoint(0, pEntryPoint.writeRef());

        slang::IComponentType* components[] = { pModule, pEntryPoint };
        Slang::ComPtr<slang::IComponentType> pProgram;
        pSession->createCompositeComponentType(components, 2, pProgram.writeRef());

        Slang::ComPtr<slang::IComponentType> pLinkedProgram;
        pProgram->link(pLinkedProgram.writeRef(), pDiagnostics.writeRef());
        if (pDiagnostics)
        {
            LOG_ERROR((const char*) pDiagnostics->getBufferPointer());
            return FShaderData{};
        }
        pDiagnostics.setNull();

        int entryPointIndex = 0;
        int targetIndex = 0;
        Slang::ComPtr<slang::IBlob> pKernelBlob;
        pLinkedProgram->getEntryPointCode(0, 0, pKernelBlob.writeRef(), pDiagnostics.writeRef());
        if (pDiagnostics)
        {
            LOG_ERROR((const char*) pDiagnostics->getBufferPointer());
            return FShaderData{};
        }
        pDiagnostics.setNull();

        FShaderData ShaderData;
        ShaderData.SetByteCode(pKernelBlob->getBufferPointer(), pKernelBlob->getBufferSize());

        SaveToCache(CachePath.c_str(), ShaderData);

        return FShaderData{};
    }
}

#elif defined(HLSL_SHADER)
    inline LPCWSTR GetTargetProfile(EShaderTarget Target)
    {
        switch (Target)
        {
        case EShaderTarget::Vertex  : return L"vs_6_5";
        case EShaderTarget::Hull    : return L"hs_6_5";
        case EShaderTarget::Domain  : return L"ds_6_5";
        case EShaderTarget::Geometry: return L"gs_6_5";
        case EShaderTarget::Pixel   : return L"ps_6_5";
        case EShaderTarget::Compute : return L"cs_6_5";
        default:
            assert(false && "There is no such shader target.");
            return L"";
        }
    }

    
    class FCustomDxcIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit FCustomDxcIncludeHandler(IDxcUtils* InUtils) : IDxcIncludeHandler(), Utils(InUtils) {}
        virtual ~FCustomDxcIncludeHandler() = default;
        HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
        {
            TComPtr<IDxcBlobEncoding> ShaderSourceBlob;
            std::string FilePathToInclude = WStringToString(pFilename);
            std::transform(
                FilePathToInclude.begin(),
                FilePathToInclude.end(),
                FilePathToInclude.begin(),
                [](char& c)
                {
                    if (c == '\\') c = '/';
                    return c;
                }
            );
            
            if (!IsFileExist(FilePathToInclude.c_str()))
            {
                ppIncludeSource = nullptr;
                return E_FAIL;
            }

            const auto Iterator = std::find_if(
                IncludedFileNames.begin(),
                IncludedFileNames.end(),
                [&FilePathToInclude](const auto& InName)
                {
                    return InName == FilePathToInclude;
                }
            );
            if (Iterator != IncludedFileNames.end())
            {
                // Return a blank blob if this file has been included before
                constexpr char nullStr[] = " ";
                Utils->CreateBlobFromPinned(nullStr, ARRAYSIZE(nullStr), DXC_CP_ACP, ShaderSourceBlob.GetAddressOf());
                *ppIncludeSource = ShaderSourceBlob.Detach();
                return S_OK;
            }

            HRESULT hr = Utils->LoadFile(StringToWString(FilePathToInclude).c_str(), nullptr, ShaderSourceBlob.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                IncludedFileNames.push_back(FilePathToInclude);
                *ppIncludeSource = ShaderSourceBlob.Detach();
            }
            else
            {
                ppIncludeSource = nullptr;
            }
            return hr;
        }
        
        HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef(void) override {	return 0; }
        ULONG STDMETHODCALLTYPE Release(void) override { return 0; }

        std::vector<std::string> IncludedFileNames;
        IDxcUtils* Utils;
    };

    namespace
    {
        TComPtr<IDxcCompiler3> pDxcCompiler;
        TComPtr<IDxcUtils> pDxcUtils;
        std::vector<std::string> SemanticNames;
    }

    void ShaderCompile::Initialize()
    {
        if (pDxcCompiler || pDxcUtils)
        {
            LOG_ERROR("StaticShaderCompiler has already initialized.");
        }

        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pDxcCompiler.GetAddressOf()))))
        {
            LOG_ERROR("Call to DxcCreateInstance failed.");
        }
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(pDxcUtils.GetAddressOf()))))
        {
            LOG_ERROR("Call to DxcCreateInstance failed.");
        }
    }

    void ShaderCompile::Destroy()
    {
        pDxcCompiler.Reset();
        pDxcUtils.Reset();
    }


    FShaderData ShaderCompile::CompileShader(const FShaderCompileDesc& crDesc)
    {
        if (pDxcCompiler == nullptr)
        {
            LOG_ERROR("Please call FTS::StaticShaderCompiler::Initialize() first.");
            return FShaderData{};
        }

        const std::string ProjPath = PROJ_DIR;
        const std::string CachePath = ProjPath + "Asset/ShaderCache/" + RemoveFileExtension(crDesc.strShaderName.c_str()) + "_" + crDesc.strEntryPoint + "_DEBUG.bin";
        const std::string ShaderPath = ProjPath + "Source/Shader/" + crDesc.strShaderName;

		SIZE_T pos = ShaderPath.find_last_of('/');
		if (pos == std::string::npos)
		{
			LOG_ERROR("Find hlsl file's Directory failed.");
			return FShaderData{};
		}
		const std::wstring wstrFileDirectory = StringToWString(ShaderPath.substr(0, pos));

        if (CheckCache(CachePath.c_str(), ShaderPath.c_str()))
        {
            return LoadFromCache(CachePath.c_str());
        }

        const std::wstring EntryPoint = StringToWString(crDesc.strEntryPoint);

        std::vector<LPCWSTR> CompileArguments
        {
            L"-E",
            EntryPoint.c_str(),
            L"-T",
            GetTargetProfile(crDesc.Target),
            L"-Qembed_debug",
            L"-Od",
            L"-I",
            wstrFileDirectory.c_str(),
            DXC_ARG_DEBUG,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_PACK_MATRIX_ROW_MAJOR
        };

        std::vector<std::wstring> wstrDefines;
        for (const auto& crDefine : crDesc.strDefines)
        {
            wstrDefines.emplace_back(StringToWString(crDefine));
        }
		for (const auto& crDefine : wstrDefines)
		{
			CompileArguments.push_back(L"-D");
			CompileArguments.push_back(crDefine.c_str());
		}

        

        TComPtr<IDxcBlobEncoding> ShaderBlob;
        if (FAILED(pDxcUtils->LoadFile(StringToWString(ShaderPath).c_str(), nullptr, ShaderBlob.GetAddressOf())))
        {
            LOG_ERROR("Load shader file failed.");
            return FShaderData{};
        }
        const DxcBuffer ShaderBuffer(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), 0u);

        FCustomDxcIncludeHandler IncludeHandler(pDxcUtils.Get());
        TComPtr<IDxcResult> CompileResult;
        if (FAILED(pDxcCompiler->Compile(
            &ShaderBuffer,
            CompileArguments.data(),
            static_cast<UINT32>(CompileArguments.size()),
            &IncludeHandler,
            IID_PPV_ARGS(CompileResult.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to IDxcCompiler3::Compile() failed");
            return FShaderData{};
        }

        TComPtr<IDxcBlobUtf8> Errors;
		CompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(Errors.GetAddressOf()), nullptr);
		if (Errors != nullptr && Errors->GetStringLength() > 0)
        {
            LOG_ERROR(Errors->GetStringPointer());
            return FShaderData{};
        }


        TComPtr<IDxcBlob> ResultShaderData;
        if (FAILED(CompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(ResultShaderData.GetAddressOf()), nullptr)))
        {
            LOG_ERROR("Call to IDxcCompiler3::GetOutput().Result failed");
            return FShaderData{};
        }

        FShaderData ShaderData;
        ShaderData.SetByteCode(ResultShaderData->GetBufferPointer(), ResultShaderData->GetBufferSize());
        ShaderData.strIncludeShaderFiles = std::move(IncludeHandler.IncludedFileNames);
        ShaderData.strIncludeShaderFiles.push_back(crDesc.strShaderName.c_str());

        SaveToCache(CachePath.c_str(), ShaderData);

        return ShaderData;
    }
#endif

}

























