#include "shader_compiler.h"

#include "../core/tools/file.h"
#include "../core/tools/log.h"
#include <string>
#include <vector>
#include <winerror.h>
#include <winnt.h>
#include <winscard.h>
#include <wrl/client.h>

#ifdef SLANG_SHADER
#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#endif

#ifdef HLSL_SHADER
#include <dxcapi.h>
#endif

namespace fantasy 
{
    bool check_cache(const char* cache_path, const char* shader_path)
    {
        if (!is_file_exist(cache_path)) return false;
        if (compare_file_write_time(shader_path, cache_path)) return false;
        return true;
    }

    void save_to_cache(const char* cache_path, const ShaderData& data)
    {
        if (data.invalid())
        {
            LOG_ERROR("Call to FShaderCompiler::save_to_cache() failed for invalid Shader data.");
            return;
        }

        serialization::BinaryOutput output(cache_path);
        output(data._data.size());
        output.save_binary_data(data._data.data(), data._data.size());
    }

    ShaderData load_from_cache(const char* cache_path)
    {
        ShaderData shader_data;
        
        serialization::BinaryInput input(cache_path);

        uint64_t ByteCodeSize = 0;
        input(ByteCodeSize);
        shader_data._data.resize(ByteCodeSize);
        input.load_binary_data(shader_data._data.data(), ByteCodeSize);

        return shader_data;
    }


#if defined(SLANG_SHADER)
namespace shader_compile
{
    namespace
    {
        Slang::ComPtr<slang::IGlobalSession> global_session;
    }

    void initialize()
    {
        SlangResult res = createGlobalSession(global_session.writeRef());
        assert(res == SLANG_OK);
    }

    void destroy()
    {
        global_session->release();
    }

    ShaderData compile_shader(const ShaderCompileDesc& desc)
    {
        if (global_session == nullptr)
        {
            LOG_ERROR("Please call fantasy::StaticShaderCompiler::initialize() first.");
            return ShaderData{};
        }

        const std::string proj_path = PROJ_DIR;
        const std::string cache_path = proj_path + "asset/ShaderCache/" + remove_file_extension(desc.shader_name.c_str()) + "_" + desc.entry_point + "_DEBUG.bin";
        const std::string shader_path = proj_path + "Source/Shader/" + desc.shader_name;

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
        target_desc.format = SLANG_DXIL;
        target_desc.profile = global_session->findProfile("sm_6_5");

        session_desc.targets = &target_desc;
        session_desc.targetCount = 1;

        std::vector<slang::PreprocessorMacroDesc> preprocessor_macro_desc;
        for (const auto& crDefine : desc.defines)
        {
            auto equal_position = crDefine.find_first_of('=');
            preprocessor_macro_desc.push_back(slang::PreprocessorMacroDesc{
                .name = crDefine.substr(0, equal_position).c_str(),
                .value = crDefine.substr(equal_position + 1).c_str()
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
        global_session->createSession(session_desc, session.writeRef());

        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> module(session->loadModule("MyShaders", diagnostics.writeRef()));
        if (diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        Slang::ComPtr<slang::IEntryPoint> entry_point;
        module->getDefinedEntryPoint(0, entry_point.writeRef());

        slang::IComponentType* components[] = { module, entry_point };
        Slang::ComPtr<slang::IComponentType> program;
        session->createCompositeComponentType(components, 2, program.writeRef());

        Slang::ComPtr<slang::IComponentType> linked_program;
        program->link(linked_program.writeRef(), diagnostics.writeRef());
        if (diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        int entryPointIndex = 0;
        int targetIndex = 0;
        Slang::ComPtr<slang::IBlob> pKernelBlob;
        linked_program->getEntryPointCode(0, 0, pKernelBlob.writeRef(), diagnostics.writeRef());
        if (diagnostics)
        {
            LOG_ERROR((const char*) diagnostics->getBufferPointer());
            return ShaderData{};
        }
        diagnostics.setNull();

        ShaderData shader_data;
        shader_data.set_byte_code(pKernelBlob->getBufferPointer(), pKernelBlob->getBufferSize());

        save_to_cache(cache_path.c_str(), shader_data);

        return ShaderData{};
    }
}

#elif defined(HLSL_SHADER)
    inline LPCWSTR get_target_profile(ShaderTarget target)
    {
        switch (target)
        {
        case ShaderTarget::Vertex  : return L"vs_6_5";
        case ShaderTarget::Hull    : return L"hs_6_5";
        case ShaderTarget::Domain  : return L"ds_6_5";
        case ShaderTarget::Geometry: return L"gs_6_5";
        case ShaderTarget::Pixel   : return L"ps_6_5";
        case ShaderTarget::Compute : return L"cs_6_5";
        default:
            assert(false && "There is no such shader target.");
            return L"";
        }
    }

    
    class CustomDxcIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit CustomDxcIncludeHandler(IDxcUtils* InUtils) : IDxcIncludeHandler(), _utils(InUtils) {}
        virtual ~CustomDxcIncludeHandler() = default;
        HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFileName, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
        {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> ShaderSourceBlob;
            std::wstring file_name_wstring(pFileName);
            std::string file_path_to_include(file_name_wstring.begin(), file_name_wstring.end());
            std::transform(
                file_path_to_include.begin(),
                file_path_to_include.end(),
                file_path_to_include.begin(),
                [](char& c)
                {
                    if (c == '\\') c = '/';
                    return c;
                }
            );
            
            if (!is_file_exist(file_path_to_include.c_str()))
            {
                ppIncludeSource = nullptr;
                return E_FAIL;
            }

            const auto iterator = std::find_if(
                _included_file_names.begin(),
                _included_file_names.end(),
                [&file_path_to_include](const auto& InName)
                {
                    return InName == file_path_to_include;
                }
            );
            if (iterator != _included_file_names.end())
            {
                // Return a blank blob if this file has been included before
                constexpr char null_str[] = " ";
                _utils->CreateBlobFromPinned(null_str, ARRAYSIZE(null_str), DXC_CP_ACP, ShaderSourceBlob.GetAddressOf());
                *ppIncludeSource = ShaderSourceBlob.Detach();
                return S_OK;
            }

            file_name_wstring = std::wstring(file_path_to_include.begin(), file_path_to_include.end());
            HRESULT hr = _utils->LoadFile(file_name_wstring.c_str(), nullptr, ShaderSourceBlob.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                _included_file_names.push_back(file_path_to_include);
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

        std::vector<std::string> _included_file_names;
        IDxcUtils* _utils;
    };

    namespace
    {
        Microsoft::WRL::ComPtr<IDxcCompiler3> dxc_compiler;
        Microsoft::WRL::ComPtr<IDxcUtils> dxc_utils;
        std::vector<std::string> semantic_names;
    }

    void shader_compile::initialize()
    {
        if (dxc_compiler || dxc_utils)
        {
            LOG_ERROR("StaticShaderCompiler has already initialized.");
        }

        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxc_compiler.GetAddressOf()))))
        {
            LOG_ERROR("Call to DxcCreateInstance failed.");
        }
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utils.GetAddressOf()))))
        {
            LOG_ERROR("Call to DxcCreateInstance failed.");
        }
    }

    void shader_compile::destroy()
    {
        dxc_compiler.Reset();
        dxc_utils.Reset();
    }


    ShaderData shader_compile::compile_shader(const ShaderCompileDesc& desc)
    {
        if (dxc_compiler == nullptr)
        {
            LOG_ERROR("Please call fantasy::StaticShaderCompiler::initialize() first.");
            return ShaderData{};
        }

        const std::string proj_path = PROJ_DIR;
        const std::string shader_path = proj_path + "Source/Shader/" + desc.shader_name;
        const std::string cache_path = 
            proj_path + "asset/ShaderCache/" + remove_file_extension(desc.shader_name.c_str()) + "_" + desc.entry_point + "_DEBUG.bin";

		size_t pos = shader_path.find_last_of('/');
		if (pos == std::string::npos)
		{
			LOG_ERROR("Find hlsl file's Directory failed.");
			return ShaderData{};
		}
        const std::string file_directory = shader_path.substr(0, pos);
		const std::wstring file_directory_wstring(file_directory.begin(), file_directory.end());

        if (check_cache(cache_path.c_str(), shader_path.c_str()))
        {
            return load_from_cache(cache_path.c_str());
        }

        const std::wstring entry_point(desc.entry_point.begin(), desc.entry_point.end());

        std::vector<LPCWSTR> compile_arguments
        {
            L"-E",
            entry_point.c_str(),
            L"-T",
            get_target_profile(desc.target),
            L"-Qembed_debug",
            L"-Od",
            L"-I",
            file_directory_wstring.c_str(),
            DXC_ARG_DEBUG,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_PACK_MATRIX_ROW_MAJOR
        };

        std::vector<std::wstring> defines;
        for (const auto& define : desc.defines)
        {
            defines.emplace_back(define.begin(), define.end());
        }
		for (const auto& define : defines)
		{
			compile_arguments.push_back(L"-D");
			compile_arguments.push_back(define.c_str());
		}

        

        Microsoft::WRL::ComPtr<IDxcBlobEncoding> shader_blob;
        std::wstring shader_path_wstring(shader_path.begin(), shader_path.end());
        if (FAILED(dxc_utils->LoadFile(shader_path_wstring.c_str(), nullptr, shader_blob.GetAddressOf())))
        {
            LOG_ERROR("Load shader file failed.");
            return ShaderData{};
        }
        const DxcBuffer shader_buffer(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), 0u);

        CustomDxcIncludeHandler include_handler(dxc_utils.Get());
        Microsoft::WRL::ComPtr<IDxcResult> compile_result;
        if (FAILED(dxc_compiler->Compile(
            &shader_buffer,
            compile_arguments.data(),
            static_cast<uint32_t>(compile_arguments.size()),
            &include_handler,
            IID_PPV_ARGS(compile_result.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to IDxcCompiler3::compile() failed");
            return ShaderData{};
        }

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
		if (errors != nullptr && errors->GetStringLength() > 0)
        {
            LOG_ERROR(errors->GetStringPointer());
            return ShaderData{};
        }


        Microsoft::WRL::ComPtr<IDxcBlob> result_shader_data;
        if (FAILED(compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(result_shader_data.GetAddressOf()), nullptr)))
        {
            LOG_ERROR("Call to IDxcCompiler3::GetOutput().Result failed");
            return ShaderData{};
        }

        ShaderData shader_data;
        shader_data.set_byte_code(result_shader_data->GetBufferPointer(), result_shader_data->GetBufferSize());
        shader_data._include_shader_files = std::move(include_handler._included_file_names);
        shader_data._include_shader_files.push_back(desc.shader_name.c_str());

        save_to_cache(cache_path.c_str(), shader_data);

        return shader_data;
    }
#endif

}

























