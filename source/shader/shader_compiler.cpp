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



    Slang::ComPtr<slang::IGlobalSession> global_session;
    ShaderPlatform platform = ShaderPlatform::DXIL;

    void set_shader_platform(ShaderPlatform in_platform)
    {
        platform = in_platform;
    }


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
}

























