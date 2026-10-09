# StreamSim 界面案例验证（2026-10-06）

四个案例均通过。使用当前源码版 Release 引擎，工程由内置浏览器中的 StreamSim 界面打开，全部模拟由“运行模拟”按钮提交，串行执行。

| 案例 | 完成节点 | 界面日志运行耗时（秒） | 128×128 双波段观测 TIFF | 统计数据条数 | 结果 |
|---|---:|---:|---:|---:|---|
| auto 日间 | 1 | 13.95 | 1 | 2 | 通过 |
| forest 日间 | 1 | 21.28 | 1 | 2 | 通过 |
| 北京 300 m ROI | 1 | 12.99 | 1 | 2 | 通过 |
| auto 全天 | 48 | 65.60 | 48 | 96 | 通过 |

耗时来自界面原生进程结束日志，包含引擎初始化；不包含点击运行前后的界面场景加载、输入准备和人工检查。运行清单起止时间的耗时另保存在 audit-summary.json。

## 检查结果

- 51 个请求节点均完成，各案例内部时间序列连续，exitCode=0；每节点实际 requestedOrders=3、completedOrders=3、legacy=false，无按散射阶贡献提前停止，各阶无无效值。
- 51 张观测影像均为 128×128、550/10500 nm 两个独立波段，CSV 和原生 TXT 各有 102 条数据。TXT 的正式表头单独识别。
- 有效过程字段全部为有限值：能量 7978854 个、辐射 5319236 个，原生运行、输入、气象及 exe/SPV 构建身份相符。
- 北京额外生成两张 60×60 流场 TIFF，72000 个流体格点的 360000 个命名字段均为有限值。1520 个实体遮罩像元为正式 NaN NoData；有效像元均有限。
- 全天案例节点 0–47，覆盖 DOY214 00:20–23:50，48 张影像及96条统计完整。19个时刻有有限未闭合条件态，后续时刻全部完成。按用户要求，预算未闭合不作为失败条件；50次迭代上限保持。
- 原始 D:/examples 工程和冻结输入 SHA 校验通过。北京沿用已有300 m ROI，auto/forest为100 m案例范围。本轮验证计算流程、运行稳定性和输出契约，未进行实测精度验证。

## 运行来源

引擎：C:/work/streamsim/models/bin_x64/Release/histream.exe

exe SHA256：`f87ebc7b8027d77d58d6e6bf76d7d7e2c067382c00681ae9c3620de4cc08eb7e`

原生源码树 SHA256：`f7de0c713af29533be9a7775625cfef72e19265ec35e60043ae4a673fd8f14c7`

- auto-day: `2026-10-06T09-35-50-793Z-48bf5cd8`；[原生运行清单](C:/work/streamsim/.test/ui-examples-20261006/auto-day/output/runs/2026-10-06T09-35-50-793Z-48bf5cd8/run-manifest.json)，[独立审计](C:/work/streamsim/.test/ui-examples-20261006/auto-day-audit.json)。
- forest-day: `2026-10-06T09-37-37-394Z-6e8dc8c4`；[原生运行清单](C:/work/streamsim/.test/ui-examples-20261006/forest-day/output/runs/2026-10-06T09-37-37-394Z-6e8dc8c4/run-manifest.json)，[独立审计](C:/work/streamsim/.test/ui-examples-20261006/forest-day-audit.json)。
- beijing-day: `2026-10-06T09-39-21-231Z-317a7b0d`；[原生运行清单](C:/work/streamsim/.test/ui-examples-20261006/beijing-day/output/runs/2026-10-06T09-39-21-231Z-317a7b0d/run-manifest.json)，[独立审计](C:/work/streamsim/.test/ui-examples-20261006/beijing-day-audit.json)。
- auto-diurnal: `2026-10-06T09-41-25-126Z-a5c7ad0a`；[原生运行清单](C:/work/streamsim/.test/ui-examples-20261006/auto-diurnal/output/runs/2026-10-06T09-41-25-126Z-a5c7ad0a/run-manifest.json)，[独立审计](C:/work/streamsim/.test/ui-examples-20261006/auto-diurnal-audit.json)。

## 界面入口修复与结果

桌面工程选择器改用绑定主窗口的 Electron 文件对话框；增加页面内“路径打开”表单，可取消、回车提交、切换中英文，运行中禁用。原生输入工具不能操作文件选择器时，内置鼠标通过该表单完成工程切换。

GUI 构建、三个修改的 JavaScript 文件语法检查、四个修改文件的限定 diff 检查通过。最终桌面已重新启动加载最新界面构建，重新打开全天工程后仍能浏览这次运行的结果。

[完整机器记录](C:\work\streamsim\.test\ui-examples-20261006/validation-summary.json) · [审计汇总](C:\work\streamsim\.test\ui-examples-20261006/audit-summary.json) · [界面日志](C:\work\streamsim\.test\ui-examples-20261006/ui-execution-log.txt)

![48个时刻完成状态](C:\work\streamsim\.test\ui-examples-20261006/auto-diurnal-completed-ui.jpg)

![最后时刻热红外结果](C:\work\streamsim\.test\ui-examples-20261006/auto-diurnal-thermal-ui.jpg)

![北京ROI观测和流场输出](C:\work\streamsim\.test\ui-examples-20261006/beijing-ui.jpg)
