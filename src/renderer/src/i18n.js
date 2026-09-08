const STORAGE_KEY = 'streamsimLanguage'
const DEFAULT_LANGUAGE = 'zh-CN'

const ENGLISH_MESSAGES = Object.freeze({
  '未打开工程': 'No project open',
  '已修改': 'Modified',
  '新建工程': 'New Project',
  '打开工程': 'Open Project',
  '保存': 'Save',
  '另存为': 'Save As',
  '工程另存为': 'Save Project As',
  '图像结果': 'Image Results',
  '分析': 'Analysis',
  '运行模拟': 'Run Simulation',
  '停止': 'Stop',
  '重置': 'Reset',
  '计算模型': 'Simulation Model',
  '面元辐射传输': 'Facet Radiative Transfer',
  '面元辐射传输（GPU Radiosity）': 'Facet Radiative Transfer (GPU Radiosity)',
  '面元能量平衡': 'Facet Energy Balance',
  '体元辐射传输': 'Voxel Radiative Transfer',
  '体元流体力学': 'Voxel Fluid Dynamics',
  '体元能量平衡': 'Voxel Energy Balance',
  '体元辐射传输与能量平衡': 'Voxel Radiative Transfer & Energy Balance',
  '简单光线追踪': 'Simple Ray Tracing',
  '主要输入': 'Primary Inputs',
  '备选输入': 'Optional Inputs',
  '按需启用': 'Enable as Needed',
  '运行与控制': 'Run & Control',
  '仿真设置': 'Simulation Settings',
  '场景': 'Scene',
  '太阳与天空': 'Sun & Sky',
  '传感器': 'Sensor',
  '场景对象': 'Scene Objects',
  '材质属性': 'Material Properties',
  '气象驱动': 'Meteorological Forcing',
  '流体力学': 'Fluid Dynamics',
  '大气影响': 'Atmospheric Effects',
  '仿真模拟': 'Simulation',
  '预设': 'Preset',
  '预览': 'Preview',
  '内置': 'Built-in',
  '仅 VoxelEB': 'VoxelEB Only',
  '关闭': 'Off',
  '开启': 'On',
  '启用': 'Enabled',
  '统计': 'Statistics',
  '引擎待命': 'Engine Ready',
  'HiStream 待命': 'HiStream Ready',
  '正在检测 HiStream...': 'Detecting HiStream...',
  '配置引擎路径': 'Configure Engine Path',
  '实体': 'Solid',
  '线框': 'Wireframe',
  '温度': 'Temperature',
  '网格': 'Grid',
  '体元预览': 'Voxel Preview',
  '当前为抽样显示': 'Currently Showing a Sample',
  '适应窗口': 'Fit View',
  '旋转': 'Rotate',
  '缩放': 'Zoom',
  '平移': 'Pan',
  '属性检查器': 'Property Inspector',
  'project.json 源码': 'project.json Source',
  '运行日志': 'Run Log',
  '待命': 'Ready',
  '清空': 'Clear',
  '查看模拟结果': 'View Results',
  'STREAMSIM 已就绪，请打开场景工程的 project.json。': 'STREAMSIM is ready. Open the project.json of a scene project.',
  '配置源码': 'Configuration Source',
  '尚未打开工程': 'No Project Open',
  '在 Windows 文件选择框中选择已有工程的 project.json。': 'Select the project.json of an existing project in the Windows file picker.',
  '打开已有工程': 'Open Existing Project',
  '未加载文件': 'No File Loaded',
  '应用并保存': 'Apply & Save',
  '取消': 'Cancel',
  '保存目录': 'Save Directory',
  '工程名称': 'Project Name',
  '当前工程': 'Current Project',
  '复制输入资源并创建空的 output 目录，不复制已有模拟结果。': 'Copies input assets and creates an empty output directory; existing simulation results are not copied.',
  '工程结构': 'Project Structure',
  '创建工程': 'Create Project',
  'project.json 是唯一工程配置和运行入口。': 'project.json is the sole project configuration and run entry point.',
  '生成简单场景对象': 'Generate Simple Scene Object',
  '基本几何': 'Basic Geometry',
  '对象名称': 'Object Name',
  '对象类型': 'Object Type',
  '包络形状': 'Envelope Shape',
  '椭球': 'Ellipsoid',
  '立方体': 'Cube',
  '生成类型': 'Generation Type',
  '几何体': 'Geometry',
  '几何体混沌': 'Geometry Chaos',
  '水体（水平面）': 'Water (Horizontal Plane)',
  '火焰': 'Fire',
  '雾': 'Fog',
  '几何体表面': 'Geometry Surface',
  '表面细分级别': 'Surface Subdivision Level',
  '填充控制': 'Fill Control',
  '面元数量': 'Facet Count',
  '单个面元面积（m²）': 'Area per Facet (m²)',
  '面元方向': 'Facet Orientation',
  '球形随机分布': 'Spherical Random Distribution',
  '近水平分布': 'Near-horizontal Distribution',
  '近垂直分布': 'Near-vertical Distribution',
  '随机种子': 'Random Seed',
  '水体位置与偏移': 'Water Position & Offset',
  '参与介质辐射参数': 'Participating-medium Radiative Parameters',
  '消光系数（m⁻¹）': 'Extinction Coefficient (m⁻¹)',
  '单次散射反照率': 'Single-scattering Albedo',
  '散射非对称因子': 'Scattering Asymmetry Factor',
  '固定温度（K）': 'Fixed Temperature (K)',
  '发射倍率': 'Emission Scale',
  '预计生成三角面元。': 'Estimated triangle facets will be generated.',
  '生成并加入场景': 'Generate & Add to Scene',
  '定义 HiStream 对象属性': 'Define HiStream Object Properties',
  '对象': 'Object',
  '植被': 'Vegetation',
  '建筑': 'Building',
  '人': 'Human',
  '车辆': 'Vehicle',
  '船舶': 'Ship',
  '土壤': 'Soil',
  '水体': 'Water',
  '其他': 'Other',
  '光谱定义': 'Spectral Definition',
  '光谱名称': 'Spectrum Name',
  '导入属性': 'Import Properties',
  '光谱来源': 'Spectrum Source',
  '自定义（面板数据）': 'Custom (Panel Data)',
  'PROSPECT 模型': 'PROSPECT Model',
  'BSM 模型': 'BSM Model',
  '导入波谱 TXT': 'Import Spectrum TXT',
  'TIR 反射率': 'TIR Reflectance',
  'TIR 透射率': 'TIR Transmittance',
  'Three.js 预览纹理': 'Three.js Preview Texture',
  '常见外观': 'Common Appearance',
  '纹理重复尺寸（m）': 'Texture Repeat Size (m)',
  '红砖墙': 'Red Brick Wall',
  '石子路 / 粗粒路面': 'Gravel / Coarse Road',
  '混凝土墙': 'Concrete Wall',
  '浅色粉刷墙': 'Light Plaster Wall',
  '自然石墙': 'Natural Stone Wall',
  '红瓦屋面': 'Red Tile Roof',
  '金属表面': 'Metal Surface',
  '木材': 'Wood',
  '草地': 'Grass',
  '只用于 Three.js 场景预览，不参与 HiStream 求解，也不改变反射率、发射率或模拟结果。': 'Used only for Three.js scene preview. It does not participate in HiStream solving or change reflectance, emissivity, or simulation results.',
  '温度材质': 'Thermal Material',
  '材质名称': 'Material Name',
  '阳面温度（K）': 'Sunlit Temperature (K)',
  '阴面温度（K）': 'Shaded Temperature (K)',
  '物化属性': 'Physical Properties',
  '属性名称': 'Property Name',
  'Mesh 属性映射': 'Mesh Property Mapping',
  'Mesh 完整属性映射': 'Complete Mesh Property Mapping',
  '光谱': 'Spectrum',
  '光谱属性': 'Spectral Properties',
  '温度属性': 'Thermal Properties',
  '结构属性': 'Structural Properties',
  '保存并加入场景': 'Save & Add to Scene',
  '编辑 OBJ 计算属性': 'Edit OBJ Simulation Properties',
  'OBJ 路径': 'OBJ Path',
  '对象属性绑定': 'Object Property Binding',
  '结构参数': 'Structural Parameters',
  '保存 OBJ 属性': 'Save OBJ Properties',
  '设置对象实例分布': 'Configure Object Instance Distribution',
  '实例分布': 'Instance Distribution',
  '分布方式': 'Distribution Method',
  '随机分布': 'Random Distribution',
  '规则网格': 'Regular Grid',
  '单实例': 'Single Instance',
  '自定义坐标': 'Custom Coordinates',
  '控制方式': 'Control Method',
  '按数量': 'By Count',
  '按密度': 'By Density',
  '实例数量（个）': 'Instance Count',
  '分布密度（个/公顷）': 'Distribution Density (items/ha)',
  '1 公顷 = 10,000 m²': '1 hectare = 10,000 m²',
  '随机移动': 'Random Movement',
  '启用随机移动': 'Enable Random Movement',
  '平均速度（m/s）': 'Average Speed (m/s)',
  '速度扰动（±m/s）': 'Speed Variation (±m/s)',
  '活动半径（m）': 'Movement Radius (m)',
  '转向间隔（s）': 'Turning Interval (s)',
  '保存修改': 'Save Changes',
  '新增材质属性': 'Add Material Property',
  '属性类别': 'Property Category',
  '光谱特征': 'Spectral Properties',
  '温度特征': 'Thermal Properties',
  '内部名称': 'Internal Name',
  '显示名称': 'Display Name',
  '加入预设库': 'Add to Preset Library',
  '删除所有结果': 'Delete All Results',
  '刷新': 'Refresh',
  '角度分析': 'Angular Analysis',
  '半球极坐标 · 角度补全': 'Hemispherical Polar · Angle Completion',
  '方位角按 0–360° 周期补全，颜色为观测方向插值结果': 'Azimuth is completed periodically over 0–360°; colors are interpolated from observed directions.',
  '波段分析': 'Band Analysis',
  '时间分析': 'Time Analysis',
  '变化分析': 'Change Analysis',
  '三维分析': '3D Analysis',
  '尚未加载输出目录': 'Output Directory Not Loaded',
  '尚无图像结果': 'No Image Results',
  '在资源管理器中打开': 'Open in File Explorer',
  '空间范围': 'Spatial Extent',
  'X 尺寸（南北）': 'X Extent (North–South)',
  'Z 尺寸（东西）': 'Z Extent (East–West)',
  'Y 最大高度': 'Maximum Y Height',
  '体元大小': 'Voxel Size',
  'OBJ填充阈值': 'OBJ Fill Threshold',
  '计算域偏移': 'Calculation Domain Offset',
  'X 偏移（北向）': 'X Offset (North)',
  'Y 偏移（高度）': 'Y Offset (Height)',
  'Z 偏移（东向）': 'Z Offset (East)',
  'DEM 地形': 'DEM Terrain',
  '启用 DEM 地形': 'Enable DEM Terrain',
  '导入 DEM 文件': 'Import DEM File',
  '背景属性': 'Background Properties',
  '背景异质性': 'Background Heterogeneity',
  '启用 Hapke 角度效应': 'Enable Hapke Angular Effect',
  '结构强度': 'Structural Strength',
  '太阳位置': 'Sun Position',
  '太阳天顶角': 'Solar Zenith Angle',
  '太阳方位角': 'Solar Azimuth Angle',
  '辐照条件': 'Irradiance Conditions',
  '直射比例': 'Direct Fraction',
  '漫射比例': 'Diffuse Fraction',
  '天空温度': 'Sky Temperature',
  '成像参数': 'Imaging Parameters',
  '图像分辨率': 'Image Resolution',
  '自定义波段 [nm]': 'Custom Bands [nm]',
  '连续波段模拟': 'Continuous-band Simulation',
  '起始波段': 'Start Wavelength',
  '结束波段': 'End Wavelength',
  '波段间隔': 'Wavelength Interval',
  '投影方式': 'Projection',
  '平行投影': 'Parallel Projection',
  '中心投影': 'Perspective Projection',
  '观测天顶角': 'View Zenith Angle',
  '观测方位角': 'View Azimuth Angle',
  '视场天顶角': 'Field-of-view Zenith Angle',
  '视场方位角': 'Field-of-view Azimuth Angle',
  '垂直视场角': 'Vertical Field of View',
  '启用航点巡航': 'Enable Waypoint Cruise',
  '巡航轨迹': 'Cruise Route',
  '航点间距': 'Waypoint Spacing',
  '航点数量': 'Waypoint Count',
  '直线往返': 'Line Shuttle',
  '中心矩形': 'Centered Rectangle',
  '随机飞行': 'Random Flight',
  '换一条随机路线': 'Generate Another Random Route',
  '太阳主平面与垂直主平面': 'Solar Principal & Perpendicular Planes',
  '128 方向半球观测': '128-direction Hemispherical Views',
  '导入 OBJ 实体': 'Import OBJ Entity',
  '新增 PRIM 原型': 'Add PRIM Prototype',
  '新增火焰': 'Add Fire',
  '新增雾': 'Add Fog',
  '分布': 'Distribution',
  '属性': 'Properties',
  '删除': 'Delete',
  '无可用属性': 'No Available Properties',
  '时间设置': 'Time Settings',
  '开始节点': 'Start Node',
  '结束节点': 'End Node',
  '驱动文件': 'Forcing File',
  '导入本地 meteo 文件': 'Import Local Meteo File',
  '温度模拟方法': 'Temperature Method',
  '土壤温度': 'Soil Temperature',
  '植被温度': 'Vegetation Temperature',
  '0 · 瞬时能量平衡': '0 · Instantaneous Energy Balance',
  '1 · 热惯性动态模型': '1 · Thermal Inertia Dynamic Model',
  '2 · 温度廓线传导模型（默认）': '2 · Soil Temperature Profile Conduction (Default)',
  '经验法 · Ball–Berry': 'Empirical · Ball–Berry',
  '机制法 · Farquhar': 'Mechanistic · Farquhar',
  '流体状态估算显存': 'Estimated Fluid-state VRAM',
  '启用流体力学': 'Enable Fluid Dynamics',
  '流体体元大小': 'Fluid Voxel Size',
  '来流边界': 'Inflow Boundary',
  '来流风向': 'Inflow Wind Direction',
  '数值求解': 'Numerical Solver',
  '热浮力系数': 'Thermal Buoyancy Coefficient',
  '树冠阻力系数': 'Canopy Drag Coefficient',
  '地表换热强度': 'Surface Heat-exchange Strength',
  '最大标量时间步': 'Maximum Scalar Time Steps',
  '数值限速上限': 'Numerical Speed Limit',
  '热力与介质耦合': 'Thermal & Medium Coupling',
  '火焰烟雾源强': 'Fire Smoke Source Strength',
  '输出风速 GeoTIFF': 'Output Wind-speed GeoTIFF',
  '输出气温 GeoTIFF': 'Output Air-temperature GeoTIFF',
  '大气辐射传输': 'Atmospheric Radiative Transfer',
  '启用 MODTRAN 查找表': 'Enable MODTRAN Lookup Table',
  '大气类型': 'Atmospheric Profile',
  '气溶胶类型': 'Aerosol Type',
  '能见度': 'Visibility',
  '柱状水汽量 PWV': 'Precipitable Water Vapor (PWV)',
  '纬度': 'Latitude',
  '经度': 'Longitude',
  '热带大气': 'Tropical Atmosphere',
  '中纬度夏季': 'Mid-latitude Summer',
  '中纬度冬季': 'Mid-latitude Winter',
  '乡村型': 'Rural',
  '城市型': 'Urban',
  '计算控制': 'Simulation Controls',
  '辐射求解方法': 'Radiative Solver',
  '传统稳健方法': 'Traditional Robust Method',
  '追踪深度': 'Trace Depth',
  '采样数目': 'Sample Count',
  'GPU 序号': 'GPU Index',
  '周期穿透边界次数': 'Periodic Boundary Traversals',
  '典型天空盒': 'Typical Skybox',
  '使用异质性体元': 'Use Heterogeneous Voxels',
  '模拟产出': 'Simulation Outputs',
  '输出反照率': 'Output Albedo',
  '输出温度': 'Output Temperature',
  '输出辐射过程': 'Output Radiation Process',
  '输出能量过程': 'Output Energy Process',
  '温度响应参数': 'Temperature Response Parameters',
  '温度收敛阈值': 'Temperature Convergence Threshold',
  '温度松弛系数': 'Temperature Relaxation Factor',
  '最大耦合迭代': 'Maximum Coupling Iterations',
  '瞬时能量平衡': 'Instantaneous Energy Balance',
  '热惯性动态模型': 'Thermal Inertia Dynamic Model',
  '温度廓线传导模型': 'Soil Temperature Profile Conduction',
  '场景统计': 'Scene Statistics',
  '计算对象': 'Simulation Objects',
  '初始状态': 'Initial State',
  '多层土壤温度': 'Multilayer Soil Temperature',
  '土层开关': 'Soil-layer Switch',
  '高度切片输出': 'Height-slice Output',
  '输出高度': 'Output Height',
  '体元过程': 'Voxel Process',
  '二维图像': '2D Image',
  '面元三维': 'Facet 3D',
  '灰度图像': 'Grayscale Image',
  '彩色图像': 'Color Image',
  '灰度波段': 'Grayscale Band',
  '显示指标': 'Display Metric',
  '有效像元': 'Valid Pixels',
  '平均差值': 'Mean Difference',
  '相关系数 R': 'Correlation R',
  '像元散点图': 'Pixel Scatter Plot',
  '图像 A 像元值': 'Image A Pixel Value',
  '图像 B 像元值': 'Image B Pixel Value',
  '差异图（B − A）': 'Difference Map (B − A)',
  '差值范围': 'Difference Range',
  '交换 A / B': 'Swap A / B',
  '整幅图像三维分析': 'Full-image 3D Analysis',
  '高度按像素亮度映射': 'Height Mapped from Pixel Brightness',
  '面方向': 'Facet Orientation',
  '正面': 'Front',
  '背面': 'Back',
  '正反两面': 'Both Sides',
  '上北 · 右东': 'North Up · East Right',
  '天空向上 · 地平线水平': 'Sky Up · Level Horizon',
  '用系统程序打开': 'Open with System Application',
  '暂不支持内置预览': 'Built-in Preview Not Available',
  '新增': 'Add',
  '方差模板': 'Variance Texture',
  '物理纹理': 'Physical Texture',
  '波谱文件': 'Spectrum File',
  '亮度': 'Brightness',
  '统一方向：+X 向北、−X 向南；+Y 向上；+Z 向东、−Z 向西。OBJ、Three.js 与计算引擎采用同一约定；方位角 0° 向北、90° 向东；平行投影固定上北、右东，中心投影保持天空向上和地平线水平。': 'Unified orientation: +X north, −X south; +Y up; +Z east, −Z west. OBJ, Three.js, and the simulation engine use the same convention. Azimuth is 0° north and 90° east. Parallel projection is north-up and east-right; perspective projection keeps the sky up and the horizon level.',
  '相机方向随飞机航向旋转。天顶角 0° 表示垂直向下；相对飞机方位角 0° 向前、90° 向右、180° 向后、270° 向左。倾斜观测保持天空向上、地平线水平；正下视保持上北、右东。': 'The camera rotates with aircraft heading. Zenith 0° points straight down; relative azimuth 0° is forward, 90° right, 180° backward, and 270° left. Oblique views keep the sky up and the horizon level; nadir views remain north-up and east-right.',
  '中心投影使用观测位置、巡航高度、视场角和视场方向；天顶角 0° 表示垂直向下。倾斜观测保持天空向上、地平线水平；正下视保持上北、右东。': 'Perspective projection uses the observation position, altitude, field of view, and view direction; zenith 0° points straight down. Oblique views keep the sky up and the horizon level; nadir views remain north-up and east-right.',
  '光学波段使用 Hapke，热红外使用红外 Hapke；0 为各向同性，1 为完整角度效应。': 'Hapke is used for optical bands and infrared Hapke for thermal infrared; 0 is isotropic and 1 applies the full angular effect.',
  '尚未打开工程。请点击“打开工程”，选择已有工程的 project.json。': 'No project is open. Click “Open Project” and select an existing project.json.',
  '生成封闭外壳，适用于建筑等连续表面。立方体按六个面细分，椭球生成连续三角网格。': 'Generates a closed shell for continuous surfaces such as buildings. Cubes are subdivided on all six faces; ellipsoids use a continuous triangular mesh.',
  '仅在包络内部生成离散三角面元，不输出封闭外表面；适用于树冠等混沌介质。': 'Generates discrete triangular facets only inside the envelope, without a closed outer surface; suitable for chaotic media such as tree crowns.',
  '最终位置为中心位置加整体偏移；生成后仍可在对象“分布”中修改。': 'The final position is the center plus the global offset; it can still be edited under Object “Distribution” after generation.',
  '坐标统一为 X 南北、Y 向上、Z 东西；火焰介质会衰减、散射并按固定温度发射。当前不生成独立烟雾层。': 'Coordinates use X north–south, Y up, and Z east–west. Fire media attenuate, scatter, and emit at a fixed temperature. A separate smoke layer is not currently generated.',
  '生成后立即加入场景；默认属性可在 PRIM 原型卡片中继续修改。': 'The object is added to the scene immediately; default properties can be edited later on the PRIM prototype card.',
  '同一 OBJ 的树干、树枝和树叶可分别绑定完整属性。': 'Trunks, branches, and leaves in one OBJ can each bind a complete property set.',
  '每个 Mesh 独立参与计算；对象属性仅作为旧工程或缺省 Mesh 的回退值。': 'Each mesh is simulated independently; object properties are only fallback values for legacy projects or meshes without explicit settings.',
  '选项来自“材质属性”预设库；保存后更新 project.json。': 'Options come from the Material Properties preset library; saving updates project.json.',
  '修改会同步写入当前工程的 project.json。': 'Changes are written to the current project.json.',
  'HiStream 自动令漫射比例 = 1 − 直射比例。': 'HiStream automatically sets the diffuse fraction to 1 − direct fraction.',
  'OBJ 导入实体': 'Imported OBJ Entities',
  'PRIM 原型几何': 'PRIM Prototype Geometry',
  '火焰场景对象': 'Fire Scene Objects',
  '参与介质场景对象': 'Participating-medium Scene Objects',
  '尚未导入 OBJ 实体。': 'No OBJ entities have been imported.',
  '尚未生成 PRIM 原型。': 'No PRIM prototypes have been generated.',
  '尚未添加火焰。': 'No fire object has been added.',
  '尚未添加火焰或雾。': 'No fire or fog object has been added.',
  '火焰参与 VoxelRT 辐射传输；VoxelEB 可另外计算烟雾浓度输送，但当前烟雾尚未耦合进辐射图像。': 'Fire participates in VoxelRT radiative transfer. VoxelEB can additionally transport smoke concentration, but smoke is not yet coupled into radiance images.',
  'OBJ 用于管理外部导入实体；PRIM 用于管理椭球、立方体、混沌介质和水平水面。对象均可独立设置属性和实例分布。': 'OBJ manages imported entities; PRIM manages ellipsoids, cubes, chaotic media, and horizontal water surfaces. Each object can have independent properties and instance distributions.',
  '点击任一属性可直接修改；保存后同步更新当前工程的 project.json。': 'Click any property to edit it directly; saving synchronizes the current project.json.',
  '混沌介质（多孔透射）': 'Chaotic Medium (Porous Transmission)',
  '树叶（多孔透射）': 'Leaves (Porous Transmission)',
  '树枝（刚性木质）': 'Branches (Rigid Wood)',
  '树干（刚性木质）': 'Trunk (Rigid Wood)',
  '刚体（仅轮廓不透光）': 'Rigid Body (Opaque Silhouette)',
  '火焰（参与介质）': 'Fire (Participating Medium)',
  '雾（参与介质）': 'Fog (Participating Medium)',
  '雾介质': 'Fog Medium',
  '雾介质温度': 'Fog-medium Temperature',
  '火焰和雾参与 VoxelRT / VoxelEB 的体元辐射传输；雾以局部参与介质体积表示，不替代 MODTRAN 大气。': 'Fire and fog participate in VoxelRT / VoxelEB radiative transfer. Fog is represented as a local participating-medium volume and does not replace the MODTRAN atmosphere.',
  '坐标统一为 X 南北、Y 向上、Z 东西；参与介质会衰减、散射并按固定温度发射。': 'Coordinates use X north–south, Y up, and Z east–west. Participating media attenuate, scatter, and emit at a fixed temperature.',
  '植被生理生化': 'Vegetation Physiology & Biochemistry',
  '木质固体热平衡': 'Woody-solid Heat Balance',
  '土壤表面物化': 'Soil Surface Physics',
  '水体物性': 'Water Physical Properties',
  '结构': 'Structure: ',
  '混沌介质': 'Chaotic Medium',
  '刚体': 'Rigid Body',
  '模型': 'Model: ',
  '木质固体': 'Woody Solid',
  '传感器高度和观测角度自动读取，参数在 LUT 节点间插值，覆盖 350–14000 nm。视场中的天空像元按各自半球天顶角计算；启用时使用 LUT，关闭时使用简化经验天空模型。': 'Sensor height and view angles are read automatically. Parameters are interpolated between LUT nodes over 350–14000 nm. Sky pixels are computed at their own hemispherical zenith angles; the LUT is used when enabled and a simplified empirical sky model when disabled.',
  '逐时间输出图像': 'Output Images for Each Time Node',
  '辐射过程：短波、长波和净辐射；能量过程：潜热、显热和表面热通量。两类结果可独立输出并用于三维分析。': 'Radiation outputs include shortwave, longwave, and net radiation; energy outputs include latent heat, sensible heat, and surface heat flux. Both can be exported independently for 3D analysis.',
  '不会复制邻域场景。0 为关闭；1–20 表示主视线离开一侧边界后，从对侧继续查询同一场景的最大次数。天空盒勾选后显示典型蓝天与云层；不勾选时保持默认 COS 天空响应。两项互相独立。': 'No neighboring scene is copied. 0 disables wrapping; 1–20 is the maximum number of times a primary ray can leave one boundary and continue from the opposite side in the same scene. Enable the skybox for a typical blue sky and clouds; when disabled, the default COS sky response is retained. The two options are independent.',
  '开启后从 OBJ 面元计算每个体元的三轴聚集指数和体密度，并在 VoxelRT/VoxelEB 消光、散射及热辐射中使用；关闭时保持均匀体元算法。默认关闭。': 'When enabled, three-axis clumping indices and volume density are calculated from OBJ facets for each voxel and used in VoxelRT/VoxelEB extinction, scattering, and thermal radiation. Disabled uses uniform voxels. Off by default.',
  '初始温度只作为求解初值；能量平衡各时刻均由所选方法动态更新。土壤方法会同步应用到全部土壤物化属性。': 'Initial temperature is only the solver starting value. Energy balance updates every time node dynamically with the selected method. The soil method applies to all soil physical properties.',
  '体元 RT–EB 耦合': 'Voxel RT–EB Coupling',
  '光谱加速宽度': 'Spectral Acceleration Width',
  '默认100 nm。直射和漫射均在窗口内积分，并使用窗口中心光学属性计算一次。场景几何和体元辐射结构只初始化一次。': 'Default: 100 nm. Direct and diffuse radiation are integrated over the window and computed once using the center-wavelength optical properties. Scene geometry and voxel radiation structures are initialized only once.',
  '时间步长 dTime': 'Time Step dTime',
  '站点参数': 'Site Parameters',
  '观测高度 z': 'Observation Height z',
  '初始土壤温度 Tsold': 'Initial Soil Temperature Tsold',
  '饱和含水量 SatWater': 'Saturated Water Content SatWater',
  '当前能量平衡初温取首个气象节点，饱和含水量以“材质属性 → 物化属性”的土壤参数为准。': 'The initial energy-balance temperature comes from the first meteorological node. Saturated water content uses the soil parameters under Material Properties → Physical Properties.',
  'HiStream 内置气象数据': 'HiStream Built-in Meteorological Data',
  '文件将复制到当前工程的 meteorology 目录，并写入 project.json。': 'The file is copied into the current project’s meteorology directory and recorded in project.json.',
  '求解器 · D3Q19 LBM': 'Solver · D3Q19 LBM',
  '空气网格 X × 高 × Y': 'Air Grid X × Height × Y',
  '空气单元': 'Air Cells',
  '流体采用 D3Q19 BGK 格子玻尔兹曼方法；分布函数、风速、气温和烟雾使用独立稠密网格，辐射传输与能量平衡仍使用场景体元。调大流体体元可显著降低显存占用。': 'Fluid dynamics uses the D3Q19 BGK lattice Boltzmann method. Distribution functions, wind speed, air temperature, and smoke use an independent dense grid, while radiative transfer and energy balance retain scene voxels. Increasing fluid voxel size significantly reduces VRAM use.',
  '来流风速逐时间读取气象驱动；限速上限只用于防止数值发散，不是入射风速或色标上限。0° 沿场景正 Y 方向，90° 沿正 X 方向。': 'Inflow speed is read from meteorological forcing at each time node. The speed limit only prevents numerical divergence; it is not the inflow speed or color-scale maximum. 0° follows scene +Y and 90° follows +X.',
  '运动黏度/标量扩散': 'Kinematic Viscosity / Scalar Diffusion',
  '每个气象节点至少推进一次来流穿越计算域的 LBM 迭代；物理风速采用固定比例映射到稳定的格子速度，温度与烟雾按这里的最大时间步输运。LBM 直接由密度分布恢复压力，不再执行压力 Jacobi 迭代。': 'Each meteorological node advances at least enough LBM iterations for inflow to cross the domain. Physical wind speed is mapped at a fixed ratio to stable lattice velocity; temperature and smoke are transported for the configured maximum scalar duration. LBM recovers pressure directly from density without a pressure Jacobi iteration.',
  '建筑和刚体为固体边界，树冠为多孔阻力区；火焰提供温度和烟雾浓度源项。': 'Buildings and rigid bodies are solid boundaries, tree crowns are porous drag zones, and fire supplies temperature and smoke-concentration sources.',
  '填写高度会定位到对应流体体元；当前实际输出体元中心约为 2.50 m。两个文件均按气象时间节点输出到 output 目录。风速文件包含水平风速大小、X 分量、垂直分量和 Y 分量；气温文件单位为 ℃。建筑内部按固体处理，在风速和气温图中写为 NoData。默认均不输出。': 'The entered height selects the corresponding fluid voxel; the current output voxel center is approximately 2.50 m. Both files are written to output for each meteorological time node. Wind files contain horizontal speed, X, vertical, and Y components; air temperature is in °C. Building interiors are solid and written as NoData. Both outputs are disabled by default.',
  'OBJ 方向统一为 +X 北、−X 南、+Y 上、+Z 东、−Z 西；Three.js 与计算引擎保持一致。MTL 不参与计算。': 'OBJ orientation is +X north, −X south, +Y up, +Z east, and −Z west; Three.js and the simulation engine use the same convention. MTL does not participate in simulation.',
  '最小缩放': 'Minimum Scale',
  '最大缩放': 'Maximum Scale',
  '高度 Y': 'Height Y',
  '自定义位置（每行：X北向 Z东向 Y高度 缩放 旋转角）': 'Custom Positions (each line: X north, Z east, Y height, scale, rotation)',
  '导入 TXT': 'Import TXT',
  'OBJ 与 PRIM 格式一致；至少填写 X北向、Z东向，缺省值为 Y=当前高度、缩放=1、旋转角=0。': 'OBJ and PRIM use the same format. Enter at least X north and Z east; defaults are Y=current height, scale=1, and rotation=0.',
  '仅用于人、车辆和船舶；保存到 project.json，并在三维场景中实时预览随机游走。': 'Available for humans, vehicles, and ships. Saved to project.json and previewed as a real-time random walk in the 3D scene.',
  '修改后同步更新 positions 文件和 project.json。': 'Changes synchronize the positions file and project.json.',
  '图像结果与面元三维结果分别浏览，互不混合。': 'Image results and facet 3D results are browsed separately.',
  '支持 .tif/.tiff、.hdr + .img、.txt、.csv、.dat': 'Supports .tif/.tiff, .hdr + .img, .txt, .csv, and .dat'
})

const originalText = new WeakMap()
const originalAttributes = new WeakMap()
let language = normalizeLanguage(localStorage.getItem(STORAGE_KEY))
let observer = null

const skipSelector = [
  'textarea', 'pre', 'code', '.console-output', '.path-input', '#resultFiles', '[data-i18n-ignore]'
].join(',')

const PHRASE_REPLACEMENTS = [
  ['X 最小值（南）', 'Minimum X (South)'], ['X 最大值（北）', 'Maximum X (North)'],
  ['Z 最小值（西）', 'Minimum Z (West)'], ['Z 最大值（东）', 'Maximum Z (East)'],
  ['中心 X·北向（m）', 'Center X · North (m)'], ['中心 Z·东向（m）', 'Center Z · East (m)'],
  ['目标位置 X（北向）', 'Target X (North)'], ['目标位置 Z（东向）', 'Target Z (East)'],
  ['目标位置 Y（高度）', 'Target Y (Height)'], ['起始位置 X（北向）', 'Start X (North)'],
  ['起始位置 Z（东向）', 'Start Z (East)'], ['起始位置 Y（高度）', 'Start Y (Height)'],
  ['矩形中心 X（北向）', 'Rectangle Center X (North)'], ['矩形中心 Z（东向）', 'Rectangle Center Z (East)'],
  ['矩形长度 X（南北）', 'Rectangle Length X (North–South)'], ['矩形宽度 Z（东西）', 'Rectangle Width Z (East–West)'],
  ['Y 最大高度（m）', 'Maximum Y Height (m)'], ['Y 高度（m）', 'Y Height (m)'],
  ['X 尺寸·南北（m）', 'X Extent · North–South (m)'], ['Z 尺寸·东西（m）', 'Z Extent · East–West (m)'],
  ['X 尺寸（m）', 'X Size (m)'], ['Z 尺寸（m）', 'Z Size (m)'], ['体元大小（m）', 'Voxel Size (m)'],
  ['水面高程 Y（m）', 'Water Surface Elevation Y (m)'], ['底部高度 Y（m）', 'Bottom Height Y (m)'],
  ['整体偏移 X', 'Global Offset X'], ['整体偏移 Z（东西）', 'Global Offset Z (East–West)'],
  ['整体偏移 Y（高度）', 'Global Offset Y (Height)'], ['X 偏移（m）', 'X Offset (m)'],
  ['Z 偏移·东西（m）', 'Z Offset · East–West (m)'], ['Y 偏移·高度（m）', 'Y Offset · Height (m)'],
  ['观测位置 Y（高度）', 'View Position Y (Height)'], ['巡航高度 Y', 'Cruise Height Y'],
  ['相对飞机方位角', 'Aircraft-relative Azimuth'], ['栅格列 × 行', 'Raster Columns × Rows'],
  ['像元大小', 'Pixel Size'], ['最低–最高高程', 'Minimum–Maximum Elevation'], ['未设置', 'Not Set'],
  ['进度', 'Progress'], ['过程结果', 'Process Result'], ['主结果', 'Primary Result'],
  ['观测角度', 'View Angle'], ['时间节点', 'Time Node'], ['模拟完成', 'Simulation Complete'],
  ['模拟异常结束', 'Simulation Ended Abnormally'], ['正在启动计算引擎', 'Starting Simulation Engine'],
  ['启动失败', 'Startup Failed'], ['准备面元计算缓冲区', 'Preparing Facet Buffers'],
  ['初始化 GPU 与计算缓冲区', 'Initializing GPU and Compute Buffers'], ['构建面元可见性图', 'Building Facet Visibility Graph'],
  ['计算太阳光照比例', 'Computing Sunlight Fraction'], ['计算辐射传输', 'Computing Radiative Transfer'],
  ['计算漫射基线与直射增强', 'Computing Diffuse Baseline and Direct-light Enhancement'],
  ['面元辐射传输计算完成', 'Facet Radiative Transfer Complete'],
  ['典型土壤', 'Typical Soil'], ['健康绿叶', 'Healthy Green Leaf'], ['干燥叶片', 'Dry Leaf'],
  ['玉米健康叶片', 'Healthy Maize Leaf'], ['小麦健康叶片', 'Healthy Wheat Leaf'], ['水稻健康叶片', 'Healthy Rice Leaf'],
  ['衰老作物叶片', 'Senescent Crop Leaf'], ['阔叶树叶片', 'Broadleaf Tree Leaf'], ['针叶树针叶', 'Conifer Needle'],
  ['树干与树枝木质部', 'Trunk & Branch Wood'], ['干燥壤土光谱', 'Dry Loam Spectrum'], ['湿润壤土光谱', 'Wet Loam Spectrum'],
  ['混凝土', 'Concrete'], ['沥青路面', 'Asphalt Pavement'], ['红砖墙面', 'Red-brick Wall'],
  ['人员典型光谱', 'Typical Human Spectrum'], ['载具典型光谱', 'Typical Vehicle Spectrum'], ['船舶典型光谱', 'Typical Ship Spectrum'],
  ['水体光谱', 'Water Spectrum'], ['火焰介质', 'Fire Medium'], ['土壤温度', 'Soil Temperature'],
  ['植被温度', 'Vegetation Temperature'], ['树干与树枝温度', 'Trunk & Branch Temperature'], ['建筑表面温度', 'Building Surface Temperature'],
  ['人员表面温度', 'Human Surface Temperature'], ['载具表面温度', 'Vehicle Surface Temperature'], ['船舶表面温度', 'Ship Surface Temperature'],
  ['水体初始温度', 'Initial Water Temperature'], ['火焰固定温度', 'Fixed Fire Temperature'], ['C3 植被', 'C3 Vegetation'],
  ['C4 植被', 'C4 Vegetation'], ['树枝木质部', 'Branch Wood'], ['树干木质部', 'Trunk Wood'],
  ['典型壤土', 'Typical Loam'], ['干燥土壤', 'Dry Soil'], ['湿润土壤', 'Wet Soil'],
  ['船舶表面物化', 'Ship Surface Physics'], ['水体物化参数', 'Water Physical Parameters'],
  ['光谱特征', 'Spectral Properties'], ['温度特征', 'Thermal Properties'], ['结构参数', 'Structural Parameters'],
  ['反射率', 'Reflectance '], ['透射率', 'Transmittance '], ['阳面', 'Sunlit '], ['阴面', 'Shaded '],
  ['结构混沌介质', 'Structure: Chaotic Medium'], ['结构刚体', 'Structure: Rigid Body'], ['结构Fire', 'Structure: Fire'],
  ['消光', 'Extinction '], ['模型木质固体', 'Model: Woody Solid'], ['比热容', 'Heat Capacity '],
  ['热容', 'Heat Capacity '], ['辐射模型', 'Radiative Model: '], ['折射率', 'Refractive Index ']
]

const DYNAMIC_TRANSLATIONS = [
  [/^场景范围\s+(.+)$/u, 'Scene Extent $1'],
  [/^计算域\s+X\s+(.+?)\s+·\s+Y\s+(.+?)\s+·\s+Z\s+(.+?)\s+m$/u, 'Calculation Domain X $1 · Y $2 · Z $3 m'],
  [/^Three\.js 显示全部实例；运行时跳过完整 OBJ 包围盒与计算域 X=\[(.+?),\s*(.+?)\]、Y=\[(.+?),\s*(.+?)\]、Z=\[(.+?),\s*(.+?)\] m 无交集的实例；跨界对象仅计算域内部分。$/u, 'Three.js displays all instances. At runtime, instances whose complete OBJ bounds do not intersect the X=[$1, $2], Y=[$3, $4], Z=[$5, $6] m domain are skipped; only the in-domain portion of crossing objects is simulated.'],
  [/^耗时\s+(.+)$/u, 'Elapsed $1'],
  [/^预计生成\s*(.+?)\s*个三角面元。?$/u, 'Estimated $1 triangle facets.'],
  [/^共\s*(\d+)\s*个$/u, '$1 total'],
  [/^预计巡航位置\s*·\s*共\s*(\d+)\s*个$/u, 'Estimated Cruise Positions · $1 Total'],
  [/^最终输出\s*(\d+)\s*个波段：连续与自定义波段合并、去重并按波长升序排列；最多\s*(\d+)\s*个波段。$/u, 'Final output: $1 bands. Continuous and custom bands are merged, deduplicated, and sorted by wavelength; maximum $2 bands.'],
  [/^平行投影共\s*(\d+)\s*个观测方向。主平面批量范围为 −75°～75°、间隔 5°，同时包含太阳主平面和垂直太阳主平面；半球采用 128 个等面积方向并限制在 VZA 0°～75°。输出统一为上北、右东。$/u, 'Parallel projection provides $1 view directions. Principal-plane batches span −75° to 75° at 5° intervals and include both solar principal and perpendicular planes; hemispherical viewing uses 128 equal-area directions limited to VZA 0°–75°. Output is north-up and east-right.'],
  [/^(光谱特征|温度特征|结构参数|物化属性)\s*·\s*(\d+)$/u, (_, name, count) => `${ENGLISH_MESSAGES[name] || name} · ${count}`],
  [/^(.+?)\s*·\s*(植被生理生化|木质固体热平衡|土壤表面物化|水体物性)$/u, (_, name, type) => `${name} · ${ENGLISH_MESSAGES[type] || type}`],
  [/^开始模拟\s*·\s*(.+?)\s*·\s*PID\s*(\d+)$/u, 'Simulation Started · $1 · PID $2'],
  [/^模拟完成，耗时\s*(.+?)\s*秒$/u, 'Simulation completed in $1 s'],
  [/^模拟已结束，退出码\s*(.+)$/u, 'Simulation ended with exit code $1'],
  [/^输出：图像=(是|否)；辐射过程=(是|否)；能量过程=(是|否)$/u, (_, image, radiation, energy) => `Outputs: Image=${image === '是' ? 'Yes' : 'No'}; Radiation=${radiation === '是' ? 'Yes' : 'No'}; Energy=${energy === '是' ? 'Yes' : 'No'}`],
  [/^温度方法：土壤=(.+?)；植被=(.+)$/u, 'Temperature methods: Soil=$1; Vegetation=$2'],
  [/^进度\s*(\d+)%\s*·\s*(.+)$/u, 'Progress $1% · $2'],
  [/^过程结果\s*·\s*(.+)$/u, 'Process Result · $1'],
  [/^主结果\s*·\s*(.+)$/u, 'Primary Result · $1'],
  [/^观测角度\s*·\s*(.+)$/u, 'View Angle · $1'],
  [/^时间节点\s*·\s*(.+)$/u, 'Time Node · $1']
]

function normalizeLanguage(value) {
  const normalized = String(value || '').toLowerCase()
  return value === 'en-US' || normalized.startsWith('en') ? 'en-US' : DEFAULT_LANGUAGE
}

function translateDynamic(source) {
  for (const [pattern, replacement] of DYNAMIC_TRANSLATIONS) {
    if (pattern.test(source)) return source.replace(pattern, replacement)
  }
  let translated = source
  for (const [from, to] of PHRASE_REPLACEMENTS) translated = translated.replaceAll(from, to)
  return translated
}

export function getLanguage() {
  return language
}

export function getLocale() {
  return language
}

export function t(source) {
  const value = String(source ?? '')
  if (language === DEFAULT_LANGUAGE) return value
  return ENGLISH_MESSAGES[value] || translateDynamic(value)
}

function isSkipped(element) {
  return element?.nodeType === Node.ELEMENT_NODE && Boolean(element.closest(skipSelector))
}

function localizeTextNode(node) {
  const parent = node.parentElement
  if (!parent) return
  if (!originalText.has(node)) originalText.set(node, node.nodeValue)
  const source = originalText.get(node)
  if (isSkipped(parent) && !ENGLISH_MESSAGES[source?.trim()]) return
  if (!source || !/[\u3400-\u9fff]/u.test(source)) {
    if (language === DEFAULT_LANGUAGE && node.nodeValue !== source) node.nodeValue = source
    return
  }
  const leading = source.match(/^\s*/u)?.[0] || ''
  const trailing = source.match(/\s*$/u)?.[0] || ''
  const content = source.trim()
  const translated = content === '关闭' && parent.matches('button') ? 'Close' : t(content)
  node.nodeValue = language === DEFAULT_LANGUAGE ? source : `${leading}${translated}${trailing}`
}

function localizeAttributes(element) {
  if (isSkipped(element)) return
  const names = ['title', 'placeholder', 'aria-label']
  let saved = originalAttributes.get(element)
  if (!saved) { saved = {}; originalAttributes.set(element, saved) }
  for (const name of names) {
    if (!element.hasAttribute(name)) continue
    if (!(name in saved)) saved[name] = element.getAttribute(name)
    const source = saved[name]
    element.setAttribute(name, language === DEFAULT_LANGUAGE ? source : t(source))
  }
}

export function localizeElement(root = document.body) {
  if (!root) return
  if (root.nodeType === Node.TEXT_NODE) {
    localizeTextNode(root)
    return
  }
  const supportedRoot = root.nodeType === Node.ELEMENT_NODE
    || root.nodeType === Node.DOCUMENT_NODE
    || root.nodeType === Node.DOCUMENT_FRAGMENT_NODE
  if (!supportedRoot) return
  if (root.nodeType === Node.ELEMENT_NODE) localizeAttributes(root)
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT)
  let node = walker.nextNode()
  while (node) { localizeTextNode(node); node = walker.nextNode() }
  const elements = root.querySelectorAll?.('[title], [placeholder], [aria-label]') || []
  elements.forEach(localizeAttributes)
  document.documentElement.lang = language
}

export function setLanguage(nextLanguage) {
  const next = normalizeLanguage(nextLanguage)
  if (next === language) return language
  language = next
  localStorage.setItem(STORAGE_KEY, language)
  document.documentElement.lang = language
  window.dispatchEvent(new CustomEvent('streamsim:languagechange', { detail: { language } }))
  return language
}

function updateToggle(button) {
  if (!button) return
  const label = button.querySelector('[data-language-label]') || button
  const nextLabel = language === DEFAULT_LANGUAGE ? 'EN' : '中文'
  const nextTitle = language === DEFAULT_LANGUAGE ? 'Switch to English' : '切换为中文'
  if (label.textContent !== nextLabel) label.textContent = nextLabel
  if (button.title !== nextTitle) button.title = nextTitle
  if (button.getAttribute('aria-label') !== nextTitle) button.setAttribute('aria-label', nextTitle)
}

export function startLocalization({ onChange } = {}) {
  const button = document.querySelector('#languageToggle')
  button?.addEventListener('click', () => setLanguage(language === DEFAULT_LANGUAGE ? 'en-US' : DEFAULT_LANGUAGE))
  window.addEventListener('streamsim:languagechange', () => {
    onChange?.(language)
    localizeElement(document.body)
    updateToggle(button)
  })
  localizeElement(document.body)
  updateToggle(button)
  if (!observer) {
    observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) mutation.addedNodes.forEach((node) => localizeElement(node))
      updateToggle(button)
    })
    observer.observe(document.body, { childList: true, subtree: true })
  }
}
