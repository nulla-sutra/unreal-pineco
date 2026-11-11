# Pineco Context

本文件集中维护 Pineco 的统一术语、职责和长期语义。架构决策、取舍与实施记录见 [ADR 0004](Docs/ADR/0004-notify-triggered-targetactor-execution.md)；Niagara 运动来源见 [ADR 0003](Docs/ADR/0003-niagara-baked-motion-and-authoritative-evaluation.md)。

## 职责与结果

- **Targeting Preset / 目标选择配置**：复用原生 `UTargetingPreset`；由原生选择、过滤和排序任务组成。当前先用 Gameplay Targeting System 的 `UTargetingSelectionTask_AOE` 验证面向技能区域的 Box 判定。
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

原生 Box 路线直接使用 Gameplay Targeting System 的区域 Overlap 和 Actor 结果去重；不沿武器或 Socket 轨迹采样。这里的命中表示目标碰撞体与查询体重叠，不要求 Actor 原点落入 Box。原生 AOE 填入的 Actor 位置不代表真实接触点。Gameplay Targeting System 自身依赖 GameplayAbilities；上述通用核心独立性描述的是现有 PinecoTrace 边界。

最小 GYM 用例通过原生 `UAbilityTask_PerformTargeting::PerformTargetingRequest` 发起同步请求。必须在 `OnTargetReady` 回调内读取结果；即时请求在完成回调后释放句柄。首次 Box 用例只验证选择结果，不接入伤害、共享命中策略、Montage Notify 或预测。

## Box GYM 用例

测试专用资产目录为 `/Game/Toilet/GYM/PinecoBox`。专用 GameMode 沿用公共玩家、物品与角色装配，已移除旧 `BPC_GymPinecoBoxSetup`。当前假人是宿主 Avatar 下的正式角色 `/Game/AsuraNext/Avatar/NPC/TrainingDummy/PCharacter_TrainingDummy`；GYM 与 `TP_Box` 引用该类，Nickit 持有其可复用美术资源。

当前 Box 由正式拳招 quan1 的播放触发：`ST_Linghou_Combo` 的 A011 第一式播放 `AM_Linghou_Combo/A011`，0.3 秒处的 `AN_AttackBoxEvent` 发送本地 `Event.Attack.Box`，项目 `GA_Linghou_Combo` 监听并仅在权威端执行 `TP_Box`。按用户本轮明确要求，不再增加专用测试按键或 Debug Key。旧 IA／IMC、F 入口、独立 Box GA 和授予它的 Setup 已清理；当前布局为 Single 1 个与 Multiple 3 个目标。

- 查询尺寸：`HalfExtent = (300, 150, 100)` cm，全尺寸 600 × 300 × 200 cm。
- 来源：Avatar；沿其朝向前移 300 cm，忽略自身，只接受测试目标类。
- 测试目标：框内、Actor 原点在外但胶囊跨边界、完全在外、身后；预期仅前两者被接受，共 2 个 Actor。
- 沿用共享 `GYM_Lighting`、现有 GYM 材质及 Framework 的 GameMode／Setup 生命周期。

以上尺寸和边界场景保留首轮用例约束；资产创建、后续布局及验证状态以本文末尾和 ADR 实施记录为准，不将预期结果当作运行结果。

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
