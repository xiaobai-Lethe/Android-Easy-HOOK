#include "art_demo/system_update_hook.hpp"

#include "art_demo/string_replace_hooker_dex.h"
#include "easy_hook/art/art_hook.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/core/process_filter.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <string>
#include <string_view>
#include <unistd.h>

/**
 * 2026年08月28日01时14分
 * ART hook 演示的实现。分三步：注入回调类 dex、解析目标方法、交给 LSPlant 安装。
 * 2026 BY white
 */

namespace easy_user_hooks::art_demo {
namespace {

using easy::core::Log;
using easy::core::LogLevel;

constexpr std::string_view kLogTag = "art-demo";

/**
 * 2026年08月28日01时14分
 * 仅在系统设置进程生效。这个演示会给每一次取字符串的调用增加一次文本比对，
 * 放开到全部进程既无意义也会平白增加开销。
 * 2026 BY white
 */
constexpr std::string_view kTargetProcess = "com.android.settings";

constexpr const char* kHookerClassName = "org.easyhook.demo.StringReplaceHooker";
constexpr const char* kMatchText = "系统更新";
constexpr const char* kReplacementText = "TEST DEMO JAVA HOOK";

/**
 * 2026年08月28日01时14分
 * 进程内一次性标记。fork 出的子进程会继承静态量，因此必须连同 pid 一起校验。
 * 2026 BY white
 */
std::atomic<pid_t> installed_pid{-1};

/**
 * 2026年08月28日01时14分
 * 需要伴随进程终生存活的全局引用。
 * 回调对象由 LSPlant 在每次命中时使用，类加载器则决定回调类是否会被卸载，
 * 两者一旦被回收，后续调用就会落到已失效的引用上。
 * 2026 BY white
 */
jobject g_dex_class_loader = nullptr;

/**
 * 2026年08月28日01时30分
 * 待 hook 的目标方法表。
 *
 * 同时挂 getText 与 getString 是有意为之：Resources.getString 内部会调用 getText，
 * 但这层内部调用可能被 ART 内联掉，只挂其中一个都可能漏掉一条取值路径。
 * 命中判断在回调里按文本比对，多挂一个目标不会造成重复替换。
 * 2026 BY white
 */
struct TargetMethod {
    const char* class_name;
    const char* method_name;
    const char* signature;
};

constexpr TargetMethod kTargets[] = {
    /**
     * 2026年08月28日01时26分
     * XML 属性这条路：Preference 与各类控件的 android:title 由 TypedArray 取值，
     * 内部直接读 AssetManager 的字符串池，不会经过 Resources.getText，
     * 因此必须单独挂上。设置里的"系统更新"正是走的这条路。
     * 2026 BY white
     */
    {"android/content/res/TypedArray", "getText", "(I)Ljava/lang/CharSequence;"},
    {"android/content/res/TypedArray", "getString", "(I)Ljava/lang/String;"},

    /**
     * 2026年08月28日01时26分
     * 代码取值这条路：getString 内部会调 getText，但这层内部调用可能被 ART 内联，
     * 两个都挂才能覆盖完整。
     * 2026 BY white
     */
    {"android/content/res/Resources", "getText", "(I)Ljava/lang/CharSequence;"},
    {"android/content/res/Resources", "getString", "(I)Ljava/lang/String;"},

    /**
     * 2026年08月28日01时26分
     * 显示环节的兜底：无论文案从哪里来，最终都要交给 TextView 显示。
     * 该方法返回 void，靠的是回调里的入参改写而不是返回值替换。
     * 2026 BY white
     */
    {"android/widget/TextView", "setText",
     "(Ljava/lang/CharSequence;Landroid/widget/TextView$BufferType;)V"},
};

constexpr std::size_t kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

/**
 * 2026年08月28日01时30分
 * 每个目标各自的回调对象。它们不能共用：回调靠自身的 backup 字段调用原方法，
 * 共用一个对象会让后装的 hook 覆盖先装的 backup，先装的那个就会调错方法。
 * 2026 BY white
 */
jobject g_hooker_objects[kTargetCount] = {};

/**
 * 2026年08月28日01时14分
 * 检查并清理挂起的 Java 异常。JNI 在异常挂起时继续调用属未定义行为，
 * 因此每一步可能抛出的调用之后都必须立即收口。
 * 2026 BY white
 */
bool HasPendingException(JNIEnv* env, const char* stage) noexcept {
    if (env->ExceptionCheck() == JNI_FALSE) {
        return false;
    }
    env->ExceptionDescribe();
    env->ExceptionClear();
    Log::Print(LogLevel::kError, kLogTag, "java exception at stage=%s", stage);
    return true;
}

/**
 * 2026年08月28日01时14分
 * 当前进程是否为演示目标。
 * 2026 BY white
 */
std::string ReadProcessNameFresh() noexcept {
    const int descriptor = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return {};
    }
    char buffer[256]{};
    const ssize_t count = read(descriptor, buffer, sizeof(buffer) - 1);
    close(descriptor);
    if (count <= 0) {
        return {};
    }
    buffer[count] = '\0';
    return std::string(buffer);
}

/**
 * 2026年08月28日01时26分
 * 判断进程名是否还停留在 zygote fork 出来、尚未改名的状态。
 *
 * 应用进程刚 fork 出来时，/proc/self/cmdline 仍是 zygote 的完整命令行，
 * 要等框架调用 setArgV0 之后才会变成真正的进程名。
 * 注意这个改名发生在同一个 pid 内，任何按 pid 失效的缓存都察觉不到，
 * 所以判断必须基于每次现读的结果。
 * 2026 BY white
 */
bool IsUnnamedZygoteChild(std::string_view name) noexcept {
    return name.empty() || name == "zygote" || name == "zygote64" ||
           name.starts_with("/system/bin/app_process");
}

/**
 * 2026年08月28日01时26分
 * 等待进程改名完成并返回最终进程名。
 * 一旦名字不再是 zygote 的形态即可判定，因此非目标进程通常在数百毫秒内就能退出，
 * 不会白白占用一个线程直到超时。
 * 2026 BY white
 */
std::string WaitForFinalProcessName() noexcept {
    constexpr int kMaxAttempts = 40;
    constexpr useconds_t kPollIntervalUs = 150000;

    std::string name = ReadProcessNameFresh();
    for (int attempt = 0; attempt < kMaxAttempts && IsUnnamedZygoteChild(name); ++attempt) {
        usleep(kPollIntervalUs);
        name = ReadProcessNameFresh();
    }
    return name;
}

/**
 * 2026年08月28日01时14分
 * 把内嵌的回调类 dex 注入当前进程，返回其类加载器的全局引用。
 *
 * 用 InMemoryDexClassLoader 直接从内存加载，不落地临时文件：
 * 被注入的进程未必对任何目录有写权限。
 * 2026 BY white
 */
jobject CreateDexClassLoader(JNIEnv* env) noexcept {
    jobject buffer = env->NewDirectByteBuffer(
        const_cast<unsigned char*>(kStringReplaceHookerDex),
        static_cast<jlong>(kStringReplaceHookerDexSize));
    if (buffer == nullptr || HasPendingException(env, "new_direct_byte_buffer")) {
        return nullptr;
    }

    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    if (loader_class == nullptr || HasPendingException(env, "find_class_loader")) {
        return nullptr;
    }
    jmethodID get_system = env->GetStaticMethodID(loader_class, "getSystemClassLoader",
                                                  "()Ljava/lang/ClassLoader;");
    if (get_system == nullptr || HasPendingException(env, "get_system_class_loader_id")) {
        return nullptr;
    }
    jobject parent = env->CallStaticObjectMethod(loader_class, get_system);
    if (HasPendingException(env, "get_system_class_loader")) {
        return nullptr;
    }

    jclass memory_loader_class = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    if (memory_loader_class == nullptr || HasPendingException(env, "find_memory_dex_loader")) {
        return nullptr;
    }
    jmethodID constructor = env->GetMethodID(
        memory_loader_class, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    if (constructor == nullptr || HasPendingException(env, "memory_dex_loader_ctor_id")) {
        return nullptr;
    }

    jobject loader = env->NewObject(memory_loader_class, constructor, buffer, parent);
    if (loader == nullptr || HasPendingException(env, "new_memory_dex_loader")) {
        return nullptr;
    }
    return env->NewGlobalRef(loader);
}

/**
 * 2026年08月28日01时14分
 * 从注入的类加载器中取出回调类。
 * 必须走 loadClass 而不是 FindClass：FindClass 只在系统与调用方的类加载器中查找，
 * 看不到我们刚注入的这份 dex。
 * 2026 BY white
 */
jclass LoadHookerClass(JNIEnv* env, jobject loader) noexcept {
    jclass loader_class = env->GetObjectClass(loader);
    if (loader_class == nullptr || HasPendingException(env, "get_loader_class")) {
        return nullptr;
    }
    jmethodID load_class = env->GetMethodID(loader_class, "loadClass",
                                            "(Ljava/lang/String;)Ljava/lang/Class;");
    if (load_class == nullptr || HasPendingException(env, "load_class_id")) {
        return nullptr;
    }

    jstring name = env->NewStringUTF(kHookerClassName);
    if (name == nullptr || HasPendingException(env, "hooker_class_name")) {
        return nullptr;
    }
    jobject loaded = env->CallObjectMethod(loader, load_class, name);
    if (loaded == nullptr || HasPendingException(env, "load_hooker_class")) {
        return nullptr;
    }
    return static_cast<jclass>(loaded);
}

/**
 * 2026年08月28日01时14分
 * 取得 Resources.getString(int) 的反射 Method 对象。
 * LSPlant 接收的是 java.lang.reflect.Method 而不是 jmethodID，因此需要转换。
 * 2026 BY white
 */
jobject ResolveTargetMethod(JNIEnv* env, const char* class_name, const char* method_name,
                            const char* signature) noexcept {
    jclass owner = env->FindClass(class_name);
    if (owner == nullptr || HasPendingException(env, "find_target_class")) {
        return nullptr;
    }
    jmethodID method_id = env->GetMethodID(owner, method_name, signature);
    if (method_id == nullptr || HasPendingException(env, "target_method_id")) {
        return nullptr;
    }
    jobject method = env->ToReflectedMethod(owner, method_id, JNI_FALSE);
    if (method == nullptr || HasPendingException(env, "to_reflected_target")) {
        return nullptr;
    }
    return method;
}

/**
 * 2026年08月28日01时30分
 * 对反射对象放开访问检查。
 * LSPlant 返回的 backup 是运行期生成的方法，部分 ROM 上直接反射调用会被访问检查拒绝，
 * 先放开可以避免回调在第一次命中时才失败。
 * 2026 BY white
 */
void MakeAccessible(JNIEnv* env, jobject reflected) noexcept {
    jclass accessible = env->FindClass("java/lang/reflect/AccessibleObject");
    if (accessible == nullptr || HasPendingException(env, "find_accessible_object")) {
        return;
    }
    jmethodID set_accessible = env->GetMethodID(accessible, "setAccessible", "(Z)V");
    if (set_accessible == nullptr || HasPendingException(env, "set_accessible_id")) {
        return;
    }
    env->CallVoidMethod(reflected, set_accessible, JNI_TRUE);
    static_cast<void>(HasPendingException(env, "set_accessible"));
}

/**
 * 2026年08月28日01时14分
 * 实例化回调对象并写入匹配串与替换串。
 * 匹配与替换文本放在字段里由这里注入，dex 本身不含任何具体文案，可以复用。
 * 2026 BY white
 */
jobject CreateHookerObject(JNIEnv* env, jclass hooker_class) noexcept {
    jmethodID constructor = env->GetMethodID(hooker_class, "<init>", "()V");
    if (constructor == nullptr || HasPendingException(env, "hooker_ctor_id")) {
        return nullptr;
    }
    jobject hooker = env->NewObject(hooker_class, constructor);
    if (hooker == nullptr || HasPendingException(env, "new_hooker")) {
        return nullptr;
    }

    jfieldID match_field = env->GetFieldID(hooker_class, "match", "Ljava/lang/String;");
    jfieldID replacement_field =
        env->GetFieldID(hooker_class, "replacement", "Ljava/lang/String;");
    if (match_field == nullptr || replacement_field == nullptr ||
        HasPendingException(env, "hooker_text_fields")) {
        return nullptr;
    }

    jstring match = env->NewStringUTF(kMatchText);
    jstring replacement = env->NewStringUTF(kReplacementText);
    if (match == nullptr || replacement == nullptr ||
        HasPendingException(env, "hooker_text_values")) {
        return nullptr;
    }
    env->SetObjectField(hooker, match_field, match);
    env->SetObjectField(hooker, replacement_field, replacement);

    return env->NewGlobalRef(hooker);
}

/**
 * 2026年08月28日01时14分
 * 完整的安装流程，运行在已附着到虚拟机的独立线程上。
 *
 * backup 字段必须在 Hook 返回之后才能写入：它正是 LSPlant 生成的原方法备份，
 * 没有它回调就拿不到未被改写的原始返回值。
 * 2026 BY white
 */
bool InstallOneTarget(JNIEnv* env, jclass hooker_class, jobject callback_method,
                      jfieldID backup_field, const TargetMethod& target,
                      jobject& hooker_slot) noexcept {
    jobject target_method =
        ResolveTargetMethod(env, target.class_name, target.method_name, target.signature);
    if (target_method == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "cannot resolve %s.%s", target.class_name,
                   target.method_name);
        return false;
    }

    hooker_slot = CreateHookerObject(env, hooker_class);
    if (hooker_slot == nullptr) {
        return false;
    }

    jobject backup =
        easy::art::ArtHook::HookMethod(env, target_method, hooker_slot, callback_method);
    if (backup == nullptr || HasPendingException(env, "lsplant_hook")) {
        Log::Print(LogLevel::kError, kLogTag, "hook failed for %s.%s", target.class_name,
                   target.method_name);
        return false;
    }

    MakeAccessible(env, backup);
    env->SetObjectField(hooker_slot, backup_field, backup);
    if (HasPendingException(env, "set_backup")) {
        return false;
    }

    Log::Print(LogLevel::kInfo, kLogTag, "hooked %s.%s", target.class_name, target.method_name);
    return true;
}

bool InstallHook(JNIEnv* env) noexcept {
    const easy::art::ArtHookInitStatus init_status = easy::art::ArtHook::Init(env);
    if (init_status != easy::art::ArtHookInitStatus::kOk &&
        init_status != easy::art::ArtHookInitStatus::kAlreadyInitialized) {
        Log::Print(LogLevel::kError, kLogTag, "art engine init failed status=%s",
                   easy::art::ArtHookInitStatusName(init_status));
        return false;
    }

    g_dex_class_loader = CreateDexClassLoader(env);
    if (g_dex_class_loader == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "callback dex injection failed");
        return false;
    }

    jclass hooker_class = LoadHookerClass(env, g_dex_class_loader);
    if (hooker_class == nullptr) {
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

    jfieldID backup_field = env->GetFieldID(hooker_class, "backup", "Ljava/lang/reflect/Method;");
    if (backup_field == nullptr || HasPendingException(env, "backup_field_id")) {
        return false;
    }

    /**
     * 2026年08月28日01时30分
     * 逐个目标安装，任一目标失败不影响其余目标：
     * 不同 Android 版本上这些方法的存在与被调用路径并不一致，
     * 只要有一个命中，演示效果就成立。
     * 2026 BY white
     */
    std::size_t installed = 0;
    for (std::size_t index = 0; index < kTargetCount; ++index) {
        if (InstallOneTarget(env, hooker_class, callback_method, backup_field, kTargets[index],
                             g_hooker_objects[index])) {
            ++installed;
        }
    }

    Log::Print(LogLevel::kInfo, kLogTag, "demo installed targets=%zu/%zu \"%s\" -> \"%s\"",
               installed, kTargetCount, kMatchText, kReplacementText);
    return installed > 0;
}

/**
 * 2026年08月28日01时14分
 * 保存虚拟机指针，供安装线程自行附着取得 JNIEnv。
 * JNIEnv 与线程绑定，不能跨线程传递，能传的只有 JavaVM。
 * 2026 BY white
 */
JavaVM* g_java_vm = nullptr;

/**
 * 2026年08月28日01时14分
 * 安装线程主体。附着到虚拟机、完成安装、再解除附着。
 * 全局引用不受解除附着影响，回调所需的对象在此之后依然有效。
 * 2026 BY white
 */
void* InstallThreadMain(void*) noexcept {
    if (g_java_vm == nullptr) {
        return nullptr;
    }

    /**
     * 2026年08月28日01时26分
     * 目标进程的判定推迟到这里，而不是在触发回调时就做。
     * 触发点是本进程的第一次 Binder 事务，那一刻 zygote 刚 fork 完、进程还没改名，
     * 此时读到的名字是 zygote 的命令行，任何按名字的判断都会落空。
     * 2026 BY white
     */
    const std::string name = WaitForFinalProcessName();
    if (name != kTargetProcess) {
        return nullptr;
    }
    Log::Print(LogLevel::kInfo, kLogTag, "target process confirmed: %s", name.c_str());

    JNIEnv* env = nullptr;
    if (g_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "attach install thread failed");
        return nullptr;
    }

    const bool installed = InstallHook(env);
    if (!installed) {
        Log::Print(LogLevel::kError, kLogTag, "demo hook not installed");
    }

    g_java_vm->DetachCurrentThread();
    return nullptr;
}

}  // namespace

void MaybeInstallInSettings(JNIEnv* env) noexcept {
    if (env == nullptr) {
        return;
    }

    /**
     * 2026年08月28日01时26分
     * 这里只做一次廉价的粗筛，真正的判定交给安装线程。
     * 进程要么已经就是目标，要么还是尚未改名的 zygote 子进程(有可能变成目标)，
     * 其余情况(system_server、各类原生进程)可以立刻排除，不必为它们起线程。
     * 2026 BY white
     */
    const std::string name = ReadProcessNameFresh();
    if (name != kTargetProcess && !IsUnnamedZygoteChild(name)) {
        return;
    }

    const pid_t current = getpid();
    pid_t expected = installed_pid.load(std::memory_order_acquire);
    if (expected == current) {
        return;
    }
    if (!installed_pid.compare_exchange_strong(expected, current, std::memory_order_acq_rel)) {
        return;
    }

    if (env->GetJavaVM(&g_java_vm) != JNI_OK || g_java_vm == nullptr) {
        Log::Print(LogLevel::kError, kLogTag, "cannot obtain JavaVM");
        return;
    }

    pthread_t thread{};
    if (pthread_create(&thread, nullptr, InstallThreadMain, nullptr) != 0) {
        Log::Print(LogLevel::kError, kLogTag, "cannot start install thread");
        return;
    }
    pthread_detach(thread);
    Log::Print(LogLevel::kInfo, kLogTag, "install thread started pid=%d", current);
}

}  // namespace easy_user_hooks::art_demo
