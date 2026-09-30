# T00 环境、版本与构建基线

查询日期：**2026-09-30（Asia/Shanghai）**。本次执行使用 ponytail full。

**T00A：资料核对完成。T00B：插件源码及本地 Git 准备完成，真实资产、编译、打包和游戏加载待 Windows 验证。** 用户确认当前使用 Mac，后续自行在 Windows 编译、测试；本轮完成可独立准备的源码和配置，不将它们视作空插件加载通过。

## 1 锁定的公开发行基线

| 组件 | 本轮基线 | 核对依据／限制 |
| --- | --- | --- |
| Satisfactory 稳定版 | **1.2.4.0，build 502094**；发行日期 2026-08-11 | 开发者 Steam 公告列表中最新版本更新；9 月周年活动公告不属于新 build |
| SML | **v3.12.0**；发行日期 2026-06-06 | GitHub `releases/latest` 返回该 tag，`draft=false`、`prerelease=false` |
| SML／Starter Project 提交 | **1a7d2ca3a4281cf589bd842a814fd7a55eac4a99** | `git ls-remote` 实际解析 v3.12.0；Starter Project 就是 SatisfactoryModLoader 仓库，没有另一个需要猜测的 Starter 仓库 |
| 定制 Unreal | **5.6.1-CSS** | 上述提交的 `FactoryGame.uproject` 中 `EngineAssociation`；必须使用社区提供的 CSS 定制引擎 |
| Alpakit | Starter 提交内置，描述版本 **2.0.0** | 与 Starter 一起取得；不单独升级；创建模板为 **C++ & Blueprint**，`game_feature=true` |
| Windows 工具链 | VS **2022**；MSVC **v143 / v14.38-17.8**；.NET **8.0** 与 .NET Framework **4.8.1 SDK** | 当前依赖文档；不能随意换成 VS 2026 默认工具链 |
| Wwise SDK／Authoring | **2023.1.14.8770** | 当前依赖及工程配置文档；即使 Mod 不改声音，配套工程也需要集成并生成 SoundBanks |
| Linux 的 Windows SDK／MSVC | SDK **10.0.22621**；MSVC **17.8** | Linux 文档的 msvc-wine 参数；Windows 环境同时使用匹配 SDK，实际安装小版本待记录 |
| Linux 交叉工具链 | **v25，clang 18.1.0** | 定制 UE 5.6.1 对应工具链；首版不做 Linux 专服，暂不要求安装专服交叉工具链 |
| SMM | **v3.1.0**；发行日期 2026-06-06 | GitHub 最新非预发行版本；提供 Windows、Linux 和 macOS 管理器，macOS 管理器存在不证明 macOS 可开发／运行游戏 |

游戏发行依据：[开发者 Steam 公告](https://steamcommunity.com/app/526870/announcements/?l=english)、[1.2.4.0 公告入口，含 build 502094](https://steamcommunity.com/app/526870/eventcomments/587308782950885179/)（只采用公告信息，不采用玩家评论）。

发行依据：[SML v3.12.0 Release](https://github.com/satisfactorymodding/SatisfactoryModLoader/releases/tag/v3.12.0)、[SML latest API](https://api.github.com/repos/satisfactorymodding/SatisfactoryModLoader/releases/latest)、[SMM v3.1.0 Release](https://github.com/satisfactorymodding/SatisfactoryModManager/releases/tag/v3.1.0)。SML Release 明确支持 Satisfactory 1.2／CL491125；固定提交的 [SML.uplugin](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/SML/SML.uplugin) 声明 `GameVersion >=491125`，因此 **502094 在其声明范围内**。这属于发行及元数据兼容证据，本项目仍未在 502094 运行。

固定工程证据：[FactoryGame.uproject](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/FactoryGame.uproject)、[Alpakit 模板列表](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/Alpakit/Templates/templates.json)、[Alpakit.uplugin](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/Alpakit/Alpakit.uplugin)。

Starter 没有另一个独立发行版本号。本轮以稳定 SML tag 对应工程作为可复现起点；文档建议从 master/dev 取得最近工程，但不将移动分支混入这个基线。如构建确需发布后的工程修正，先记录新 commit、原因和兼容证据，再统一更新本表。

## 2 取得依赖和账号边界

1. **Windows 开发机优先。** 使用能运行游戏和定制 Editor 的 Windows x64 机器，在 SSD 短英文路径准备 Starter；不要放在云同步目录。先安装稳定版游戏并启动一次，再按上表安装 VS、引擎和 Wwise。
2. 引擎下载需要 GitHub 账号与 Epic 开发者账号关联、接受 EpicGames 组织邀请，再通过 [Unreal Linker](https://linker.ficsit.app/link) 加入社区引擎仓库的访问范围。取得文件的位置是 [社区引擎 Releases](https://github.com/satisfactorymodding/UnrealEngine/releases/latest)。Windows 下载同一发行中的 `UnrealEngine-CSS-Editor-Win64.exe` 和 `-1.bin`、`-2.bin`、`-3.bin`，一起放置后安装。
3. **引擎发行的精确 tag／commit 尚未取得。** 未认证访问该私有仓库 API 本轮返回 HTTP 404；不能将版本标识 `5.6.1-CSS` 当成已经核对的安装包 tag。有权限后记录 tag、commit、安装包文件名和校验值，并确认 EngineAssociation 匹配，才能放行工程构建。没有下载整套引擎。
4. Wwise Launcher 下载／集成需要 Audiokinetic 账号。集成上述版本到 `FactoryGame.uproject` 并生成空 SoundBanks；不点击将集成更新到其他版本的按钮。账号权限和安装状态目前未知。
5. SML／Starter 源码是公开仓库，可无需账号读取；用下节的固定 tag clone。游戏需要 Steam／Epic 授权；SMM 从 [官方 Releases](https://github.com/satisfactorymodding/SatisfactoryModManager/releases/tag/v3.1.0) 取得。SMR 发布账号和公开上传授权属于 T11，不属于本次环境验收。

安装依据：[Required Software](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html)、[Project Setup](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/project_setup.html)、[Starter Project](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/StarterProject/ObtainStarterProject.html)。

**Linux 路径已核实存在，尚未实测。** 社区支持 Linux Editor；Win64 打包依赖 Wine 与 msvc-wine（`ue-patches` 分支）、MSVC 17.8 和 SDK 10.0.22621。引擎为同一私有发行的 `UnrealEngine-CSS-Editor-Linux.tar.zst.*`，还需要 git、msitools、tar、zstd、dos2unix。Wwise SDK 是 2023.1.14.8770，Linux 文档的 integration 参数是 **2023.1.14.3555**，二者不能混写。Authoring 通过 Wine 运行；游戏客户端及 Win64 包的加载验证仍须另行落实。当前没有 Linux 主机，因此没有执行这些安装步骤。[Linux 官方社区流程](https://docs.ficsit.app/satisfactory-modding/latest/Development/Linux/LinuxSetup.html)

**macOS 当前用途：** 资料、Git 和以后不依赖 UE 的核心算法检查。配套工程列出的目标平台没有 Mac；现有公开流程没有本机原生 Mac Editor／客户端产物，不能用系统 clang、普通 Epic UE 或空文件替代 T00B。

## 3 T00B 取得机器后的最短执行步骤

以下为**待执行步骤**，不是本轮已通过的构建命令。Starter 和引擎放在本 Mod 仓库之外；本仓库最终只追踪本 Mod 自有文件、文档及必要资产。

### 3.1 固定 Starter 并配置工程

```powershell
git clone --branch v3.12.0 --depth 1 https://github.com/satisfactorymodding/SatisfactoryModLoader.git SatisfactoryStarter
git -C SatisfactoryStarter rev-parse HEAD
```

输出必须为 `1a7d2ca3a4281cf589bd842a814fd7a55eac4a99`。先完成引擎注册、Wwise 集成和 SoundBanks，再执行：

```powershell
$T00Engine = Read-Host '定制 UE 5.6.1-CSS 安装目录'
$T00Project = (Resolve-Path '.\SatisfactoryStarter\FactoryGame.uproject').Path
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" -projectfiles "-project=$T00Project" -game -rocket -progress
if ($LASTEXITCODE -ne 0) { throw '生成工程文件失败' }
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development "-Project=$T00Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Editor 构建失败' }
```

Editor target 的真实名称是 **FactoryEditor**，不是 FactoryGameEditor；来源是固定提交的 [FactoryEditor.Target.cs](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryEditor.Target.cs)。安装目录只在命令执行时输入，不写入项目配置或公开源码。

### 3.2 使用真实 Alpakit 模板创建插件

- 本项目固定 **Mod Reference：`FactoryProductionStats`**；显示名称 **Factory Production Stats**（中文：幸福工厂生产统计），首个开发版本 `0.1.0`。SMR 本轮查重未发现同名公开条目；尚未在 SMR 注册，注册／存档使用后不随意改名。
- 打开配套 Editor，在 Alpakit Dev 中 `Create Mod`，选 **C++ & Blueprint**，使用上述 Reference。当前 Game Feature 路径为 `<Starter>/Mods/GameFeatures/FactoryProductionStats/`。最终将模板生成的本 Mod 文件纳入自己的 Git 仓库；不要把 Starter 根目录或第三方插件一起纳入。
- 本仓库已提供真实 `.uplugin`、Runtime 模块和 Build.cs，位于 `FactoryProductionStats/`。T00 初始模块仅使用 Core；T01 诊断 Actor 已增加 CoreUObject、Engine、FactoryGame 依赖，Runtime 注册 `fps.Probe`，T02 公共契约独立于 UE。未添加正式采集或 SML C++ 子系统接线，不修改游戏／SML 头文件绕过私有访问。Windows 接入与资产步骤见 [WINDOWS_SETUP](WINDOWS_SETUP.md)，诊断与契约证据见 [API_EVIDENCE](API_EVIDENCE.md)。
- 创建并保存真正的 `FGGameFeatureData`，位于插件 Content 根目录，名字与 Reference 相同，`BuiltInInitialFeatureState=Active`。保留模板必要扫描规则。需要 SML 世界入口时创建 `GameWorldModule` 蓝图并勾选 Root Module，同一模块类型只有一个 root。
- `.uplugin` 已将 `SemVersion`、`VersionName` 设为 `0.1.0`，必需 SML 依赖锁到 `3.12.0`，GameVersion 精确限定待测 build `502094`，作者为用户指定的 `guopeng1994`。**这些是源码配置，尚无可宣称兼容的本项目包。** 待 Alpakit 检查实际打包元数据。

机制依据：[固定 ExampleMod.uplugin](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/GameFeatures/ExampleMod/ExampleMod.uplugin)、[固定 C++ 模板](https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/Alpakit/Templates/CPPAndBlueprintBlank)、[3.12 Game Feature 迁移规则](https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html)、[根世界模块与加载检查](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/gameworldmodule.html)。部分教程仍写旧路径 `Mods/<Reference>`；本轮按 3.12 模板／迁移规则使用 `Mods/GameFeatures/`。

### 3.3 干净构建、打包和游戏验收

关闭 Editor，重新生成工程文件，并重新运行上节 FactoryEditor 构建。目标游戏构建为：

```powershell
& "$T00Engine\Engine\Build\BatchFiles\Build.bat" FactoryGame Win64 Shipping "-Project=$T00Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Shipping 构建失败' }
```

`FactoryGame` target 来源：[FactoryGame.Target.cs](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame.Target.cs)。

然后使用当前 Alpakit Dev 的 Windows 游戏目标打包／复制到实际游戏安装，必要时按官方开发流程一并提供 SML；用 Alpakit Release 生成候选包。记录 UI 选择、Alpakit 实际输出的完整 UAT 命令、产物路径和文件哈希。未跑过当前模板前，不拼凑旧教程的 `PackagePlugin -PluginName` 命令或猜测输出目录。

从空开发世界验证主菜单 Mod 列表、进入世界、退出和再次加载；保留 Editor／Shipping／Alpakit 日志和游戏 `FactoryGame.log`，证明 **Runtime 模块与 Game Feature 实际加载**，并记录启动／加载／退出无崩溃。先在只有必要依赖的环境做干净构建；只能删除生成目录，不删除源资产。T00B 完成后再将真实源文件路径写回 TASKS。

## 4 当前机器、取得状态和真实验证

实际执行机：`Darwin arm64`，macOS **27.0 / 26A428**；Git **2.54.0 (Apple Git-157)**。项目开始时仅有 README、PLAN、TASKS 和四张参考图，无 `.uproject`、`.uplugin`、源代码或 Git 仓库。

| 项目 | 资料已核对 | 文件已取得／安装 | 编译已通过 | 游戏已运行 |
| --- | --- | --- | --- | --- |
| 游戏 1.2.4.0 | 是，官方公告 | 当前工作区无游戏；其他位置安装未核实 | 不适用 | 否 |
| SML／Starter v3.12.0 | 是，tag SHA 与原始文件 | 仅在线读取部分源码／元数据，未 clone 全工程 | 否 | 否 |
| 定制 UE 5.6.1-CSS | 版本要求已核对，私有发行 tag 未核对 | 未取得；`UnrealEditor` 不在 PATH | 否 | 否 |
| MSVC、Wwise | 版本要求已核对 | 未配置；`wine` 不在 PATH | 否 | 否 |
| 本 Mod 插件与资产 | 模板／注册方式已核对 | 描述、Build.cs、Runtime 代码已写入；真实资产待 Editor | 否 | 否 |
| 本地 Git／忽略规则 | 已建立并检查 | 是 | 不适用 | 不适用 |

本轮实际执行：

- `uname -sm`、`sw_vers`、`git --version`：得到上述机器和 Git 信息。
- `command -v gh`、`command -v UnrealEditor`、`command -v wine`：均未找到；cmake、系统 clang 存在，不能据此推断具备游戏构建环境。之后通过现有 Git 凭据与 GitHub 连接核实了账号 `guopeng1994`，没有输出或写入凭据。
- `git ls-remote https://github.com/satisfactorymodding/SatisfactoryModLoader.git refs/tags/v3.12.0`：返回本表精确 SHA。
- Python 标准库 urllib 读取 GitHub latest API：SML v3.12.0、SMM v3.1.0 均为非 draft、非 prerelease；读取固定提交的 uproject、模板、uplugin 和 Target.cs，与本表一致。
- 未认证读取 `https://api.github.com/repos/satisfactorymodding/UnrealEngine/releases/latest`：HTTP 404，私有资源访问待落实。
- codegraph 两次返回 `attempt to write a readonly database`；已停止本任务的 codegraph 调用，改用 rtk，没有初始化／重建索引。
- `git init -b main`、`git check-ignore`、`git diff --check` 和 `git add --dry-run`：结果见本节交付记录。没有执行 UE 编译、Alpakit 或游戏命令，因此没有这些成功日志。

## 5 GitHub 与公开内容边界

| 字段 | 当前状态 |
| --- | --- |
| 公开开源意图 | 用户已授权，无需再次确认意图 |
| GitHub 账号／组织 | 用户指定 `guopeng1994`；GitHub 连接及现有 Git 凭据均核实为该账号 |
| 仓库名 | 用户授权选名：`satisfactory-production-stats` |
| 许可证 | 用户要求充分开放／免费授权，已选 **0BSD** 并创建真实 LICENSE；允许使用、修改、复制、分发、商用，不要求署名 |
| GitHub 访问 | 现有 Git 凭据可用，gh 非必需；通过 GitHub REST 创建，Git 推送 |
| Git 身份 | 本仓库使用账号名及 GitHub ID 对应的 noreply 地址，不修改全局身份 |
| 公开仓库 URL | [guopeng1994/satisfactory-production-stats](https://github.com/guopeng1994/satisfactory-production-stats)，已通过 GitHub REST 创建并重命名（public、main），源码已推送；首轮干净 clone 通过 |

本地已初始化 `main` 分支并写入 `.gitignore`。当前公开候选包含文档、忽略规则、LICENSE、插件描述、Build.cs 和实际 C++ Runtime 代码；四张参考图继续保留在原位置，按精确文件名忽略。后续还须提交必要的自有 `.uasset`、配置和资源，不能只上传 C++ 后宣称完整交付。

名称检查使用实际 [SMR GraphQL API](https://api.ficsit.app/v2/query)：`getModByReference(modReference:"FactoryProductionStats")` 返回 `ent: mod not found`。通过 `getMods(filter:{search,limit:100,offset})` 分页检查 Reference 搜索的 475 个结果与英文显示名搜索的 752 个结果，没有名称／Reference 精确匹配；中文显示名搜索结果为 0。检查的是当时可查询的公开条目，后续 SMR 注册仍需重新检查。许可证文本依据 [OSI 0BSD](https://opensource.org/license/0bsd)，只授权本项目自有内容；第三方游戏／引擎／SML／Wwise 按各自许可获取。

引擎、Starter 下载、Wwise、游戏内容、缓存、个人存档、凭据和本机路径不进入公开仓库；按第 3 节的版本记录获取依赖。不要为禁止参考图公开而忽略所有 png／uasset，否则会漏掉运行所需自有资产。需要 LFS 时先以实际资产大小决定，再做干净 clone 检查。

## 6 任务交付与放行

**T00A：准备完成（资料验证通过）。** 变更为本文件；锁定公开版本、兼容依据、取得路径、账号限制和真实缺项。允许 T01 静态调查和 T02 契约草案；不能据此放行真实采集、UI 或游戏集成。

**T00B：源码准备完成，验收待 Windows。** 已提供实际插件描述、C++ Runtime／构建规则及 Windows 资产接入说明；已落实仓库归属、名称、许可证和访问。用户后续提供配套机器并执行编译、打包和加载检查。当前尚无完整 Starter、私有引擎安装包、真实 Game Feature／根世界资产或构建／加载证据，因此 T00B 按原验收条件仍未通过。T01 后续加入一次性诊断源码，T02 加入契约草案；没有伪造二进制资产或正式生产功能。

本轮 Python 标准库静态检查通过：插件／模块名称、版本、必需 SML、目标 build、Core 构建依赖及实际启动／退出方法；12 个应忽略路径被排除，6 个源码／自有资产路径可追踪。首次 dry-run 发现本地 `.serena` 工具配置，已加入忽略规则并保留本地文件。最终公开候选为 10 个自有文件；这些检查不能替代 Unreal 编译或游戏运行。原始参考图完整保留。

Git 交付验证：`git diff --cached --check` 无错误；初次源码提交 `8f4b7c00a3969a155c504147ff07fb3bbe785179` 已推送。实际 `git clone --depth 1` 到临时干净目录，`git ls-files` 返回全部 10 个候选文件，逐文件字节与本地一致；没有参考图／`.serena`，本地四张参考图仍在。随后按用户修正将仓库名改为 `satisfactory-production-stats`，同步描述和文档链接；没有创建发行 tag、GitHub Release 或上传 SMR。**干净 clone 仅验证源码交付，不是干净构建成功。**
