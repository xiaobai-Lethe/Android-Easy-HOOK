#include "flyme-FOD-xiaomi/fingerprint/service_access.hpp"

#include "flyme-FOD-xiaomi/jni/reflect.hpp"
#include "flyme-FOD-xiaomi/jni/service_resolver.hpp"

namespace fod::fingerprint {
namespace {

/**
 * 2026年08月27日23时04分
 * 反射链上用到的全部框架类签名。集中在此处，系统结构变化时只需改这一份。
 * 2026 BY white
 */
constexpr char kOuterServiceField[] = "this$0";
constexpr char kOuterServiceSignature[] =
    "Lcom/android/server/biometrics/sensors/fingerprint/FingerprintService;";
constexpr char kRegistryField[] = "mRegistry";
constexpr char kRegistrySignature[] =
    "Lcom/android/server/biometrics/sensors/fingerprint/FingerprintServiceRegistry;";
constexpr char kServiceProvidersField[] = "mServiceProviders";
constexpr char kListSignature[] = "Ljava/util/List;";
constexpr char kFingerprintSensorsField[] = "mFingerprintSensors";
constexpr char kSensorListSignature[] = "Lcom/android/server/biometrics/sensors/SensorList;";
constexpr char kSchedulerField[] = "mScheduler";
constexpr char kSchedulerSignature[] =
    "Lcom/android/server/biometrics/sensors/BiometricScheduler;";
constexpr char kCurrentOperationField[] = "mCurrentOperation";
constexpr char kOperationSignature[] =
    "Lcom/android/server/biometrics/sensors/BiometricSchedulerOperation;";
constexpr char kPendingOperationsField[] = "mPendingOperations";
constexpr char kDequeSignature[] = "Ljava/util/Deque;";
constexpr char kHandlerField[] = "mHandler";
constexpr char kHandlerSignature[] = "Landroid/os/Handler;";
constexpr char kClientMonitorField[] = "mClientMonitor";
constexpr char kClientSignature[] = "Lcom/android/server/biometrics/sensors/BaseClientMonitor;";
constexpr char kOperationStateField[] = "mState";
constexpr char kInternalCallbackMethod[] = "getInternalCallback";
constexpr char kInternalCallbackSignature[] =
    "()Lcom/android/server/biometrics/sensors/ClientMonitorCallback;";

}  // namespace

ServiceAccess::ServiceAccess(JNIEnv* env, jobject preferred_loader) noexcept
    : env_(env), preferred_loader_(preferred_loader) {}

jobject ServiceAccess::ResolveService() noexcept {
    jni::ServiceResolver resolver(env_, preferred_loader_);
    return resolver.GetFingerprintService();
}

jobject ServiceAccess::ResolveScheduler(jobject service) noexcept {
    if (env_ == nullptr || service == nullptr) {
        return nullptr;
    }

    jni::LocalRefScope scope(env_);
    jobject outer_service = scope.Track(
        jni::ReadObjectField(env_, service, kOuterServiceField, kOuterServiceSignature));
    jobject registry =
        scope.Track(jni::ReadObjectField(env_, outer_service, kRegistryField, kRegistrySignature));
    jobject providers =
        scope.Track(jni::ReadObjectField(env_, registry, kServiceProvidersField, kListSignature));
    jobject provider = scope.Track(jni::ReadIndexedObject(env_, providers, "get", 0));
    jobject sensors = scope.Track(
        jni::ReadObjectField(env_, provider, kFingerprintSensorsField, kSensorListSignature));
    jobject sensor = scope.Track(jni::ReadIndexedObject(env_, sensors, "valueAt", 0));

    /**
     * 2026年08月27日23时04分
     * 调度器不纳入作用域：它要交还给调用方，不能在本函数返回时被释放。
     * 2026 BY white
     */
    return jni::ReadObjectField(env_, sensor, kSchedulerField, kSchedulerSignature);
}

jobject ServiceAccess::ResolveCurrentOperation(jobject scheduler) noexcept {
    return jni::ReadObjectField(env_, scheduler, kCurrentOperationField, kOperationSignature);
}

jobject ServiceAccess::ResolvePendingOperations(jobject scheduler) noexcept {
    return jni::ReadObjectField(env_, scheduler, kPendingOperationsField, kDequeSignature);
}

jobject ServiceAccess::ResolveHandler(jobject scheduler) noexcept {
    return jni::ReadObjectField(env_, scheduler, kHandlerField, kHandlerSignature);
}

jobject ServiceAccess::ResolveCurrentClient(jobject operation) noexcept {
    return jni::ReadObjectField(env_, operation, kClientMonitorField, kClientSignature);
}

jobject ServiceAccess::ResolveInternalCallback(jobject scheduler) noexcept {
#if defined(__ANDROID__)
    if (env_ == nullptr || scheduler == nullptr) {
        return nullptr;
    }

    jclass scheduler_class = env_->GetObjectClass(scheduler);
    if (jni::ConsumeException(env_) || scheduler_class == nullptr) {
        return nullptr;
    }
    const jmethodID method =
        env_->GetMethodID(scheduler_class, kInternalCallbackMethod, kInternalCallbackSignature);
    jni::ConsumeException(env_);
    env_->DeleteLocalRef(scheduler_class);
    if (method == nullptr) {
        return nullptr;
    }

    jobject callback = env_->CallObjectMethod(scheduler, method);
    if (jni::ConsumeException(env_)) {
        if (callback != nullptr) {
            env_->DeleteLocalRef(callback);
        }
        return nullptr;
    }
    return callback;
#else
    (void)scheduler;
    return nullptr;
#endif
}

bool ServiceAccess::ReadOperationState(jobject operation, int& state) noexcept {
    return jni::ReadIntField(env_, operation, kOperationStateField, state);
}

jni::ObjectDescription ServiceAccess::Describe(jobject object) noexcept {
    return jni::DescribeObject(env_, object);
}

void ServiceAccess::Release(jobject reference) noexcept {
#if defined(__ANDROID__)
    if (env_ != nullptr && reference != nullptr) {
        env_->DeleteLocalRef(reference);
        jni::ClearException(env_);
    }
#else
    (void)reference;
#endif
}

}  // namespace fod::fingerprint
