# Factory Production Stats（幸福工厂生产统计 Mod）

目标：在《幸福工厂》中按 **P** 打开生产统计窗口，查看物品、流体和电力的当前状态及历史趋势。顶部按 `items.png` 排列为时间范围按钮、标签按钮；具体页面分别参考工程内的物品、流体和电力图片。

发布形态：标准 SML Mod，经 Alpakit 打包，支持通过 ficsit.app 的 Satisfactory Mod Manager（SMM）管理安装、启用、禁用、升级和卸载。

源码托管：[guopeng1994/satisfactory-production-stats](https://github.com/guopeng1994/satisfactory-production-stats)，自有内容使用 [0BSD](LICENSE)，允许免费使用、复制、修改、分发和商用，不要求署名。游戏、引擎和其他第三方依赖按各自许可获取。

已完成 T00A～T02 的资料／源码准备、T03 历史核心、T04／T05 采集适配，以及 T06～T08 的输入／界面／随档保存源码。此前 T02～T05 核心 C++20 检查在 Mac 通过；**T06～T08 及本轮修改尚未编译或运行**，按用户安排留到 Windows。真实输入／Game Feature 资产、UE 构建、游戏对照和可安装包仍待完成；T01 尚未完整验收，T02 尚未冻结。更新日期：2026 年 9 月 30 日。

| 文档 | 用途 |
| --- | --- |
| [产品与技术规划](docs/PLAN.md) | 需求、图片解读、统计口径、架构、边界和验收 |
| [Agent 执行任务](docs/TASKS.md) | 任务依赖、输入输出、负责范围、完成标准及可复制提示词 |
| [环境与构建基线](docs/ENVIRONMENT.md) | 已核对版本、精确工程提交、依赖取得路径、待执行命令与环境缺项 |
| [Windows 接续步骤](docs/WINDOWS_SETUP.md) | 接入源码、在 Editor 生成真实资产、构建、打包与加载验收 |
| [API 证据与契约](docs/API_EVIDENCE.md) | T01 固定版本声明、覆盖缺口、诊断步骤、T02 类型与检查命令 |
| [T03～T05 交付证据](docs/IMPLEMENTATION.md) | 历史算法、采集接线、容量、此前检查和 Windows 待验场景 |
| [T06～T08 交付记录](docs/UI_SAVE_IMPLEMENTATION.md) | 原生界面、输入资产脚本、字节保存、检查代码及待验边界 |

用户已授权先完成编码，将 UE 构建和实机验证留到 Windows。T03～T05 源码已接线到一个 SML 世界子系统；采集数值保留未验证覆盖标记。T06～T08 已接入同一查询和游戏保存回调；输入资产生成及真实功能验收仍需 Windows。

后续允许编译时，在 Mac／支持 clang 的环境执行 `./checks/run.sh` 可编译并运行本仓库实际标准 C++ 实现。新增 T07／T08 检查已接入，但本轮未执行；Windows MSVC 命令见接续文档。

已确认先完成单人版，再支持联机；目标为游戏和 SML 的最新版，按最新稳定发布理解。目前没有可用于编译和实机验证的 Windows 电脑。具体发布版本与环境门槛见规划文档；T00 分为现在可完成的资料基线和需要配套环境的构建验收。

执行规则：每个任务使用 **ponytail full**；查找代码优先 codegraph，其次 rtk。实现中先复用游戏／SML／Unreal 已有机制，再增加本 Mod 必要的代码。

本地 codegraph 已按用户授权初始化并同步，索引放在被忽略的 `.codegraph/`。新 clone 可以执行 `codegraph init --yes .`，修改源码后用 `codegraph sync .` 更新；固定版本外部资料位于被忽略的 `_dependencies/`，不纳入公开仓库或源码索引。
