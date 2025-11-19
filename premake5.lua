-- Shared build scripts from repo_build package.
repo_build = require("omni/repo/build")

-- Repo root
root = repo_build.get_abs_path(".")
-- Run repo_kit_tools premake5-kit that includes a bunch of Kit-friendly tooling configuration.
kit = require("_repo/deps/repo_kit_tools/kit-template/premake5-kit")

-- Here it's being build with cfg.system
kit.setup_all({ cppdialect = "C++17" })


-- Registries config for testing
repo_build.prebuild_copy {
    { "%{root}/tools/deps/user.toml", "%{root}/_build/deps/user.toml" },
}

repo_build.prebuild_copy {
    {"tools/isaacsim/data/python/shared/*",  "_build/%{platform}/%{config}"},
    {"tools/isaacsim/data/python/%{platform}/*",  "_build/%{platform}/%{config}"},
    {"tools/isaacsim/data/jupyter_kernel",  "_build/%{platform}/%{config}/jupyter_kernel"},
    {"tools/isaacsim/data/python_packages", "_build/%{platform}/%{config}/python_packages" },
}

-- Isaac Sim default apps
define_app("kvantron.optical_cap_sorting.kit")
define_app("kvantron.optical_cap_sorting_zero_delay.kit")