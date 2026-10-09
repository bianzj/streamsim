# VoxelEB 雾/火介质边界修正（2026-10-06）

按本次要求，有限但未闭合的节点继续进入下一时刻。原有50轮上限、一次历史推进和残差诊断均保留；不增加收敛等待、不强制残差归零。

## 修改

原传统一阶SW和公用LW路径把 `alignBufferId` 所指的雾/火入口当作不透明表面：LW额外加入黑体表面发射，两条路径都把透射率设零并终止。即使介质 `emissionScale=0` 或 `extinction=0`，也可能产生热源或阻断天空。

现在先判断对侧是否为 `TYPE_VEGETATION` 且 `isParticipatingMedium`。此类边界跳过实体表面源/终止分支，执行原有 `OffsetRay` 后继续追踪。已走过的体积段仍由背侧 `bufferId` 积分，使用消光、散射比例和发射比例；没有重复积分入口侧。

```text
空气 → 雾：跨过入口，下一命中积分雾内段。
雾A → 雾B：当前只积分A，后续再积分B。
雾 → 空气：完成末段后，实际向上miss才加入天空。
雾 → 固体：先积分雾并衰减透射，再计算固体源。
```

生产修改仅涉及：

- `models/histream/shader/voxeleb/voxelrad_diffuse_TIR_single.comp`
- `models/histream/shader/voxeleb/voxelrad_diffuse_VNIR_single.comp`

普通叶冠层的既有表面分支、土壤/建筑/水体分支及相机路径没有改动。这是参与介质边界修正，不代表已统一所有植被与实体分类。

## 验证

独立小场景为12×8×8 m、1 m体素、96个黑地表接收体元和一个雾体积。固定介质温度298.15 K，散射率为0，逐接收体元检查吸收LW/SW；火的体积生成方式不同，只比较相同地表接收位置，不要求全体积链接相同。

| 检查 | 旧构建 | 修正后 |
|---|---:|---:|
| 零消光、天空LW=350 W/m² | 地表352.394–448.077 | 全部350.000 |
| 零天空、零雾发射比例 | 最大419.480 W/m² | 0 |
| 零天空、零火发射比例 | 最大77721.836 W/m² | 0 |
| 零消光、SW=700 W/m² | 部分接收为0 | 全部700.00067 |

另验证：增加消光时天空贡献逐点不增；`emissionScale=0.5` 的体积热源为比例1的一半；纯吸收热源有限且不超过σT⁴；薄介质的热源加透过天空满足均匀源项恒等式。

保留原有透射率0.01提前截断。在均匀介质与天空同温、无其他热源且步数足够的专用场景中，完整路径应得到B=σT⁴=448.07731 W/m²；提前截断可能遗漏B乘剩余透射率。实际结果447.80951–448.07733 W/m²，最大亏损约0.0598%，满足本场景的1%单边界。此界不适用于任意光源或最大步数截断；没有修改阈值或补回虚构辐射。

强迫测试使用三个连续节点、Rin=10000 W/m²，刻意保留有限但未闭合状态：

| 节点 | 迭代数 | 超阈值条件态数 | 后续行为 |
|---|---:|---:|---|
| 0 | 50 | 192 | 继续节点1 |
| 1 | 50 | 192 | 继续节点2 |
| 2 | 50 | 192 | 正常完成 |

三个节点的过程字段均有限，退出码为0；不把残差非零作为测试失败条件。12个介质/步进案例、14个计算节点及10项检查通过。另有14项生理回归、3个天空截断案例和5项边界检查通过。

## 构建与证据

```text
工作区：C:/work/streamsim
源树 SHA256：f7de0c713af29533be9a7775625cfef72e19265ec35e60043ae4a673fd8f14c7
引擎及着色器组合 SHA256：49c161fb874e8a520ee8d05df70bbc4e28a08c5edebcdfd2d5f20fd172908f6c
旧组合 SHA256：0d175f23d5579e846b58b883067201bc98173a5a1fe2479c6cc71f82655d9a63
```

Release已重新构建；236个构建源文件、36个部署SPV均与清单一致，全部SPV通过校验。此次只有两个SPV变化，C++可执行文件哈希保持 `f87ebc7b8027d77d58d6e6bf76d7d7e2c067382c00681ae9c3620de4cc08eb7e`，因此仅看EXE哈希不能区分新旧着色器。

```powershell
Set-Location C:/work/streamsim
./tools/build-engine.ps1 -Configuration Release
node tools/test-medium-boundary.mjs
node tools/test-sky-truncation.mjs
node tools/test-physiology-engine.mjs
```

证据目录：`C:/work/streamsim/.test/medium-boundary-20261006/`。`candidate-result.json` 为最终检查结果，引用实际GPU输出及 `inspection-result.json`；初始严格等温检查的失败记录保留在原 `result.json`，随后按既有0.01截断的解析界检查同一输出。`baseline-result.json`、`build-validation.json`、`build-difference.json` 和各日志分别保留旧问题复现、构建身份及回归结果。

已有Stream3D任务使用各自的AppData引擎副本，本轮小规模测试串行运行。未替换安装版或运行中的引擎。验证范围为上述边界与纯吸收源项，不包含观测精度、普通冠层表面分类或高散射率介质的完整长波回散射。
