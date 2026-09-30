# T06～T08 源码交付记录

2026-09-30。用户授权先完成 T06～T08 编码，需要编译、构建、游戏验证或发布的步骤延期至 Windows。本轮使用 ponytail full，并在界面实现中应用 game-ui-ux；不启动其他任务或 agent。保持游戏 1.2.4.0／502094、SML v3.12.0／`1a7d2ca3a4281cf589bd842a814fd7a55eac4a99` 基线。

**状态：源码交付，尚未编译／运行／验收。** 本轮没有运行 C++ 编译器、UHT、Unreal Editor、Alpakit、游戏或发布流程；新增可运行检查尚未执行，不能沿用此前检查结果声称当前全部源码通过。

## T06：输入、窗口与生命周期

文件均在 `FactoryProductionStats/Source/FactoryProductionStats/`：`Public/ProductionStatsPlayerComponent.h`、`Private/ProductionStatsPlayerComponent.cpp`、`Public/ProductionStatsWidget.h`、`Private/ProductionStatsWidget.cpp`；Runtime 模块注册入口位于 `Private/FactoryProductionStats.cpp`。

- 通过锁定声明中的 `AFGCharacterPlayer::OnPlayerInputInitialized(AFGCharacterPlayer*,UInputComponent*)` 接入本地控制器组件；重复初始化先解除本组件旧绑定。使用 Enhanced Input 的 Started，每次按下只触发一次；死亡、换 Pawn、退出世界和模块卸载清理自身输入和窗口，不改原有绑定。
- 原生 `UFGInteractWidget` 承载 C++ Slate 内容，经游戏 `UFGGameUI::PushWidget`／`PopWidget` 进入窗口栈。鼠标、移动输入和恢复沿用原生流程，未另设暂停、全局输入模式或直接 AddToViewport。
- 游戏操作上下文负责打开；界面上下文保留同一映射名供重绑查询。窗口内关闭按键通过 `UFGInputLibrary::GetCurrentMappingForAction` 获取实际键及修饰键；不硬编码 P 关闭。文字焦点、其他窗口及暂停菜单不打开统计，搜索里的 P 不关窗；Esc 首次离开搜索焦点，再次关闭。原生弹层先处理自己的键，按键重复不反复开关。
- 控制器缓存一个窗口实例，不启用游戏的 WidgetPool 缓存；NativeConstruct／OnPushed 启动每秒查询，关闭／NativeDestruct 清除定时器并调用 Super，仅仍在原生栈内的窗口查询。关窗继续采集。死亡／焦点恢复及原生 Push／Pop 的实际蓝图行为仍需 A01 验证。
- 公共顶部是标题／搜索／排序／恢复／关闭、九个时间按钮、三个标签，时间栏位于标签栏上方。默认 1m，切页保留时间选择。SafeZone、视口 DPI 和容器控制尺寸；时间按钮自动换行，正文双向滚动保留小屏／大字号可达性，列表使用 Slate 虚拟化。

原生界面无需 Widget Blueprint，未伪造 UMG 二进制资产。**输入仍需要三份真实 `.uasset`**：`IA_ProductionStats`、`MC_ProductionStats`、`MC_ProductionStats_UI`。已提供 `tools/create_input_assets.py`，在匹配 Editor 中生成／保存；本轮没有执行。脚本按 5.6 的 PlayerMappableKeySettings 设置稳定映射名 `FactoryProductionStats_Toggle` 和默认 P，两个 FGChildInputMappingContext 分别继承 PlayerActions／UserInterfaceBase，非消费输入，菜单优先级200。已有资产保留编辑设置，类型／映射名不匹配时报错。脚本还把 Inputs 加入 Starter 项目显式 cook 目录；实际发现、设置菜单、重绑及打包包含均待验。

## T07：三页、列表与共用图表

同一 `SProductionStatsWindow` 查询现有 `AProductionStatsSubsystem` 和 History，没有第二份统计状态，也没有随机曲线、库存扫描或配方推算。`Public/ProductionStatsView.h` 提供实际展示值、排序、比例和缺口分段逻辑，供界面与可运行检查共用。

- 物品／流体为产出、消耗双栏；电力为当前世界概览，以及所选时间的消耗、发电、储能三栏。功率是时间加权 MW，储能是区间末值 MWh；储能不作为功率相加。数量累计保留 int64 文本，平均速率使用实际有效秒数。
- 当前概览标快照时间、实际消耗／需求、实际发电／容量、MWh储能、独立充放电、网络／跳闸数及负载／储能百分比。容量0显示无容量／无储能，缺失显示尚无数据，未分类负残差保持带符号。
- 建筑类型行注明当前覆盖设备数；容量、需求、增益、充放电等是所选区间的相关明细。搜索使用本地化名称，只过滤展示，不影响采集、历史或世界概览。默认数值降序，名称排序和稳定 ID 处理并列。
- 列表保留所有匹配条目，默认前8条画曲线，复选框控制曲线。图表每栏最多绘制32条，并显示选中／实际绘制数量；超过上限不删除历史或列表。每次查询最多300点，仍在契约600点上限内。此上限是当前实现的明确显示限制，后续只在实测成本允许时提高。
- 颜色由稳定资源 ID 决定，图例复选框表达选择，不仅靠颜色。比例条按本栏显示的最大绝对值计算，并说明不是库存／供电率。恢复显示只重置搜索、排序和曲线，不清空历史。
- 坐标、单位和悬停显示时间／数值／有效秒数／缺口；储能悬停另说明实际快照时间。未知或部分观测桶断线；来源覆盖未验证的小计仍可显示，但有明确提示。粗桶实际范围和“约”标记、已记录时长、窗口历史不足均可见。
- 优先从游戏 Item／Building Descriptor 取名称和图标；建筑描述类只作一次索引，不逐帧或逐条扫描世界。资源缺失保留历史 ID 和数量，显示未知资源与问号。中文文本使用 LOCTEXT／FText，字体回退及各分辨率视觉结果尚未验证。

## T08：真实保存接线与字节格式

`Public/ProductionStatsSave.h` 定义带原生 struct serializer 的 SaveGame 属性载荷；`AProductionStatsSubsystem` 实现 `IFGSaveInterface` 的 PreSave／PostSave／PreLoad／PostLoad、ShouldSave、NeedTransform 和依赖回调。不是独立外部存档文件，不调用会覆盖玩家整个存档的 SaveGameToSlot。

- PreSave 在游戏线程完成上一工厂区间的数量交接，再编码一致历史；PreFactoryTick 和 PreSave 共用一条 flush 路径，PendingDelta 消耗后置0，避免二次推进。**原生保存回调是否处于工厂线程完成屏障之后尚未证实**，这是 Windows T01／T08 必验项；源码接线不能替代这一时序证据。
- PostSave 释放正常载荷；PreLoad 关闭旧 Inbox、解绑设备／tick／spawn，清空旧历史；PostLoad／BeginPlay 按实际加载顺序恢复一次。新世界重新分配 epoch，磁盘不保存 epoch、UObject 指针或临时电网 ID；旧回调不能写入新 Inbox。已知电力类型从恢复的序列 ID 重建。
- 字节格式为 little endian、FPSH magic、格式1、契约草案schema0、时钟、序列和类别覆盖、CRC32；保存精确整数、double 流体／积分／能量、五级桶、All、pending数量及持有电力。不是内存布局直接dump，不依赖结构体填充。
- 编码／解析每次仅持有一条序列 DTO，复用 History 的导入校验。载荷上限128MiB，资源 ID 上限4096字节，桶／All／序列数量先界定再分配；校验校验和、枚举、bool、时间、单位、槽位、守恒、重复 ID、溢出和尾部多余字节。恢复先建立完整合法候选，再替换历史；任何失败保留活跃历史。
- 游戏恢复失败时保留原始字节、停用采集并向 UI／日志报告；下次保存原样保留未来格式／损坏载荷。编码失败设置 archive error，避免以空数据继续写出。原生存档系统对该错误的传播仍需实际验证。
- 这是首份未发行字节格式，没有先前发行格式可迁移；相同格式支持旧时间线恢复，未来格式／schema被拒绝并保留。今后改变格式必须增加真实迁移，不能把仅检查schema0称为已支持所有旧版本。

内存：消除了完整 SaveData DTO 和另一次 TArray 载荷拷贝，但快照期间仍有 History＋字节 vector，读取时另有待校验 History。UE 自身的 archive 缓冲也需计入；**完整保存／恢复峰值不保证低于128MiB**。核心历史准入和编码大小分别有128MiB上限，不是完整进程的实测结果。T09需要在目标场景测量编码耗时、500序列文件增量和峰值；如果整体预算不满足，应改为游戏 archive 的流式载荷，而非删校验或静默丢历史。

## 检查与接续

已新增 `checks/ProductionStatsPersistenceViewCheck.cpp`，接入 `checks/run.sh`，直接使用本仓库 History／Persistence／View：覆盖超过2^53整数、pending数量、MW／MWh、epoch、未来版本、损坏和截断、旧档替换未来历史、未知／零／部分桶断线、排序／比例、500序列及超过1000h的字节往返。

**本轮只运行以下非编译检查，均通过：**

- `git diff --check`
- `sh -n checks/run.sh`
- Python `ast.parse` 检查资产生成脚本语法（未导入 unreal／未生成资产）
- `codegraph sync /Users/dev/projects/Mods`（仅代码索引，不能作为编译证明）

新增 C++ 检查未编译／未执行。此前 T02～T05 Mac 检查记录仍为此前提交证据；本轮修改后尚未重新验证。Windows 具体命令、资产步骤和 A01／A07／A10～A13接续见 [WINDOWS_SETUP](WINDOWS_SETUP.md)。

## API依据

新增固定声明指纹已加入 [API_SOURCES.tsv](API_SOURCES.tsv)：FGCharacterPlayer／FGPlayerController／FGInputLibrary、FGChildInputMappingContext、FGInteractWidget／FGGameUI、Descriptor、FGSaveInterface及真实SML ModSubsystem实现。第三方源文件保留在被忽略的 `_dependencies`，不推送。FactoryGame私有生成桩不作为真实执行证据。

[输入文档](https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/EnhancedInputSystem.html) 有迁移残留；代码使用固定头文件。脚本字段核对 [UE5.6 InputAction](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/InputAction?application_version=5.6)、[PlayerMappableKeySettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/PlayerMappableKeySettings?application_version=5.6)、[EnhancedActionKeyMapping](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EnhancedActionKeyMapping?application_version=5.6)。[SListView](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Slate/SListView?lang=en-US) 和 [SSafeZone](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Slate/SSafeZone) 的通用机制参考当前官方页面，不代替定制5.6.1-CSS的编译证据。
