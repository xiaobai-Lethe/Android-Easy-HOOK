# 内置插件

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

`plugins/` 下的插件默认不编译，按需[开启](plugins.md#开关内置插件)。

| 插件 | 生效进程 | 说明 |
| --- | --- | --- |
| [`flyme-FOD-xiaomi`](../plugins/flyme-FOD-xiaomi) | system_server、Settings、SystemUI | 小米 + Flyme 屏下指纹修复 |
| [`flyme-fix-aod`](../plugins/flyme-fix-aod) | system_server | 小米 + Flyme 息屏显示（AOD）修复，依赖 `flyme-common` |
| [`flyme-fix-vibrator`](../plugins/flyme-fix-vibrator) | system_server | 小米 + Flyme 震动修复，依赖 `flyme-common` |
| [`flyme-common`](../plugins/flyme-common) | system_server | 公共库：`system_server` 初始化入口分发器 |
| [`art_demo`](../plugins/art_demo) | Settings | ART 方法 hook 演示 |
| [`example_hooks.cpp`](../plugins/example_hooks.cpp) | — | 三种定位方式的最小示例（默认由 `EASY_HOOK_EXAMPLE_ENABLED` 关闭） |

## 屏下指纹修复 · `flyme-FOD-xiaomi`

让 Flyme 在小米硬件上正确驱动屏下指纹的按下 / 抬起 / 点亮。JNI 反射负责判断"何时该亮"
并修正传感器类型、位置与调度器状态，小米 AIDL 与 `mi_display` 节点负责实际点亮。

| 进程 | 做的事 |
| --- | --- |
| `system_server` | 修正传感器与调度器，管理 FOD 会话，解开卡死的清理与挑战任务 |
| Settings / SystemUI | 识别指纹 Binder 事务，补齐按下、抬起与会话关闭动作 |
| 其余进程 | 完全不介入 |

## 息屏显示修复 · `flyme-fix-aod`

Flyme 原本靠写 `/sys/kernel/display_drivers/ofp_press_status` 等节点点亮 AOD，这些节点在小米硬件上不存在。
本插件改为**纯系统属性**检测 AOD（`persist.sys.meizu.aod` + `debug.tracing.screen_state`），
进入 doze 时把点亮状态写到小米真实节点。

- **事件驱动**：`__system_property_wait` 阻塞等待屏幕状态变化，几乎无延迟、不空转
- **进 doze 后短时高频补写**：避免面板未就绪导致首写不生效
- **纯 native 线程**：不用 JNI 反射、不依赖 Binder

| 系统属性 | 默认 | 作用 |
| --- | --- | --- |
| `persist.easy_hook.aod_fix_enabled` | `1` | 总开关 |
| `persist.easy_hook.aod_node` | `/sys/class/mi_display/disp-DSI-0/doze_brightness` | 目标节点 |
| `persist.easy_hook.aod_on_value` | `1` | 点亮写入值 |
| `persist.easy_hook.aod_off_value` | `0` | 熄灭写入值 |

## 震动修复 · `flyme-fix-vibrator`

Flyme 把 `VibratorController` 的震动出口全部改道到 AAC RichTap，而小米硬件没有该 HAL，导致震动失效。
本插件用 ART hook 挂住 `system_server` 中的三个出口，恢复标准 `android.hardware.vibrator` 路径：

| 方法 | 改写为 |
| --- | --- |
| `on(PrebakedSegment, long, long)` | `mNativeWrapper.perform(...)` |
| `on(long, long, long)` | `mNativeWrapper.on(...)` |
| `setAmplitude(float)` | 补调 `mNativeWrapper.setAmplitude(...)`，否则震感偏弱 |

改动回调类后运行 `tools/build_flyme_vibrator_dex.sh` 重新生成内嵌 dex。
