# STREAMFIELD

纯 Three.js Web 界面，通过本地 Node 桥接服务运行 `C:\work\histream`。

```powershell
cd C:\Users\jiank\Documents\ChatGPT\streamfield
npm install
npm run build
npm start
```

浏览器打开：

```text
http://127.0.0.1:4173
```

开发模式：

```powershell
npm run dev
```

点击“打开工程”，输入已有工程目录、`project.json` 或 `Input.xml` 完整路径。打开时只读取现有文件，点击“保存”后才写回工程。

默认优先使用已测试通过的：

```text
C:\work\bin_x64\Debug\histream.exe
```

支持 `eVoxelEB`、`eWaterEB`、`eVoxelRT`、`eRaytracing`，以及 XML 编辑、OBJ 预览、实时日志和停止任务。
