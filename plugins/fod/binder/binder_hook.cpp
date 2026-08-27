#include "fod/binder/binder_hook.hpp"

#include "fod/binder/fingerprint_transactions.hpp"
#include "fod/binder/parcel.hpp"
#include "easy_hook/core/log.hpp"
#include "fod/core/process_context.hpp"
#include "fod/jni/reflect.hpp"
#include "fod/pointer/pointer_context.hpp"
#include "fod/pointer/session_pointer_transaction.hpp"
#include "fod/vendor/aidl_int_transaction.hpp"
#include "fod/vendor/fod_sysfs.hpp"
#include "fod/vendor/fod_vendor_action.hpp"
#include "fod/vendor/xiaomi_fingerprint.hpp"
#include "fod/vendor/xiaomi_touch.hpp"

#include "easy_hook/dynamic_hook.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>

namespace fod::binder {
namespace {

constexpr std::string_view kLogTag = "binder-hook";
constexpr std::string_view kBinderProxyClass = "android.os.BinderProxy";
constexpr std::string_view kTransactNative = "transactNative";
constexpr std::string_view kTransact = "transact";

using TransactFunction = jboolean (*)(JNIEnv*, jobject, jint, jobject, jobject, jint);
using RegisterNativesFunction = jint (*)(JNIEnv*, jclass, const JNINativeMethod*, jint);

std::mutex install_mutex;
std::atomic<TransactFunction> original_transact{nullptr};
std::atomic<RegisterNativesFunction> original_register_natives{nullptr};
std::atomic<PostForkCallback> post_fork_callback{nullptr};
void* register_natives_target = nullptr;
bool register_natives_installed = false;

/**
 * 2026年08月27日23时50分
 * 把厂商动作后端组装到一起。显示节点、指纹扩展与触控扩展三者需要按动作协同下发。
 * 2026 BY white
 */
class VendorActionBackend final : public vendor::FodVendorActionBackend {
public:
    explicit VendorActionBackend(vendor::AidlIntTransactionBackend& backend) noexcept
        : fingerprint_(backend), touch_(backend) {}

    vendor::FodSysfsResult ApplyDisplayState(vendor::FodDisplayState state) noexcept override {
        return vendor::DefaultFodSysfsController().Apply(state);
    }

    vendor::AidlIntTransactionResult SetFingerprintDown(bool enabled) noexcept override {
        return fingerprint_.SetFingerDown(enabled);
    }

    vendor::AidlIntTransactionResult SetTouchMode(bool enabled) noexcept override {
        return touch_.SetFodTouchMode(enabled);
    }

private:
    vendor::XiaomiFingerprintExtension fingerprint_;
    vendor::XiaomiTouchFeature touch_;
};

/**
 * 2026年08月27日23时50分
 * 判断该事务码是否值得读取接口描述符。
 * 读描述符要调三次 JNI，而 Binder 事务在系统中极为频繁，
 * 因此先用事务码范围快速筛掉绝大多数无关调用。
 * 2026 BY white
 */
bool ShouldInspect(core::ProcessRole role, int code) noexcept {
    if (role == core::ProcessRole::kSystemServer) {
        return FingerprintTransactions::IsCandidateSessionCode(code);
    }
    if (role == core::ProcessRole::kSettings || role == core::ProcessRole::kSystemUi) {
        return FingerprintTransactions::IsCandidateServiceCode(code);
    }
    return false;
}

/**
 * 2026年08月27日23时50分
 * 会话关闭时补发一次指针抬起。
 * 系统在某些路径下会直接关闭会话而不发 onPointerUp，导致按压状态残留、屏幕一直亮着局部高亮。
 * 2026 BY white
 */
void SendPointerUpFallback(JNIEnv* env, jobject binder_proxy) noexcept {
    void* original = OriginalTransact();
    if (original == nullptr) {
        return;
    }

    const pointer::SessionPointerFallbackRequest request =
        pointer::SessionPointerFallback::Build(pointer::PointerEventKind::kUp,
                                               pointer::PointerContext{});
    const pointer::SessionPointerTransactionResult result =
        pointer::SessionPointerTransaction::Transact(env, binder_proxy, original, request);
    if (result.status != pointer::SessionPointerTransactionStatus::kTransacted) {
        easy::core::Log::Print(easy::core::LogLevel::kDebug, kLogTag,
                               "pointer up fallback not delivered status=%d",
                               static_cast<int>(result.status));
    }
}

void HandleTransaction(JNIEnv* env, jobject binder_proxy, jint code, jobject data) noexcept {
    const core::ProcessRole role = core::ProcessContext::CurrentRole();
    const int transaction_code = static_cast<int>(code);
    if (env == nullptr || data == nullptr || !ShouldInspect(role, transaction_code)) {
        return;
    }

    const ParcelDescriptorResult parcel = ReadParcelInterfaceDescriptor(env, data);
    if (parcel.status != ParcelDescriptorStatus::kRead) {
        return;
    }

    const FingerprintTransactionDecision decision =
        FingerprintTransactions::Classify(role, transaction_code, parcel.descriptor);
    if (decision.action == FingerprintTransactionAction::kNone) {
        return;
    }

    if (decision.action == FingerprintTransactionAction::kSessionClose) {
        SendPointerUpFallback(env, binder_proxy);
    }

    vendor::JniAidlIntTransactionBackend aidl_backend(env);
    VendorActionBackend action_backend(aidl_backend);
    vendor::FodVendorActionDispatcher dispatcher(action_backend);
    const vendor::FodVendorActionResult result = dispatcher.Dispatch(decision.action);
    if (!result.all_succeeded) {
        easy::core::Log::Print(easy::core::LogLevel::kWarning, kLogTag,
                               "vendor action partially failed action=%d succeeded=%d/%d",
                               static_cast<int>(decision.action), result.operations_succeeded,
                               result.operations_attempted);
    }
}

jboolean TransactReplacement(JNIEnv* env, jobject binder_proxy, jint code, jobject data,
                             jobject reply, jint flags) {
    /**
     * 2026年08月27日23时50分
     * 每个 Binder 请求都会经过这里，属于绝对热路径。
     * 这里只做一次读取，是否真的需要初始化交由运行时的一次性状态判断。
     * 2026 BY white
     */
    if (const PostForkCallback callback = post_fork_callback.load(std::memory_order_acquire);
        callback != nullptr) {
        callback(env);
    }

    HandleTransaction(env, binder_proxy, code, data);

    const TransactFunction original = original_transact.load(std::memory_order_acquire);
    if (original == nullptr) {
        return JNI_FALSE;
    }
    return original(env, binder_proxy, code, data, reply, flags);
}

#if defined(__ANDROID__)

bool IsBinderProxyClass(std::string_view class_name) noexcept {
    return class_name == kBinderProxyClass ||
           (class_name.size() > kBinderProxyClass.size() &&
            class_name.starts_with(kBinderProxyClass) &&
            class_name[kBinderProxyClass.size()] == '$');
}

std::string ReadClassName(JNIEnv* env, jclass clazz) noexcept {
    if (env == nullptr || clazz == nullptr) {
        return {};
    }

    jclass class_class = env->FindClass("java/lang/Class");
    jni::ConsumeException(env);
    if (class_class == nullptr) {
        return {};
    }
    const jmethodID get_name = env->GetMethodID(class_class, "getName", "()Ljava/lang/String;");
    jni::ConsumeException(env);
    env->DeleteLocalRef(class_class);
    if (get_name == nullptr) {
        return {};
    }

    auto name = static_cast<jstring>(env->CallObjectMethod(clazz, get_name));
    if (jni::ConsumeException(env) || name == nullptr) {
        return {};
    }

    const char* bytes = env->GetStringUTFChars(name, nullptr);
    std::string result;
    if (bytes != nullptr) {
        try {
            result.assign(bytes);
        } catch (...) {
            result.clear();
        }
        env->ReleaseStringUTFChars(name, bytes);
    }
    env->DeleteLocalRef(name);
    jni::ClearException(env);
    return result;
}

/**
 * 2026年08月27日23时50分
 * 从一批原生方法中挑出事务方法。不同系统版本的方法名在 transactNative 与 transact 之间变动，
 * 优先取前者。
 * 2026 BY white
 */
void* SelectTransactMethod(const JNINativeMethod* methods, jint count) noexcept {
    for (std::string_view wanted : {kTransactNative, kTransact}) {
        for (jint index = 0; index < count; ++index) {
            const JNINativeMethod& method = methods[index];
            if (method.name == nullptr || method.fnPtr == nullptr) {
                continue;
            }
            if (std::string_view(method.name) == wanted) {
                return method.fnPtr;
            }
        }
    }
    return nullptr;
}

void TryInstallTransactHook(JNIEnv* env, jclass clazz, const JNINativeMethod* methods,
                            jint count) noexcept {
    if (env == nullptr || clazz == nullptr || methods == nullptr || count <= 0) {
        return;
    }
    if (original_transact.load(std::memory_order_acquire) != nullptr) {
        return;
    }

    const std::string class_name = ReadClassName(env, clazz);
    if (class_name.empty() || !IsBinderProxyClass(class_name)) {
        return;
    }

    void* target = SelectTransactMethod(methods, count);
    if (target == nullptr) {
        return;
    }

    void* original = nullptr;
    if (!easy::HookFunction(target, reinterpret_cast<void*>(TransactReplacement), &original) ||
        original == nullptr) {
        easy::core::Log::Print(easy::core::LogLevel::kError, kLogTag, "failed to hook %s",
                               class_name.c_str());
        return;
    }

    original_transact.store(reinterpret_cast<TransactFunction>(original),
                            std::memory_order_release);
    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag, "transact hook installed on %s",
                           class_name.c_str());
}

jint RegisterNativesReplacement(JNIEnv* env, jclass clazz, const JNINativeMethod* methods,
                                jint count) {
    /**
     * 2026年08月27日23时50分
     * 目标一旦找到就只做转发。继续在每次原生方法注册时回调 JNI，
     * 会干扰 system_server 的早期类加载，没任何用。
     * 2026 BY white
     */
    if (original_transact.load(std::memory_order_acquire) == nullptr) {
        TryInstallTransactHook(env, clazz, methods, count);
    }

    const RegisterNativesFunction original =
        original_register_natives.load(std::memory_order_acquire);
    if (original == nullptr) {
        return JNI_ERR;
    }
    return original(env, clazz, methods, count);
}

#endif

}  // namespace

void SetPostForkCallback(PostForkCallback callback) noexcept {
    post_fork_callback.store(callback, std::memory_order_release);
}

void* OriginalTransact() noexcept {
    return reinterpret_cast<void*>(original_transact.load(std::memory_order_acquire));
}

bool InstallRegisterNativesHook(JNIEnv* env) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr) {
        return false;
    }

    const std::lock_guard lock(install_mutex);
    if (register_natives_installed) {
        return true;
    }

    void* target = reinterpret_cast<void*>(env->functions->RegisterNatives);
    if (target == nullptr) {
        return false;
    }

    void* original = nullptr;
    if (!easy::HookFunction(target, reinterpret_cast<void*>(RegisterNativesReplacement),
                            &original) ||
        original == nullptr) {
        return false;
    }

    original_register_natives.store(reinterpret_cast<RegisterNativesFunction>(original),
                                    std::memory_order_release);
    register_natives_target = target;
    register_natives_installed = true;
    return true;
#else
    static_cast<void>(env);
    return false;
#endif
}

void UninstallRegisterNativesHook() noexcept {
#if defined(__ANDROID__)
    const std::lock_guard lock(install_mutex);
    if (!register_natives_installed || register_natives_target == nullptr) {
        return;
    }

    /**
     * 2026年08月27日23时50分
     * 只撤除 RegisterNatives。transactNative 不能撤：当前调用很可能正处在它内部，
     * 还原正在执行的指令会立刻崩溃，而且后续仍要靠它识别指纹事务。
     * 2026 BY white
     */
    static_cast<void>(easy::UnhookFunction(register_natives_target));
    register_natives_target = nullptr;
    register_natives_installed = false;
    original_register_natives.store(nullptr, std::memory_order_release);
#endif
}

}  // namespace fod::binder
