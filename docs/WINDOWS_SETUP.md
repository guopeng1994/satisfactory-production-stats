# T00B Windows 接续步骤

状态：**源码准备完成，未编译／未运行。** 版本及依赖取得方式见 [ENVIRONMENT](ENVIRONMENT.md)。本页说明工程和加载门槛。T03～T05 源码已补齐，实际算法在 Mac 检查通过；UE／游戏部分仍未构建或运行，见 IMPLEMENTATION。

## 1 准备配套工程

使用 Windows x64，按环境基线安装 UE 5.6.1-CSS、VS 2022／MSVC 14.38、Wwise 2023.1.14.8770。取得私有引擎后，记录实际 release tag、commit 和安装包校验值。

在短英文路径 clone 本仓库和固定 Starter。例如两个仓库并列在同一个工作目录，在其父目录执行：

```powershell
git clone https://github.com/guopeng1994/satisfactory-production-stats.git FactoryProductionStatsRepo
git clone --branch v3.12.0 --depth 1 https://github.com/satisfactorymodding/SatisfactoryModLoader.git SatisfactoryStarter
git -C SatisfactoryStarter rev-parse HEAD
```

Starter SHA 必须为 `1a7d2ca3a4281cf589bd842a814fd7a55eac4a99`。按官方流程集成 Wwise、生成 SoundBanks，使用 ENVIRONMENT 中命令生成工程并构建 FactoryEditor。

## 2 接入源码并生成真正的资产

先打开 Starter Editor，Alpakit Dev → Create Mod → **C++ & Blueprint**，Mod Reference 输入 **FactoryProductionStats**。由当前模板生成 Game Feature 资产。确认路径为 `Mods/GameFeatures/FactoryProductionStats`，资产类为 `FGGameFeatureData`，名称为 `FactoryProductionStats`，初始状态 Active。

在该插件 Content 下创建 Blueprint，父类为 SML 的 `GameWorldModule`，命名 `RootGameWorld_FactoryProductionStats`，Class Defaults 中勾选 **Root Module**。保存并编译蓝图；同类型只保留一个 root。采集由 native Runtime 的世界初始化通知调用 SML `RegisterSubsystemActor` 注册 `AProductionStatsSubsystem`；不要再在 root 的 Mod Subsystems 列表中注册另一份 Blueprint 子系统。root 保留模板所需生命周期，不添加假数据。

关闭 Editor，在两个仓库父目录执行：

```powershell
$ErrorActionPreference = 'Stop'
$T00Repo = (Resolve-Path '.\FactoryProductionStatsRepo').Path
$T00Starter = (Resolve-Path '.\SatisfactoryStarter').Path
$T00Generated = Join-Path $T00Starter 'Mods\GameFeatures\FactoryProductionStats'
$T00Plugin = Join-Path $T00Repo 'FactoryProductionStats'
$T00Backup = Join-Path (Split-Path $T00Starter -Parent) 'FactoryProductionStats.template-backup'
if (Test-Path "$T00Plugin\Content") { throw '源码仓库已有 Content，请先核对资产，不覆盖' }
if (Test-Path $T00Backup) { throw '已有模板备份，请先核对，不覆盖' }
Copy-Item "$T00Generated\Content" "$T00Plugin\Content" -Recurse
Move-Item $T00Generated $T00Backup
New-Item -ItemType Junction -Path $T00Generated -Target $T00Plugin
```

这将 Editor 生成的真实资产复制回源码仓库，在 Starter 之外保留完整模板备份，再把 Starter 的标准插件路径连接到仓库插件目录。只执行一次；已有目录／备份时先检查，不强行覆盖。工程内不能保留两个同 Reference 的 uplugin。

当前 Runtime 使用 SML 世界子系统、FactoryGame 采集声明以及 Engine；Build.cs 公共依赖为 Core／CoreUObject／Engine／FactoryGame／SML，C++20。`Config/AccessTransformers.ini` 声明友元访问，不编辑上游头文件；第一次接入或修改这些规则后必须重新运行 UHT 并构建，核查 unused transformer 错误。SML 的必需版本依赖已在 uplugin 中声明，使用当前 Alpakit 检查元数据。插件描述中 GameVersion 精确限定 502094，这是待验证目标，不能称为已经通过的兼容承诺。

## 3 构建、打包、运行

关闭 Editor后重新生成工程，并执行以下命令（父目录仍是上节工作目录）：

```powershell
$T00Engine = Read-Host '定制 UE 安装目录'
$T00Project = (Resolve-Path '.\SatisfactoryStarter\FactoryGame.uproject').Path
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" -projectfiles "-project=$T00Project" -game -rocket -progress
if ($LASTEXITCODE -ne 0) { throw '生成工程失败' }
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development "-Project=$T00Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Editor 构建失败' }
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" FactoryGame Win64 Shipping "-Project=$T00Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Shipping 构建失败' }
```

打开 Editor，用 Alpakit Dev 选择 Windows 游戏目标与实际安装目录，打包／复制插件；游戏中需要 SML 3.12.0。用 Alpakit Release 生成候选 zip；保存实际 UAT 命令、打包日志和真实输出路径，再用 `Get-FileHash -Algorithm SHA256 <实际包路径>` 记录哈希。

运行游戏 1.2.4.0／502094：

1. 主菜单 Mod 列表出现 **Factory Production Stats 0.1.0**。
2. `FactoryGame.log` 出现 `FactoryProductionStats 0.1.0 runtime module started`；这条日志证明 native Runtime 加载，不能单独证明资产加载。
3. SML／Game Feature 日志证明 `FactoryProductionStats` 的 Game Feature 已激活、RootGameWorld 已被发现并收到世界生命周期；检查无缺失资产／重复 root／重复插件报错。
4. 进入空测试世界、退回菜单、重新加载、正常退出无崩溃。正常完整关闭时可检查 `FactoryProductionStats runtime module stopped`。
5. 记录机器 CPU／内存、Windows／工具链／游戏 build、工程与 Mod 提交，以及 Editor、Shipping、Alpakit、游戏证据。干净重新构建时只清理 Binaries／Intermediate／Saved 等生成目录，保留 Content 源资产。

最后提交真正的 Game Feature／根世界 `.uasset`，将 TASKS 的 T00B 改为验证通过。当前提交尚无这些资产，无任何编译或游戏验证结果。不要将源文件直接复制进游戏 Mods 目录当作可用安装包；必须经 Alpakit 生成二进制和 cooked 内容。

## 4 许可证和来源

本仓库自有代码采用 [0BSD](../LICENSE)，允许免费复制、修改、分发和商业使用，不要求署名／相同许可证。配套 SML、游戏、UE、Wwise 和其他第三方资源按各自许可获取，不受本仓库 LICENSE 重新授权。原始设计参考图继续只保留本地，不推送。

官方步骤：[工程配置](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/project_setup.html)、[Alpakit 与 Root Module](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/gameworldmodule.html)、[3.12 Game Feature 要求](https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html)。

## 5 T01／T02 接续

本轮新增 `fps.Probe` 原生诊断命令（含 Shipping Runtime）。在 T00B 通过后进入备份单人测试存档，打开开发者控制台并运行命令；按 [SML Commands](https://docs.ficsit.app/satisfactory-modding/latest/SMLChatCommands.html) 的当前说明确认控制台开启方式。不要把聊天里的 `/` 命令当成 Unreal 控制台命令。

下一次安全工厂 tick 应输出 `LogFactoryStatsProbe` 的建筑类计数，以及 `LogProductionStatsPowerEvidence` 的全电路注册表原始数据：Circuit／Group ID、local与Stats字段、采样／更新时间、PowerInfo所属类与实际／请求功耗、基础／动态／增益发电及单个BatteryInfo的MW／MWh。它不提供已验证世界总量或成功产量；Editor生成桩读数不能作为游戏验收。该命令的UE编译与运行尚未通过。

完整的受控场景、证据缺口、父子覆盖、精度及契约冻结条件见 [API_EVIDENCE.md](API_EVIDENCE.md)。T02 纯 C++ 检查的 Mac／MSVC 命令也在该文件；Mac 检查通过只证明公共契约边界，不证明 Unreal API／采集功能。

## 6 T03～T05 Windows 验证接续

运行时应出现一次 `T03-T05 source integration active` 日志；退出再进入产生新的世界历史。Editor 下 native hooks 按 SML 机制停用，子系统也不采集，不能用 Editor 桩返回值证明游戏统计。当前没有 P 窗口；在游戏控制台执行 `fps.Stats` 可将 All 数量及当前 power 快照写入日志，含单位、时点和 provisional 覆盖。同一 History 不依赖 UI；也可用 C++ 调试器读取 Query／CurrentPower／RecordingTime。完整待验表见 [IMPLEMENTATION.md](IMPLEMENTATION.md)。

在 VS 的 x64 Native Tools Command Prompt 中，从仓库根目录执行独立数学检查（不替代 UHT／游戏检查）：

```bat
set FPS_SRC=FactoryProductionStats\Source\FactoryProductionStats
cl /nologo /std:c++20 /EHsc /W4 /I %FPS_SRC%\Public checks\ProductionStatsTypesCheck.cpp /Fe:%TEMP%\fps-types.exe /Fo:%TEMP%\fps-types.obj
%TEMP%\fps-types.exe
cl /nologo /std:c++20 /EHsc /W4 /I %FPS_SRC%\Public %FPS_SRC%\Private\ProductionStatsHistory.cpp checks\ProductionStatsHistoryCheck.cpp /Fe:%TEMP%\fps-history.exe /Fo:%TEMP%\
%TEMP%\fps-history.exe
cl /nologo /std:c++20 /EHsc /W4 /I %FPS_SRC%\Public %FPS_SRC%\Private\ProductionStatsHistory.cpp %FPS_SRC%\Private\ProductionStatsCollectors.cpp checks\ProductionStatsCollectorsCheck.cpp /Fe:%TEMP%\fps-collectors.exe /Fo:%TEMP%\
%TEMP%\fps-collectors.exe
```

逐条检查退出码；不定义 NDEBUG。MSVC 命令当前未运行。实际游戏保存／读档历史恢复在 T08 接入前不会生效，不能因 DTO 往返通过就宣称随档保存完成。
