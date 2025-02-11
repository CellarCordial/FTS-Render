
target("shader")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")
    add_deps("core", { public=true })
    add_includedirs("$(projectdir)/external/slang/include", { public=true })
    add_linkdirs("$(projectdir)/external/slang/lib", { public=true })
    add_links("slang.lib", { public=true })
    after_build(
        function (target)
            os.cp("$(projectdir)/external/slang/bin/slang.dll", target:targetdir())
        end
    )
target_end()