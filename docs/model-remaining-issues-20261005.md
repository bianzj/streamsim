# StreamSim 尚存问题核查（2026-10-05）

最初记录来自只读源码核查。2026-10-05 后续已完成原生修正、Release 构建和 GPU 对照：独立净辐射、最终温度/通量同步、比例法 G 及实际天空逃逸分支已修正。当前236个构建源文件、36个SPV与构建清单一致。具体证据见 [终态同步验证记录](C:/work/streamsim/docs/energy-final-state-validation-20261005.md)。数值有效性不能排除其余物理偏差和功能边界。

2026-10-06更新：按要求，有限但未闭合的节点继续下一时刻，不调整收敛策略；雾/火入口被误作不透明表面的分支已修正，见 [介质边界验证记录](C:/work/streamsim/docs/medium-boundary-validation-20261006.md)。

## 优先处理的正确性问题

1. **已修正：净辐射独立输出及终态同步。** 旧版将 `netRadiation` 写成加权 H+LE+G，现在使用吸收SW+吸收LW−最终温度热发射独立计算；循环末尾重算LW、空气动力阻力、生理和交换通量，再推进一次历史。预算显式保存真实 method 0 `G=0.35Rn`。新增每节点独立残差、迭代数和预算阈值状态；关闭过程导出也保留诊断。25项解析断言及10个受控GPU节点通过，不能据此认为所有实际案例均收敛。

2. **已修正：旧短波和热红外达到追踪上限后补天空。** 两条漫射路径仅在实际向上miss时加入天空；达到上限保留已累积源项，不补虚构天空。传统一阶SW及公用TIR在3个GPU夹具、5项边界检查中通过，旧构建的相同夹具未通过逃逸检查。该验证使用受控域外续追踪层，不能代替参与介质、太阳遮挡或实际复杂建筑误差评估。

3. **固体吸收和反射的投影约定不一致。** 新短波 [voxelrad_scattering_VNIR.comp:242](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_scattering_VNIR.comp:242) 对固体吸收沿用 2/N 方向平均，而反射在 144–152 行使用命中法线及入射余弦。太阳主反射 [155 行](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_scattering_VNIR.comp:155) 仅检查表面朝向，缺少从水平太阳辐照度到表面辐照度的投影。固体直射吸收也沿用水平约定：[voxelrad_direct_VNIR.comp:104](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_direct_VNIR.comp:104)。建筑立面与斜面需要统一吸收/反射的法线和投影定义，再做解析场景验证，不能只改反射一项。

## 数值近似、功能边界和性能

4. **太阳遮挡的截断缺少诊断和误差约束。** [voxelrad_solar.comp:57](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_solar.comp:57) 达到最大步数后仍返回剩余透射率，没有检查更后方的遮挡；0.001 阈值仍在。新短波漫射的 0.01 阈值和最大步数有计数，但 [command.cpp:125](C:/work/streamsim/models/histream/src/voxeleb/command.cpp:125) 明确排除了太阳可见性。路径数及无量纲剩余 throughput 不能作为遗漏能量误差界。

5. **三阶尚未进入光学传感器图像。** 改动作用于 VoxelEB 短波能量/PAR 场；热红外传播、VoxelRT/FacetEB 和光学观察 shader 是独立路径。光学图像仍在 [voxelrad_image.comp:463](C:/work/streamsim/models/histream/shader/voxeleb/voxelrad_image.comp:463) 从直射、天空和材质构造辐亮度，没有读取新逐阶方向场。温度变化能影响热红外结果，但不能据此称光学图像已支持完整三阶散射。

6. **夜间仍有可避免的计算。** [command.cpp:213](C:/work/streamsim/models/histream/src/voxeleb/command.cpp:213) 没有在零短波节点跳过短波谱组和全部请求阶数。夜间数值为零已经验证，但仍在做射线追踪。应先清零短波状态，再安全跳过零光源计算，保留完整诊断，避免上一节点辐射残留。

此外，三阶后的收敛误差、64 方向积分、水面窄镜面峰、全域能量闭合、实测精度和北京完整 5 km 场景成本尚未验证。北京已有测试仅为 300 m ROI。

7. **预算残差保留，未闭合继续下一时刻。** 既有诊断显示 auto/forest 白天节点最大混合残差约21–23 W/m²，北京ROI约4.24 W/m²。auto的48节点中29个通过全部条件态2 W/m²阈值，19个未通过。按2026-10-06要求不为此增加迭代、等待或中断；强迫测试已确认首节点50轮仍有限未闭合时，后续两个节点正常完成。

8. **已修正：雾/火入口的多余表面热源与遮挡。** 传统一阶SW和公用LW现在让参与介质入口继续追踪，体积段按消光/发射比例求值。黑地表零天空、零介质发射时，旧雾最大419.480 W/m²、旧火最大77721.836 W/m²均降为0；透明天空、消光单调性和源项比例检查通过。普通叶冠层既有表面分类、高散射率介质长波回散射仍未改动。

## 部署状态

- 当前工作区 Release 引擎：`C:\work\streamsim\models\bin_x64\Release\histream.exe`，SHA256 `f87ebc7b8027d77d58d6e6bf76d7d7e2c067382c00681ae9c3620de4cc08eb7e`。
- `D:\streamsim\resources\engine\histream.exe` 仍为 `9b852d9e02e40048be230dd707387bfc33552f881cb61a651e48ed9a0c4126e3`，未同步本次三阶改动。
- 源码默认阶数仍为 1，默认走旧路径；2/3 或 `STREAMSIM_NEW_SCATTERING=1` 才启用新短波路径。

预算未闭合按要求保留为诊断并继续步进。后续模型工作仍包括固体几何投影、普通冠层边界及介质长波散射；太阳截断诊断、夜间优化及光学成像的高阶接入另行处理。既有三阶测试证据见 [2026-10-04 测试记录](C:/work/streamsim/docs/scattering-order-validation-20261004.md)。
