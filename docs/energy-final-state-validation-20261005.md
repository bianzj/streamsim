# VoxelEB 最终状态同步修正与验证（2026-10-05）

本次优先修正能量通量与最终表面温度不同步的问题。范围为 `C:/work/streamsim` 的原生 VoxelEB 引擎；所有结果属于实现与数值一致性验证，不代表实测精度验证。

## 修改

原循环最后一次 budget 更新了温度，但长波、H、LE、G 和生理量仍可能对应更新前的温度。现在结束温度迭代后依次执行：

```text
diffuseTIR → aero → bio → evapo → budget(energyFinalize=1) → updateTp
```

最终 budget 只回写比例法 G 和评估残差，不更新求解表面温度；历史仅由一次 updateTp 推进。method 1、method 2 和水体储热在历史推进之前，以最终温度重新求值。method 0 的输出由无量纲 `GST=0.35` 改为真实热通量 `G=GST*Rn`。

`energyFinalize` 位于 CPU/GLSL push constant 的偏移112，总长度仍为128字节。无效面不进入预算；残差读回增加计算写→传输读和传输写→主机读的 Vulkan 内存屏障。

本次构建同时纳入之前已保存的独立 Rn 输出及旧短波/长波实际天空逃逸判断。Rn 来自吸收 SW、吸收 LW 与最终温度热发射；H+LE+G 单独保留，残差不裁剪、不强制归零。现有过程二进制字段偏移不变。

## 诊断契约

每个节点生成 `diagnostics/energy_balance_node=...json`，关闭过程导出时也会生成。主要字段为：

```json
{
  "finalStateRefreshed": true,
  "temperatureIterations": 50,
  "budgetResidualToleranceWm2": 2,
  "budgetStatesOutsideTolerance": 11702,
  "budgetResidualWithinTolerance": false
}
```

这是 auto 白天案例的实际记录。超阈值数按阳面/阴面条件态统计，不是体素数；水体按共享混合残差占两个条件槽。叶片交换项为每叶面积的 `2*(H+LE)`；固体/水体为 `H+LE+G`。规定温度的火/雾不参与表面闭合统计。体素均值不按面积加权，不能解释为全域功率或全域闭合。

表面通量同步不代表完整的生理和空气状态非线性收敛：bio 仍读取现有交错算法的上一轮空气 es/cs，随后 evapo 更新空气状态。保留原50轮上限和退出码，计算完成与预算残差通过分别记录。

## 受控验证

- 独立 Rn 辅助函数的25项解析断言通过。
- 生产土壤导热函数的稳态、瞬态、导数及材料分派检查通过；稳态导热8.00003 W/m²，与解析8 W/m²相符。
- GPU 测试包含 method 0 白天、method 1 夜间、method 2 白天各3个连续节点，以及关闭导出的1个节点；共10个节点，最终预算残差均通过阈值。
- method 0 的 `G-0.35Rn` 最大绝对偏差为0.0000534 W/m²；旧输出为0.35这一固定数值。
- 非叶独立 Rn 的最大恒等式偏差为0.0000460 W/m²。
- 水体 `Cvol*depth*(Tcurrent-Tprevious)/dt` 的最大偏差为0.00711 W/m²；旧构建为0.39035 W/m²。该检查覆盖连续节点，防止历史重复推进或储热被重置。
- 叶片以导出温度、H反推共享空气动力阻力，再结合独立导出的阳阴rss检验LE；最大偏差为0.000136 W/m²。旧版该量约0.0173 W/m²，绝对幅度有限。
- 14项生理/异常输入/参与介质/流体回归通过。
- 传统与加速 VoxelRT 的封闭黑体回归通过；两者相对辐亮度误差约3.74e-8。
- 天空边界的3个GPU夹具与5项检查通过：采用域外三层受控续追踪几何，只保留96个地面状态。SW和LW均有96个接收状态因截断而减光，截断不增加光源，零外界照明时两者均为零；旧构建在同一夹具上未通过两个实际逃逸检查。该测试覆盖边界分支，不验证参与介质或建筑物理。
- 236个构建源文件及36个部署SPV的哈希与构建清单一致；所有SPV按其实际版本通过 spirv-val。

## 现有案例对照

使用冻结的 geometry、meteorology 和三阶散射输入，GPU 串行运行。北京为此前的300 m ROI，未测试完整5 km场景。全部案例的过程字段均有限，并生成原生影像。

| 案例 | 体素数 | 节点 | 旧构建耗时 | 新构建耗时 | 预算通过节点 | 最大混合残差 W/m² |
|---|---:|---:|---:|---:|---:|---:|
| auto 白天 | 24984 | 1 | 13.62 s | 13.28 s | 0/1 | 21.3315 |
| forest 白天 | 103859 | 1 | 20.01 s | 19.42 s | 0/1 | 22.6970 |
| beijing ROI 白天 | 1734 | 1 | 12.19 s | 12.50 s | 0/1 | 4.2438 |
| auto 全天 | 24984 | 48 | 60.42 s | 61.34 s | 29/48 | 29.3357 |

三个单节点案例的导出温度与旧构建完全相同。48节点序列的最大温度差为0.000640869 K。全天耗时单次测量增加1.52%；单节点耗时波动约±3%，不能视为稳定性能差异。这三个案例均采用土壤 method 2，比例法 G 修正不直接改变它们的温度求解。

新增诊断揭示部分状态在50轮后仍未达到2 W/m²阈值。上表中的“通过”仅指预算条件态阈值，不是观测精度或完整耦合收敛。后续更值得评估温度迭代停止条件与强迫下的收敛速度；建筑法向投影仍需多面状态及SW/LW联合改造。

另一个保留的既有问题是部分雾几何表面命中分支没有使用介质 `emissionScale`。原雾测试的零天空/零介质发射设置仍产生426.424 W/m²入射LW，旧构建也复现相同数值，所以它无法隔离天空截断。此次改用明确隔离热源的续追踪夹具；没有通过删掉必要断言来绕过失败，也没有把天空分支验证等同于雾输运验证。

## 构建与复现

```text
当前 Release: C:/work/streamsim/models/bin_x64/Release/histream.exe
引擎 SHA256: f87ebc7b8027d77d58d6e6bf76d7d7e2c067382c00681ae9c3620de4cc08eb7e
源树 SHA256: 8eb7bf81ae3aa22865af5c900ab45f2ab7c950eded0aae902981376260ed8087
旧构建 SHA256: d0b39dc07204ccfe5e61574c42aec481ecfbad278576a7587fba91370157cda1
```

```powershell
Set-Location C:/work/streamsim
./tools/build-engine.ps1 -Configuration Release
node tools/test-energy-output.mjs
node tools/test-energy-core.mjs
node tools/test-energy-final-state.mjs
node tools/test-energy-final-state.mjs --suite examples
node tools/test-physiology-engine.mjs
node tools/test-voxelrt-radiation.mjs --engine
node tools/test-sky-truncation.mjs
```

原始输入、旧引擎快照、全部日志和量化对照位于 `C:/work/streamsim/.test/energy-final-state-20261005/`，主要机器可读记录为 `inputs.json`、`candidate-fixtures-result.json`、`candidate-examples-result.json`、`comparison.json` 和 `build-validation.json`。before/after 的变化包含上述既有候选修正，不能全部归因于最终状态刷新。

天空回归原始结果位于 `C:/work/streamsim/.test/sky-truncation/1791209008052/result.json`；旧构建失败复现位于 `baseline-sky-reproduction.json`，旧雾退化设置复现位于 `baseline-fog-control/result.json`。

`D:/streamsim` 的安装版未在本次替换；本记录对应源码工作区的 Release 引擎。未修改固体面状态、光学图像高阶散射、太阳截断或夜间优化。
