# Three.js 面元辐射度界面

## 启动

先构建原生 CPU/Vulkan 运行器：

```text
cmake --build <build-dir> --target radiosity_web_runner
```

然后启动网页：

```text
cd gui
npm install
npm start
```

打开 <http://127.0.0.1:43177>。

服务会依次查找：

- `cmake-build-gui/radiosity_web_runner.exe`
- `cmake-build-vulkan/radiosity_web_runner.exe`
- `cmake-build-release/radiosity_web_runner.exe`
- `cmake-build-debug/radiosity_web_runner.exe`

也可以通过 `RADIOSITY_RUNNER` 指定可执行文件。

网页中的 CPU 与 GPU 按钮都会运行原生模型。Three.js 只负责场景交互和逐面元结果着色；Vulkan 计算仍由原生后端执行。
