# StreamSim

StreamSim 是调用本地辐射传输模型的 Web GUI。源码、编译结果和运行文件均集中在本项目目录。

```text
streamsim/
├─ src/renderer/                GUI 源码
├─ gui/                         GUI 编译结果
├─ server.mjs                   Node 后端桥接
├─ assets/                      内置 OBJ、DEM、光谱和大气数据
├─ build/histream/              HiStream CMake 中间文件
├─ models/
│  ├─ histream/                 HiStream 源码、shader 和属性库
│  ├─ bin_x64/Release/          HiStream Release 编译结果
│  ├─ bin_x64/Debug/            HiStream Debug 编译结果
│  └─ nvpro_core/               HiStream 公共 C++ 依赖源码
├─ runtime/                     运行期资源和日志
└─ tools/                       构建、测试和数据生成脚本
```

启动：

```powershell
cd C:\work\streamsim
npm install
npm run build:gui
npm start
```

浏览器打开 `http://127.0.0.1:4173`。

文档：

- [模型使用手册](docs/user-manual.md)
- [模型理论手册](docs/theory-manual.md)

统一构建：

```powershell
npm run build:all
```

`build:all` 先编译 GUI，再编译 Release 引擎。CMake 优先使用环境变量 `VCPKG_ROOT`，未设置时会查找项目同级的 `vcpkg` 目录。

模型重新编译入口：

- `models/histream/CMakeLists.txt`
- `models/histream/src/main.cpp`
- `models/histream/src/base/engine.cpp`
- `models/histream/src/facetrt/`（Facetrt、FacetrtIO、Vulkan 核心）
- `models/histream/src/faceteb/`（Faceteb、FacetebIO）
- `models/histream/shader/facetrt/`
- `models/histream/shader/faceteb/`

CMake 编译仍需要本机 Vulkan SDK、Visual Studio 和 vcpkg 依赖。`eFacetRT` 和 `eFacetEB` 现在统一由 `histream.exe` 调用，分别加载 `shader/facetrt` 和 `shader/faceteb`；运行时不再调用独立 `radiosity_web_runner.exe`。
