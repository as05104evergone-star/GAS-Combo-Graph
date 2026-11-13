# GAS 连招图（GAS Combo Graph）

面向 UE 战斗策划的连招状态机。把 Montage 拖进图，在招式上填时间和伤害行，在转移上选按键、优先级与条件。角色接好输入和命中接口之后，换连招就换图资产。

```text
起手 → 轻击 1 → [左键] → 轻击 2 → [左键] → 轻击 3
                  └ [右键 + 命中] → 追击
```

## 插件依赖

**GASComboGraph 单向依赖 GASCombatBridge 1.1.0。** 两个插件分别安装。GASCombatBridge 可以独立使用。

| 组件 | 职责 |
|---|---|
| GASComboGraph | 图编辑、输入缓冲、Montage 切段、伤害窗口、挥击编号和命中去重 |
| GASCombatBridge | ASC 初始化、通用战斗属性集、伤害效果施加、受击和死亡事件 |
| Enhanced Input | 角色的输入映射和 Input Action |
| 你的角色蓝图 | 武器 Trace、目标防御、硬直和闪避等动作互斥 |

GASComboGraph 的描述文件声明了 GASCombatBridge 和 Enhanced Input。伤害通过桥接的 `ApplyDamageToTarget` 提交，按 `Data.Damage` 传给 Gameplay Effect。Combo Graph（mklabs）是交互设计参考，安装依赖以 [GASComboGraph.uplugin](GASComboGraph.uplugin) 为准。[GASCombatBridge 仓库](https://github.com/as05104evergone-star/GAS-Combat-Bridge)

## 安装与上手

当前源码在 **UE 5.7.4 / Windows x64** 验证。需要 Visual Studio 2022 的 **使用 C++ 的游戏开发** 工作负载与 Windows SDK。关闭编辑器，从项目根目录执行：

```powershell
git clone https://github.com/as05104evergone-star/GAS-Combat-Bridge.git Plugins/GASCombatBridge
git -C Plugins/GASCombatBridge checkout 8135bc5657d721098f9b1bcbac686d0146402d07
git clone https://github.com/as05104evergone-star/GAS-Combo-Graph.git Plugins/GASComboGraph
```

已安装 GASCombatBridge 的工程可复用同一份。确保引擎和工程的插件目录中只有一份同名桥接插件。用 UE 5.7.4 打开工程，在 **编辑 → 插件** 启用 **GAS 连招图** 后重启，按提示编译缺少的模块。

1. 在 Character 的蓝图添加 **GAS战斗桥接组件**，属性集选择 `GASCombatCoreAttributeSet`。试打目标也要初始化这套属性。
2. 在内容浏览器创建 **GAS 连招图**，设置起手输入，拖入独立 Montage 并连好转移。校验后保存。
3. 给玩家添加 **GAS 连招执行器** 并指定图资产。把 Input Action 的 `Started` 接到 **提交连招输入**。
4. 在伤害窗口内把 Trace 得到的目标和本次挥击编号交给 **按挥击编号提交命中**。

[完整中文使用说明书](使用说明.md)

本仓库发布插件源码与文档。上手时使用自己项目的角色、Montage、输入和数据资产。首版面向单人和权威端执行，联网输入转发、复制和预测由项目接入。
