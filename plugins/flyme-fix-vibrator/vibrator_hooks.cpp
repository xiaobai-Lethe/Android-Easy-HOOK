/**
 * 2026年08月29日
 * Flyme 震动修复接入点(ART / Java 方法 hook)。
 *
 * Flyme 把 VibratorController 的震动出口全改道走 AAC RichTap，而移植到的小米硬件没有该
 * HAL，导致震动全失效。本插件用 LSPlant 挂 system_server 里的三个出口：
 *   on(PrebakedSegment, long, long)  -> 翻译成 mNativeWrapper.perform(...)
 *   on(long, long, long)             -> 翻译成 mNativeWrapper.on(...)
 *   setAmplitude(float)              -> 补上 mNativeWrapper.setAmplitude(...)
 * 也就是绕过 RichTap、恢复标准 android.hardware.vibrator 路径(dumpsys 已验证可用)。
 * 其中 setAmplitude 决定震感强弱：Flyme 只喂给已失效的 RichTap，不补这一刀的话所有
 * 定时/波形震动都跑在 HAL 默认幅度上，表现为“能震但很弱”。
 *
 * 入口选 libandroid_servers.so 的 JNI_OnLoad：它只在 system_server 加载，天然限定进程，
 * 不需要 EASY_HOOK_ONLY_IN(那会全框架过滤，误伤别的插件)。
 * 2026 BY white
 */

#include "easy_hook/art/art_hook.hpp"
#include "easy_hook/core/log.hpp"
#include "flyme-common/system_server_init.hpp"
#include "flyme-fix-vibrator/vibrator_reroute_hooker_dex.h"

#define FLYME_VIBRATOR_FIX_ENABLED 1

#if FLYME_VIBRATOR_FIX_ENABLED && defined(__ANDROID__)

#include <jni.h>

#include <atomic>
#include <cstddef>
#include <pthread.h>
#include <string_view>
#include <unistd.h>

namespace flyme_vibrator {
namespace {

using easy::core::Log;
using easy::core::LogLevel;

constexpr std::string_view kLogTag = "flyme-vib";
constexpr char kHookerClassName[] = "org.easyhook.flymevib.VibratorRerouteHooker";
constexpr char kVibratorControllerBinary[] = "com/android/server/vibrator/VibratorController";
constexpr char kVibratorControllerDot[] = "com.android.server.vibrator.VibratorController";

// 三个目标:都在 VibratorController 上，mode 区分翻译分支。
struct TargetMethod final {
    const char* name;
    const char* signature;
    int mode;  // 0=Prebaked(perform), 1=时长(on), 2=幅度(setAmplitude)
};

constexpr TargetMethod kTargets[] = {
    {"on", "(Landroid/os/vibrator/PrebakedSegment;JJ)J", 0},
    {"on", "(JJJ)J", 1},
    /**
     * 2026年08月29日
     * setAmplitude 同样被 Flyme 改道:它只把幅度喂给已失效的 RichTap，从不调用
     * mNativeWrapper.setAmplitude，导致定时/波形震动全跑在 HAL 默认幅度上（震感偏弱）。
     * 2026 BY white
     */
    {"setAmplitude", "(F)V", 2},
};
constexpr std::size_t kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

std::atomic<pid_t> g_installed_pid{-1};
JavaVM* g_java_vm = nullptr;
jobject g_preferred_loader = nullptr;  // system_server 的类加载器(含 services.jar)
jobject g_dex_class_loader = nullptr;
jobject g_hooker_objects[kTargetCount] = {};

bool HasPendingException(JNIEnv* env, const char* stage) noexcept {
    if (env->ExceptionCheck() == JNI_FALSE) {
        return false;
    }
    env->ExceptionDescribe();
    env->ExceptionClear();
    Log::Print(LogLevel::kError, kLogTag, "java exception at stage=%s", stage);
    return true;
}

jobject CaptureContextClassLoader(JNIEnv* env) noexcept {
    jclass thread_class = env->FindClass("java/lang/Thread");
    if (thread_class == nullptr || HasPendingException(env, "find_thread")) {
        return nullptr;
    }
    jmethodID current = env->GetStaticMethodID(thread_class, "currentThread",
                                               "()Ljava/lang/Thread;");
    jmethodID get_loader = env->GetMethodID(thread_class, "getContextClassLoader",
                                            "()Ljava/lang/ClassLoader;");
    if (current == nullptr || get_loader == nullptr ||
        HasPendingException(env, "thread_methods")) {
        return nullptr;
    }
    jobject thread = env->CallStaticObjectMethod(thread_class, current);
    if (thread == nullptr || HasPendingException(env, "current_thread")) {
        return nullptr;
    }
    jobject loader = env->CallObjectMethod(thread, get_loader);
    if (HasPendingException(env, "context_loader") || loader == nullptr) {
        return nullptr;
    }
    return env->NewGlobalRef(loader);
}

jobject CreateDexClassLoader(JNIEnv* env) noexcept {
    jobject buffer = env->NewDirectByteBuffer(
        const_cast<unsigned char*>(kVibratorRerouteHookerDex),
        static_cast<jlong>(kVibratorRerouteHookerDexSize));
    if (buffer == nullptr || HasPendingException(env, "new_direct_byte_buffer")) {
        return nullptr;
    }

    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    if (loader_class == nullptr || HasPendingException(env, "find_class_loader")) {
        return nullptr;
    }
    jmethodID get_system = env->GetStaticMethodID(loader_class, "getSystemClassLoader",
                                                  "()Ljava/lang/ClassLoader;");
    if (get_system == nullptr || HasPendingException(env, "get_system_loader_id")) {
        return nullptr;
    }
    // 父加载器优先用 system_server 的类加载器，回退到系统类加载器。
    jobject parent = g_preferred_loader != nullptr
                         ? g_preferred_loader
                         : env->CallStaticObjectMethod(loader_class, get_system);
    if (HasPendingException(env, "parent_loader")) {
        return nullptr;
    }

    jclass memory_loader_class = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    if (memory_loader_class == nullptr || HasPendingException(env, "find_memory_loader")) {
        return nullptr;
    }
    jmethodID constructor = env->GetMethodID(
        memory_loader_class, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    if (constructor == nullptr || HasPendingException(env, "memory_loader_ctor")) {
        return nullptr;
    }
    jobject loader = env->NewObject(memory_loader_class, constructor, buffer, parent);
    if (loader == nullptr || HasPendingException(env, "new_memory_loader")) {
        return nullptr;
    }
    return env->NewGlobalRef(loader);
}

jclass LoadClassByName(JNIEnv* env, jobject loader, const char* dot_name) noexcept {
    jclass loader_class = env->GetObjectClass(loader);
    if (loader_class == nullptr || HasPendingException(env, "loader_class")) {
        return nullptr;
    }
    jmethodID load_class = env->GetMethodID(loader_class, "loadClass",
                                            "(Ljava/lang/String;)Ljava/lang/Class;");
    if (load_class == nullptr || HasPendingException(env, "load_class_id")) {
        return nullptr;
    }
    jstring name = env->NewStringUTF(dot_name);
    if (name == nullptr || HasPendingException(env, "class_name")) {
        return nullptr;
    }
    jobject loaded = env->CallObjectMethod(loader, load_class, name);
    if (loaded == nullptr || HasPendingException(env, "load_class")) {
        return nullptr;
    }
    return static_cast<jclass>(loaded);
}

// 先试 FindClass，失败再用 system_server 的类加载器加载 services.jar 内部类。
jclass ResolveVibratorController(JNIEnv* env) noexcept {
    jclass found = env->FindClass(kVibratorControllerBinary);
    if (found != nullptr) {
        return found;
    }
    env->ExceptionClear();
    if (g_preferred_loader != nullptr) {
        jclass via_loader = LoadClassByName(env, g_preferred_loader, kVibratorControllerDot);
        if (via_loader != nullptr) {
            return via_loader;
        }
    }
    return nullptr;
}

jclass LoadHookerClass(JNIEnv* env) noexcept {
    return LoadClassByName(env, g_dex_class_loader, kHookerClassName);
}

void MakeAccessible(JNIEnv* env, jobject reflected) noexcept {
    jclass accessible = env->FindClass("java/lang/reflect/AccessibleObject");
    if (accessible == nullptr || HasPendingException(env, "find_accessible")) {
        return;
    }
    jmethodID set_accessible = env->GetMethodID(accessible, "setAccessible", "(Z)V");
    if (set_accessible == nullptr || HasPendingException(env, "set_accessible_id")) {
        return;
    }
    env->CallVoidMethod(reflected, set_accessible, JNI_TRUE);
    static_cast<void>(HasPendingException(env, "set_accessible"));
}

jobject CreateHookerObject(JNIEnv* env, jclass hooker_class, int mode) noexcept {
    jmethodID constructor = env->GetMethodID(hooker_class, "<init>", "()V");
    if (constructor == nullptr || HasPendingException(env, "hooker_ctor_id")) {
        return nullptr;
    }
    jobject hooker = env->NewObject(hooker_class, constructor);
    if (hooker == nullptr || HasPendingException(env, "new_hooker")) {
        return nullptr;
    }
    jfieldID mode_field = env->GetFieldID(hooker_class, "mode", "I");
    if (mode_field == nullptr || HasPendingException(env, "mode_field")) {
        return nullptr;
    }
    env->SetIntField(hooker, mode_field, static_cast<jint>(mode));
    return env->NewGlobalRef(hooker);
}

bool InstallOneTarget(JNIEnv* env, jclass vibrator_controller, jclass hooker_class,
                      jobject callback_method, jfieldID backup_field,
                      const TargetMethod& target, jobject& hooker_slot) noexcept {
    jmethodID method_id = env->GetMethodID(vibrator_controller, target.name, target.signature);
    if (method_id == nullptr || HasPendingException(env, "target_method_id")) {
        Log::Print(LogLevel::kError, kLogTag, "no %s%s", target.name, target.signature);
        return false;
    }
    jobject target_method = env->ToReflectedMethod(vibrator_controller, method_id, JNI_FALSE);
    if (target_method == nullptr || HasPendingException(env, "to_reflected")) {
        return false;
    }

    hooker_slot = CreateHookerObject(env, hooker_class, target.mode);
    if (hooker_slot == nullptr) {
        return false;
    }

    jobject backup =
        easy::art::ArtHook::HookMethod(env, target_method, hooker_slot, callback_method);
    if (backup == nullptr || HasPendingException(env, "lsplant_hook")) {
        Log::Print(LogLevel::kError, kLogTag, "hook failed mode=%d", target.mode);
        return false;
    }

    MakeAccessible(env, backup);
    env->SetObjectField(hooker_slot, backup_field, backup);
    if (HasPendingException(env, "set_backup")) {
        return false;
    }

    /**
     * 2026年08月29日
     * system_server 跑在 AOT 编译的 boot image 上，on() 很可能已被调用方内联，
     * 这种情况下即使 ArtMethod 被 hook，内联的调用点仍走旧代码，回调不触发、震动不改道。
     * 装好 hook 后对目标方法做一次去优化，强制其调用回落到被 hook 的实现，覆盖内联路径。
     * 失败不致命：多数路径本就未内联，仅记录以便排查。
     * 2026 BY white
     */
    if (easy::art::ArtHook::Deoptimize(env, target_method)) {
        Log::Print(LogLevel::kInfo, kLogTag, "deoptimized %s%s", target.name, target.signature);
    } else {
        static_cast<void>(HasPendingException(env, "deoptimize"));
        Log::Print(LogLevel::kInfo, kLogTag, "deoptimize skipped %s%s", target.name,
                   target.signature);
    }

    Log::Print(LogLevel::kInfo, kLogTag, "hooked %s%s mode=%d", target.name, target.signature,
               target.mode);
    return true;
}

bool InstallHook(JNIEnv* env) noexcept {
    const easy::art::ArtHookInitStatus init_status = easy::art::ArtHook::Init(env);
    if (init_status != easy::art::ArtHookInitStatus::kOk &&
        init_status != easy::art::ArtHookInitStatus::kAlreadyInitialized) {
        Log::Print(LogLevel::kError, kLogTag, "art init failed status=%s",
                   easy::art::ArtHookInitStatusName(init_status));
        return false;
    }

    g_dex_class_loader = CreateDexClassLoader(env);
    if (g_dex_class_loader == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "callback dex injection failed");
        return false;
    }
    jclass hooker_class = LoadHookerClass(env);
    if (hooker_class == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "load hooker class failed");
        return false;
    }
    jclass vibrator_controller = ResolveVibratorController(env);
    if (vibrator_controller == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "cannot resolve VibratorController");
        return false;
    }

    jmethodID callback_id =
        env->GetMethodID(hooker_class, "callback", "([Ljava/lang/Object;)Ljava/lang/Object;");
    if (callback_id == nullptr || HasPendingException(env, "callback_id")) {
        return false;
    }
    jobject callback_method = env->ToReflectedMethod(hooker_class, callback_id, JNI_FALSE);
    if (callback_method == nullptr || HasPendingException(env, "to_reflected_callback")) {
        return false;
    }
    jfieldID backup_field =
        env->GetFieldID(hooker_class, "backup", "Ljava/lang/reflect/Method;");
    if (backup_field == nullptr || HasPendingException(env, "backup_field")) {
        return false;
    }

    std::size_t installed = 0;
    for (std::size_t index = 0; index < kTargetCount; ++index) {
        if (InstallOneTarget(env, vibrator_controller, hooker_class, callback_method,
                             backup_field, kTargets[index], g_hooker_objects[index])) {
            ++installed;
        }
    }
    Log::Print(LogLevel::kInfo, kLogTag, "vibrator reroute installed %zu/%zu", installed,
               kTargetCount);
    return installed > 0;
}

void* InstallThreadMain(void*) noexcept {
    if (g_java_vm == nullptr) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    if (g_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "attach install thread failed");
        return nullptr;
    }
    static_cast<void>(InstallHook(env));
    g_java_vm->DetachCurrentThread();
    return nullptr;
}

void StartInstallOnce(JNIEnv* env) noexcept {
    const pid_t current = getpid();
    pid_t expected = g_installed_pid.load(std::memory_order_acquire);
    if (expected == current) {
        return;
    }
    if (!g_installed_pid.compare_exchange_strong(expected, current,
                                                 std::memory_order_acq_rel)) {
        return;
    }

    if (env->GetJavaVM(&g_java_vm) != JNI_OK || g_java_vm == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "cannot obtain JavaVM");
        return;
    }
    g_preferred_loader = CaptureContextClassLoader(env);  // 可能为 null，ResolveVibratorController 会兜底

    pthread_t thread{};
    if (pthread_create(&thread, nullptr, InstallThreadMain, nullptr) != 0) {
        Log::Print(LogLevel::kError, kLogTag, "cannot start install thread");
        return;
    }
    pthread_detach(thread);
    Log::Print(LogLevel::kInfo, kLogTag, "install thread started pid=%d", current);
}

// 由 flyme-common 分发器在 system_server 的 JNI_OnLoad 时调用(每进程一次)。
void OnSystemServerInit(JNIEnv* env) noexcept {
    StartInstallOnce(env);
}

const flyme_common::SystemServerInitRegistrar kRegistrar{&OnSystemServerInit};

}  // namespace
}  // namespace flyme_vibrator

#endif  // FLYME_VIBRATOR_FIX_ENABLED && __ANDROID__
