# T03～T05 源码交付与验证记录

2026-09-30。用户明确要求先完成编码，UE 构建和实机验证后续在 Windows 进行；本轮使用 ponytail full，没有安装引擎、执行 UHT／Alpakit 或运行游戏。基线保持游戏 1.2.4.0／502094、SML v3.12.0／`1a7d2ca3a4281cf589bd842a814fd7a55eac4a99`。

| 任务 | 本轮交付 | 验证状态 |
| --- | --- | --- |
| T03 | 一个标准 C++20 History，数量累计、功率积分、能量末值、九窗口、分层历史、All、缺口和 DTO 校验恢复 | 实际源码在 Mac 编译／运行，ASan／UBSan 通过；UE 接线待验 |
| T04 | 原生生产作用域＋实际库存通知，Sink／SimpleProducer 候选、描述类缓存、世界注册与有界线程交接 | 交接／数量换算核心检查通过；UHT／hook 是否进入、成功语义、父子覆盖、原生精度未验证 |
| T05 | 原生电路注册表读取、逻辑网络计数、建筑组、独立电池充放电及带符号残差 | 汇总核心检查通过；实时字段是否 circuit-local、特殊负载、增益归属和采样相位未验证 |

## T03：时间和历史

实际源文件：`Public/ProductionStatsHistory.h`、`Private/ProductionStatsHistory.cpp`（以下 C++ 路径均相对于 `FactoryProductionStats/Source/FactoryProductionStats/`）。全程一份 History，由游戏线程拥有，不引用 Widget，也没有后台逐帧重算全历史。

- 正的实际数量事件暂存在当前时钟，归入下一段 `[clock,end)`；恰在查询 end 的数量不会进入该查询。固体使用 int64，超过 2^53 仍保持整数精确；溢出拒绝写入。流体使用 canonical m³ 的 double。
- `AdvanceTo` 只推进非负、有限、单调的仿真时间。同一时间不累积，乱序返回错误；离线时间不补。为保证 double 时间索引有效，上限为 2^52 秒（约一亿四千万年）。
- `SetQuantityCoverage` 保存类别观测状态。新物品继承该类别首次事件以前的观测时间并填已观测零，因此停机也计入速率分母。未知区间没有虚构零；已收到的正数量在故障时仍保留，零有效秒数意味着速率未知。
- 功率保存上一快照的值，按真实间隔积分 MW·s；使用窗口有效秒数得到 MW 均值。失效／缺失快照清除持有值。储能独立保存 MWh 和有效时点，降采样选末值；当前端点的新储能快照可以更新查询末值，不改变半开区间的数量／积分。
- `completeSources=false` 表示覆盖来源的小计，和采集时间缺口分别表达。压缩后的桶保留有效秒数及原因；丢失缺口的精确位置时，展示层必须标记整个部分桶，不能重新画成连续完整采集。

| 桶宽 | 固定容量 | 常用保留范围 |
| --- | --- | --- |
| 1s | 601 | 10m＋边界桶 |
| 10s | 361 | 1h＋边界桶 |
| 60s | 601 | 10h＋边界桶 |
| 600s | 301 | 50h＋边界桶 |
| 3600s | 1001 | 1000h＋边界桶 |

长时间跳跃最多更新每层固定容量，不按跨越秒数循环。查询只选一个层级，不叠加重叠层；选择满足保留期和点数约束的最细层，必要时按不重叠桶继续合并。每曲线最多 600 点。请求落在粗桶内时返回整个桶的 actualRange 和 approximateBoundary；例如 120 秒历史查询 end=113、1m、最多8点，实际范围为 `[50,120)`，70个单件事件，不伪造 `[53,113)` 的精确数量。

All 累计独立精确保存，不受 1000h 保留限制；趋势最多512点，超限成对合并，保留数量／积分／末值和覆盖。各序列 All 压缩边界可不同，历史 end 落在压缩桶内时使用该序列完整桶范围；QueryResult 给外包实际范围，明细仍可查看每个 summary.range。当前 end=clock 的 All summary 使用精确全生命周期累计。

保存往返目前是 **内存 DTO**：含类别覆盖、累计、五级桶、All、待归桶数量和持有功率。恢复先验证 schema、单位、时间、长度、槽位、连续 All、数量守恒及溢出，再替换活跃历史；失败保留原历史。schema=0 仍是未发行草案。实际游戏 SaveGame、编码、迁移、读档接线属于 T08。

### 容量测算和明确限制

本机内部 Cell 为56字节；500个活跃序列（含 ID／容器估算）分配为 **95,154,892字节，约90.75MiB**，低于核心的128MiB准入上限。运行中桶／All vector 容量不随小时数增长；序列超预算拒绝创建，并由适配器记录缺口。拒绝后已有历史保持。

DTO Bucket 为72字节。500条序列＋2条类别覆盖在所有桶／All均填满时，单是 Bucket 载荷的内存上界约116.44MiB，另有容器、ID等开销。**这不是实际编码后的存档体积**；T08尚未选择编码并测量文件。Export／Restore 为独立检查采用 DTO 和临时 History，峰值可能同时包含旧历史、新历史和DTO，明显超过128MiB；该峰值不计入上述90.75MiB。T08必须在新的世界／原子保存链路控制暂存（例如流式编码），T09再测全部历史、队列、缓存和存档峰值，当前不能宣称完整进程预算通过。

## T04：成功事实和生命周期

实际源文件：`Private/ProductionStatsHooks.*`、`Public/ProductionStatsCollectors.h`、`Private/ProductionStatsCollectors.cpp`、`Public/ProductionStatsSubsystem.h`、`Private/ProductionStatsSubsystem.cpp`。

由 Runtime 世界初始化通知调用 SML 的 `RegisterSubsystemActor`，每个权威游戏世界只持有一个 `AProductionStatsSubsystem`（SpawnOnServer，无复制）。BeginPlay 调用 Super；初始从 BuildableSubsystem 原生注册表发现机器，便携矿机另做一次 census。之后只处理新增／移除／spawn／destroy 通知，未初始化库存的 Actor 暂存到下次安全 PreFactoryTick。EndPlay 先关闭 Inbox，再解除绑定和 handler，旧 callback 即使持有 shared Inbox 也无法写入下一世界。UI 开关不参与采集。`fps.Stats` 控制台命令读取同一 History 的 All 数量和当前 power 快照，打印原生整数／canonical m³、MW／MWh、时点及覆盖，方便 Windows 对照；它不会推进时间或额外采集。

候选 hook 只设置当前线程的生产作用域并原样调用 Next 一次。数量来自库存 `OnItemsAdded(idx,num,source)`／`OnItemsRemoved(idx,num,item,target)` 的实际 num；同时要求作用域、正确输入／输出库存、方向和无搬运对端。配方推算、全局库存净变化和原生 single-cast delegate 的替换均未使用。嵌套父子作用域只有最内层消费一次库存通知；scope结束恢复父作用域，没有两层各加一遍数量。

| 来源 | 作用域／成功事实 | 待验证点 |
| --- | --- | --- |
| Manufacturer、VariablePower | TickProducing中的输出加入；ConsumeIngredients中的输入移除 | 是否全部输出发生于作用域内、Super关系、Converter的蓝图父类和专门路径 |
| ResourceExtractor（含采矿／泵／Fracking子类） | TickProducing中的输出加入 | 子类覆盖、流体精度、满库存、远处生产 |
| PortableMiner | TickProducing中的输出加入 | Tick线程／流送、生成时机和事件捕获 |
| GeneratorFuel、Nuclear | LoadFuel／LoadSupplemental 中的实际移除；TryProduceWaste 中的输出加入 | 燃料／配套资源缓存装载是否是不可逆投入；废料时机、失败／退款 |
| PowerBooster | LoadFuel中的移除 | 可选燃料及父子回调覆盖 |
| AWESOME Sink | CollectInput作用域内 AddPoints_ThreadSafe 返回true的单件 | 返回成功是否与实际销毁一一对应，不能把可积分资格误当销毁 |
| FactorySimpleProducer | 成功 GrabOutput 返回的实际单件 | 原生虚拟实现是否生成于抓取，蓝图特殊路径 |

库存通知本身混合搬运和生产；对端为空本身不够。上表接入点按固定声明实现，仍未见实际游戏函数体／调用链；**当前一律 completeSources=false／UnsupportedSource，不宣称全机器完整采集**。没有已证实的转换器父类时不能据候选名称宣称支持；Windows矩阵不通过时调整这层作用域，不改成理论值。

游戏线程缓存资源描述类的完整路径、Form，以及原生 `GetAmountConvertedByForm(1,Form)` 返回的单位量。worker在库存自身正在执行的通知内获取刚加入槽位的 class token，或使用移除参数中已复制的 item class token；元数据仅从线程安全缓存读取，不在worker调用 GetWorld／GetPathName／GetForm，不保存裸 UObject 到历史／Inbox。原生通知抑制和未调用基类的 override 仍可能漏采，须在Windows验证。

数量 Inbox 使用一个 mutex、最多32768条复制事实（另设16MiB载荷预算），提交时校验正数量／单位／有限值／ID。批量 drain 后由游戏线程以 History.Clock() 作为刚结束工厂区间的起点记数，再用上一次 PreFactoryTick 的 DeltaTime 推进；事件时间精度为工厂tick，非亚帧时间。队列溢出、未知描述类或History拒绝写入会标缺口，已收到的事实仍保存。未知描述类在安全线程刷新缓存后可覆盖未来事件，不补猜过去遗漏。锁竞争、队列分配、时间推进相位和真正暂停行为属于T01／T09实测。

## T05：去重与单位

实际源文件：`Private/ProductionStatsPowerReader.*`＋共用 Collectors／History。

每约1秒在安全 PreFactoryTick 从 `AFGCircuitSubsystem::mCircuits` 读取全部原生 power circuits，不扫描全世界Actor，不仅限 Factory 附属电网。`AccessTransformers.ini` 使用SML友元机制取得注册表和 `UFGPowerCircuit` 本地实时字段；不改SDK头文件。World指标读取 mPowerConsumed／mPowerProduced／mPowerProductionCapacity／mMaximumPowerConsumption，不将 GetStats 的一秒图表副本或 group ID 复制数据再次相加。

单次快照先按 circuit token去重；逻辑网络用有效group ID计数，无效group的独立circuit ID带不同标签，防止把它们当一个网。多个circuit同属group只计一个网络／异常网络；其数值暂按头文件注释的 circuit-local 字段汇总。**该字段范围仍须验证**，若游戏实际写入的是group总值，应在这一适配层改为每group一次读取，不能继续叠加。临时token和network ID均不进入History，合并／拆分仍写相同世界／建筑类型序列。

分类从每个circuit注册的PowerInfo读取 GetActualConsumption、GetBaseProduction、GetRegulatedDynamicProduction、GetActualProductionBoost；不把target demand当actual consumption。设备先按component token去重，再按owner去重类型数量。候选发电量为base＋regulated dynamic＋actual boost，boost单列保留，不再加到world native总值。库存／车辆等类型仍需确认实际归属，native世界总量减分类值作为带符号未分类／电网调整：负值保留，缺失值保持unknown。

电池按独立BatteryInfo去重，仅对当前接入的active电池求和。先把每台有符号MW拆成非负充电／放电，再各自汇总，避免5MW充电与8MW放电被净成3MW。储能及容量为MWh，与MW积分隔离；电池不算发电来源。断开的储能不在原生接入电网覆盖中，当前设备数指当前覆盖的设备，不是所有存档设备或历史平均数。

成功取得空注册表得到0个网络、0容量、0储能；取得失败则清空持有快照为unknown，不能沿用旧值或伪造零。已消失的已知类型在成功注册表快照中补0／设备数0；已知类型集合限制600条。所有读数仍保留 provisional 覆盖。电池、悬浮背包、铁路／车辆、电梯／Portal等特殊负载、开关、跳闸、增益和采样相位均按下表验收，当前无实机结果。

## 本轮实际检查

执行机器：macOS ARM64，Apple clang21.0.0（clang-2100.3.34.2，Xcode27.0）。命令从仓库根目录运行：

```sh
./checks/run.sh
```

脚本编译本仓库真实头文件／History／Collectors实现，使用 `-std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined`；交接检查另用 `-pthread`。可用 `CXX` 指定编译器；不使用另写的模拟统计算法，不依赖UE桩。临时二进制自动删除。

```text
T02 canonical contract checks passed
T03 history checks passed; 500-series allocated bytes=95154892; Bucket bytes=72
T04/T05 core handoff and power aggregation checks passed
```

三个退出码均0，ASan／UBSan无错误。覆盖：300件／60秒、120m³／60秒、停机时间作分母、100MW×60秒=6000MW·s=5/3MWh、不等长10秒100MW＋30秒200MW=175MW、储能端点、半开数量、暂停、缺口和新条目分母、九窗口／17点上限、跨400万秒＋3000次事件、All／DTO往返、损坏DTO保持旧历史、超过2^53整数和溢出、粗桶范围、500序列容量；另含两线程2000条事实累计6000件、满队列缺口和关闭拒绝、电路／component／BatteryInfo去重、switch group合并拆分、跳闸网络、独立充放电、负差额、缺失值和空网络。

这些检查证明的是本仓库算法，不证明游戏原生getter范围、hook调用时机、线程／生命周期或游戏精度。未执行MSVC、UHT、UE Editor／Shipping、Alpakit、游戏运行或性能验收。

## Windows待验场景

| 场景 | 必须取得的证据 |
| --- | --- |
| UHT／Editor／Shipping | 正确友元生成，无Unused transformer，所有hook签名及native模块链接通过；Editor下不采集 |
| 世界进入／退出／读档 | 只有一个源子系统，新增／移除／PortableMiner回调正确，重复世界无旧callback写入；历史随档恢复尚待T08 |
| 固体／流体、多输入输出、增产、包装／解包 | 逐次原生成功量对照、精度、方向及无父子双计；包括Converter蓝图真实父类 |
| 堵料、缺料、停电、配方更换、搬运／拆除 | 未发生生产不计数；换配方／退款不误计；传送带／管道／仓储移动不计 |
| 发电燃料／配套资源／核废料、Sink、特殊生产器 | 上表不可逆投入和返回成功与真实生成／销毁一致 |
| 原生电路字段范围 | 两个switch连接的circuit vs两个独立网络，字段到底local还是group；先解此项再宣称总量准确 |
| 特殊负载及增益 | 铁路／火车／悬浮背包／Portal／电梯实际消费被覆盖；增益归属和生产公式无双计；残差可解释 |
| 储能、跳闸、开关、暂停 | MW／MWh对照，5充＋8放不抵消，异常网络数与合并／拆分正确，暂停不计时间 |
| 远离玩家和规模 | 无流送漏采；10,000设备／500序列测锁竞争、worker＋GT成本、队列、内存和保存峰值 |

T03源码可供T06／T07接线。T04／T05完成了当前授权的编码，游戏验收继续挂起，T01实测／T02冻结／T00B加载门槛仍不能据此标通过。没有顺带制作UI、联网或游戏存档实现。

## 来源和索引

固定原生声明沿用 [API_EVIDENCE](API_EVIDENCE.md) 及 [API_SOURCES.tsv](API_SOURCES.tsv)。本轮另外核对同SHA的 [SubsystemActorManager.cpp](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/1a7d2ca3a4281cf589bd842a814fd7a55eac4a99/Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp)：真实SML实现会对class去重并按ReplicationPolicy生成／取得actor；这不是FactoryGame生成桩。其SHA256另附于API_SOURCES，外部文件只保留在被忽略的依赖目录。

[Native Hooking](https://docs.ficsit.app/satisfactory-modding/latest/Development/Cpp/hooking.html)、[Access Transformers](https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/AccessTransformers.html)、[SML Subsystems](https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/Subsystems.html) 支持机制选择；[Epic FWorldDelegates](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FWorldDelegates) 说明全局世界初始化通知，不代替定制5.6.1-CSS构建证据。

按用户新增授权执行 `codegraph init --yes /Users/dev/projects/Mods` 成功，随后增量sync成功；源码查询优先codegraph、固定依赖回退rtk。索引位于 `.codegraph/`，不推送，也不把 codegraph 的索引解析当C++编译证明。
