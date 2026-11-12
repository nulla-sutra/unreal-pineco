# Pineco Context

本文件集中维护 Pineco 的统一术语、职责和长期语义。现行源码以本文前半部分为准；迁移前的实现说明和运行证据保留在下方历史记录中，不作为已删除类型的兼容承诺。

## 现行职责与模块

Pineco 是原生 Gameplay Targeting System 的动画事件与 GAS 规则扩展。原生 `UTargetingPreset` 保存按顺序执行的 Task，原生 Subsystem／AbilityTask 管理请求、上下文、结果和生命周期。形状查询、Actor 去重、距离排序和调试绘制复用引擎实现；Pineco 不再提供独立的 Trace、请求系统或执行器。

- `PinecoAnimation` 仅包含 `UAnimNotify_PinecoTargeting`，把动画时机转换为携带原生 Preset 的本地 GameplayEvent。
- `PinecoGAS` 仅包含 `UPinecoTargetingFilterTask_GAS` 与 `UPinecoTargetingFilterTask_Limit`，补充原生插件缺少的 ASC 资格过滤和数量上限。
- 项目 Combo GA 负责原生 `WaitGameplayEvent` 监听、权限与动画上下文校验、原生 Targeting AbilityTask 和 GE 结算；Emboar 继续只负责连招推进与播放。
- 其他触发机制直接使用原生 Targeting API，无须构造 AnimNotify 或伪造动画事件。Preset／Task 配置是共享资产，不在每次请求中改写；运行状态由原生请求持有。

插件名仍为 `Pineco`，两个保留模块仍为 `PinecoAnimation`／`PinecoGAS`；`PinecoTargeting` 是 Notify 的业务名称。本次没有重命名插件目录、描述文件或保留模块。

## UAnimNotify_PinecoTargeting

类型位于 `Source/PinecoAnimation/Notifies/AnimNotify_PinecoTargeting.h/.cpp`；C++ include 为 `PinecoAnimation/Notifies/AnimNotify_PinecoTargeting.h`。编辑器显示名为 `Pineco Targeting`，只配置两个字段：

| 字段 | 类型／默认值 | 语义 |
| --- | --- | --- |
| `EventTag` | 原生 `FGameplayTag`，默认无效 | 由宿主 GA 使用原生 `WaitGameplayEvent` 监听。插件不硬编码攻击 Tag。 |
| `TargetingPreset` | `TObjectPtr<UTargetingPreset>`，默认空 | 携带共享的原生目标选择配置；形状、对象类型与数量上限均在 Preset 的 Task 实例中配置。 |

Notify 覆写原生 `Notify(USkeletalMeshComponent*, UAnimSequenceBase*, const FAnimNotifyEventReference&)` 和 `GetNotifyName_Implementation()`。只在游戏世界、有有效 Owner／来源动画／Tag／Preset／Owner ASC 时，通过原生 `SendGameplayEventToActor` 发送本地事件；无效配置不发送，不提供默认 Preset 或旧 Trace 回退。编辑器预览触发默认关闭。

| GameplayEvent 字段 | 内容 |
| --- | --- |
| `EventTag` | Notify 的 `EventTag` |
| `Instigator`／`Target` | Mesh Owner |
| `OptionalObject` | 原生 `UTargetingPreset`，不再是 Notify 自身 |
| `OptionalObject2` | 触发该 Notify 的来源动画 |

预测端和权威端各自接收本地动画事件，不在 Notify 内查询目标、发送 RPC、处理网络预测或施加 GE。Notify 不持有单次请求／角色／播放的运行状态，也不修改 Preset／Task。消费方从 `OptionalObject` 解析 Preset，在权威分支执行原生 Targeting；需要的当前 Montage 校验由项目 GA 完成。

## 原生 Preset 与两个补充 Task

预期管线为 `原生 AOE → GAS Filter → 原生 SortByDistance → Limit`。原生 `UTargetingSelectionTask_AOE` 提供 Box／Cylinder／Sphere／Capsule／SourceComponent 及对象类型、偏移、旋转和忽略来源等配置；不再经过 `FInstancedStruct` 或结构自己的 `Evaluate`。结果按原生语义消费，Actor 位置不能解释为真实接触点。

`UPinecoTargetingFilterTask_GAS` 继承原生 `UTargetingFilterTask_BasicFilterTemplate`，通过构造函数转交 `FObjectInitializer`，只覆写 `bool ShouldFilterTarget(const FTargetingRequestHandle&, const FTargetingDefaultResultData&) const`。返回 `true` 表示排除；拒绝无效请求／来源／世界、无效目标、自身、其他世界及缺少有效同世界 ASC 的目标。来源读取原生 `FTargetingSourceContext`，遍历、结果移除及完成状态由原生基类管理。

ASC 过滤不调用 `FGameplayTargetDataFilterHandle`，不承担去重、排序、数量限制、效果或权限判断。原生 AOE 已按 Actor 去重；过滤模板的 `RemoveAtSwap` 可改变候选顺序，不承诺旧实现的接受顺序或等距平局结果。敌我、死亡和标签规则尚未实现。

`UPinecoTargetingFilterTask_Limit : UTargetingTask` 保留 `void Execute(const FTargetingRequestHandle&) const`，只读取原生 Preset 内嵌 Task 实例上的可编辑 `int32 MaxTargets = 0`。`0` 不限；正数保留原生 `FTargetingDefaultResultsSet.TargetResults` 的前 N 项，保持 HitResult、Score 和现有顺序；负数清空结果。字段通过 `ClampMin/UIMin = 0` 约束编辑值。有效空结果及缺少结果集正常结束 Task；非游戏线程或无效句柄不访问结果。

Limit 不检查来源／ASC／权限，不重做过滤、去重或排序，不提交 GE。两个 Task 不依赖 Pineco 请求状态，不在 Task／CDO 保存单次状态；遵循原生管线的游戏线程执行约定，Limit 在即时或异步请求到达该阶段时同步完成截断，不持有跨帧状态。

宿主 quan1 已保存为原生 `/Game/AsuraNext/Avatar/Player/Roles/Linghou/Combo/TP_Linghou_Quan1`：Box `HalfExtent = (75,70,100)`、相对来源偏移 `(75,0,0)`、Pawn 对象类型、忽略来源／Instigator；后接 ASC Filter、原生 Actor 中心距离升序／稳定排序、Limit 实例的 `MaxTargets = 3`。旧无引用 `TP_Box` 已通过原生非强制删除入口清理。

项目 `GA_Linghou_Combo` 已使用原生 `UAbilityTask_PerformTargeting::PerformTargetingRequest`，quan1 的 `bAllowAsync = false`。在 `OnTargetReady` 回调内通过原生 Subsystem 的 `GetTargetingResultsActors` 读取 Actor 数组，经原生 `AbilityTargetDataFromActorArray` 转换 TargetData，再由现有 GE 节点逐目标结算；原生完成释放后不再借用请求结果。其他触发方式直接选用原生 Subsystem 或 Async Action，不添加新的 Pineco 请求包装器。

## 依赖、命名迁移与调试

`Pineco.uplugin` 只声明两个运行模块，显式依赖 `GameplayAbilities` 和 `TargetingSystem` 插件。`PinecoAnimation` 公开依赖 `GameplayTags`／`TargetingSystem`，私有依赖 `GameplayAbilities`；`PinecoGAS` 公开依赖 `TargetingSystem`，私有依赖 `GameplayAbilities`。两个模块不再互相依赖，均显式设置 `PCHUsageMode.Default` 和 `bLegacyParentIncludePaths = true`。引擎源码未修改。

`Config/DefaultPineco.ini` 使用原生 ClassRedirect，将 `/Script/PinecoAnimation.AnimNotify_PinecoTrace` 映射为 `/Script/PinecoAnimation.AnimNotify_PinecoTargeting`。这只保留 Notify 的类身份；旧形状数据不会自动变成 Preset，也不为已删除的函数创建签名不匹配的重定向。

自定义 `pineco.Trace.DebugDraw`、`PinecoTrace::Debug` 与自定义绘制实现均已删除。`/Game/ConsoleVariables/Gameplay` 已移除旧开关，保留并勾选原生 `ts.debug.EnableTargetingDebugging = 1`、`ts.debug.OverrideTargetingLifeTime = 1.25` 及同进程 PIE 的 `ShowFlag.ServerDrawDebug = 1`，其他条目保留原值、顺序和勾选状态，共 16 项。项目 `Config/DefaultEngine.ini` 的 `[ConsoleVariables]` 已登记同样的三个启动值，落实默认开启约定；当前运行值已核对，配置写入后未再次重启，启动加载未单独复验。

Box 轮廓由原生 AOE Task 绘制为红色，使用与查询相同的来源、偏移、旋转和碰撞形状；Pineco 没有另写绘制或调试 RPC。专服绘制到客户端的转发只在本次同进程 PIE 下验证，不代表独立进程／打包服务器会网络复制调试线。

## 资产迁移与定点双客户端验证（2026-11-13）

此前 C++ 收缩后源码只保留三个业务类型、两个模块；整个 `Source/PinecoTrace`、旧 RunTrace AbilityTask、事件／通用请求入口、自定义 Selection／Distance 和请求状态已删除。用户完整编译重启后，本轮确认加载新 Notify 和两个 Task，只编辑宿主资产、调试默认配置及本文，不执行 C++ 编译、Live Coding 或编辑器重启。

`AM_Linghou_Combo/A011` 的 0.3 秒标记已为 `UAnimNotify_PinecoTargeting`，`EventTag = Event.Attack`，绑定上述正式 Preset。47 条 Notify 的类型、时间、轨道、持续时间均保持原状，只配置攻击标记的两个字段；其余音效、Combo Window 和 Motion Warping 标记保留。

项目 GA 保留原生 `WaitGameplayEvent`、Commit／取消／结束生命周期。权威分支解析 Preset，校验事件来源／目标为 Avatar 且来源动画等于有效的当前 Montage，然后执行原生 Preset 和 GE。已删除 `EvaluateTraceEvent` 旧节点和 HitResult 循环转换分支。EventGraph 共 51 节点、8 个中文职责注释区，节点按网格排列，无同坐标节点、注释框无重叠；数据节点和 GE 输入按依赖顺序整理。最终蓝图编译 0 错误／0 警告并保存；`ResolveComboTargetLocation` 的 47 节点及数据与迁移前逐项一致。

用户先结束外部 Footsteps PIE 后，本轮由原生 MCP 启动专服 `Context_10` 和客户端 `Context_11/12`，PlayerId 260／261。沿用用户正式快捷栏验证授权，通过官方 `WoolooPIE.InjectInputAction` 对明确的 `Context_12`、本地玩家 0 和对应 Controller／Pawn 提交按下／释放；不新增 IA／IMC／Debug Key，不通过 Python 调用技能、GameplayEvent 或 RPC。每次输入均另以预测端／服务端日志和实际结果核验，服务器结果依次为 `0、3、3、3`。

首拳站位未命中，随后只在 PIE 中临时移动原有四个假人。原生只读 `BoxOverlapActors` 确认四个有效候选；来源 `(-2150,-1100,100)`、Yaw 0，Box 中心 `(-2075,-1100,100)`、半尺寸 `(75,70,100)`。远处两只假人的中心在前脸外，其半径 50 cm 胶囊与查询体重叠 5 cm，按碰撞体相交接受。另一玩家位于范围外。

| 开关用例 | 三端一致的 Health，按 `C_0/C_1/C_2/C_3` 顺序 | 本次扣血目标 |
| --- | --- | --- |
| 初始 | `100/100/100/100` | — |
| 开启 | `90/90/100/90` | 3 个，各扣 10 |
| 关闭 | `80/80/100/80` | 3 个，各扣 10 |
| 重新开启 | `70/70/100/70` | 3 个，各扣 10 |

三段采样窗口共 48.922 秒、1554 次采样；MaxHealth 均为 100。两客户端共捕获 18 次受击播放，每次只出现一个 Section，覆盖 `Hit_01/02/03`，观测位置均位于所选段范围内，采样到的活动窗口为约 0.593～0.625 秒后结束，没有连续播放三段。随机选择仍可在两客户端不同，Health 由服务器统一。

实际查看两客户端明确世界的原生 SceneCapture 渲染：关闭时均无 Box，重新开启时两端具有一致的红色 Box 投影和位置。为截取画面，运行时绘制寿命临时设为 10 秒，完成后恢复 1.25 秒；曝光调整仅用于临时捕获组件。四张通过截图位于 `Saved/Screenshots/WindowsEditor/PinecoPreset13/`，文件为 `Off_Client1.png`、`Off_Client2.png`、`Reenabled_Client1.png` 和 `Reenabled_Client2.png`。跨客户端有歧义的全局 HighResShot 与曝光准备图不作证据，已删除。

日志观察到 6 条 PIE 启动阶段的 NetworkPrediction 回滚提示；临时移动假人产生 4 条 Mover 带外位置警告，未修改 Mover 或推断这已完成网络稳定性验收。四次攻击窗口未见事件拒绝、Blueprint Runtime Error 或 Deep PredictionKey Chain。其他工具诊断和验证边界保留在报告中。

完成后本轮 PIE 已停止，原生 `Worlds=[]`；回调已注销，两个临时捕获组件销毁、渲染目标释放，公开 Python 任务变量清理。地图无未保存状态，临时摆位未写盘，其他未保存资产保持原状。报告追加到 `Saved/Automation/TrainingDummyGAS/verification.json` 的 `native_preset_migration_verification`，保留原有 20 个根字段；未新增持久 Python 脚本。该验证只覆盖当前同进程固定站位 Box 功能，不覆盖延迟／丢包、其他形状、32 玩家负载或性能目标。

## 迁移前职责与结果（历史记录）

以下内容保留此前的设计演进和运行证据；其中“当前”“已实现”均指对应历史版本，不描述上方精简后的现行源码。

- **Targeting Preset / 目标选择配置**：复用原生 `UTargetingPreset`；由原生选择、过滤和排序任务组成。首轮 Box 用例使用 Gameplay Targeting System；当前 quan1 改为统一 Notify 配置及结构自己的原生 Overlap 算法，不再经过 `TP_Box`。
- **Targeting Request / 目标选择请求**：复用原生 `FTargetingRequestHandle` 和 `FTargetingSourceContext`。请求句柄只代表本地执行，不充当网络身份或共享命中历史。
- **Trace / 候选提供者**：同步产生候选，既可执行原生物理查询，也可消费已有碰撞结果或目标选择结果。
- **Rule / 候选规则**：无副作用地接受或拒绝候选；拒绝不消耗命中次数，不在规则中提交 GameplayEffect。
- **TraceExecution / 判定执行**：管理本次求值的上下文、时间区间、候选处理和终态。当前 API v1 的命中记录仍位于执行内部。
- **HitGroup / 命中组（规划）**：保存一组参与执行共同使用的接受历史。共享哪个组决定去重范围，组的策略决定按什么身份计数、允许几次及何时再次接受。
- **HitPolicy / 命中策略**：不持有对象引用或运行历史的配置值；供命中组创建时复制。修改外部配置不应改变已运行组的规则。
- **TargetActor / 运行宿主（规划）**：继承原生 `AGameplayAbilityTargetActor`，管理本次攻击的时间、输入、运动、判定调度、子单元和结束条件。
- **单次 Montage Notify**：只标记本次播放中的激活时机；宿主的有效期和结束条件独立配置。
- **玩法消费方**：解释判定结果，负责权威效果提交及其幂等。命中组的接受记录不证明 GameplayEffect 已成功施加。

核心结果主载荷为原生 `TArray<FHitResult>`。每条结果只保证其来源实际提供的身份和几何字段；Overlap 或纯目标选择没有提供的接触信息不得虚构。GAS 适配层负责转换为原生 TargetData，通用核心保持独立于 GAS、StateTree、Mutable 和具体运动来源。

当前源码由原生 Targeting Selection 适配 Task 调用 `FPinecoBoxTraceConfig::Evaluate` 执行区域 Overlap，再由后续 Task 完成 GAS 过滤、Actor 去重、距离评分和数量限制；不沿武器或 Socket 轨迹采样。命中表示目标碰撞体与查询体重叠，不要求 Actor 原点落入 Box，也不把 Actor 位置作为真实接触点。`PinecoTrace` 的配置与算法独立于动画和 GAS。2026-11-12 用户完整编译并重启后，已通过定点双客户端的目标上限、扣血、单段随机受击、Health 同步及 Box 绘制开关验证；另观察到每次受控攻击一条 NetworkPrediction 回滚警告，原因未定位，详见下方运行记录。

首轮最小 GYM 用例通过原生 `UAbilityTask_PerformTargeting::PerformTargetingRequest` 发起同步请求，结果在 `OnTargetReady` 内读取。该段仅说明历史实现；当前 quan1 不使用这个 AbilityTask，新的通用入口在内部管理原生 Targeting Request 句柄。

## Box GYM 用例

测试专用资产目录为 `/Game/Toilet/GYM/PinecoBox`。专用 GameMode 沿用公共玩家、物品与角色装配，已移除旧 `BPC_GymPinecoBoxSetup`。当前假人是宿主 Avatar 下的正式角色 `/Game/AsuraNext/Avatar/NPC/TrainingDummy/PCharacter_TrainingDummy`；GYM 与 `TP_Box` 引用该类，Nickit 持有其可复用美术资源。

当前 Box 由正式拳招 quan1 的播放触发：`ST_Linghou_Combo` 的 A011 第一式播放 `AM_Linghou_Combo/A011`，0.3 秒处唯一的 `UAnimNotify_PinecoTrace` 发送本地 `Event.Attack`。项目 `GA_Linghou_Combo` 通过原生 `WaitGameplayEvent` 精确监听，各端接收自己的动画事件；仅权威端调用 `EvaluateTraceEvent`，逐个原生 HitResult 转 TargetData，再通过 `GE_Damage` 向目标 ASC 结算。Emboar 继续只负责连招推进与播放。旧 `AN_AttackBoxEvent` 标记与 GA 的 `TP_Box` 接线已移除，旧资源本轮未删除。

- Notify 的 `TraceConfig` 为原生 `FInstancedStruct`，类型 `FPinecoBoxTraceConfig`；`HalfExtent = (75, 70, 100)`、`Offset = (75, 0, 0)`、`Rotation = (0, 0, 0)`，单位 cm／角度。
- 全尺寸 150 × 140 × 200 cm，局部前方范围 0～150 cm，左右各 70 cm、上下各 100 cm；按拳1动画收窄，不声称已精确还原原游戏表中 AreaAttr 的未明语义。
- 来源为权威 Avatar 的当前位置与朝向，忽略自身；对象类型为 Pawn，接受同世界、具有 ASC 且通过原生 TargetFilter 的目标。
- `MaxTargets = 3`，过滤、Actor 去重后取最近三个；每次查询内每个 Actor 最多结算一次，不是跨 Notify 的共享命中记录。
- `GE_Damage` 每次扣 10 Health，由 GE 执行受击 Cue；客户端表现保持三个受击 Section 随机播放其中一个。
- 沿用共享 `GYM_Lighting`、现有 GYM 材质及 Framework 的 GameMode／Setup 生命周期。

按用户明确要求，正式 quan1 输入是本用例入口，不增加专用测试按键或 Debug Key。旧 IA／IMC、F 入口、独立 Box GA 和授予它的 Setup 已清理；地图仍为 Single 1 个与 Multiple 3 个目标。首轮大 Box 的边界结果保留于历史记录，不作为本次缩小范围的验收证据。源码已将 Box 接回原生 Targeting Task 管线；查询轮廓仍由形状算法通过下述 `pineco.Trace.DebugDraw` 控制，`ts.debug.EnableTargetingDebugging` 控制原生 Task 诊断，不代替 Box 轮廓绘制。本轮不更改既有绘制 CVar 或 Gameplay preset。

## FPinecoTraceConfig

类型位于 `Source/PinecoTrace/Trace/PinecoTraceConfig.h`，属于 `PinecoTrace` 模块；C++ 消费方使用 `#include "PinecoTrace/Trace/PinecoTraceConfig.h"`。

这是各形状共享的反射配置与原生算法接口，用于后续原生 `FInstancedStruct` 的类型限制、局部坐标配置和多态求值。配置不持有角色引用、请求句柄或命中历史；运行状态归执行实例所有。

| 字段 | 默认值 | 统一语义 |
| --- | --- | --- |
| `Offset` | `(0, 0, 0)` | 相对来源角色局部坐标的偏移，单位 cm。 |
| `Rotation` | `(0, 0, 0)` | 相对来源角色朝向的旋转，使用原生 `FRotator` 的角度约定。 |

原生算法入口为 `virtual bool Evaluate(const UWorld& World, const FTransform& SourceTransform, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& QueryParams, TArray<FHitResult>& OutResults) const`。具体结构覆写自己的算法，通用调用方通过 `FInstancedStruct::GetPtr<FPinecoTraceConfig>()` 取得基类指针后做虚调用，无需识别 Box 等具体类型。

- 同步、仅游戏线程求值；配置只读，World 与结果缓冲由调用方保证本次调用期间有效，不转移所有权。
- 返回 `true` 表示求值完成，包括未命中的空结果；返回 `false` 表示配置或求值上下文无效。游戏线程失败会清空输出；非游戏线程拒绝调用且不触碰输出缓冲。
- 来源变换只提供世界位置和朝向；偏移沿来源朝向旋转，查询朝向组合局部 `Rotation`，尺寸保持 cm，不随来源缩放伸缩。
- 碰撞对象掩码和原生查询参数由调用方传入；忽略自身等规则通过 `QueryParams` 配置。此接口仅产生候选，去重、目标上限和 GAS 结算归消费方或执行层。
- 基类具有虚析构函数，并以默认返回失败代替纯虚函数，保证 UE 反射能够构造基类；统一 Notify 的 `FInstancedStruct` 选择器排除基类本身。

AnimNotify 是一种触发适配，不是 Trace 的通用运行前提。`PinecoTrace` 的形状参数和 `Evaluate` 不依赖动画、GameplayEvent 或 GAS；未来其他触发机制可提供来源变换与查询参数，调用同一多态算法。当前 `PinecoGAS` 事件适配内部的配置求值、过滤与排序只接收通用参数，不把 Notify 类型传进算法；这不代表其他触发适配或新的通用公开执行入口已经实现。

已确认的接入方向是单一 `UAnimNotify_PinecoTrace` 持有 `FInstancedStruct` 形状配置，具体 Box 等参数由派生结构提供；项目 Combo GA 监听事件并在服务端发起判定与 GAS 结算，Emboar 继续只负责连招推进与播放。Notify 仅保存只读配置，不保存角色之间或播放之间共享的运行状态。

2026-11-12：基础配置、多态 `Evaluate`、Box、统一 Notify 与 GAS 事件执行适配的 C++ 已由用户完整编译并重启。编辑器已确认加载原生 Notify 类及 `EvaluateTraceEvent` 节点；quan1／项目 GA 已迁移到上述统一链路，GA 蓝图编译通过。运行结果另列于本次验证记录；代理本轮未执行 C++ 构建或 Live Coding。

## FPinecoBoxTraceConfig

类型位于 `Source/PinecoTrace/Trace/PinecoBoxTraceConfig.h`，属于 `PinecoTrace` 模块，继承 `FPinecoTraceConfig` 的 `Offset` 与 `Rotation`；C++ 消费方使用 `#include "PinecoTrace/Trace/PinecoBoxTraceConfig.h"`。

| 字段 | 默认值 | 统一语义 |
| --- | --- | --- |
| `HalfExtent` | `(50, 50, 50)` | 三轴半尺寸，单位 cm；默认全尺寸为 100 × 100 × 100 cm。每个分量须有限且大于零。 |

编辑器 `ClampMin` 提供正尺寸的制作约束；Box 自己的 `Evaluate` 同时校验运行时参数：必须具有物理场景、有效来源变换、有限偏移与旋转、非空且合法的对象类型掩码，以及可表示为原生碰撞形状浮点值的正半尺寸。无效输入返回 `false`，不发起物理查询。

Box 通过原生 `UWorld::OverlapMultiByObjectType` 与 `FCollisionShape::MakeBox` 执行自身算法，再复用 `UPinecoTraceLibrary::MakeOverlapResult` 输出 `TArray<FHitResult>`。保留原生重叠身份信息，不虚构接触点、法线或路径时间，也不在形状结构内做 Actor 去重、数量截断或伤害提交。

quan1 当前使用 `Offset = (75, 0, 0)`、`Rotation = (0, 0, 0)`、`HalfExtent = (75, 70, 100)`，表达角色局部前方 0～150 cm、左右各 70 cm、上下各 100 cm 的 Box。

2026-11-12：反射 Box 配置、默认值与自身的 `Evaluate` 已实现并随用户的完整构建加载；`PhysicsCore` 为私有依赖。统一 Notify 提供 `FInstancedStruct` 配置，项目 GA 通过 GAS 事件适配调用基类虚函数，无需在 GA 或 Notify 中识别具体形状。

## Trace 调试绘制

`PinecoTrace` 模块注册整数 CVar `pineco.Trace.DebugDraw`，默认 `1`；`0` 停止新的绘制，非零开启。注册、读取函数、绘制依赖与调用均受 `UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT` 编译隔离，Test／Shipping 不包含此功能。开关由模块内部 `Debug/PinecoTraceDebug.h/.cpp` 管理，各形状仍负责自己的几何与算法，不新增反射字段或改动 `Evaluate` 接口。

当前 Box 在有效原生 Overlap 查询后，使用同一 `QueryLocation`、归一化 `QueryRotation` 和实际 float 碰撞半尺寸调用原生 `DrawDebugBox`。查询体为青色、线宽 1.5、保留 1.25 秒，未命中也显示；配置／上下文无效且未执行查询时不绘制。关闭后已有线条自然到期，不清除其他系统的调试线。青框仅表示查询体，不表示经过 ASC／TargetFilter、Actor 去重和 MaxTargets 截断后的最终命中，也不虚构接触位置。

原生 `DrawDebugBox` 在编辑器中支持把 Dedicated Server 的绘制转到同进程 PIE 客户端视口，要求相应视口的 `ServerDrawDebug` ShowFlag 开启。此处直接复用引擎实现，不新增跨世界遍历、客户端判定或调试 RPC；独立进程／打包专用服务器不具备这条编辑器转发路径。

`/Game/ConsoleVariables/Gameplay` 已新增并勾选以下两个条目，其他既有条目保持原值、顺序和勾选状态：

```text
pineco.Trace.DebugDraw 1
ShowFlag.ServerDrawDebug 1
```

`r.EnableDrawDebugHelpers` 是原生绘制总开关，仍遵循其现有设置（引擎默认开启）。仅关闭本模块的新绘制可使用 `pineco.Trace.DebugDraw 0`。

2026-11-12：源码及 Gameplay preset 已完成并保存，preset 回读为 17 项、原有 15 项不变。用户随后完整编译重启，PinecoTrace DLL 时间为 06:27:26；运行时确认 `pineco.Trace.DebugDraw=1` 的来源为 Constructor。代理未执行 C++ 编译、Live Coding 或编辑器重启。

新增绘制的复验分为两次同进程 Dedicated Server＋双客户端 PIE：06:33:10.835～06:44:17.451（约 666.616 秒，Context_1／2／3）及 06:46:52.021～06:54:04.897（约 432.876 秒，Context_4／5／6）。首次会话由外部操作结束后，重新发现第二轮 Controller／Pawn／世界身份，没有复用旧世界地址。

通过 Rider 原生 MCP 的一次性诊断断点，在物理查询和绘制调用完成后读取服务端求值帧及两个明确客户端世界的公开 `GetLineBatcher(WorldPersistent)->BatchedLines`。来源位置 `(-2150,-1100,100)`、朝向零，实际 `QueryLocation=(-2075,-1100,100)`。两端各有 12 条青色边，全部顶点逐项一致，范围 X=`[-2150,-2000]`、Y=`[-1170,-1030]`、Z=`[0,200]`，得出全尺寸 `150×140×200 cm`、中心 `(-2075,-1100,100)`；线宽 1.5、初始寿命 1.25 秒。与同一查询复用的中心、朝向和 float 半尺寸一致。此时 Overlap 为空，证明未命中的有效查询同样绘制。优化后的 `this` 不可读，没有将其冒称为运行时属性回读。

第二轮以固定站位执行 `pineco.Trace.DebugDraw` 的 `1→0→1` 对照，各次 quan1 仍从原生正式快捷栏入口触发。分别以两个客户端本地相机的位置、朝向、FOV 和分辨率，用临时原生 SceneCaptureComponent2D 捕获各自世界，实际查看 On／Off／Reenabled 共 6 张 PNG：开启时两端均有青框，关闭后同站位再次出拳均无新框，重新开启后两端恢复。图像位于 `Saved/Screenshots/WindowsEditor/PinecoTraceDraw/`。这些是场景捕获而非完整视口／UI 截图，未复制相机曝光历史，地面有过曝；线框渲染与精确几何已分别核对。先尝试的自动化 UI 截图没有落盘，未作为画面验收证据。

移动／转向补充用例被自动审批按 GYM 的 IA 规则拒绝，未提交移动输入，不宣称旋转姿态已实测。期间存在外部攻击操作、诊断暂停与 218 条输入流饥饿恢复日志，不用整段日志证明隔离输入、连招生命周期或性能达标。两个本轮一次性断点均已消费并确认不存在；观察器已注销，临时捕获对象随 PIE 销毁、Python 引用已清除。最终本轮 PIE 已停止、`Worlds=[]`、未保存包为 0，三个相关开关均保留为 1。未验证跨进程／打包服务器转发；完整证据追加于 `Saved/Automation/TrainingDummyGAS/verification.json` 的 `trace_debug_draw_verification`。

## UAnimNotify_PinecoTrace

类型位于 `Source/PinecoAnimation/Notifies/AnimNotify_PinecoTrace.h/.cpp`，属于 `PinecoAnimation` 模块；C++ 消费方使用 `#include "PinecoAnimation/Notifies/AnimNotify_PinecoTrace.h"`。它是各形状共用的单次标记，编辑器名称为 `Pineco Trace`，不按 Box 等形状创建不同 Notify 类。

| 字段 | 默认值 | 统一语义 |
| --- | --- | --- |
| `EventTag` | 空 | 发送到所属角色 ASC 的本地事件 Tag，与项目原生 `WaitGameplayEvent` 监听一致；插件不固定宿主项目 Tag。 |
| `TraceConfig` | 空 | 原生 `FInstancedStruct`，只选择 `FPinecoTraceConfig` 的派生结构，排除基类；各结构持有自己的参数与 `Evaluate` 算法。 |
| `CollisionObjectTypes` | Pawn | 原生碰撞对象类型列表，供接收方构建查询掩码。 |
| `MaxTargets` | `0` | 接收方完成过滤后的最大目标数；`0` 表示不限，Notify 自身不截断目标。 |

原生 `Notify(MeshComp, Animation, EventReference)` 只负责向所属角色 ASC 发送一条 `FGameplayEventData`：`Instigator` 与 `Target` 都是所属角色，`OptionalObject` 是当前 Notify 的只读配置引用，`OptionalObject2` 是来源动画。事件不通过 RPC 传输；各端由本地动画触发，项目 GA 使用原生 `WaitGameplayEvent` 监听，并在服务端分支读取配置及调用结构体算法、消费结果和提交 GAS 效果。

空或错误类型配置、基类配置、无效 Tag、无效角色／动画／ASC，以及非游戏世界均不发送事件。动画编辑器预览默认关闭。Notify 不执行物理查询，不提交 GE，也不保存某个角色或本次播放的命中历史；其资产对象会被多个角色和播放共享。

拳1当前配置为 Box `Offset = (75, 0, 0)`、`HalfExtent = (75, 70, 100)`、`MaxTargets = 3`、`EventTag = Event.Attack`。插件不固定宿主的事件 Tag；显式依赖为 `GameplayTags` 公有、`GameplayAbilities` 私有。

2026-11-12：用户完整编译重启后，已替换 Montage 中的旧攻击标记，保留其余 Combo／MotionWarping／音效标记。原生属性回读确认 `FInstancedStruct` 的实际类型、Box 数值、Pawn 对象类型与目标上限；Notify 的 `bTriggerOnDedicatedServer = true`。

## UPinecoTraceEventLibrary::EvaluateTraceEvent

类型位于 `Source/PinecoGAS/Trace/PinecoTraceEventLibrary.h/.cpp`，属于 `PinecoGAS` 模块；C++ 消费方使用 `#include "PinecoGAS/Trace/PinecoTraceEventLibrary.h"`。这是当前 Montage Notify 的 GAS 事件适配入口；其对 PinecoAnimation 的依赖仅在适配模块私有声明，不进入 `PinecoTrace` 核心。

蓝图可调用签名为 `static bool EvaluateTraceEvent(UGameplayAbility* Ability, const FGameplayEventData& EventData, const FGameplayTargetDataFilterHandle& TargetFilter, TArray<FHitResult>& OutResults)`。`Ability` 默认自身，`TargetFilter` 可留空，复用原生 GAS 过滤句柄。调用流程为项目 Combo GA 的原生 `WaitGameplayEvent.EventReceived` → 本入口 → 原生结果／TargetData → 项目 GE 结算。

- 仅游戏线程同步执行，不转移输入对象的所有权，不跨帧补判或借用输入／输出；调用方拥有输出缓冲。有效空结果返回 `true`，无效输入返回 `false` 并清空输出；非游戏线程拒绝调用且不改输出。
- 必须是当前激活的实例化 Ability、权威 ActorInfo 与 Avatar，并处于服务端或 Standalone 游戏世界；客户端不执行物理查询。源 ASC 的 Avatar 也必须与 Ability 当前 Avatar 一致。
- 核对事件的 Instigator／Target、Notify 配置引用、Tag、来源动画及该 Notify 所属动画，要求均对应本 Ability 的当前 Montage。这一项属于当前触发适配的验证，不是形状算法的约束。
- 从 Notify 解析出配置、碰撞对象类型与目标上限后，委托 `UPinecoTargetingLibrary::EvaluateTrace`；事件适配层不再自行过滤、排序或截断。通用入口只依赖来源 Actor、`FInstancedStruct` 与原生查询／过滤参数，Selection Task 虚调用 `FPinecoTraceConfig::Evaluate`，忽略自身，不识别 Box 等具体形状，也不施加效果或发送 RPC。
- 只保留同世界的有效 ASC Actor，先应用原生 `TargetFilter`，再按 Actor 接受一次；过滤器应保持只读。拒绝项不占目标名额，Actor 多组件命中保留首个通过过滤的原生结果。此处只做本次查询内去重，不提供跨事件或跨播放的命中历史。
- 距离评分采用本次来源位置快照到目标 Actor 中心的距离平方，原生基类以 float Score 归一化、累计并升序稳定排序；同分保持候选顺序。随后由 Limit Task 应用 `MaxTargets`：`0` 不限，负值或非法碰撞对象类型拒绝求值。
- 在求值后再次检查 Ability 与 Avatar／ASC／Montage 上下文，避免把结果交回已结束或已换上下文的 Ability。输出保持原生 `TArray<FHitResult>`，不编造 Overlap 缺失的接触几何，GAS 效果由项目 GA 消费结果后提交。

2026-11-12（本次接回之前）：事件入口及显式私有模块依赖（PinecoAnimation、GameplayTags）已随用户的完整构建加载。项目 GA 已用原生 `WaitGameplayEvent` 连接此入口，移除旧 `PerformTargetingRequest` 与 `TP_Box` 分支；原生 HitResult 逐条转换为 TargetData 后施加现有 GE。本次只更改原入口内部的 C++ 委托，尚未重新构建；其他触发机制本轮未接入。

## UPinecoTargetingLibrary::EvaluateTrace

类型位于 `Source/PinecoGAS/Targeting/PinecoTargetingLibrary.h/.cpp`，属于 `PinecoGAS` 模块；C++ 消费方使用 `#include "PinecoGAS/Targeting/PinecoTargetingLibrary.h"`。这是与动画、Notify 和 GameplayEvent 无关的蓝图可调用入口，其他触发方式可直接提供来源 Actor 与查询配置。

签名为 `static bool EvaluateTrace(AActor* SourceActor, const FInstancedStruct& TraceConfig, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, int32 MaxTargets, const FGameplayTargetDataFilterHandle& TargetFilter, TArray<FHitResult>& OutResults)`。只接受权威服务端／Standalone 游戏世界中的有效来源 Actor、具体派生配置和合法对象类型；来源不要求自身具有 ASC，目标的 GAS 资格由过滤 Task 判断。`TargetFilter` 可留空，`MaxTargets = 0` 不限，负数失败。有效空结果返回 `true`；失败清空输出，非游戏线程拒绝且不触碰输出。

入口调用原生 `UTargetingSubsystem::ExecuteTargetingRequestWithHandle`，采用程序配置的原生 `FTargetingTaskSet` 顺序运行四个 Task。TaskSet 地址具有稳定生命周期，使用类持有的无单次状态 CDO；配置、来源变换、查询参数、过滤句柄及目标上限按句柄隔离，完成回调将最终原生 HitResult 复制到本请求拥有的快照，再由调用方取走。入口和完成回调均不施加 GE 或发送 RPC，现有项目 GA 继续消费结果后结算。

来源 Actor、World、配置的反射对象引用及原生过滤器基类引用由请求状态的 `FGCObject` 跟踪。完成回调不捕获调用栈。使用原生即时请求的完成自动释放标记；返回时主动关闭并移除 Pineco 请求载荷、清除原生来源上下文的对象引用，即使原生调试保留句柄也不留下参数或栈地址。原生异步 Tick 若将即时请求推迟，本入口立即返回 `false`，延后 Task 找不到有效 Pineco 载荷，不再调用形状算法或补交结果，原生请求在其完成时处理释放。执行期间拒绝递归进入此管线，避免同步请求存储在任务回调重入时失效。

当前 Combo 的调用链为 `WaitGameplayEvent → EvaluateTraceEvent（事件校验）→ EvaluateTrace（原生 Request）→ Selection → GAS Filter → Distance → Limit → HitResult → 项目 TargetData／GE`。通用入口不解析 Notify；现有蓝图、Notify 的 Box 配置及 `MaxTargets = 3` 不变。已随用户完整构建载入，并通过下述定点双客户端功能验证。

## 原生 Targeting Task 管线接回（已构建，定点双客户端功能验证通过）

用户确认保留统一 Notify、原生 `FInstancedStruct` 与各结构的 `Evaluate`，将候选选择接入原生 Targeting Request，再由 Task 管线执行过滤、排序和目标上限。形状算法继续属于 `PinecoTrace`；Targeting 适配属于 `PinecoGAS`，不向通用形状模块引入 GAS／TargetingSystem 依赖。`EvaluateTraceEvent` 的现有蓝图签名保持不变。

首个已确认并实现的 API 为 `UPinecoTargetingSelectionTask_Trace::Execute(const FTargetingRequestHandle&) const`，位于 `Source/PinecoGAS/Targeting/`。它继承原生 `UTargetingTask`，读取该句柄的只读配置快照、来源变换和原生查询参数，虚调用结构的 `Evaluate`，将未经过滤、去重、排序或截断的原生 HitResult 候选写入 `FTargetingDefaultResultsSet`。不识别 Box 等具体类型，不提交效果或 RPC，不依赖 Notify 类型。

模块内部 `FPinecoTargetingRequestData` 使用原生 `TTargetingDataStore` 按句柄隔离。共享状态持有独立 `FInstancedStruct` 快照并通过原生 `FGCObject` 跟踪其对象引用，避免把本次配置保存在 Task／Notify 上或跨请求借用临时内存；共享持有同时避免原生数据表在重入插入时移动条目使当前求值失效。算法返回后重新核对句柄对应状态与来源世界，已关闭／替换的请求不发布结果。

该 Task 仅支持游戏线程上的即时请求；异步请求、无效配置或上下文记录为失败并保持空候选，有效未命中单独记录为成功。`PinecoGAS.Build.cs` 已公开声明 `TargetingSystem` 模块依赖，插件描述文件已有对应插件依赖。

接回阶段的第二个 API 原为 `UPinecoTargetingFilterTask_GAS::Execute(const FTargetingRequestHandle&) const`，同属 `PinecoGAS/Targeting/`。它曾消费 Selection 成功后的原生结果集，排除无效 Actor、自身、其他世界、无效位置及缺少同世界 ASC 的目标，调用请求中原生 `FGameplayTargetDataFilterHandle`，按 Actor 去重且保留首个通过过滤的原始 HitResult；不排序、不截断、不提交 GE。当时因保留接受顺序的约定而直接继承 `UTargetingTask`；该实现现已替换为上方迁移状态所述的原生过滤模板，以下请求状态与运行证据仅描述旧版本。

过滤句柄共享持有原生多态过滤器，不复制基类而丢失派生行为；其原生 `SelfActor`／`RequiredActorClass` 引用纳入请求状态的 `FGCObject`。自定义派生过滤器若持有额外 UObject 引用，仍须由该过滤器的持有方按原生 GAS 句柄约定管理。过滤过程中先移出候选数组，外部 ASC／过滤器回调后重新核对请求、来源与世界；请求已关闭或替换时不发布结果。有效空结果标记过滤成功，失效上下文保持空结果并标记失败；Task 本身不保存单次请求状态。

第三个已确认并实现的 API 为 `UPinecoTargetingSortTask_Distance`，继承原生 `UTargetingSortTask_Base`，仅在构造时设置升序／稳定排序并覆写 `float GetScoreForTarget(const FTargetingRequestHandle&, const FTargetingDefaultResultData&) const`。分数采用请求捕获的来源位置到目标 Actor 中心的距离平方；不重新读取来源 Actor 当前坐标作为距离起点。原生基类负责归一化、Score 累计和排序，适配 Task 不保存单次请求状态。

用户已明确确认排序语义改为原生 float Score，同分保持过滤后的候选顺序，取代迁移前直连链路的 double 距离比较及 ActorID 平局规则。超过 float 范围的距离限制为最大有限 float；无效请求／目标／坐标也返回该最低距离优先级分数，避免原生归一化引入无穷或 NaN。只服务于游戏线程上的即时管线，不在评分函数中删除候选、限制数量或执行效果。

接回阶段的第四个 API 为 `UPinecoTargetingFilterTask_Limit::Execute(const FTargetingRequestHandle&) const`，位于同一 Targeting 目录。旧实现曾在 `Selection → GAS Filter → Distance → Limit` 末尾读取请求状态的 `int32 MaxTargets`，原位保留排序结果前 N 项，并记录限制阶段结果；当时 quan1 的上限从 Notify 传入。该实现现已替换为上方迁移状态所述的 Preset Task 实例配置，不再读取 Pineco 请求状态；以下运行记录仍只描述迁移前版本。

四个 Task、通用 `EvaluateTrace` 入口及 `EvaluateTraceEvent` 委托均已完成源码接线，事件适配层原有直接过滤／排序／截断已移除。C++ 实现阶段未自动构建、Live Coding 或启动 PIE；之后由用户完整编译并重启，agent 按用户的新验证指令执行以下测试。本次验证未修改 C++、蓝图资产、Notify 参数、GE 或 Gameplay preset。

### 2026-11-12 原生 Task 管线双客户端运行验证

最终受控会话为专服 `Context_21`、客户端 `Context_22/23`，PlayerId 为 259／260。正式 quan1 通过原生 `WoolooPIE.InjectInputAction` 驱动既有快捷栏，并明确提交按下／释放；沿用用户的正式快捷栏验证授权，不新增 IA、IMC 或 Debug Key，不从 Python 调用技能、GameplayEvent 或 RPC。前一准备会话在记录窗口外出现多次攻击、目标已到 0 Health，因此不作为单拳证据；已结束该自建会话并重开 PIE。

最终数量上限用例由 `Context_23`／PlayerId 260 释放，服务端 Avatar 位于 `(-2150, -1100, 100)`、Yaw 0。Box 中心 `(-2075, -1100, 100)`、半尺寸 `(75, 70, 100)`。四个既有假人仅在 PIE 中临时摆位；原生只读 `BoxOverlapActors` 确认四个同世界 ASC 候选，原始顺序为 `C_3、C_0、C_1、C_2`。远处两只假人的 Actor 原点在 Box 前脸外，其半径 50 cm 的胶囊与 Box 重叠 5 cm，符合碰撞体相交规则。另一玩家在该用例范围外。

以下数组顺序均为 `BP_BoxTarget_C_0／C_1／C_2／C_3`；每一行的前后值在专服和两个客户端均一致。

| 绘制状态 | 攻击前 Health | 攻击后 Health | 服务端原生请求释放日志 |
| --- | --- | --- | --- |
| 开启 | `[90,90,100,100]` | `[80,80,100,90]` | Handle 81 |
| 关闭 | `[80,80,100,90]` | `[70,70,100,80]` | Handle 82 |
| 重新开启 | `[70,70,100,80]` | `[60,60,100,70]` | Handle 83 |

三次受控攻击各接受 3 个假人、每个仅扣 10，未入选的 `C_2` 始终为 100；关闭绘制不改变判定和伤害。三个采样窗口累计 44.922 秒、1415 帧，两个客户端的 18 次假人受击播放均只经过一个 Section 后结束，覆盖 `Hit_01／Hit_02／Hit_03`；观察到的活跃区间约 0.59～0.61 秒。随机 Section 由各客户端本地选择，本次不要求二者选择相同，也不作概率分布验收。

两端各自世界的临时 SceneCapture 已实际查看：开启及重新开启时显示同位置、同投影大小的青色服务端 Box，关闭时两端均无 Box。绘制继续使用形状查询的实际 QueryLocation／QueryHalfExtent／QueryRotation；本轮没有重新抓取 C++ 调试器中的逐边顶点。六张有效截图位于 `Saved/Screenshots/WindowsEditor/PinecoTargeting/` 的 `MultiOn、MultiOff、Reenabled_Client1/2.png`，属于客户端场景采集，不是含 UMG 的视口截图；拍摄曝光仅覆盖临时组件。共享专服绘制时显式使用 `ShowFlag.ServerDrawDebug = 1`，清理恢复原值 2，`pineco.Trace.DebugDraw` 最终保持 1。

功能结果通过，但每次受控攻击之后均有一条 `LogNetworkPrediction` 的 `RollbackFrame EQUAL PendingFrame` 警告（Frame 21662／32520／38961，Offset 75／86／99）。未定位其触发原因，不把本次功能通过表述为联机运行无警告；本次未更改移动或预测逻辑。当前空 TargetFilter 仍接受同世界的其他 ASC Actor，敌我／死亡状态筛选未在本次配置或验收。

本轮自建 PIE 已停止，原生 `DescribeSession` 最终为 `Worlds: []`；限时采样已停止，两个拍摄组件及 RenderTarget 已清理，Python 任务全局引用已移除，没有新增持久 Python 脚本或保存测试摆位。其余未保存的脚步对比资产保持原样。详细证据在既有 `Saved/Automation/TrainingDummyGAS/verification.json` 的 `native_targeting_pipeline_verification` 字段，原有 19 个字段已逐项核对保留。本次只验证同进程、定点 PIE 功能，不作为移动场景、网络模拟、多进程绘制或性能验收。

## FPinecoHitPolicy

类型位于 `Source/PinecoTrace/Hits/PinecoHitPolicy.h`，属于 `PinecoTrace` 模块；C++ 消费方使用 `#include "PinecoTrace/Hits/PinecoHitPolicy.h"`。

| 字段 | 默认值 | 统一语义 |
| --- | --- | --- |
| `bPerBody` | `false` | 默认同一 Actor 共用次数与间隔；为 `true` 时按有效部位身份分别记录。 |
| `MaxHitsPerTarget` | `1` | 同一目标身份在一个命中组整个生命周期内允许接受的最大次数；`0` 表示不限次数。 |
| `MinHitIntervalSeconds` | `0.0` | 同一目标身份两次接受之间所需的最小仿真时间间隔，单位秒；`0` 表示无时间间隔限制。 |

次数上限和时间间隔同时约束接受行为。默认策略表达一个命中组内每个 Actor 只接受一次；不限次数且间隔为零时，策略本身不抑制重复候选。

按部位记录时沿用已有身份规则：保留目标身份，骨骼部位优先使用 Component 与有效 BoneName；骨骼身份缺失时保守回退到 Component。非骨骼结果保留其有效 Item / 实例信息，不将 PA 的 BodyIndex 当作长期稳定身份，也不猜测 Body 0。

时间由调用方提供，使用共同约定的仿真时钟；`FHitResult.Time` 不代表本次执行的仿真时间。共享一个组的参与者必须使用同一时间基准。周期结算由明确的脉冲身份或仿真时间策略表达，不通过每渲染帧清空记录实现。

配置约束为 `MaxHitsPerTarget >= 0`，且 `MinHitIntervalSeconds` 有限、非负。编辑器的 `ClampMin` 只提供制作约束，不能替代运行时检查；拒绝非法配置属于后续命中组创建入口的契约，该入口当前尚未实现。

命中策略只定义接受限制。目标合法性与 Rule 过滤先于接受记账；组内计数与实际效果提交的成功状态分开管理。客户端预测记录与服务端权威记录相互独立。

## 当前实现与验证状态

- 2026-11-11：已新增 `FPinecoHitPolicy` 的反射配置类型、三个字段及默认值；本次未添加运行时方法。
- 当前 `TraceExecution` 仍使用 `EPinecoTraceHitPolicy` 和自身的 `SeenHits`。新策略尚未接入，既有判定行为保持原状。
- 已声明原生 `TargetingSystem` 插件依赖。Box 用例的 Preset、目标、技能、Setup、GameMode 和地图已创建并保存；4 个蓝图编译通过，无错误、无警告。技能 EventGraph、Setup 的 EventGraph／SetupPlayer、目标与 GameMode 图按职责分块并添加中文注释。
- 2026-11-11 02:24:06～02:24:49（Asia/Shanghai）执行了一次单玩家 Standalone PIE，持续约 43 秒，结束前原生会话快照的游戏时间为 39.193 秒。明确核对 `Context_3`／本地玩家 0、Controller、Pawn 及其 ASC；Owner／Avatar 均指向本世界的 `PCharacter_Linghou_C_0`，角色为 Authority。
- 日志实际输出 `PinecoBox RESULT actors=2 hits=2`，命中名称为 `Box_Inside` 与 `Box_Edge`；框外、身后目标未出现在结果中。运行时读取跨边界目标胶囊半径 50 cm、半高 90 cm、Pawn／QueryOnly；来源位于 `(0,0,100)`、旋转为零，验证了 Actor 中心在外而碰撞体相交的 Box Overlap。
- 测试技能通过 GYM Setup 自然授予并激活；结束后的 ASC 快照中已移除该一次性技能。已停止本次 PIE，原生会话查询确认无剩余 PIE 世界。此结果仅覆盖当前静止 Box 的单机选择行为，不证明联机、伤害、预测或性能目标达标。
- 制作期间 Monolith 的独立 AbilityTask 引脚检查导致调试器中断；已绕开该工具，最终节点通过现有 Wooloo Blueprint 工具创建，未修改引擎或 Monolith 源码。
- 清理完成：测试地面位于测试主关卡。用户明确同意后，重载测试关卡并仅丢弃共享 `GYM_Lighting` 的临时未保存状态；查询确认当前无未保存包。共享层 5 个原有 Actor 与位置保留，磁盘文件无 Git 差异、时间戳未变；测试地面和 4 个目标均从已保存地图重新加载成功。
- 画面修复与补验：首次只核对了命中结果，漏验灯光的运行时加载。Monolith 的 `manage_sublevel/add` 忽略 `streaming_class`，实际创建了启动加载／可见均为 false 的 `LevelStreamingDynamic`。现已仅修改测试地图的引用为真正的 `LevelStreamingAlwaysLoaded`；2026-11-11 02:51:42～02:52:22 再做约 40 秒 Standalone PIE，确认灯光已加载且可见，实际截图显示地面、角色及 4 个目标，命中仍为 2。截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00000.png`。已停止 PIE，重载地图确认 AlwaysLoaded 持久化；共享灯光资产未修改。
- 共享命中组、配置校验、次数与间隔执行、TargetActor、实例池及其预测适配仍待逐项评审和实现。项目蓝图 Notify 接入的后续实施结果见本文末尾。
- 配置类型仅做静态核对；Box 资产通过已运行的编辑器创建并按上述记录执行 PIE。未执行 UHT、C++ 构建或 Live Coding。

### 可交互木桩靶场（2026-11-11）

本节记录首次可交互靶场的制作与实测历史，其中 F／IA／独立 Box GA 入口已在后述 quan1 清理中删除。

当前 `GYM_PinecoBox` 已从启动即执行一次的最小用例改为可移动、可重复施放的靶场。沿用公共 Pawn、ASC、移动与相机；专用 PlayerController 叠加 F 键映射，Setup 仅授予一次可重复激活的 GA，GA 每次完成后结束但不移除。WASD／鼠标左键移动、Z 跳跃使用已有框架映射。场地为 60×40 米，补齐边界、地面碰撞与 NavMesh，继续常驻共享 `GYM_Lighting`。

目标显示 Nickit 的斗战神原木桩，资源语义见 [Nickit Context](../unreal-nickit/Context.md)。胶囊继续作为 Pawn／QueryOnly 受选体积；视觉网格不参与碰撞或导航。GA 对每个原生结果调用 `ApplyDamage(1)`，仅借用原生 AnyDamage 事件重播源 `Ok` 动画；没有实现生命值或正式伤害结算。当前 Box 范围由原生 Targeting System 的 CVar 控制，GA 不再自行计算或绘制调试框。

实测：2026-11-11 03:43:27～03:49:05（Asia/Shanghai）Standalone PIE，约 338 秒墙钟时间；慢放用于动画抽样，结束前游戏时间 165.596 秒。明确选择 `Context_10`／本地玩家 0，专用 Controller 与 Linghou Pawn 均为 Authority，ASC Owner／Avatar 与该世界一致。原生 Enhanced Input 提交后日志实际出现 5 次结果：首轮 `actors=2 hits=2`、Inside／Edge；移动及改变朝向后另 4 次均为 `actors=1 hits=1`、Outside，证明不是只在启动执行。

方向输入前后位置从 `(123.258,143.523,92.150)` 到 `(113.140,154.384,92.150)`，平面位移约 14.84 cm。寻路从约 `(-801.966,-0.597,92.150)` 返回原点附近，地面目标 `(0,0,0)` 的原生状态为 `Completed`；误传角色中心高度 100 cm 的前次查询到达平面终点后报 PartialPath，未据此修改框架。`Ok` 播放时间在再次命中前后由 1.326 秒重置为 0.155 秒，Bone04 的运行时变换也改变。实际查看截图 `Saved/Screenshots/WindowsEditor/HighresScreenshot00001.png` 与 `HighresScreenshot00004.png`，角色、木桩、地面、阴影和 Box 均可见。

5 个相关蓝图编译无错误、无警告；核对 GA、目标、Setup、GameMode、Controller 的图坐标、执行连线与中文分块注释，构造图保留对应父级流程。停止本次 PIE 后原生会话 `Worlds=[]`、未保存包为 0，并重新加载已保存地图。仅后续调整了纯取值节点的位置与局部连线以缩短跨区连线，重新编译并保存通过。没有执行 C++ 构建／Live Coding；未验证联机、预测或 32 玩家性能。

### 原生碰撞查询显示

用户要求通过 Console Variable 控制显示后，已从 `GA_PinecoBoxTest` 移除 DrawDebugBox 及其位置／方向计算节点，激活入口直接连接原生 PerformTargetingRequest，并同步整理 EventGraph 布局与中文注释。无需新 C++ API 或自定义 CVar。

```text
ts.debug.EnableTargetingDebugging 1
ts.debug.OverrideTargetingLifeTime 1.25
```

开启后，每次 F 施放由原生 AOE 任务按实际查询参数绘制红色 Box；设置 `ts.debug.EnableTargetingDebugging 0` 停止新的绘制，已有线条到期消失。此开关显示本次查询体；场景实体自身的碰撞体是另一项显示，不等同于 Box 查询。

04:20:14～04:21:10（Asia/Shanghai）再次运行约 56 秒 Standalone PIE，明确选择 `Context_17`／本地玩家 0 与专用 Controller、Linghou Pawn，均为 Authority。关闭和开启时各提交一次原生 F 输入，日志均为 `actors=2 hits=2`（Inside、Edge）；实际查看截图 `HighresScreenshot00005.png`（关闭，无 Box）与 `HighresScreenshot00006.png`（开启，原生红色 Box）。截图验证临时使用 10 秒寿命，随后改为 1.25 秒；当前编辑器会话保留 Targeting 调试开启，未写入持久配置。已停止 PIE，`Worlds=[]`、未保存包为 0；相关蓝图重新编译无错误、无警告，未编译 C++。

### 项目 Combo GA 的攻击 Notify 接入

本节记录首次 Notify 接入及联机实测历史；后续已按用户要求收敛为 quan1／A011 单处攻击标记，并移除 F 输入及旧测试资产，见本文末尾。

本轮直接复用原生 `UAbilityTask_WaitGameplayEvent`，不新增 AbilityTask、网络同步点或攻击 RPC。项目蓝图 `GA_Linghou_Combo` 在激活时 CommitAbility，安装 `OnlyTriggerOnce=false`、`OnlyMatchExact=true` 的 `Event.Attack.Box` 监听；收到事件后核对 `Payload.OptionalObject` 与本 Ability 当前 Montage，再在 HasAuthority 分支执行原生 `TP_Box`。结果仍在完成回调内读取，不在 Box 返回时结束连招 GA。

`/Game/AsuraNext/Animation/Notifies/AN_AttackBoxEvent` 覆写原生 `Received_Notify`，通过 `SendGameplayEventToActor` 向所属角色 ASC 发送本地事件，动画引用放入原生 `FGameplayEventData.OptionalObject`。编辑器预览禁用。`AM_Linghou_Combo` 的 A011、A012 分别新增 Montage 绝对时间 0.3、1.3 秒的点标记，均允许 Dedicated Server 触发；保留原有组合窗口和 Warping 标记。复用现有通知轨道 `1`，没有创建 NotifyState。

服务端通过攻击者 ASC 的原生 ExecuteGameplayCueWithParams 发送 `GameplayCue.PinecoBox.Hit`，以 `TargetAttachComponent` 标识命中目标的根组件。项目 Cue `/Game/AsuraNext/Ability/GameplayCues/GCN_PinecoBox_Hit` 在目标有效时播放 Nickit 的原 Ok 动画；位于现有 GameplayCue 扫描目录内。此处仍是木桩表现用例，没有加入真实生命值或伤害公式。木桩启用 Actor 复制，F 键改为发送已有 `Emboar.Combo.Input`，载荷使用已有 `IA_Linghou_Quan`；旧 Setup 和旧单独 Box GA 保留，F 不再直接激活它。地图增加第二个 PlayerStart `(0,-100,100)`。

Emboar 只恢复原生 GAS 蓝图激活分发：在上下文校验、Mover 绑定后调用 Super::ActivateAbility，再检查 Ability 是否已结束，最后才创建 Session。这样项目蓝图可在第一段播放前安装监听；Emboar 中没有攻击事件 Tag、Notify 名称、Preset 或判定逻辑。未新增公开 C++ API。

制作验证：Notify、Cue、Combo GA、靶场 Controller、目标等相关蓝图编译无错误、无警告；整理了 Notify 的 Received_Notify、Cue 的 OnExecute、Combo GA 和 Controller 的 EventGraph。原有 OnEndAbility 清理连线保留，原生 GameplayCue 参数使用专用 Make／Break 函数。诊断 PrintString 使用原生 DevelopmentOnly 节点，不参与生产判定。

构建前联机探测已创建 1 个 Dedicated Server（Context_19）和 2 个客户端（Context_20、Context_21），逐一核对 PlayerId 264／265、Controller、Pawn 与 ASC。只向客户端 2／玩家 265 的原生 F InputAction 提交一帧输入后，客户端 2 与服务器的对应 Combo GA 均为 Active，玩家 264 保持未激活；未取得 Notify／Box／Cue 完整链路通过的证据。本次 PIE 已停止，Worlds=[]，资产保存后未保存包为 0。

用户已手动编译并重启；载入的 Emboar DLL 修改时间为 2026-11-11 05:51:06。本 agent 未执行 C++ 编译、Live Coding 或编辑器重启。以下为重启后实际完成的验证，独立于上述构建前探测。

2026-11-11 06:03:38～06:10:32（Asia/Shanghai），运行约 414 秒 Dedicated Server + 双客户端 PIE。原生 WoolooPIE 快照明确核对服务端 Context_1、客户端 Context_2／Context_3、本地玩家 0、PlayerId 256／257、Controller／Pawn 与各自 ASC 的 Owner／Avatar；客户端均为 AutonomousProxy，服务端为对应 Authority。所有攻击和移动输入均直接通过官方 MCP 的原生 InjectInputAction 提交。

| 攻击日志时间 | 输入玩家 | 仅服务端的 Box 结果 | 两个客户端的 Cue |
| --- | --- | --- | --- |
| 06:03:58 | 客户端 2／257 | Inside、Edge；actors=2 hits=2 | 各 2 次 |
| 06:08:14 | 客户端 2／257 | Inside、Edge；actors=2 hits=2 | 各 2 次 |
| 06:09:21 | 客户端 1／256，起点 Y=-100 | Inside；actors=1 hits=1 | 各 1 次 |
| 06:10:27 | 客户端 2／257 | Inside、Edge；actors=2 hits=2 | 各 2 次 |

每次预测端和对应权威均先输出 LISTEN、再收到 EVENT；日志共 4 次服务端 Box，客户端 Box 为 0。每个客户端各收到 7 次命中 Cue，没有叠加事件回调。模拟代理也会播放 Notify，但未出现该远端角色的 GA 事件消费。前几次自然结束后的原生快照确认双方 Combo 为 Inactive、Montage 为空，随后可重新激活；最后一次命中后停止 PIE，未据此声称验证了取消。

两个明确客户端世界的木桩动画实例只读回查确认：Inside／Edge 当前资产为 AS_TrainingDummy_Ok，已播放到 2.1 秒；Outside／Behind 仍循环播放 Embedded。客户端 1 的原生移动输入使其 Y 从 -100 到 -85.1214，平面位移约 14.88 cm，服务端对应玩家位置一致。

测试中没有 Deep PredictionKey Chain Detected；日志出现 80 条 Network Prediction 输入流饥饿恢复记录，本次未定位其原因或据此作性能达标结论。一次只读动画检查误用了未反射到 Python 的 GetSingleNodeInstance，随后改为 GetAnimInstance 的原生 GetAnimationAsset 完成回查；Python 没有触发输入、技能、事件或 RPC。未充分覆盖 A012 衔接、连招中途取消／移动续接、延迟丢包和 32 玩家性能。停止后原生会话 Worlds=[]，未保存资产包为 0。结构化记录位于 Saved/Automation/PinecoNotify/verification.json。

### PCharacter 木桩与导航修复（2026-11-11）

用户反馈木桩下方有大片导航空白。只读检查确认蓝图模板的 TargetCapsule.bCanEverAffectNavigation=false，但地图中四个旧实例仍为 true；四个脚下点在 XY 搜索半径 5 cm 时均无法投影到导航。两侧地面可投影，横穿靶区的完整路径有 4 个点、长度约 1519.68 cm，绕开了空洞。

BP_BoxTarget 现直接继承已有 `/Script/PawniardCharacter.PCharacter`。移除旧 TargetCapsule／TargetVisual，复用原生 CollisionCapsule、CharacterMesh0、MoverComponent、NavMoverComponent 和 PathFollowingComponent；每个实例仅有一个胶囊和一个骨骼网格。胶囊保留半径 50、半高 90 cm，使用 Pawn 碰撞；角色 bCanAffectNavigationGeneration、胶囊／Mesh 的 bCanEverAffectNavigation 和胶囊 bDynamicObstacle 均为 false。AutoPossessAI=Disabled，木桩保持无自主 AI 行为的靶子。Box 的 ActorClass 过滤、资产路径、标签和原生 GameplayCue 继续适用；兼容 AnyDamage 的图改用原生 Mesh 变量。

重设父类不会把原生组件的模板覆盖自动同步到既有地图实例，实际出现过默认尺寸和空 Mesh。已在原实例上同步配置，保留标签、XY 和对象身份，保存并重载关卡验证。现有 Monolith／官方 Wooloo 工具覆盖模板编辑，没有置放实例原生组件的通用属性同步入口；为四个实例的一次迁移扩展工具需要额外公共 API 与 C++ 构建，因此使用短内联原生编辑器 API，同步后立即回读，没有新增脚本或运行时入口。

贴地修正：原中心 Z=100 配合半高 90 导致模型离地约 11.01 cm。第一次将中心降到 90 后，PIE 中原生 Mover 将胶囊维持在地面上方 2.4 cm。最终中心为 Z=92.4，Mesh 相对 Z=-93.410923，补偿源模型底部约 1.010922 cm 的原点偏移与 Mover 地面间隙；不改变公共角色移动参数。已在蓝图注释中写明该偏移的原因。

重建并保存本地图的导航后，保存／重载及最终 PIE 期间的查询均确认四个脚下点可投影；横穿靶区变为 1500 cm、2 个点的直线路径，Inside 到 Outside 为 250 cm 的完整直线路径。共享灯光和其他 GYM 未修改。

最终验收：06:50:10.768～06:51:14.880（Asia/Shanghai），约 64 秒 Standalone PIE，明确 Context_10／本地玩家 0／PlayerId 261、专用 Controller、Linghou Pawn 与对应 ASC。四个木桩实际中心均为 Z=92.4、Mesh 包围盒底面 Z 约 -0.00000081 cm，胶囊与 Mesh 导航影响均关闭；实际查看截图 HighresScreenshot00008.png。官方 MCP 原生 F 输入触发项目 Combo Notify，来源 `(0,-100,100)` 的 Box 命中 Inside（actors=1 hits=1），继承的 CharacterMesh0 实际播放 Ok 至 2.1 秒，其余保持 Embedded。Box 后 Combo 已退出。前两次定位 PIE 分别为约 45 秒与 70 秒，曾观察到 11.01 cm 和 2.4 cm 的离地，均已停止，不作为最终贴地通过记录。

蓝图编译错误 0、警告 0；检查 EventGraph 的执行链、原生 Mesh 取值和中文注释框布局，Construction Script 保持空制作流程。最终停止后 Worlds=[]、未保存包为 0。本轮未修改或编译 C++、未执行 Live Coding；未重跑联机验收。导航修复记录：Saved/Automation/PinecoNavigation/verification.json。

### 九宫格测试布局（2026-11-11）

当前 GYM_PinecoBox 按用户确认的 3×3 九宫格组织，只启用 Single Target 和 Multiple Targets，另外七格标为 RESERVED。每格 17×11 米，总范围 51×33 米；X 边界 -2550／-850／850／2550 cm，Y 边界 -1650／-550／550／1650 cm。复用原四个 PCharacter 木桩分成 1＋3，旧 Inside／Edge／Outside／Behind 为此前摆位历史。

| Case | 标记站位 XY（cm），统一朝 +X | 木桩 XY（cm），Z 均为 92.4 | 预期命中 |
| --- | --- | --- | --- |
| 01 Single Target | `(-2150,-1100)` | `(-1850,-1100)` | 1 |
| 02 Multiple Targets | `(-450,-1100)` | `(-200,-1190)`、`(-200,-1010)`、`(50,-1100)` | 3 |

各区有名称、预期数量、站位和朝向标识，地面文字按默认游戏视角转正；Outliner 按 PinecoBox/Cases 分组，网格位于 PinecoBox/Layout/Grid。共新增 18 个静态标识和 11 个原生文字 Actor，全部无碰撞、不影响导航，复用 MI_GYM_Ramp／MI_GYM_Step。两个 PlayerStart 移至 Single 站位及后方 1 米。仅修改本地图的摆放、Actor 标签与标识，既有木桩蓝图、TP_Box、Combo GA、Notify、Cue、共享灯光和其他 GYM 未修改。

保存重载后检查四个木桩仍贴地，29 个标识无碰撞且导航影响关闭；目标脚下、站位及区间通道的导航查询通过。07:03:19.964～07:05:59.011（Asia/Shanghai），约 159 秒 Standalone PIE，明确 Context_20／本地玩家 0／PlayerId 262 的 Controller、Pawn 和 ASC，均为 Authority。官方 MCP 原生 F 输入分别实测 Single 的 actors=1 hits=1 与 Multiple 的 actors=3 hits=3；原生 MCP 调用已有 MoveControl.RequestNavMove 途经南侧通道到达多目标站位，请求状态 Completed，实际位置约 `(-451.450,-1099.851,92.150)`。没有通过 Python 触发输入、技能、事件或 RPC。

后续两次 PIE 只检查文字及整体画面，最终实际查看 HighresScreenshot00011.png（默认视角）和 HighresScreenshot00013.png（完整九宫格俯视图）。俯视用临时原生 DebugCamera；明确世界与 Controller 后的一次原生截图 API 调用仅补齐 MCP 无法指定已移交 Viewport 的 Controller 的缺口。文字内容、旧 Actor 文件夹及组件导航标志使用短内联原生编辑器 API 补齐当前工具缺口，未新增脚本或公共 API。最终 PIE 已停止，Worlds=[]、未保存包为 0；未修改蓝图／C++、执行 C++ 编译／Live Coding 或重跑联机／性能验收。结构化记录：Saved/Automation/PinecoRangeLayout/verification.json。

### 正式假人归入 Avatar（2026-11-11）

用户确认后，通过原生资产重命名将 `/Game/Toilet/GYM/PinecoBox/BP_BoxTarget` 迁移为 `/Game/AsuraNext/Avatar/NPC/TrainingDummy/PCharacter_TrainingDummy`。本文较早记录中的 BP_BoxTarget 为迁移前名称；角色属于宿主 Avatar，GYM 只摆放实例，模型、独立 Skeleton、源动画和派生 Montage 仍属于 Nickit。

已保存测试地图和 `TP_Box`，两者直接引用新类。旧包包含蓝图、生成类与默认对象三个重定向，Wooloo 当前单重定向包预检无法处理；确认旧包磁盘引用为零后，通过 Monolith 原生非强制删除入口清理三个重定向，没有强制删除或修改工具实现。最终旧文件及注册表条目均不存在，新类的磁盘引用者为测试地图和 `TP_Box`。

蓝图编译错误 0、警告 0，保存重载后四个实例均为新类，保留 Single 1 个／Multiple 3 个的坐标、标签和原对象身份。原生 PCharacter 父类、每实例单胶囊／单 Mesh、胶囊半径 50／半高 90、中心 Z=92.4、Mesh 相对 Z=-93.410923、组件导航影响关闭及源 Embedded／Ok 引用均已回读。检查 EventGraph 和 Construction Script 的执行链、节点位置与中文注释，迁移未改图逻辑。未启动 PIE、执行 C++ 编译／Live Coding，也未重跑攻击、导航或联机测试。结构化记录：Saved/Automation/TrainingDummyAvatar/verification.json。

### quan1 攻击 Notify 与旧入口清理（2026-11-11）

用户明确要求直接在 quan1 播放时触发判定，不增加 Debug Key。已确认该拳招是 `ST_Linghou_Combo` 的 A011 第一式，使用 `AM_Linghou_Combo/A011`。保留其 0.3 秒点标记，移除 A012 的 1.3 秒 Box 标记；当前 Montage 共 18 条 Notify，其中只有一条 `AN_AttackBoxEvent`，17 条原组合窗口与 Motion Warping 标记保留。攻击标记允许 Dedicated Server 触发，概率为 1。

`BP_PinecoRangePlayerController` 已移除 F 的 Enhanced Input Action 事件、专用 IMC 装卸和替代拳招事件发送；保留父级 BeginPlay、现有本地控制器判断和更新后的靶场说明。专用 GameMode 已移除 PinecoBoxSetup 组件，保留继承的 StarterItemsSetup。已删除 `/Game/Toilet/GYM/PinecoBox/` 下的 `IA_PinecoBox`、`IMC_PinecoRange`、`GA_PinecoBoxTest`，以及 Framework 中的 `BPC_GymPinecoBoxSetup`；没有创建替代测试入口。

删除前磁盘引用检查通过。IMC 和 Setup 最初仅被本次已移除节点的撤销记录、旧生成类和瞬态缓存引用；逐项核对范围后使用原生强制删除清理这些编辑器残留，再非强制删除 IA 与旧 GA。未直接删除二进制文件。

项目 Combo GA 的原生 WaitGameplayEvent、当前 Montage 核对与 HasAuthority → PerformTargetingRequest 连线保持有效。整理并检查控制器和 GameMode 的 EventGraph：执行节点同基线、节点吸附网格、中文注释覆盖各自职责且互不重叠。两份修改蓝图最终编译均为错误 0、警告 0，资产已保存，未保存包为 0。此次只完成资产和静态接线验证，未启动 PIE 或执行 C++ 编译／Live Coding；此前联机记录不替代本次清理后的运行验收。当前反馈仍由攻击者 ASC 发送 Cue，尚未接入目标 ASC 的属性与正式伤害结算。结构化记录：Saved/Automation/PinecoQuan1/verification.json。

### 假人 ASC 与 Health 接入（2026-11-12）

当前资产链路为 quan1 Notify → 项目 `GA_Linghou_Combo` 的服务端 Box → 原生 `AbilityTargetDataFromActorArray` → `ApplyGameplayEffectSpecToTarget` → 目标 ASC 的 Instant `GE_Damage` → 目标侧 `GameplayCue.PinecoBox.Hit`。WaitGameplayEvent、当前 Montage 校验、仅权威执行查询及原有 OnEndAbility 清理保留；移除旧结果打印循环和攻击者直接发命中 Cue 的支路。

`PCharacter_TrainingDummy` 继续继承原生 PCharacter，新增复制的原生 ASC，BeginPlay 在各端以 Self 初始化 Owner／Avatar。DefaultStartingData 同时指定 `/Script/AsuraNext.AsuraHealthAttributeSet` 与 `/Game/AsuraNext/Combat/Attributes/DT_HealthDefaults`，Health／MaxHealth 初始为 100／100。AnyDamage 和假人自身直接播放动画的支路已删除，角色复制及无导航贡献配置保留。ASC 使用原生默认 Full 复制模式；本次没有新增 ASC 派生类或运行时入口。

HealthSet 通过原生属性回调限制 Base／Current：MaxHealth ≥ 1，Health 位于 0～MaxHealth；上限降低时权威端截断保存的生命值，提高上限不回血。`/Game/AsuraNext/Combat/Effects/GE_Damage` 只含一次 Health Add -10，并携带命中 Cue。当前 `GCN_PinecoBox_Hit.OnExecute` 使用 MyTarget 的 SkeletalMeshComponent，原生 SingleNode 播放 `AM_TrainingDummy_Hit` 后随机跳到一个 Hit Section；三段独立结束，单次命中不再播放完整 Ok。随机只属于各客户端的 Cue 表现，服务端伤害结算不变。

按用户明确授权，通过 Rider MCP 终端在编辑器退出后构建 `AsuraNextEditor Win64 Development -NoHotReloadFromIDE`，退出码 0、耗时 30.51 秒；HealthSet 与生成代码重新编译链接，随后冷启动加载正式 AsuraNext DLL。未使用 Live Coding／Hot Reload。Rider 的通用 build_solution_start 在本安装中展开了整个解决方案，已停止该次构建，最终成功依据为明确 Editor 目标的独立构建日志。

冷启动后另行定位到 Monolith 0.24.0 的确定性内存写错：gas.link_cue_to_effect 将真实的 TArray<FGameplayEffectCue> 错当作 FInheritedTagContainer，导致 GE PreSave 中崩溃。这次复现独立于 Live Coding；已停用该动作及同一错误辅助函数的相关动作，改用 WoolooBlueprint.SetClassDefaults 按原生反射类型写入 GameplayCues。正确写入后 GE 已编译、保存并重载成功；未修改引擎或引擎目录中的 Monolith。禁用动作与调用栈位置见 [属性接入文档](../../Source/AsuraNext/GameplayAttributes/README.md#构建与编辑器交接)。

最终检查：假人、Combo GA、Cue、GE 均编译为错误 0／警告 0；五个相关资产保存后经原生 ReloadPackages 从磁盘重载，初始化表、ASC Starting Data、GE -10 与 Cue 数组、GA／Cue 连线均回读通过，未保存包为 0。检查并整理了假人 EventGraph、Combo GA EventGraph、Cue OnExecute 的坐标、执行流和中文分块，Cue 的失败分支与网格数据经原生 Reroute 整理。quan1 的 0.3 秒单处攻击 Notify 保留，Montage 总 Notify 数仍为 18。

上述制作阶段未启动 PIE；随后按用户明确要求进行了以下双客户端验证。结构化记录为 Saved/Automation/TrainingDummyGAS/verification.json，冷构建日志为同目录 cold-build.log。

### 双客户端 GAS 验证（2026-11-12）

01:30:01.568～01:39:27.633（Asia/Shanghai），约 566.065 秒墙钟时间，运行一个 Dedicated Server 和两个客户端。原生 DescribeSession 明确核对服务器 Context_1、客户端 Context_2／Context_3，本地玩家均为 0，PlayerId 分别为 256／257，Controller、Pawn、ASC Owner／Avatar 均属于选定世界。末次运行快照服务器游戏时间为 556.935 秒，两个玩家的 Combo 均已自然结束、Montage 为空。

测试通过官方 MCP 的原生 InjectInputAction 驱动现有正式快捷栏 1 号动作 `Asura_IA_Hotbar_1`：快捷栏第 0 格链接 `Linghou_Skill_Quan`，物品使用流程传入 `IA_Linghou_Quan` 作为连招载荷，播放 A011／quan1 的 0.3 秒攻击 Notify。`IA_Linghou_Quan` 本身不是当前输入栈的直接绑定动作；首个探测被原生工具拒绝且未提交输入，随后只核对并使用已有正式入口。没有创建 GYM 测试 IA／IMC、Debug Key 或替代触发支路。

| 单目标用例 | 服务器 Health | 客户端 1 Health | 客户端 2 Health |
| --- | --- | --- | --- |
| 初始 | 100 | 100 | 100 |
| 客户端 1／256 的一次 quan1 | 90 | 90 | 90 |
| 客户端 2／257 的一次 quan1 | 80 | 80 | 80 |

两次预测端／权威端均先记录 LISTEN，再记录 EVENT：首次为 01:37:05.479／05.528，第二次为 01:38:08.979／09.034。两次各扣 10，后续快照保持 80；MaxHealth 始终为 100，另外三个 Multiple Target 作为未命中对照，三端始终为 100。此处验证的是 Single 用例，未据此宣称三目标攻击已经实测。

该次检查中，两个客户端的 Single 假人均由 Embedded 切换为 `AS_TrainingDummy_Ok`，原生网格 GetPosition 回读到 2.1 秒；其他三个目标仍为 Embedded。这个检查只证明完整源动画被播放，没有验证“一拳只做一次受击”。用户随后指出源动画含三次受击，因此该次动画验收不足；以下修正替代其单次受击结论，Health 同步的观测记录仍有效。专用服务器不播放客户端表现，权威 Health 正常扣减。

属性与动画采样使用明确 WorldPath 和目标类，通过公开 GetAttributeSet／GetHealth／GetMaxHealth、GetAnimationAsset／GetPosition 只读查询；现有 Monolith 属性检查只选第一个 PIE 世界，官方工具当前也不提供假人属性读取，因此使用短内联编辑器适配，没有增加脚本文件。输入、技能、GameplayEvent 和 RPC 均未通过 Python 触发。

结束后已停止本次 PIE，原生会话 Worlds=[]、未保存包为 0；本轮未修改生产资产、执行 C++ 编译或 Live Coding。日志没有 Deep PredictionKey Chain Detected、AbilitySystem Error 或 Blueprint Runtime Error；出现 96 条 NetworkPrediction 输入流饥饿恢复记录，未在本轮定位，不作网络稳定性或性能达标结论。零生命、MaxHealth 变化、模拟延迟／丢包和 32 玩家性能尚未验证。

### 受击随机单段播放修正（2026-11-12）

按用户要求将一次命中收敛为三段随机选一段。只修改 `GCN_PinecoBox_Hit.OnExecute`：先通过原生 PlayAnimation 将 SingleNode 资产设为 `AM_TrainingDummy_Hit`，再通过原生 Montage_JumpToSection 跳到所选 Section。Hit_01／02／03 的源范围仍为 0～0.7／0.7～1.4／1.4～2.1 秒，Next Section 均为 None；原 Sequence、Montage、假人和 GE 未改写。原生 SingleNode 支持 Montage，不需要新增 AnimBP 或 C++。

随机使用原生 RandomBoolWithWeight(1/3) 和 RandomBool：先以 1/3 选择 Hit_01，其余均分 Hit_02／03。两个 SelectName 只向本次 JumpToSection 提供一个名称，没有持久化 Cue 实例状态。这是各客户端本地的纯表现随机，同一拳在不同客户端可以选择不同段，不改变目标 ASC 的权威扣血与 Health 复制。

02:11:29.347～02:19:41.972（Asia/Shanghai），约 492.625 秒墙钟时间，重新运行 Dedicated Server＋双客户端 PIE，明确服务器 Context_6、客户端 Context_7／Context_8，以及玩家 259／260 的 Controller、Pawn 和 ASC。正式快捷栏 quan1 共提交 6 次输入：第一次收到 Notify 但未观察到命中，后续 5 次命中使 Single 假人三端 Health 一致由 100 降至 50，其余三个目标保持 100。

本次增加有 60 秒上限、4096 行上限和显式清理的只读采样，通过公开 Montage_GetCurrentSection／Montage_GetPosition 读取明确客户端世界的 Single 假人；输入仍单独通过官方原生 MCP 提交，采样回调不触发输入、技能、事件或 RPC，没有新增 Python 文件。共保存 286 行采样，覆盖 4 次命中的 8 次客户端播放；Hit_01／02／03 均已出现。每次播放只出现一个 Section，位置均未越过该段范围，原生 Active Montage 的采样窗口为 0.625～0.656 秒后结束；资源段长约 0.7 秒、Blend Out 为 0.05 秒。没有再把“完整 2.1 秒动画播完”作为单次受击的通过条件。

OnExecute 的执行链、随机取值区、失败分支与 Reroute 已重新整理，中文注释框互不重叠；蓝图编译错误／警告为 0，保存后从磁盘重载通过。只读回调已结束并注销，PIE 已停止，Worlds=[]、未保存包为 0；本次没有 C++ 编译、Live Coding 或编辑器重启。逐段范围、时间和 Health 证据保存在 Saved/Automation/TrainingDummyGAS/verification.json 的 hit_reaction_correction 字段。

### 统一 Trace Notify 接入与双客户端复验（2026-11-12）

用户完整编译重启后，将 `AM_Linghou_Combo/A011` 的 0.3 秒旧攻击标记替换为唯一的原生 `UAnimNotify_PinecoTrace`。实际属性回读及后续用户重启后的磁盘重载均确认：Tag 为 `Event.Attack`，`TraceConfig` 为 `FPinecoBoxTraceConfig`，半尺寸 `(75,70,100)`、偏移 `(75,0,0)`、旋转零，Pawn 对象类型，`MaxTargets=3`，允许 Dedicated Server 触发。保留其余 Combo、MotionWarping 与现有音效标记；未删除旧 Notify／TP_Box 资源。

项目 `GA_Linghou_Combo.EventGraph` 已改用原生 `WaitGameplayEvent`（精确匹配、非一次性），服务端分支调用 `EvaluateTraceEvent`。成功后遍历原生 HitResult，以 `AbilityTargetDataFromHitResult` 转为 TargetData，再施加现有 `GE_Damage`。移除旧 TargetingSubsystem／PerformTargetingRequest／TP_Box 及旧 OptionalObject 动画比较分支，结束清理不变。蓝图编译通过；7 个中文职责注释框无重叠，6 个 Reroute 整理长数据线。`ResolveComboTargetLocation` 的 47 个节点及接线与修改前一致。

用户明确授权本轮在 GYM 例外通过原生 MCP 驱动已有正式快捷栏 `Asura_IA_Hotbar_1`，不新增 IA／IMC 或 Debug Key。输入不通过 Python 激活技能、发送 GameplayEvent 或 RPC。此前非本轮的单机会话由用户结束；编辑器调试暂停后由用户重启。下面记录仅属于重启后本轮启动并停止的靶场会话，不把先前单机空结果计入验收。

05:54:15.241～06:05:27.261（Asia/Shanghai），约 672.020 秒墙钟时间，运行 Dedicated Server＋双客户端 PIE。官方 `DescribeSession` 明确核对服务器 Context_1、客户端 Context_2／Context_3，两个本地玩家均为索引 0、PlayerId 256／257；Controller、Pawn、ASC Owner／Avatar 均属于各自世界。只读采样使用这些明确 WorldPath，通过公开 AttributeSet 与 AnimInstance getter 获取 Health 和动画；每次采样上限 30 秒／4096 行，输入仍单独经官方原生 MCP 提交。

| quan1 用例 | 服务端 Single Health | 客户端 1 | 客户端 2 | 服务端结果数 |
| --- | --- | --- | --- | --- |
| 初始 | 100 | 100 | 100 | — |
| 客户端 1 靠近后命中 | 90 | 90 | 90 | 1 |
| 客户端 2 原站位出拳（距靶约 400 cm） | 90 | 90 | 90 | 0 |
| 客户端 2 靠近后命中 | 80 | 80 | 80 | 1 |
| 客户端 2 重复命中 | 70 | 70 | 70 | 1 |

四次正式输入中，预测端与对应权威端均先 LISTEN、后 EVENT；只有服务端输出结果，顺序为 `1,0,1,1`，无 REJECT。其余三个假人三端始终为 100，所有 MaxHealth 为 100。命中时来源位置分别约 `(-2035.286,-1080.805,92.150)`／Yaw 4.862° 和 `(-2041.699,-1100,92.150)`／Yaw 0°，Single 位于 `(-1850,-1100,92.4)`。目标中心虽在前方 150 cm 外，其半径 50 cm 的胶囊与 Box 相交，符合 Overlap 语义，不代表查询体被放大。

捕获前两次命中的 4 次客户端受击播放：各次只出现一个 Section，实际为 Hit_01／Hit_03；位置始终停留在所选段范围内，观测到活动 Montage 持续 0.625～0.641 秒后结束，没有连续播放三段。重复命中的扣血已验证，未另采样其动画。随机表现仍可在两客户端选到不同 Section，Health 权威值一致。

完成后两个玩家的 Combo 均自然结束、Montage 为空，采样已结束并注销，PIE 已停止，原生会话 `Worlds=[]`、未保存包为 0。日志中 Deep PredictionKey Chain、Blueprint Runtime Error 与事件拒绝均为 0；出现 114 条 NetworkPrediction 输入流饥饿恢复记录，本轮未定位，不作性能或网络稳定性达标结论。`MaxTargets=3` 已持久化并静态核对，本轮没有摆放四个有效目标验证截断，也未验证死亡、MaxHealth 变化、延迟丢包或 32 玩家负载。未执行 C++ 编译或 Live Coding；证据追加到 `Saved/Automation/TrainingDummyGAS/verification.json` 的 `unified_trace_migration`，保留既有验证记录。
