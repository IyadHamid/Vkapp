module;

#include <slang.h>
#include <slang-com-ptr.h>

#include "log.h"

module vkapp;

using namespace vkapp;

ShaderSession::ShaderSession(std::span<const char* const> search_paths, std::span<const MacroDesc> macros) {
    SlangGlobalSessionDesc desc = {};
    check(slang::createGlobalSession(&desc, global_session.writeRef()), "unable to create slang global session");
    std::vector<slang::CompilerOptionEntry> compiler_options = {
#ifndef NDEBUG
        // { slang::CompilerOptionName::DebugInformation, slang::CompilerOptionValue{ .intValue0 = SLANG_DEBUG_INFO_LEVEL_MINIMAL } },
#endif
        { slang::CompilerOptionName::Optimization, slang::CompilerOptionValue{ .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL } },
        { slang::CompilerOptionName::FloatingPointMode, slang::CompilerOptionValue{ .intValue0 = SLANG_FLOATING_POINT_MODE_FAST } }
    };

    std::array target_descs = { slang::TargetDesc{
        .format = SLANG_SPIRV, // SLANG_SPRIV_ASM (?)
        .profile = global_session->findProfile("spirv_1_6"),
        .compilerOptionEntries = compiler_options.data(),
        .compilerOptionEntryCount = static_cast<std::uint32_t>(compiler_options.size())
    } };
    slang::SessionDesc session_desc{
        .targets = target_descs.data(),
        .targetCount = static_cast<SlangInt>(target_descs.size()),
        .searchPaths = search_paths.data(),
        .searchPathCount = static_cast<SlangInt>(search_paths.size()),
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = static_cast<SlangInt>(macros.size())
    };
    check(global_session->createSession(session_desc, session.writeRef()), "unable to create slang session");
}

ShaderSession::CodesStages ShaderSession::compile(zstring_view name, std::span<const zstring_view> entry_point_names) {
    Slang::ComPtr<slang::IBlob> diagnostics;
    auto diagnostics_error = [&] {
        if (not diagnostics)
            return;
        SPDLOG_WARN("shader creation failed {}", std::string_view(static_cast<const char*>(diagnostics->getBufferPointer())));
        return;
        fail("shader creation failed");
    };

    Slang::ComPtr module(session->loadModule(name.c_str(), diagnostics.writeRef()));
    diagnostics_error();

    std::vector entry_points(std::from_range,
        entry_point_names | std::views::transform([&](zstring_view name) {
            Slang::ComPtr<slang::IEntryPoint> entry_point;
            check(module->findEntryPointByName(name.c_str(), entry_point.writeRef()), "unable to find entry point");
            return entry_point;
        })
    );

    // TODO[C++26]: replace with std::views::concat
    std::vector<slang::IComponentType*> components{ module };
    components.insert_range(components.end(), entry_points);

    Slang::ComPtr<slang::IComponentType> composed_program;
    session->createCompositeComponentType(components.data(), components.size(), composed_program.writeRef(), diagnostics.writeRef());
    diagnostics_error();

    Slang::ComPtr<slang::IComponentType> linked_program;
    composed_program->link(linked_program.writeRef(), diagnostics.writeRef());
    diagnostics_error();

    auto layout = linked_program->getLayout();

    std::vector codes(std::from_range, std::views::iota(0u, layout->getEntryPointCount()) | std::views::transform([&](auto index) {
        Slang::ComPtr<slang::IBlob> code;
        linked_program->getEntryPointCode(index, 0, code.writeRef(), diagnostics.writeRef());
        diagnostics_error();
        auto stage = layout->getEntryPointByIndex(index)->getStage();
        return std::pair{ code, stage };
    }));

    return codes;
}

std::pair<std::vector<ShaderSession::PipelineShaderCreateInfo>, ShaderSession::CodesStages> ShaderSession::load(vk::Device device, zstring_view name, std::span<const zstring_view> entry_point_names) {
    auto codes_stages = compile(name, entry_point_names);

    auto code_bytes = [](Slang::ComPtr<slang::IBlob> blob) { 
        return vk::ArrayProxyNoTemporaries(blob->getBufferSize() / 4, static_cast<const std::uint32_t*>(blob->getBufferPointer())); 
    };

    std::vector create_infos(std::from_range,
        codes_stages
        | std::views::transform([&](const auto& code_stage) {
            auto stage = stageCompat(code_stage.second);

            return ShaderSession::PipelineShaderCreateInfo{
                vk::PipelineShaderStageCreateInfo(
                    {},
                    stage,
                    {},
                    "main"
                ),
                vk::ShaderModuleCreateInfo(
                    {},
                    code_bytes(code_stage.first)
                )
            };
        })
    );
    
    return { create_infos, std::move(codes_stages) };
}