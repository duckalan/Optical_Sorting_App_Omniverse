# Description
TODO

# Prerequisites
Common:
- Builded OpenCV 4.12.0.
  
For Windows:
- Visual Studio 2022;
- Windows SDK 10.0.17763.0;
- MSVC 142.
  
For Linux:
- In [`repo.toml`](repo.toml) comment lines for [Windows development](https://github.com/isaac-sim/isaacsim-app-template/blob/main/readme-assets/additional-docs/windows_developer_configuration.md):
```toml
# "platform:windows-x86_64".enabled = true

[repo_build.msbuild]
# link_host_toolchain = true
# vs_version = "vs2022"
# vs_path = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\"
# vs_edition = "Community"
# msvc_version = "v142"
# msbuild_version = "17"
# winsdk_version = "10.0.17763.0"
```

# Build
1. Specify path to the OpenCV build folder in [`tools/deps/ext-deps.packman.xml`](tools/deps/ext-deps.packman.xml):
```xml
<dependency name="opencv" linkPath="../../_build/target-deps/opencv">
  <source path="path/to/OpenCV_4_12_0/build" />
</dependency>
```
2. Run `repo.bat build` (Windows) or `repo.sh build` (Linux)
