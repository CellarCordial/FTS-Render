

target("dynamic_rhi")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")
    add_deps("core", { public=true })
    add_includedirs("$(projectdir)/external/VulkanSDK/include", { public=true })
    add_linkdirs("$(projectdir)/external/VulkanSDK/lib", { public=true })
    add_links("vulkan-1.lib", { public=true })
target_end()