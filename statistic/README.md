# StreamSim 外部统计工具

代码不依赖 StreamSim GUI，可读取输出 GeoTIFF、VoxelRT/VoxelEB 过程数据、体元流体数据以及 FacetRT/FacetEB 面元结果。

## 安装

```powershell
cd C:\work\streamsim
python -m pip install -r statistic\requirements.txt
```

若无法安装 `rasterio`，只安装 `numpy Pillow` 仍可读取多数单波段 TIFF；多波段、NoData、坐标系信息建议使用 `rasterio`。

## 使用

分析一个 TIFF：

```powershell
python statistic\streamsim_statistics.py "工程\output\结果.tif"
```

分析一个三维过程 JSON，并查看 GPP/NPP：

```powershell
python statistic\streamsim_statistics.py "工程\output\process\voxeleb_T=DOY214_12-20.json" --fields gpp,npp
```

分析一个工程的全部输出：

```powershell
python statistic\streamsim_statistics.py "工程\output" -o "工程\statistics"
```

流体三维结果：

```powershell
python statistic\streamsim_statistics.py "工程\output\process\voxelfluid_T=DOY214_12-20.json" --fields windSpeed,airTemperature
```

面元正反面可选择 `--facet-side mean`、`front` 或 `back`。大量数据默认最多抽样 50000 个点写入 CSV 和三维图；完整数据始终用于统计，可用 `--max-points` 修改显示抽样数量。

## 输出

每个输入文件对应一个独立子目录：

- `summary.txt`：来源、模型、时间节点及各字段统计摘要；
- `statistics.csv`：全部波段或字段的数量、均值、标准差、分位数和极值；
- `sample_points.csv`：三维坐标及所选字段抽样数据；
- `*_heatmap.png`：TIFF 二维热力图；
- `*_3d.png`：三维结构及字段着色图；
- `*_histogram.png`：数值直方图。

所有 CSV 使用 UTF-8 BOM，可直接由 Excel 打开。统计使用全部有效值；抽样只影响点 CSV 和三维图片。
