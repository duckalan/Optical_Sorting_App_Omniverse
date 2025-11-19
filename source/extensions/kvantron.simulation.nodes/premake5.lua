-- Setup the basic extension variables
local ext = get_current_extension_info()
ext.group = "kvantron"
-- Set up the basic shared project information
project_ext(ext)

local ogn = get_ogn_project_information(ext, "kvantron/simulation/nodes")
-- -------------------------------------
-- Breaking this out as a separate project ensures the .ogn files
-- are processed before their results are needed
project_ext_ogn(ext, ogn)

-- -------------------------------------
-- Build the C++ plugin that will be loaded by the extension
project_ext_plugin(ext, ogn.plugin_project)
add_files("impl", ogn.plugin_path)
add_files("nodes", ogn.nodes_path)
add_files("python", "*.py")
add_files("python/_impl", "python/_impl/**.py")
add_files("python/nodes", "python/nodes")
add_files("config", "config")
add_files("docs", ogn.docs_path)
add_files("data", "data")

add_ogn_dependencies(ogn, { "python/nodes" })
add_usd("UsdUtils")

filter { "configurations:debug" }
defines { "_DEBUG" }
filter { "configurations:release" }
defines { "NDEBUG" }
filter {}

cppdialect "C++17"

-- -------------------------------------
-- Link/copy folders and files to be packaged with the extension
repo_build.prebuild_link {
    { "data",        ext.target_dir .. "/data" },
    { "docs",        ext.target_dir .. "/docs" },
    { "python/impl", ogn.python_target_path .. "/impl" },
}

repo_build.prebuild_copy {
    { "python/*.py", ogn.python_target_path },
}

includedirs {
    kit_dev_dir .. "/include",
    kit_dev_dir .. "/gsl/include",
    kit_dev_dir .. "/fabric/include",
    extsbuild_dir .. "/isaacsim.core.simulation_manager/include",
    extsbuild_dir .. "/isaacsim.core.includes/include",
    target_deps .. "/omni_physics/%{config}/include",
}

-- BEGIN OpenCV
-- Link static OpenCV libraries and copy dlls to the plugin /bin folder.
libdirs { target_deps .. "/opencv/x64/vc16/lib" }

filter { "configurations:debug" }
links { "opencv_world4120d" }
postbuildcommands
{
    '{COPY} "' .. target_deps .. '/opencv/x64/vc16/bin' .. '/opencv_world4120d.dll" "%{cfg.targetdir}"'
}
filter {}

filter { "configurations:release" }
links { "opencv_world4120" }
postbuildcommands
{
    '{COPY} "' .. target_deps .. "/opencv/x64/vc16/bin" .. '/opencv_world4120.dll" "%{cfg.targetdir}"'
}
filter {}

includedirs { target_deps .. "/opencv/include" }
-- END OpenCV

-- Link and include omni_physics and PhysX libraries.
-- (the contents is from include_physx() from isaacsim-kit-premake5.lua)
defines { "PX_PHYSX_STATIC_LIB" }

-- filter { "system:windows" }
-- libdirs { "%{root}/_build/target-deps/nvtx/lib/x64" }
-- filter {}
-- filter { "system:linux" }
-- libdirs { "%{root}/_build/target-deps/nvtx/lib64" }
-- filter {}

-- filter { "configurations:debug" }
-- defines { "_DEBUG" }
-- filter { "configurations:release" }
-- defines { "NDEBUG" }
-- filter {}

filter { "system:windows", "platforms:x86_64", "configurations:debug" }
libdirs {
    "%{root}/_build/target-deps/physx/bin/win.x86_64.vc142.md/debug",
}
filter { "system:windows", "platforms:x86_64", "configurations:release" }
libdirs {
    "%{root}/_build/target-deps/physx/bin/win.x86_64.vc142.md/checked",
}
filter {}

filter { "system:linux", "platforms:x86_64", "configurations:debug" }
libdirs {
    "%{root}/_build/target-deps/physx/bin/linux.x86_64/debug",
}
filter { "system:linux", "platforms:x86_64", "configurations:release" }
libdirs {
    "%{root}/_build/target-deps/physx/bin/linux.x86_64/checked",
}
filter {}

links {
    "PhysXExtensions_static_64",
    "PhysX_static_64",
    "PhysXPvdSDK_static_64",
    "PhysXCooking_static_64",
    "PhysXCommon_static_64",
    "PhysXFoundation_static_64",
}

includedirs {
    "%{root}/_build/target-deps/physx/include",
    "%{root}/_build/target-deps/usd_ext_physics/%{cfg.buildcfg}/include",
}
