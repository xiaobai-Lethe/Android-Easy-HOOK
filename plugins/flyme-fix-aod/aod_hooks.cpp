/**
 * Flyme 移植到小米：AOD 息屏点亮修复接入点。
 *
 * 背景：Flyme 自带的光学指纹/AOD 框架靠写 /sys/kernel/display_drivers/ofp_press_status、
 * fp_result 等节点来点亮，而这些节点在小米硬件上并不存在。本插件不依赖那套节点，
 * 改为纯属性检测 AOD 状态（persist.sys.meizu.aod + debug.tracing.screen_state），
 * 再把点亮状态写到小米真实存在的显示节点（默认 mi_display 的 doze_brightness，可用属性覆盖）。
 *
 * 与 flyme-FOD-xiaomi 完全解耦：不需要 JNI 反射、不依赖 Binder，只在 system_server 生效。
 */

#include "easy_hook/easy_hook.hpp"

#include "easy_hook/core/log.hpp"
#include "flyme-fix-aod/doze_monitor.hpp"

// 总开关：置 0 则本文件不安装任何 hook，插件退化为空壳。
#define FLYME_AOD_FIX_ENABLED 1

#if FLYME_AOD_FIX_ENABLED && defined(__ANDROID__)

#include <jni.h>

/**
 * 注意：这里【绝对不能】用 EASY_HOOK_ONLY_IN("system_server")。
 * 该宏是全框架级的进程白名单，一旦声明会把整个 libeasyhook.so 的 hook 都限制在该进程，
 * 连 flyme-FOD-xiaomi 在 app_process/zygote 里安装的 startVm/transactNative 一起过滤掉，
 * 导致 FOD 整体失效（表现为按压无反应）。
 * 本插件无需进程过滤即天然只在 system_server 生效：libandroid_servers.so 只在 system_server
 * 加载，因此下面这个 JNI_OnLoad hook 也只会在 system_server 命中。
 */

/**
 * libandroid_servers.so 只在 system_server 加载，它的 JNI_OnLoad 是一个稳定、
 * 进程专属、时机合适的原生入口——不需要 JNIEnv 反射，也不依赖 Binder。
 * 这里先调用原实现（让 system_server 正常注册它的原生方法），再拉起纯属性驱动的
 * AOD 点亮桥接线程（幂等，仅首次真正启动）。
 */
EASY_HOOK_SYMBOL(FlymeAodServerInit, "libandroid_servers.so", "JNI_OnLoad", jint, JavaVM* vm,
                 void* reserved) {
    // 必须先跑原 JNI_OnLoad，否则 system_server 的原生方法不会注册，系统会崩。
    const jint result = EASY_HAS_ORIGINAL() ? EASY_ORIGINAL(vm, reserved) : JNI_VERSION_1_6;

    const flyme_aod::DozeMonitorStatus status = flyme_aod::DozeMonitor::Start();
    easy::core::Log::Print(easy::core::LogLevel::kInfo, "flyme-aod",
                           "system_server init, doze monitor=%s",
                           flyme_aod::DozeMonitorStatusName(status));
    return result;
}

#endif  // FLYME_AOD_FIX_ENABLED && __ANDROID__
