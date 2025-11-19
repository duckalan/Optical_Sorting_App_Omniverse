# Description
TODO

# Prerequisites
- Builded OpenCV 4.12.0

# Build
1. Specify path to the OpenCV build folder in [`tools/deps/ext-deps.packman.xml`](tools/deps/ext-deps.packman.xml):
```xml
<dependency name="opencv" linkPath="../../_build/target-deps/opencv">
  <source path="path/to/OpenCV_4_12_0/build" />
</dependency>
```
2. Run `repo.bat build` (Windows) or `repo.sh build` (Linux)
