# T01 静态证据与 T02 契约草案

日期：2026-09-30。游戏基线 1.2.4.0／502094，SML v3.12.0，工程提交固定为 `1a7d2ca3a4281cf589bd842a814fd7a55eac4a99`。环境证据见 [ENVIRONMENT.md](ENVIRONMENT.md)。本文件中的行号均指该提交，不能用 `master`／`latest` 替换后沿用结论。

**结论：T01 静态调查已交付，诊断源码待 UE 编译和实机运行；T02 是可编译的标准 C++20 草案，尚未冻结。没有任何生产类别通过实际计数验收。** 当前 Mac 没有配套 Unreal Editor、游戏或 Windows 工具链；不会以理论产能、库存净差或生成桩代替真实成功操作。

## 1 证据来源与限制

本轮读取固定提交的 113 个头文件／实现桩／说明文件及递归资产树。文件 SHA256 见 [API_SOURCES.tsv](API_SOURCES.tsv)，可用固定提交的 raw GitHub 文件复核；本地资料放在被 Git 忽略的 `_dependencies/T01-SML-v3.12.0/`，不再分发第三方源码。

来源根目录：[固定工程提交](https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99)。以下链接指向原始声明，证明签名、注释和可见访问级别；不证明发行游戏二进制的完整实现。

[Manufacturer.cpp](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Private/Buildables/FGBuildableManufacturer.cpp) 明确是 Unreal Header Implementation 工具生成的文件，多数函数是空体／默认返回。少量补充实现不能使整个文件变成实际游戏实现。因而未证实生产调用链、成功数量、父子 `Super` 调用关系、delegate 分发线程及 Blueprint 覆盖。只有这些证据加上 Windows 受控操作，才允许 T04 接入。

检索遵循 ponytail full。codegraph 已在当前会话报只读数据库错误，未重建索引；后续使用 rtk 搜索固定版本资料。本地标准 C++ 检查使用实际共享头文件，没有第二份 Python 统计算法。

## 2 数量候选接入点

所有数量候选均需先确认权威世界、实际执行线程和不可逆成功操作。表中“数量来源待证实”意味着不能注册正式统计 hook，也不能默认数量为配方值。

| 位置／签名 | 已证实边界 | 成功数量、线程和覆盖缺口 |
| --- | --- | --- |
| [Factory.h:401](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h#L401)，`Factory_ProductionCycleCompleted(float)` | 虚函数，完成周期通知；参数名是 overProductionRate | 参数不是数量；不以回调次数乘配方数；子类／BP 覆盖和成功库存操作待实测 |
| [Manufacturer.h:182、236](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableManufacturer.h#L182)，`Factory_TickProducing(float)`、`Factory_ConsumeIngredients()` | protected 虚函数；后者注释为移除实际原料，并指出 Converter 行为可能不同 | 消耗时点候选，void 不代表成功数量；输出实际增加点、多产物、增产、内部转移和线程待实测；不得假设子类调用父实现 |
| [ResourceExtractor.h:35、69](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableResourceExtractor.h#L35)，输出库存及生产 tick | 输出库存可读；每周期数量／每分钟抽取量是配置或计算值 | 固体、液体、气体的实际插入点待验证；管道排出是物流，不是新的生产事件 |
| [PortableMiner.h:31、87](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGPortableMiner.h#L31)，`TickProducing(float)`、输出库存 | 继承 AActor，不在 Factory 子类路径内 | 独立发现／销毁路径；取出矿石不是消耗；生产成功和远离玩家后的行为待验证 |
| [GeneratorFuel.h:132](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableGeneratorFuel.h#L132)，`LoadFuel()`、`LoadSupplemental()` | protected 虚函数，尝试装载；CollectFuel/CollectSupplemental 是搬入缓冲 | void 不能证明成功；核对实际不可逆装载数量与剩余燃料。内部 mCurrentFuelAmount 是 MW·s，不是燃料数量；不能据其差值报告件数／m³ |
| [GeneratorNuclear.h:48、54](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableGeneratorNuclear.h#L48)，`LoadFuel()`、`TryProduceWaste()` | 核电覆盖父类装载，另有废料库存和待产废料状态 | 尝试产废料不是成功产出；堵废料与跨燃料边界待实测；父子 hook 不得双计 |
| [PowerBooster.h](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGBuildablePowerBooster.h)，`LoadFuel()` | 独立 Factory 子类，额外燃料增加增益；不继承 GeneratorFuel | 燃料成功消耗点独立验证；增益百分比不能当 MW；实际增益读 PowerInfo 候选 |
| [ResourceSinkSubsystem.h:118](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGResourceSinkSubsystem.h#L118)，`AddPoints_ThreadSafe(TSubclassOf<UFGItemDescriptor>)` | 返回 bool，注释说明可投放才加分，线程安全排队后在游戏线程处理 | 没有数量参数；必须证明一次 true 对应哪个 Sink 的一次实际销毁，排除其他调用者／奖励操作；积分、券数量不能换算为物品数 |
| [SimpleProducer.h](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableFactorySimpleProducer.h)，Factory tick 与输出抓取 | 单产物 Factory，具有事件有效状态及距上次抓取时间 | 礼物树等按需抓取可能不同于一般输出库存；抓取是否就是生成时点待证实 |

[InventoryComponent.h:139、467](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGInventoryComponent.h#L139) 的动态 multicast 提供物品类型、int32 数量和对端库存，但混合搬运、清空、配方切换与生产，而且可被 suppress。原生快速 delegate 是 single-cast，游戏也用它维护缓存；**禁止用 Bind 替换现有原生回调**。动态通知最多作为诊断对照，不能凭对端 null 推断生产。客户端通知顺序也不保证。

### 单位与精度

[InventoryLibrary.h:56、63、68](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGInventoryLibrary.h#L53) 已有按 ResourceForm 换算的原生函数及液体库存换算示例；应先使用原生换算／scalar，而不是散布固定常数。固体保持 int32 原始成功数量，进入契约提升为 int64；流体／气体仅在采集边界转换为 double m³。scalar 的精确含义、气体分支、分数量、流体内部计量与实际 UI 仍需实测。Fuel 的装载字段另注明升与 m³ 关系，不能把其 MW·s 剩余能量当作升。

固体累计必须零误差，超 int64 返回 Overflow 并停止该写入；不先转 double。流体／电力原生多数是 float，契约 double 只能避免后续额外降精度。**原生误差容限尚未测得**，不能给一个宽松百分比宣称通过；Windows 记录原始数、转换后数、操作期望和最大误差，图表粗桶误差另记。

## 3 工厂类别覆盖矩阵

资产名来自固定提交的 [Factory 资产目录](https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Content/FactoryGame/Buildable/Factory)。存在资产文件仅证明名称存在，不能证明该版本实际可建造、运行中的完整父类链或 hook 覆盖；Windows 用实际加载类确认。以下所有“候选”均未通过实际计数。

| 类型／已枚举资产 | 调查路径 | 当前覆盖与必须验证的情况 |
| --- | --- | --- |
| 便携采矿机 | AFGPortableMiner | 独立路径候选；不被本轮 Factory census 发现；远距离、拆除、玩家取出 |
| MinerMk1／Mk2／Mk3 | ResourceExtractor | 候选；堵输出、超频、取出／搬运 |
| WaterPump、OilPump | ResourceExtractor | 候选；液体量、管道排出、缓冲满／空 |
| FrackingExtractor、FrackingSmasher | Extractor／Activator 两条声明 | 提取器和资源井增压器不可各算一次资源；Activator 的输出是否为零、气液资源单位待验证 |
| SmelterMk1、ConstructorMk1、AssemblerMk1、FoundryMk1、ManufacturerMk1 | Manufacturer | 候选；逐一确认实际类和执行点；多输入、产量增益、配方更换 |
| OilRefinery、Blender | Manufacturer 候选 | 固液混合、多产物，独立成功操作数量，副产物堵塞 |
| Packager | Manufacturer 候选 | 包装／解包两个方向；空罐和流体各自入正确页面，搬运不计 |
| HadronCollider、QuantumEncoder | ManufacturerVariablePower 声明 | 该类覆盖生产 tick 和可变功耗；BP 实际父类待 Editor 确认；不得只 hook 父类就宣称覆盖 |
| Converter | Manufacturer 原料方法明确提到特殊行为；无独立 Converter 原生头文件 | 首要缺口：确认 BP 覆盖、催化材料是否实际移除、产出与增产，禁止按配方全数扣原料 |
| GeneratorBiomass（手动／自动／HUB 集成）、GeneratorCoal、GeneratorFuel | GeneratorFuel 候选 | 装载成功及辅助流体；没有生产时的缓冲搬入不计；实际 BP 类型待确认 |
| GeneratorNuclear | GeneratorNuclear | 核燃料与废料候选；废料堵塞、不同燃料、父子重入 |
| GeneratorGeoThermal | GeneratorGeoThermal | 无资源数量路径候选；电力实际生产与波动容量分开 |
| AlienPowerBuilding | PowerBooster | 独立燃料路径及实际增益；无燃料时基本生产仍存在 |
| ResourceSink | Sink + ResourceSinkSubsystem | bool 候选；无效物品、真实销毁、多个 Sink、积分延后 |
| Holiday/Build_TreeGiftProducer | SimpleProducer | 条件原生生产路径未验证；未启用季节内容时不得宣称已实测 |
| Portal／PortalSatellite、Elevator | Factory 子类，分别见 PortalBase／Elevator 头文件 | 特殊电力负载须纳入；Portal 燃料是传送用途，按 PLAN 的运输燃料排除数量统计，在覆盖说明列明 |
| 储物箱、管道／传送带、搬运缓冲、卡车／无人机燃料、手工台、建造／拆除、商店／发券 | PLAN 排除项 | 不作为生产数量；交通设施的实际电力仍需电力总量核对 |

首要失败出口：若上述生产类型找不到实际成功计量，给该 source 标记 UnsupportedSource，结果 completeSources=false，不发布“完整原生生产覆盖”。最小调整是补充可验证接入点或明确收窄发布覆盖，不能以理论／库存净差补齐。

## 4 电力与网络证据

| 固定声明 | 能支持的读取 | 未证明的部分 |
| --- | --- | --- |
| [PowerInfoComponent.h:126](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGPowerInfoComponent.h#L126) | ActualConsumption 是每帧电力系统结果；TargetConsumption 是异步请求；正常条件 MaximumTargetConsumption 只是信息 | 在工厂安全回调读取的是哪个电力 tick 的结果；世界采样频率和时间绑定待实测 |
| 同文件的 BaseProduction、RegulatedDynamicProduction、ActualProductionBoost | 基础、调节动态输出、实际 MW 增益有独立 getter；容量和增益百分比也独立 | 相加是否与网络 Produced 对应、发电增益归属及是否已包含待核对，不能重复加 boost |
| [PowerCircuit.h:171](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGPowerCircuit.h#L171) | GetStats 复制当前统计；公开 Produced、Consumed、Capacity、MaximumConsumption、BatteryPowerInput、BoostProduced | 当前字段与约 1 秒原生图表采样的更新顺序待核对；不能使用历史图表点假冒当前快照 |
| [PowerCircuit.h:215](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGPowerCircuit.h#L215) | 储能存量／容量，净输入拆正负 | 净值拆分不能保证保留不同设备同时充放电。按单个 BatteryInfo 候选分开累计，不能按全世界净功率推导两者 |
| [PowerStorage.h:46、147](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildablePowerStorage.h#L46) | 存量／容量注释为 MWh；内部净输入字段注释为 MW、负数为输出 | GetPowerInput/Output 的相邻注释却写 MWh，存在文档矛盾。必须对原生 UI 与 Δ储能／Δt 测试，不能只信 getter 注释 |
| [Circuit.h:57、71](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGCircuit.h#L57)、[PowerCircuit.h:377](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGPowerCircuit.h#L377) | Circuit ID 与 CircuitGroup ID 不同；开关连通的电路构成同一逻辑组；组只存在服务端 | 各电路的 Stats／储能字段是否重复组总量尚未证明。不得把每条 Circuit 求和直接作为世界总量 |
| [CircuitSubsystem.h](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGCircuitSubsystem.h) | FindCircuit、GetCircuitGroup、重建 multicast 存在 | circuits／groups 容器是 private，没有已发现的公开全网枚举；GetCircuitGroup 直接索引，不能传未验证 ID；空网、仅特殊消费者的网可能被 Factory 枚举漏掉 |

电力汇总路径仍是候选：安全时机发现实际参与者 → 当前网络／组去重 → 检验组字段语义 → 世界总值与设备类型分类 → 保留有符号残差。临时 Circuit ID／Group ID 只用于同次采样去重和诊断，不写进历史 ID 或存档。不能靠世界正负总和判定各网是否跳闸；networkCount 和 trippedNetworkCount 需要逻辑网络层证据。

特殊消费者调查：

- [Locomotive.h:120](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGLocomotive.h#L120) 暴露 PowerInfo，继承 RailroadVehicle；不能用 Factory 类扫描保证发现车头。
- [HoverPack.h](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Equipment/FGHoverPack.h) 暴露当前连接，但内部 PowerInfo 私有；切换连接、接铁路、未分类消耗与 UI 总量待验证。不可用额定背包功耗代替实际。
- [RailroadTrack.h](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/Buildables/FGBuildableRailroadTrack.h) 继承 Buildable，非 Factory；铁路、车站和车头不能重复分类。
- PortalBase、Elevator 是 Factory 子类，但有专门 tick；充电／生产峰值、闲置／活动须核对。普通车辆／无人机燃料按 PLAN 排除；车站等耗电需要保留在总量中。
- 可变功耗设备读取 ActualConsumption；ManufacturerVariablePower 的 min/max 是功耗范围，不是实际值。

## 5 线程、生命周期与最小诊断

[BuildableSubsystem.h:188–199](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Source/FactoryGame/Public/FGBuildableSubsystem.h#L188) 明确一般 FactoryTick 可能并行，读取其他建筑状态不安全；PreFactoryTick 明确在游戏线程顺序调用且可安全读写建筑。注释提及的 RegisterFactoryTickHandler 不是当前声明名，实际 API 是第 251／253 行的 **AddFactoryTickHandler / RemoveFactoryTickHandler**。本轮一次性探针采用 PreFactoryTick；读取前一轮完成后的候选状态，不宣称与同一帧电力 tick 同步。

正式采集生命周期候选：权威世界起点获得 BuildableSubsystem，一次初始发现；随后监听其新增／移除 multicast。读档、流送、便携矿机及移动负载另有生命周期，尚未实测。所有绑定由世界拥有者登记，EndPlay 对应解除；未知线程回调不得访问其他 UObject 或修改历史。必要时将最小数值／稳定标识送安全队列，回游戏线程验证 world epoch；退世界／加载旧档后旧 epoch 不可写入。

SML 提供 [ModSubsystem / SubsystemActorManager](https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/SML/Source/SML/Public/Subsystem) 的注册／复制策略，可由后续世界模块统一接线；这不是本轮已运行的统计子系统。[Native Hook 文档](https://docs.ficsit.app/satisfactory-modding/latest/Development/Cpp/hooking.html) 说明虚函数覆盖、inline 限制和 Editor 差异；after 不保证实际成功，子类重入需要单事件去重，protected 接入须用受支持 Access Transformer／friend，不能改游戏头文件。固定 NativeHookManager 提供 unsubscribe 宏，实际解绑仍需世界退出测试。

本轮最小源码：`Private/Diagnostics/FactoryStatsProbe.h/.cpp`，通过原生控制台命令 `fps.Probe` 创建临时 Actor，不需要探针 Blueprint：

1. 命令在 Runtime 注册（含 Shipping，便于实际游戏验证），限定游戏线程、游戏世界、非客户端；Actor 再检查 HasAuthority。
2. BeginPlay 注册一次工厂 handler；第一次 PreFactoryTick 输出建筑类路径计数，以及 Factory 附属的去重 Circuit 原始字段、储能存量／容量和 Group ID。没有库存差分、产量累加或生产 hook。
3. 不在 handler 遍历中移除自己；短 lifespan 延后销毁，EndPlay 配对移除 handler。没有 tick 到来时最长 10 秒寿命。模块卸载先注销命令，再销毁存活探针；世界退出走 Actor EndPlay。注册／移除请求日志包含探针对象路径，移除还记录是否已采样和退出原因；日志不能单独证明原生数组中已无残留 handler，仍需反复进出世界验证。
4. 这是一次 O(建筑数) 诊断扫描，只在人工调用时发生，非逐帧采集。它不发现便携矿机、所有特殊消费者或全电网；日志明确标注不提供世界总量。

控制台注册使用 [Unreal IConsoleManager](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/IConsoleManager/RegisterConsoleCommand?application_version=5.6) 的原生 world delegate。本机未取得定制引擎头文件、未执行 UHT／UE 编译；以上为源码行为，**不是运行验收结果**。Windows 以实际游戏中的插件构建验证，记录控制台启用方法；Editor 的生成桩读数不能替代发行游戏结果。

## 6 Windows 受控验证与冻结门槛

先执行 [WINDOWS_SETUP.md](WINDOWS_SETUP.md) 中 T00B，记录 Editor／Development 构建日志、插件加载和真实根资产。之后在备份单人存档运行 `fps.Probe`，保存 `LogFactoryStatsProbe` 原始日志；类 census 与 Editor 实际父类链共同补全矩阵。该命令只做 census／电力读数，以下数量试验仍需在已证实的实际成功操作附近追加最小日志，尚未实现：

| 验证 | 受控操作与对照 | 必须留下的结果 |
| --- | --- | --- |
| 固体 | 单台机器、已知材料，记录每次成功投入及产出；随后多产物、超频／增产、换配方 | 成功资源／数量、时点、线程 ID、权威状态，父子调用栈与操作计数一致；零误差 |
| 堵料／停电／搬运 | 输出堵塞、断电、缺料再恢复；仅搬动同一批物品 | 停机无事件，恢复无双计；搬运不改变生产累计；不得只看净库存 |
| 流体 | 隔离管路的已知配方；原始单位与 UI m³ 对照，再包装／解包 | 原生 scalar、气体分支、实际成功数量；所有产物／原料方向；量化 float 误差 |
| 发电／废料／Sink | 分别控制燃料装载、辅助水、核废料堵塞及 Sink 有效／无效物品 | void／bool 返回与真实不可逆动作关系；内部缓冲、父子重入与全调用者核对 |
| 电路 | 一个可控负载网，约定时点 `fps.Probe` 与原生 UI 截图；变化前后分别采样 | 实际／需求／容量／增益拆分，PreFactoryTick 与电力更新顺序，误差 |
| 网络／储能 | 两个独立网，一个跳闸；开关合并／拆分；可控储能充放电 | Circuit/Group 字段的复制或局部语义，网络去重规则，MW 对 ΔMWh／Δt；零容量不求比例 |
| 特殊负载／生命周期 | 铁路车头、悬浮背包、Portal、Elevator；远离设备、拆建、反复读档／退世界 | Factory 外负载的发现和分类残差；没有流送漏采、重复注册或退出后回调 |

实测记录必须包含游戏 build、SML SHA、插件构建提交、机器信息、场景、原始日志／截图、期望／实际数、精度和结论。当前这些行全部 **待执行**。T01 关键生产类别和网络语义未通过前，T04／T05 不得声称真实集成完成；T02 不冻结版本相关适配。

## 7 T02 公共契约映射

实际定义在 [ProductionStatsTypes.h](../FactoryProductionStats/Source/FactoryProductionStats/Public/ProductionStatsTypes.h)，全部为同一标准 C++20 类型，不依赖 UE，供 T03 使用同一实现。负责游戏转换、世界线程和保存的适配层留给后续任务。没有网络代码／跨世界全局状态。

| 需求 | 类型／规则 |
| --- | --- |
| 序列 ID | SeriesId = Category + Metric + Direction + Scope + key。物品／流体 key 为描述类完整路径；BuildingType 为建筑完整类路径；World／Unclassified 电力 key 为空。资源路径是稳定查找键，不是已加载 UObject 保证 |
| 精度／单位 | Quantity 为 int64 件数或 double m³；类别与 variant 必须相符。PowerReading 明确指标，通过 UnitFor 区分 MW／MWh；禁止 NaN、∞、物理负值；仅 Unclassified 的 MW 差额可为负 |
| 成功数量输入 | QuantityEvent 只接收正的成功量、仿真时间和 source 类路径。zero 是已观测无事件的桶，不发送零事件。游戏 raw → canonical 转换未接入，不能将原生数量直接构造流体 double 后自称 m³ |
| 权威边界 | WriteContext + activeEpoch 允许当前权威世界写入；主菜单／客户端／旧世界拒绝。context 由游戏拥有者生成，不信任外部 bool；T04 必须在实际写入处调用检查。epoch 不保存 |
| 电力快照 | PowerSnapshot 按同一时点包含各 PowerReading、当前网络／跳闸数。optional 空值是未知，0 是已知；同一快照不可重复 SeriesId。设备数是当前覆盖设备数，不是历史平均 |
| 覆盖／缺口 | CoverageGap 表达类别、缺失来源、半开时间范围和原因。Bucket 保存 union observedSeconds、completeSources、原因掩码；停机也计观测时间，新资源首次出现前已覆盖部分为零，不从首次事件缩短分母 |
| 聚合 | Aggregate 三选一：Quantity 精确量、PowerIntegral MW·s、EnergyState MWh＋末时点。Bucket 与 SeriesId 验证阻止单位混用；功率均值=积分/有效秒，区间电量=积分/3600；储能只取末值 |
| 时间／边界 | 非负有限 save-local 仿真秒；事件 `[begin,end)`，恰在 end 的事件归下一桶。能量末值时点允许等于 end。事件乱序由 T03 返回 OutOfOrder，不能悄悄夹到 now；pause／offline 不推进 clock |
| 查询／结果 | Query 一套服务三类别，九窗口、默认 1m、maxPoints≤600。QueryResult 带 requestedRange、actualRange、resolution、approximateBoundary；SeriesResult 带展示资源、单位、summary 和 points；未知资源仍保留稳定 ID 和数量 |
| 错误 | InvalidSeries／Time／Value、UnitMismatch、Overflow、OutOfOrder、NoCoverage、UnsupportedSchema。无覆盖不返回虚假的均值；非法输入拒绝并标缺口，不影响游戏实际生产 |
| 保存 | SaveData 含 draft schema、记录起点／时钟、SavedSeries lifetime、分层桶和 All 趋势。DraftSchemaVersion=0 是未发行 DTO；CheckSchema 不接受其他版本，T08 不能覆盖未知未来格式。不是 UE SaveGame 实现 |

草案数值辅助只处理契约边界：CheckedAddItems 保持溢出时原值；RatePerMinute、AverageMegawatts 无有效时间时返回 nullopt。大整数累计保持 int64，到最终显示速率时才转浮点，速率不承诺超过 double 整数精度后的逐件分辨率。

缺口降采样约定：部分桶保留观测秒和原因，不把未知部分按零摊入分母；UI 对部分／未知桶断线或明确标注该桶不完整。粗桶已经失去精确缺口位置时，不宣称知道缺口在桶内哪个位置。source 缺失与时间缺失分开：有观测但部分机器未覆盖时，数值是已覆盖来源的小计，completeSources=false，不能标全世界精确总量。

T03 要在一个历史实现里落实已要求的有界层级／All 以及区间覆盖；DTO 的 vector 并不自动有界。候选为 1s/10m、10s/1h、1m/10h、10m/50h、1h/1000h，All trend≤512，查询每序列≤600；最终内存预算和长度校验由 T03／T08实现。本轮没有历史分桶、序列化、UI 或真实数据注入。

查询不足：now=20、选择 1m 时保留请求窗口长度为 60 秒，save-local requestedRange 从 0 起，actualRange 是已记录的 20 秒；显示“已采集20秒／请求1分钟”，按20秒求速率。All 使用 recordingStart，不代指1000h。粗边界必须返回实际对齐范围并标 approximateBoundary，禁止偷偷分摊整数累计。空世界尚无有效区间时 NoCoverage；不将零宽区间作为有效桶。

## 8 本机验证记录

一个可运行检查：[ProductionStatsTypesCheck.cpp](../checks/ProductionStatsTypesCheck.cpp)。断言使用上面的真实类型／校验／数值辅助。测试中的资源字符串是确定输入样例，不证明对应游戏资源已成功加载；也没有创建假生产数据去通过 T01。

在仓库根目录执行：

```sh
clang++ -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined -I FactoryProductionStats/Source/FactoryProductionStats/Public checks/ProductionStatsTypesCheck.cpp -o /tmp/fps-t02-check
/tmp/fps-t02-check
```

执行机器：macOS ARM64，Apple clang 21.0.0（clang-2100.3.34.2，Xcode 27.0）。结果：`T02 canonical contract checks passed`，退出码 0；AddressSanitizer／UndefinedBehaviorSanitizer 未报错。检查包括固体／流体事件、电力快照、同一 Query/Bucket 约定的缺口、未知与真实零、非法时间／NaN／∞／负值、重复序列、权限／过期 epoch、int64 溢出及超过 double 精确整数范围、300件/60秒、120m³/60秒、停机计分母、100MW×60秒=6000MW·s=5/3MWh、储能类型隔离和600点限制。

Windows 的纯契约检查可在 VS 开发者终端运行（不要定义 NDEBUG）：

```bat
cl /nologo /std:c++20 /EHsc /W4 /I FactoryProductionStats\Source\FactoryProductionStats\Public checks\ProductionStatsTypesCheck.cpp /Fe:%TEMP%\fps-t02-check.exe /Fo:%TEMP%\fps-t02-check.obj
%TEMP%\fps-t02-check.exe
```

未运行：MSVC、UHT／Unreal 编译、fps.Probe、任何游戏计数、电力 UI 对照、世界销毁、资产加载、Alpakit。T02 草案可供 T03 独立核心算法开始；T01 实机成功语义、误差和网络边界确认后再冻结。
