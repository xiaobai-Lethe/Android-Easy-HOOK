#pragma once

#include <jni.h>

/**
 * 2026年08月28日00时52分
 * ART(Java 方法)hook 的框架级封装。
 *
 * 本框架原有的三种声明式 hook(符号/偏移/特征码)都作用于原生指令，由 Dobby 实现。
 * 但 Android 应用的绝大多数逻辑运行在 ART 托管环境里，方法可能处于解释执行、
 * 也可能已被 JIT/AOT 编译，直接改写入口地址既不稳定也无法覆盖全部执行路径。
 * ART 方法的 hook 必须由理解 ArtMethod 内部结构的引擎接管，这里选用 LSPlant。
 *
 * 本类只做一件事：把 LSPlant 需要的四个能力——inline hook、inline unhook、
 * libart 精确符号解析、libart 前缀符号解析——用框架已有的组件填齐，
 * 再把 LSPlant 的方法级 hook 接口以最小转发的形式暴露出去。
 * 使用者拿到的是干净的 easy::art 接口，不直接依赖 LSPlant 的类型与头文件。
 * 2026 BY white
 */

namespace easy::art {

/**
 * 2026年08月28日00时52分
 * ArtHook::Init 的结果。初始化只应成功一次，重复调用会短路返回 kAlreadyInitialized。
 * 2026 BY white
 */
enum class ArtHookInitStatus : unsigned char {
    kOk,
    kAlreadyInitialized,
    kInvalidEnvironment,
    kLibartSymbolsUnavailable,
    kLsplantInitFailed,
};

/**
 * 2026年08月28日00时52分
 * 返回初始化状态的可读名称，便于日志排查。
 * 2026 BY white
 */
[[nodiscard]] const char* ArtHookInitStatusName(ArtHookInitStatus status) noexcept;

/**
 * 2026年08月28日00时52分
 * ART hook 引擎门面。全部为静态方法，进程内共享同一份初始化状态。
 * Init 必须在拿到具备隐藏 API 访问权限的 JNIEnv 之后、执行任何方法 hook 之前调用，
 * 最自然的时机是 ART 虚拟机创建完成的早期(例如 AndroidRuntime::startVm 之后)。
 * 2026 BY white
 */
class ArtHook final {
public:
    ArtHook() = delete;

    /**
     * 2026年08月28日00时52分
     * 载入 libart.so 符号表，用框架组件填齐 LSPlant 的初始化信息并完成初始化。
     * 为与 LSPosed 共存，生成的桩类采用独立的类名/字段名/源名，避免与其默认命名冲突。
     * env 不得为空；成功后方可调用 HookMethod 等接口。
     * 2026 BY white
     */
    [[nodiscard]] static ArtHookInitStatus Init(JNIEnv* env) noexcept;

    /**
     * 2026年08月28日00时52分
     * 是否已成功初始化。未初始化时任何 hook 接口都会直接失败返回。
     * 2026 BY white
     */
    [[nodiscard]] static bool initialized() noexcept;

    /**
     * 2026年08月28日00时52分
     * hook 一个 Java 方法。
     * target_method 是目标方法(java.lang.reflect.Method / Constructor 的 jobject)；
     * hooker_object 承载 hook 上下文，callback_method 是其上签名为
     * "Object 名字(Object[] args)" 的回调方法。返回可反射调用原方法的 backup，失败返回 nullptr。
     * 语义与线程安全性等同于底层 LSPlant::Hook。
     * 2026 BY white
     */
    [[nodiscard]] static jobject HookMethod(JNIEnv* env, jobject target_method,
                                            jobject hooker_object,
                                            jobject callback_method) noexcept;

    /**
     * 2026年08月28日00时52分
     * 撤销先前对某个 Java 方法安装的 hook，成功返回 true。
     * 撤销后再调用其 backup 属未定义行为。
     * 2026 BY white
     */
    [[nodiscard]] static bool UnhookMethod(JNIEnv* env, jobject target_method) noexcept;

    /**
     * 2026年08月28日00时52分
     * 查询某个 Java 方法当前是否处于被 hook 状态。
     * 2026 BY white
     */
    [[nodiscard]] static bool IsMethodHooked(JNIEnv* env, jobject method) noexcept;

    /**
     * 2026年08月28日00时52分
     * 去优化一个方法，强制其调用方以非内联方式回落到被 hook 的实现。
     * 当短方法被调用方内联后 hook 不生效时，可对调用方去优化以让 hook 生效。
     * 2026 BY white
     */
    [[nodiscard]] static bool Deoptimize(JNIEnv* env, jobject method) noexcept;
};

}  // namespace easy::art
