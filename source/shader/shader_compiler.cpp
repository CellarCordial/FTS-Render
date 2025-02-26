#include "shader_compiler.h"

#include "../core/tools/file.h"
#include "../core/tools/log.h"
#include <cstdint>
#include <string>
#include <vector>
#include <winerror.h>
#include <winnt.h>
#include <winscard.h>
#include <wrl/client.h>

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>


namespace fantasy 
{
    bool check_cache(const char* cache_path, const char* shader_path)
    {
        if (!is_file_exist(cache_path)) return false;
        if (compare_file_write_time(shader_path, cache_path)) return false;
        return true;
    }

    void save_to_cache(const ShaderCompileDesc& desc, const char* cache_path, const ShaderData& data)
    {
        if (!data.is_valid())
        {
            LOG_ERROR("Call to FShaderCompiler::save_to_cache() failed for invalid Shader data.");
            return;
        }

        serialization::BinaryOutput output(cache_path);
        for (const auto& define : desc.defines) output(define);
        output(data._data.size());
        output.save_binary_data(data._data.data(), data._data.size());
    }

    ShaderData load_from_cache(const ShaderCompileDesc& desc, const char* cache_path)
    {
        ShaderData shader_data;
        
        serialization::BinaryInput input(cache_path);

        for (const auto& define : desc.defines)
        {
            std::string cache_define;
            input(cache_define);
            if (cache_define != define) return ShaderData{};
        }

        uint64_t ByteCodeSize = 0;
        input(ByteCodeSize);
        shader_data._data.resize(ByteCodeSize);
        input.load_binary_data(shader_data._data.data(), ByteCodeSize);

        return shader_data;
    }



    ShaderPlatform platform = ShaderPlatform::DXIL;

    void set_shader_platform(ShaderPlatform in_platform)
    {
        platform = in_platform;
    }


#ifdef SLANG_SHADER

    Slang::ComPtr<slang::IGlobalSession> global_session;

    ShaderData compile_shader(const ShaderCompileDesc& desc)
    { 
        if (global_session == nullptr)
        {
            SlangResult res = createGlobalSession(global_session.writeRef());
            assert(res == SLANG_OK);
        }

        if (global_session == nullptr)
        {
            LOG_ERROR("Please call fantasy::StaticShaderCompiler::initialize() first.");
            return ShaderData{};
        }

        const std::string proj_path = PROJ_DIR;
        const std::string cache_path = proj_path + "asset/cache/shader/" + remove_file_extension(desc.shader_name.c_str()) + "_" + desc.entry_point + "_DEBUG.bin";
        const std::string shader_path = proj_path + "source/shader/" + desc.shader_name;

        if (check_cache(cache_path.c_str(), shader_path.c_str()))
        {
            return load_from_cache(cache_path.c_str());
        }

        size_t pos = shader_path.find_last_of('/');
        if (pos == std::string::npos)
        {
            LOG_ERROR("Find hlsl file's Directory failed.");
            return ShaderData{};
        }
        const std::string file_directory = shader_path.substr(0, pos);


        slang::SessionDesc session_desc{};

        slang::TargetDesc target_desc = {};
        switch (platform)
        {
        case ShaderPlatform::DXIL:
            target_desc.format = SLANG_DXIL;
            target_desc.profile = global_session->findProfile("sm_6_6");
            break;
        case ShaderPlatform::SPIRV:
            target_desc.format = SLANG_SPIRV;
            target_desc.profile = global_session->findProfile("spirv_1_0");
            break;        
        }
        

        session_desc.targets = &target_desc;
        session_desc.targetCount = 1;

        std::vector<std::string> macro_names;
        std::vector<std::string> macro_values;
        for (auto& define : desc.defines)
        {
            auto equal_position = define.find_first_of('=');
            macro_names.emplace_back(define.substr(0, equal_position));
            macro_values.emplace_back(define.substr(equal_position + 1));
        }

        std::vector<slang::PreprocessorMacroDesc> preprocessor_macro_desc;
        for (uint32_t ix = 0; ix < desc.defines.size(); ++ix)
        {
            preprocessor_macro_desc.push_back(slang::PreprocessorMacroDesc{
                .name = macro_names[ix].c_str(),
                .value = macro_values[ix].c_str()
            });
        }

        session_desc.preprocessorMacros = preprocessor_macro_desc.data();
        session_desc.preprocessorMacroCount = preprocessor_macro_desc.size();

        std::array<slang::CompilerOptionEntry, 1> options = 
        {
            { 
                slang::CompilerOptionName::DebugInformation,
                { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr }
            }
        };
        session_desc.compilerOptionEntries = options.data();
        session_desc.compilerOptionEntryCount = options.size();

        const char* searchPaths[] = { file_directory.c_str() };
        session_desc.searchPaths = searchPaths;
        session_desc.searchPathCount = 1;

        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(global_session->createSession(session_desc, session.writeRef())))
        {
            LOG_ERROR("Create session failed.");
            return ShaderData{};
        }

        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> module(session->loadModule(shader_path.c_str(), diagnostics.writeRef()));
        if (diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(module->findEntryPointByName(desc.entry_point.c_str(), entry_point.writeRef())))
        {
            LOG_ERROR("Find entry point failed.");
            return ShaderData{};
        }

        slang::IComponentType* components[] = { module, entry_point };
        Slang::ComPtr<slang::IComponentType> program;
        if (SLANG_FAILED(session->createCompositeComponentType(components, 2, program.writeRef())))
        {
            LOG_ERROR("Create composite component type failed.");
            return ShaderData{};
        }

        Slang::ComPtr<slang::IComponentType> linked_program;
        if (SLANG_FAILED(program->link(linked_program.writeRef(), diagnostics.writeRef())) || diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        int32_t entryPointIndex = 0;
        int32_t targetIndex = 0;
        Slang::ComPtr<slang::IBlob> pKernelBlob;
        if (SLANG_FAILED(linked_program->getEntryPointCode(0, 0, pKernelBlob.writeRef(), diagnostics.writeRef())) || diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        ShaderData shader_data;
        shader_data.set_byte_code(pKernelBlob->getBufferPointer(), pKernelBlob->getBufferSize());

        save_to_cache(cache_path.c_str(), shader_data);

        return shader_data;
    }
#endif
    
#ifdef HLSL_SHADER
#include <dxcapi.h>


    inline LPCWSTR GetTargetProfile(ShaderTarget target)
    {
        switch (target)
        {
        case ShaderTarget::Vertex  : return L"vs_6_6";
        case ShaderTarget::Hull    : return L"hs_6_6";
        case ShaderTarget::Domain  : return L"ds_6_6";
        case ShaderTarget::Geometry: return L"gs_6_6";
        case ShaderTarget::Pixel   : return L"ps_6_6";
        case ShaderTarget::Compute : return L"cs_6_6";
        default:
            assert(false && "There is no such shader target.");
            return L"";
        }
    }

    class CustomDxcIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit CustomDxcIncludeHandler(IDxcUtils* InUtils) : IDxcIncludeHandler(), Utils(InUtils) {}
        virtual ~CustomDxcIncludeHandler() = default;
        HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
        {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> ShaderSourceBlob;
            std::wstring wstr_file_name = pFilename;
            std::string FilePathToInclude(wstr_file_name.begin(), wstr_file_name.end());
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
            
            if (!is_file_exist(FilePathToInclude.c_str()))
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

            HRESULT hr = Utils->LoadFile(std::wstring(FilePathToInclude.begin(), FilePathToInclude.end()).c_str(), nullptr, ShaderSourceBlob.GetAddressOf());
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
        Microsoft::WRL::ComPtr<IDxcCompiler3> dxc_compiler;
        Microsoft::WRL::ComPtr<IDxcUtils> dxc_utils;
        std::vector<std::string> SemanticNames;
    }

    ShaderData compile_shader(const ShaderCompileDesc& desc)
    {
        if (dxc_compiler == nullptr)
        {
            if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxc_compiler.GetAddressOf()))))
            {
                LOG_ERROR("Call to DxcCreateInstance failed.");
            }
            if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utils.GetAddressOf()))))
            {
                LOG_ERROR("Call to DxcCreateInstance failed.");
            }
        }

        const std::string proj_path = PROJ_DIR;
        const std::string cache_path = proj_path + "asset/cache/shader/" + remove_file_extension(desc.shader_name.c_str()) + "_" + desc.entry_point + "_DEBUG.bin";
        const std::string shader_path = proj_path + "source/shader/" + desc.shader_name;

		size_t pos = shader_path.find_last_of('/');
		if (pos == std::string::npos)
		{
			LOG_ERROR("Find hlsl file's Directory failed.");
			return ShaderData{};
		}

        std::string str_file_directory = shader_path.substr(0, pos);
		const std::wstring wstr_file_directroy = std::wstring(str_file_directory.begin(), str_file_directory.end());

        if (check_cache(cache_path.c_str(), shader_path.c_str()))
        {
            ShaderData data =load_from_cache(desc, cache_path.c_str());
            if (data.is_valid()) return data;
        }

        const std::wstring EntryPoint = std::wstring(desc.entry_point.begin(), desc.entry_point.end());

        std::vector<LPCWSTR> compile_arguments
        {
            L"-E",
            EntryPoint.c_str(),
            L"-T",
            GetTargetProfile(desc.target),
            L"-Qembed_debug",
            L"-Od",
            L"-I",
            wstr_file_directroy.c_str(),
            DXC_ARG_DEBUG,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_PACK_MATRIX_ROW_MAJOR
        };

        std::vector<std::wstring> wstrDefines;
        for (const auto& define : desc.defines)
        {
            wstrDefines.emplace_back(std::wstring(define.begin(), define.end()));
        }
		for (const auto& define : wstrDefines)
		{
			compile_arguments.push_back(L"-D");
			compile_arguments.push_back(define.c_str());
		}

        

        Microsoft::WRL::ComPtr<IDxcBlobEncoding> dxc_shader_blob;
        if (FAILED(dxc_utils->LoadFile(std::wstring(shader_path.begin(), shader_path.end()).c_str(), nullptr, dxc_shader_blob.GetAddressOf())))
        {
            LOG_ERROR("Load shader file failed.");
            return ShaderData{};
        }
        const DxcBuffer dxc_shader_buffer(dxc_shader_blob->GetBufferPointer(), dxc_shader_blob->GetBufferSize(), 0u);

        CustomDxcIncludeHandler dxc_include_handler(dxc_utils.Get());
        Microsoft::WRL::ComPtr<IDxcResult> CompileResult;
        if (FAILED(dxc_compiler->Compile(
            &dxc_shader_buffer,
            compile_arguments.data(),
            static_cast<uint32_t>(compile_arguments.size()),
            &dxc_include_handler,
            IID_PPV_ARGS(CompileResult.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to IDxcCompiler3::compile() failed");
            return ShaderData{};
        }

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> dxc_errors;
		CompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(dxc_errors.GetAddressOf()), nullptr);
		if (dxc_errors != nullptr && dxc_errors->GetStringLength() > 0)
        {
            LOG_ERROR(dxc_errors->GetStringPointer());
            return ShaderData{};
        }


        Microsoft::WRL::ComPtr<IDxcBlob> result_shader_data;
        if (FAILED(CompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(result_shader_data.GetAddressOf()), nullptr)))
        {
            LOG_ERROR("Call to IDxcCompiler3::GetOutput().Result failed");
            return ShaderData{};
        }

        ShaderData shader_data;
        shader_data.set_byte_code(result_shader_data->GetBufferPointer(), result_shader_data->GetBufferSize());
        shader_data._include_shader_files = std::move(dxc_include_handler.IncludedFileNames);
        shader_data._include_shader_files.push_back(desc.shader_name.c_str());

        save_to_cache(desc, cache_path.c_str(), shader_data);

        return shader_data;
    }
#endif
}






















