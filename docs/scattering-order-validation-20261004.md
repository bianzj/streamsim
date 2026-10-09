# StreamSim 三阶短波散射测试记录（2026-10-04）

三阶在本机、已测试的 VoxelEB 场景中可以运行。三个日间案例及草地连续 48 个昼夜节点全部完成，没有发现非法数值。草地昼夜总时间为旧一阶 35.544 秒、新一阶 48.004 秒、新三阶 60.821 秒：同一新算法增至三阶增加 26.7%，相对原有一阶路径增加 71.1%。单节点时间受初始化影响较大，不能据此预测长期任务的成本。

## 版本和配置

源码及实验引擎位于 `C:\work\streamsim`。本次没有替换 `D:\streamsim` 的安装引擎，也没有将默认散射阶数改成三阶。

| 项目 | 值 |
|---|---|
| 实验引擎 | `C:\work\streamsim\models\bin_x64\Release\histream.exe` |
| 实验引擎 SHA256 | `d0b39dc07204ccfe5e61574c42aec481ecfbad278576a7587fba91370157cda1` |
| 构建源树 SHA256 | `05c9f6431504d04935af207fdfc2de1168b8d4308ced6e0a06cfbe5ec24542af` |
| 安装引擎 SHA256 | `9b852d9e02e40048be230dd707387bfc33552f881cb61a651e48ed9a0c4126e3` |
| GPU | NVIDIA RTX PRO 6000 Blackwell Workstation Edition，97887 MiB |
| 调度 | 单个 GPU 引擎串行运行，各组一次冷启动 |
| 短波 | 400–2400 nm，共 2001 个采样点；案例分组宽度 100 nm |
| 角度 | 64 个固定方向 |

在已有项目 JSON 的 `configuration.control` 中设置：

```json
{
  "shortwaveScatteringOrders": 3,
  "radiationMaxSteps": 64
}
```

`shortwaveScatteringOrders` 接受 1–3，默认 1；2/3 启用新散射路径。1 默认保留旧路径，基准中的“新一阶”通过子进程环境 `STREAMSIM_NEW_SCATTERING=1` 启用。`radiationMaxSteps` 接受 1–4096，默认 64，是射线几何追踪上限，不能替代散射阶数。前端 JSON 保存/归一化及 XML 导出均保留这两个字段。

三组对比使用同一个实验引擎，分别选择旧一阶、新一阶、新三阶。旧到新一阶还包含天空一次散射、叶片相函数归一化、参与介质吸收、自身体元边界及天空截断处理的变化。因此，二、三阶的独立影响应看“新三阶减新一阶”。准备阶段 manifest 的 `preparedEngine` 记录构建前的引擎；实际运行引擎的身份见各组 `result.json`。

## 提前截断的代码和含义

**存在单条射线的提前截断，三阶本身没有按贡献大小提前终止。**

| 路径 | 终止条件 | 代码 |
|---|---|---|
| 新短波漫射追踪 | `throughput < 0.01` | [voxelrad_scattering_VNIR.comp](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_scattering_VNIR.comp:207) |
| 新短波漫射追踪 | `step < setting.maxStep`，默认最多 64 次几何追踪 | [voxelrad_scattering_VNIR.comp](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_scattering_VNIR.comp:176) |
| 原太阳可见性追踪 | `alphaToSolar <= 0.001` 或追踪上限 | [voxelrad_solar.comp](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_solar.comp:99) |
| 散射阶循环 | 从 1 完整执行到配置阶数，无贡献阈值提前停止 | [command.cpp](C:/work/streamsim/models/histream/src/voxeleb/command.cpp:234) |

真实逃逸或遇到不透明表面也会结束射线。防止自身相交的偏移属于数值保护，与衰减阈值不同。旧漫射代码的 `kstep <= maxStep` 实际允许 65 次几何追踪，并在达到上限、剩余透射率大于 0.05 时直接补天空辐射；新路径移除了这个补天空分支，天空只在真正向上逃逸时加入。默认旧一阶路径仍保留旧行为。

新诊断写入 `output/diagnostics/scattering_node=<节点>.json`，包括实际完成阶数、逐阶耗时、路径数、两类截断次数及残余透射率。统计覆盖短波漫射，**不覆盖太阳可见性**。GPU 每个谱组清零 uint32 计数，CPU 用 uint64 汇总，避免跨谱组计数溢出。

草地每阶透射率阈值停止率为 18.842%，北京 ROI 为 17.439%；森林为 0.002242%。这些是射线路径比例，**不是能量损失率或输出误差**。残余统计是无量纲 throughput 的求和，逐阶吸收统计是体元通量密度之和，均不能当作区域总功率。

## 实际应用案例

原案例来自 `D:\examples`，项目文件及引用资产哈希均复核未变。每对对比使用相同冻结几何、材料、土壤、生理参数、气象和输出预算。观察产品统一为 128×128、550/10500 nm、单视角；内部短波谱积分仍保留 2001 个采样点。

| 案例 | 测试范围 | 有效体元 | 节点 |
|---|---|---:|---:|
| auto 草地/松树 | 100×100×16 m，体元 1 m，400 个实例 | 24984 | 日间 1；昼夜 48 |
| forest | 100×100×20 m，体元 0.5 m，220 棵树 | 103859 | 日间 1 |
| beijing | 原 5 km 场景中 origin(x=4500,z=3350) 的 300×300×100 m ROI，体元 10 m | 1734 | 日间 1 |

forest、beijing 原案例为 VoxelRT，本次临时转为 VoxelEB；北京保留 416/50456 个实例及 ROI 内建筑几何，不能将其耗时外推到完整 5 km 城市。原 fluid 配置保留；观察路径的周期设置不改变本次中央 VoxelEB 辐射计算域。光伏原 FacetEB 和船舶原 RayTracing 不适用这次 VoxelEB 短波改动，未计入三阶测试。

气象使用冻结的引擎内置气象文件，不是实测验证。日间选择源节点 26（DOY 214.5556，入射短波 962 W/m²）；昼夜使用同一源文件的连续前 48 个半小时节点。

| 测试 | 旧一阶总秒 | 新一阶总秒 | 新三阶总秒 | 新三阶/旧一阶 | 新三阶/新一阶 |
|---|---:|---:|---:|---:|---:|
| auto 日间 | 13.268 | 12.937 | 13.213 | 0.996 | 1.021 |
| forest 日间 | 18.492 | 19.503 | 20.269 | 1.096 | 1.039 |
| beijing ROI 日间 | 12.060 | 12.628 | 12.635 | 1.048 | 1.001 |
| auto 连续昼夜 | 35.544 | 48.004 | 60.821 | 1.711 | 1.267 |

| 测试 | 旧一阶短波秒 | 新一阶短波秒 | 新三阶短波秒 | 新三阶/旧一阶阶段 | 新三阶/新一阶阶段 |
|---|---:|---:|---:|---:|---:|
| auto 日间 | 0.0162 | 0.2617 | 0.5348 | 32.991 | 2.044 |
| forest 日间 | 0.0844 | 0.8519 | 1.8078 | 21.410 | 2.122 |
| beijing ROI 日间 | 0.0103 | 0.0714 | 0.1579 | 15.363 | 2.210 |
| auto 连续昼夜 | 0.7932 | 12.4262 | 25.4557 | 32.094 | 2.049 |

短波阶段计时不包含先前的太阳可见性和直射短波阶段。总耗时包含加载、初始化、能量迭代、成像及输出，因此单节点比值受固定开销稀释。auto 日间新三阶略短于旧一阶不能作为提速结论。本次没有多次重复的统计置信区间。

12 组运行、153 个案例气象节点全部退出 0，生理诊断和散射诊断有效；无非法体元。每组昼夜的 19 个无短波节点均 PAR/GPP 为零，叶片净同化非正。草地与北京 ROI 的最大步数截断均为零；森林每阶为 105/139586496，约 0.0000752%。整卡显存峰值约 5.5 GiB，包含桌面程序；WDDM 进程显存不可读，未将整卡用量当作引擎专属需求。

| 新三阶减新一阶 | 叶体元漫射 PAR 均值差 | 温度均值差 K | 温度空间 RMSE K | 温度最大绝对差 K |
|---|---:|---:|---:|---:|
| auto 日间 | +12.274 | +0.931 | 1.125 | 4.023 |
| forest 日间 | +8.989 | +0.227 | 0.278 | 2.945 |
| beijing ROI 日间 | +20.320 | +0.591 | 0.714 | 3.826 |
| auto 连续昼夜 | +4.863 | +0.538 | 0.809 | 8.678 |

PAR 单位为 μmol photons m⁻²叶 s⁻¹；均值未按叶面积加权。昼夜均值按节点简单平均，空间 RMSE 汇总体元记录。温度变化和 PAR 变化表示模型敏感性，没有据此宣称精度提高。

## 回归和边界测试

| 检查 | 结果 | 证据 |
|---|---|---|
| 生产 PAR float32 | 59653 项断言通过 | [par-regression.log](C:/work/streamsim/.test/scattering-20261004/par-regression.log) |
| 生产 C3/C4 生化核心 float32 | 2612+2648 个支持域案例、38 个非法案例、32 个基线对比通过 | [biochemical-regression.log](C:/work/streamsim/.test/scattering-20261004/biochemical-regression.log) |
| 默认一阶引擎回归 | 14/14 通过，包括夜间、参与介质、C3/C4、fluid 和非法输入拒绝 | [result.json](C:/work/streamsim/.test/physiology-engine/1791108547934/result.json) |
| 新散射基本 GPU fixture | 11 次运行、7 项物理/实现断言通过 | [result.json](C:/work/streamsim/.test/scattering-core/1791107668341/result.json) |
| 截断/窄谱补充 GPU fixture | 5 次运行、4 项断言通过 | [result.json](C:/work/streamsim/.test/scattering-core/1791108121952/result.json) |
| Release 引擎和 GUI 构建 | 通过 | [build-final.log](C:/work/streamsim/.test/scattering-20261004/build-final.log)、[gui-build.log](C:/work/streamsim/.test/scattering-20261004/gui-build.log) |
| 构建身份及配置往返 | 235 个源文件及 36 个 SPV 哈希匹配；1/2/3 阶 JSON/XML 往返通过 | [final-build-verification.json](C:/work/streamsim/.test/scattering-20261004/final-build-verification.json) |

GPU fixtures 覆盖：黑材料高阶为零、夜间为零、照明倍增线性、重复输出一致、Sun/sky 独立注入、叶/雾/固体/水共存及高反射材料的高阶增量。新三阶相对新一阶的吸收差与二/三阶诊断增量吻合到浮点累积误差，没有反复累加低阶输出。

将最大步数设为 1 的 fixture 触发 181356 次截断，匹配的 64 步 fixture 为零；高密度冠层触发 75096 次透射率阈值停止。恒定光谱的 8 nm 与 100 nm 分组短波比为 0.9999997995，PAR 差约 −0.167%，属于光谱分组敏感性。

## 仍需验证的科学边界

本次确认了三阶执行、源注入与增量累积、数值有效性和所测场景性能，尚未建立解析封闭域能量守恒金标准、实测精度或三阶之后的收敛误差界。太阳可见性仍使用原来的 64 步/0.001 近似；新漫射截断诊断不含这部分。

固体反射使用命中法线和余弦积分，但固体吸收仍沿用体元方向平均，主太阳反射仍保留水平入射约定；建筑墙面辐射尚需独立验证。64 方向积分存在约 2% 的法线方向依赖误差，水面窄镜面峰没有单独验证。`netRadiation` 输出由 H+LE+G 构造，不能拿它与相同三项比较来证明辐射闭合。详见[独立审阅](C:/work/streamsim/.test/scattering-20261004/PHYSICS_REVIEW.md)。

## 复现及原始证据

在 `C:\work\streamsim` 运行；准备命令只生成冻结计划，返回新的 manifest 路径，再用该路径执行：

```powershell
$scatterDayPlan = node tools/test-scattering-orders.mjs --profile=day --case=auto,forest,beijing --orders=1,3 | ConvertFrom-Json
node tools/test-scattering-orders.mjs --run "--plan=$($scatterDayPlan.manifest)"

$scatterDiurnalPlan = node tools/test-scattering-orders.mjs --profile=diurnal --case=auto --orders=1,3 | ConvertFrom-Json
node tools/test-scattering-orders.mjs --run "--plan=$($scatterDiurnalPlan.manifest)"

node tools/test-scattering-core.mjs
node tools/test-scattering-core.mjs --additional
node tools/test-physiology-engine.mjs
```

每次准备生成新的冻结目录，上述命令使用新目录并保留本次已有证据。所有 GPU 测试应串行执行。

- [案例汇总](C:/work/streamsim/.test/scattering-20261004/EXAMPLE_TEST_SUMMARY.md)
- [日间结果](C:/work/streamsim/.test/scattering-20261004/benchmark-1791106902349/result.json)及[冻结计划](C:/work/streamsim/.test/scattering-20261004/benchmark-1791106902349/manifest.json)
- [昼夜结果](C:/work/streamsim/.test/scattering-20261004/benchmark-1791106966694/result.json)及[冻结计划](C:/work/streamsim/.test/scattering-20261004/benchmark-1791106966694/manifest.json)
- [原案例和冻结资产复核](C:/work/streamsim/.test/scattering-20261004/example-verification.json)
- [案例基准工具](C:/work/streamsim/tools/test-scattering-orders.mjs)及[GPU 核心测试工具](C:/work/streamsim/tools/test-scattering-core.mjs)
