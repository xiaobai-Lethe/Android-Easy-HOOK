/**
 * 2026年08月28日00时40分
 * 屏下指纹修复的接入点。
 *
 * 这是使用 Easy Hook 实现一个完整功能的实例：用声明式宏拿到进程启动时机，
 * 再把后续复杂逻辑交给 user/fod 目录下的模块。
 *
 * 修复目标是让 Flyme 在小米硬件上正确驱动屏下指纹：
 *   system_server  修正传感器类型与位置、修正调度器类型、管理 FOD 会话、解开卡死任务；
 *   Settings 与 SystemUI  识别指纹 Binder 事务，补齐按下、抬起与会话关闭动作；
 *   其余进程  完全不介入。
 *
 * 整套修复分为两端：JNI 反射负责判断"何时该亮"并修正传感器与调度器状态，
 * 小米 AIDL 与 mi_display 节点负责实际执行点亮。两端缺一不可。
 * 2026 BY white
 */

#include "easy_hook/easy_hook.hpp"

#include "art_demo/system_update_hook.hpp"
#include "fod/binder/binder_hook.hpp"
#include "easy_hook/core/log.hpp"
#include "fod/jni/jni_env.hpp"
#include "fod/runtime/fod_runtime.hpp"

#include <string_view>

#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

/**
 * 2026年08月28日00时40分
 * 总开关。关闭后本文件不安装任何 hook，框架退化为一个不做事的空壳，
 * 便于在排查其他问题时快速排除指纹修复的影响。
 * 2026 BY white
 */
#define FOD_FIX_ENABLED 1

#if FOD_FIX_ENABLED

namespace {

constexpr std::string_view kLogTag = "fod-entry";

/**
 * 2026年08月28日00时40分
 * 由 Binder 拦截在每个进程的第一次事务上回调，驱动本进程的修复启动。
 * 运行时内部按 pid 做一次性判断，这里无需担心重复调用。
 * 2026 BY white
 */
void OnPostFork(JNIEnv* env) noexcept {
    /**
     * 2026年08月28日01时20分
     * 这里是框架中唯一一处"进程已 fork 完毕且 JNIEnv 可用"的时机，
     * ART hook 演示同样需要这个时机，因此一并挂在此处。
     * 该函数自身判断进程并做一次性保护，非设置进程会立即返回。
     * 2026 BY white
     */
    easy_user_hooks::art_demo::MaybeInstallInSettings(env);

    const fod::runtime::FodStartStatus status = fod::runtime::FodRuntime::Start(env);
    if (status == fod::runtime::FodStartStatus::kAlreadyStarted) {
        return;
    }
    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag, "runtime start status=%s",
                           fod::runtime::FodStartStatusName(status));
}

/**
 * 2026年08月28日00时40分
 * 在取得 JNIEnv 后装上 RegisterNatives 拦截，这是捕获 BinderProxy 原生事务方法的前提。
 * 该拦截必须早于 startReg 注册框架原生方法，否则会错过 BinderProxy。
 * 2026 BY white
 */
void InstallBinderDiscovery(JNIEnv* env, const char* source) noexcept {
    if (env == nullptr) {
        return;
    }
    fod::binder::SetPostForkCallback(OnPostFork);
    const bool installed = fod::binder::InstallRegisterNativesHook(env);
    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                           "jni env acquired from %s, register_natives_hook=%d", source,
                           installed ? 1 : 0);
}

/**
 * 2026年08月28日00时40分
 * 取得进程中已创建的 JavaVM 并附着当前线程，随后装上 Binder 拦截。
 * 用于 startVm 拦截未命中时的兜底路径。
 * 2026 BY white
 */
bool TryAttachAndInstall() noexcept {
#if defined(__ANDROID__)
    using GetCreatedJavaVmsFunction = jint (*)(JavaVM**, jsize, jsize*);
    auto get_created_vms =
        reinterpret_cast<GetCreatedJavaVmsFunction>(dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
    if (get_created_vms == nullptr) {
        return false;
    }

    JavaVM* vm = nullptr;
    jsize count = 0;
    if (get_created_vms(&vm, 1, &count) != JNI_OK || count <= 0 || vm == nullptr) {
        return false;
    }

    fod::jni::ScopedThreadEnv scoped_env(vm);
    JNIEnv* env = scoped_env.get();
    if (env == nullptr) {
        return false;
    }

    InstallBinderDiscovery(env, "created-vm");
    OnPostFork(env);
    return true;
#else
    return false;
#endif
}

}  // namespace

/**
 * 2026年08月28日00时40分
 * 拦截 AndroidRuntime::startVm，这是本框架能拿到 JNIEnv 的最早且最可靠的位置。
 *
 * 不选 JNI_CreateJavaVM 的原因：它由 APEX 中的 libnativehelper.so 提供，
 * 该库要等运行期 dlopen 才出现，库入口构造函数阶段尚不可见；
 * 等模块加载监听补装时虚拟机往往已创建完毕，拦截失去意义。
 * libandroid_runtime.so 则是 app_process64 的直接依赖，构造函数阶段即可解析。
 *
 * 不选 onVmCreated 的原因：它是虚函数，实际派发到 app_process 中的 AppRuntime 覆盖版本，
 * 拦截基类实现根本不会被调用。startVm 非虚，且出参直接给出 JNIEnv。
 * 2026 BY white
 */
EASY_HOOK_SYMBOL(AndroidRuntimeStartVm, "libandroid_runtime.so",
                 "_ZN7android14AndroidRuntime7startVmEPP7_JavaVMPP7_JNIEnvbb", int, void* thiz,
                 JavaVM** vm, JNIEnv** env, bool zygote, bool primary_zygote) {
    const int result = EASY_ORIGINAL(thiz, vm, env, zygote, primary_zygote);
    if (result == 0 && env != nullptr && *env != nullptr) {
        InstallBinderDiscovery(*env, "startVm");
    }
    return result;
}

/**
 * 2026年08月28日00时40分
 * 拦截 AndroidRuntime::start 作为兜底。
 *
 * 修饰名中的 7String8 不能写成 8String8：该数字是后随标识符的长度，而 String8 为七个字符。
 * 原实现此处写错，导致这个拦截从未真正安装过，问题一直被 JNI_CreateJavaVM 兜底所掩盖。
 * 2026 BY white
 */
EASY_HOOK_SYMBOL(AndroidRuntimeStart, "libandroid_runtime.so",
                 "_ZN7android14AndroidRuntime5startEPKcRKNS_6VectorINS_7String8EEEb", void,
                 void* thiz, const char* class_name, const void* options, bool zygote) {
    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                           "android runtime starting class=%s zygote=%d",
                           class_name != nullptr ? class_name : "<null>", zygote ? 1 : 0);
    fod::binder::SetPostForkCallback(OnPostFork);

    /**
     * 2026年08月28日00时40分
     * system_server 通常不会从 start 返回，因此调用原函数之前必须先尝试一次；
     * 返回之后的第二次尝试只为其他进程兜底。
     * 2026 BY white
     */
    static_cast<void>(TryAttachAndInstall());
    EASY_ORIGINAL(thiz, class_name, options, zygote);
    static_cast<void>(TryAttachAndInstall());
}

#endif  // FOD_FIX_ENABLED
