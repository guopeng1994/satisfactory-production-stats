# Factory Production Stats（幸福工厂生产统计 Mod）

目标：在《幸福工厂》中按 **P** 打开生产统计窗口，查看物品、流体和电力的当前状态及历史趋势。顶部按 `items.png` 排列为时间范围按钮、标签按钮；具体页面分别参考工程内的物品、流体和电力图片。

发布形态：标准 SML Mod，经 Alpakit 打包，支持通过 ficsit.app 的 Satisfactory Mod Manager（SMM）管理安装、启用、禁用、升级和卸载。

源码托管：[guopeng1994/satisfactory-production-stats](https://github.com/guopeng1994/satisfactory-production-stats)，自有内容使用 [0BSD](LICENSE)，允许免费使用、复制、修改、分发和商用，不要求署名。游戏、引擎和其他第三方依赖按各自许可获取。

已完成 T00A 环境资料基线和 T00B 插件源码准备：真实插件描述、C++ Runtime 启动／退出日志及构建规则已就绪。**尚无统计功能、真实 Game Feature／根世界资产或可安装游戏包**；配套工程、编译、打包和游戏验收由用户后续在 Windows 执行。更新日期：2026 年 9 月 30 日。

| 文档 | 用途 |
| --- | --- |
| [产品与技术规划](docs/PLAN.md) | 需求、图片解读、统计口径、架构、边界和验收 |
| [Agent 执行任务](docs/TASKS.md) | 任务依赖、输入输出、负责范围、完成标准及可复制提示词 |
| [环境与构建基线](docs/ENVIRONMENT.md) | 已核对版本、精确工程提交、依赖取得路径、待执行命令与环境缺项 |
| [Windows 接续步骤](docs/WINDOWS_SETUP.md) | 接入源码、在 Editor 生成真实资产、构建、打包与加载验收 |

后续先完成 **T00B 构建与空插件加载**；现在可开展 **T01 静态调查**。完成真实数据可行性验证后再进入正式实现。不要跳过采集验证直接制作完整界面。

已确认先完成单人版，再支持联机；目标为游戏和 SML 的最新版，按最新稳定发布理解。目前没有可用于编译和实机验证的 Windows 电脑。具体发布版本与环境门槛见规划文档；T00 分为现在可完成的资料基线和需要配套环境的构建验收。

执行规则：每个任务使用 **ponytail full**；查找代码优先 codegraph，其次 rtk。实现中先复用游戏／SML／Unreal 已有机制，再增加本 Mod 必要的代码。
