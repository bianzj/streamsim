# StreamSim

StreamSim 是调用本地辐射传输模型的 Web GUI。GUI、Node 桥接服务、模型源码和运行文件均位于本目录。

```text
streamsim/
├─ src/                         Web GUI
├─ server.mjs                   Node 后端桥接
├─ models/
│  ├─ histream/                 HiStream 源码、shader、属性库和可执行文件
│  └─ nvpro_core/               HiStream 公共 C++ 依赖源码
├─ runtime/                     兼容运行资源
└─ legacy-root/radiosity-source/ 原独立 Radiosity 备份
```

启动：

```powershell
cd C:\Users\jiank\Documents\ChatGPT\streamfield\streamsim
npm install
npm run build
npm start
```

浏览器打开 `http://127.0.0.1:4173`。

模型重新编译入口：

- `models/histream/CMakeLists.txt`
- `models/histream/src/main.cpp`
- `models/histream/src/base/engine.cpp`
- `models/histream/src/facetrt/`（Facetrt、FacetrtIO、Vulkan 核心）
- `models/histream/src/faceteb/`（Faceteb、FacetebIO）
- `models/histream/shader/facetrt/`
- `models/histream/shader/faceteb/`

CMake 编译仍需要本机 Vulkan SDK、Visual Studio 和 vcpkg 依赖。`eFacetRT` 和 `eFacetEB` 现在统一由 `histream.exe` 调用，分别加载 `shader/facetrt` 和 `shader/faceteb`；运行时不再调用独立 `radiosity_web_runner.exe`。
