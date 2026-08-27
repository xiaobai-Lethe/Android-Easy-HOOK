#include "fod/udfps/scheduler_patcher.hpp"

#include "fod/jni/reflect.hpp"

namespace fod::udfps {
namespace {

/**
 * 2026年08月27日23时40分
 * BiometricScheduler 中代表屏下指纹的传感器类型值。
 * 调度器按此值决定是否走屏下指纹的按压与高亮流程。
 * 2026 BY white
 */
constexpr int kSchedulerTypeUdfps = 1;

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
constexpr char kSensorPropertiesField[] = "mSensorProperties";
constexpr char kSensorPropertiesSignature[] =
    "Landroid/hardware/fingerprint/FingerprintSensorPropertiesInternal;";
constexpr char kSchedulerField[] = "mScheduler";
constexpr char kSchedulerSignature[] =
    "Lcom/android/server/biometrics/sensors/BiometricScheduler;";
constexpr char kSchedulerSensorTypeField[] = "mSensorType";
constexpr char kSensorIdField[] = "sensorId";

FieldPatchState MergeState(FieldPatchState current, FieldPatchState next) noexcept {
    if (current == FieldPatchState::kFailed || next == FieldPatchState::kFailed) {
        return FieldPatchState::kFailed;
    }
    if (current == FieldPatchState::kChanged || next == FieldPatchState::kChanged) {
        return FieldPatchState::kChanged;
    }
    if (current == FieldPatchState::kAlreadyCorrect || next == FieldPatchState::kAlreadyCorrect) {
        return FieldPatchState::kAlreadyCorrect;
    }
    return FieldPatchState::kNotAttempted;
}

void RecordFailure(PatchResult& result, PatchFailureStage stage) noexcept {
    if (result.failure_stage == PatchFailureStage::kNone) {
        result.failure_stage = stage;
    }
}

void MergeInto(PatchResult& target, const PatchResult& source) noexcept {
    target.sensor_type = MergeState(target.sensor_type, source.sensor_type);
    target.illumination = MergeState(target.illumination, source.illumination);
    target.location = MergeState(target.location, source.location);
    target.scheduler_type = MergeState(target.scheduler_type, source.scheduler_type);
    if (source.failure_stage != PatchFailureStage::kNone) {
        RecordFailure(target, source.failure_stage);
    }
}

#if defined(__ANDROID__)

/**
 * 2026年08月27日23时40分
 * 修正调度器的传感器类型。
 * 传感器属性改对了但调度器类型没改，按压事件依然不会走屏下指纹分支，二者必须同时修正。
 * 2026 BY white
 */
[[nodiscard]] FieldPatchState PatchSchedulerType(JNIEnv* env, jobject sensor) noexcept {
    jobject scheduler = jni::ReadObjectField(env, sensor, kSchedulerField, kSchedulerSignature);
    if (scheduler == nullptr) {
        return FieldPatchState::kFailed;
    }

    int current_type = 0;
    FieldPatchState state = FieldPatchState::kFailed;
    if (jni::ReadIntField(env, scheduler, kSchedulerSensorTypeField, current_type)) {
        if (current_type == kSchedulerTypeUdfps) {
            state = FieldPatchState::kAlreadyCorrect;
        } else if (jni::WriteIntField(env, scheduler, kSchedulerSensorTypeField,
                                      kSchedulerTypeUdfps)) {
            state = FieldPatchState::kChanged;
        }
    }

    env->DeleteLocalRef(scheduler);
    return state;
}

/**
 * 2026年08月27日23时40分
 * 处理一个 id 匹配的传感器：先改传感器属性，再改它所属调度器的类型。
 * 2026 BY white
 */
void PatchMatchedSensor(JNIEnv* env, jobject sensor, jobject sensor_properties,
                        const SensorPatchConfig& config, PatchResult& result) noexcept {
    const PatchResult sensor_result =
        SensorPatcher::PatchJavaSensor(env, sensor_properties, config);
    MergeInto(result, sensor_result);

    const FieldPatchState scheduler_state = PatchSchedulerType(env, sensor);
    result.scheduler_type = MergeState(result.scheduler_type, scheduler_state);
    if (scheduler_state == FieldPatchState::kFailed) {
        RecordFailure(result, PatchFailureStage::kScheduler);
    }
}

#endif

}  // namespace

PatchResult SchedulerPatcher::PatchJavaService(JNIEnv* env, jobject service,
                                               const SensorPatchConfig& config) noexcept {
    PatchResult result;
#if defined(__ANDROID__)
    if (env == nullptr || service == nullptr) {
        result.failure_stage = PatchFailureStage::kServiceLookup;
        return result;
    }

    jni::LocalRefScope scope(env);
    jobject outer_service = scope.Track(
        jni::ReadObjectField(env, service, kOuterServiceField, kOuterServiceSignature));
    jobject registry =
        scope.Track(jni::ReadObjectField(env, outer_service, kRegistryField, kRegistrySignature));
    jobject providers =
        scope.Track(jni::ReadObjectField(env, registry, kServiceProvidersField, kListSignature));
    if (providers == nullptr) {
        result.failure_stage = PatchFailureStage::kScheduler;
        return result;
    }

    int provider_count = 0;
    if (!jni::ReadCollectionSize(env, providers, "size", provider_count)) {
        result.failure_stage = PatchFailureStage::kScheduler;
        return result;
    }

    /**
     * 2026年08月27日23时40分
     * 遍历所有 provider 与其下全部传感器，只处理 id 与配置匹配的那一个。
     * 设备可能同时存在侧边或后置指纹，改错对象会破坏原本正常的指纹功能。
     * 2026 BY white
     */
    for (int provider_index = 0; provider_index < provider_count; ++provider_index) {
        jni::LocalRefScope provider_scope(env);
        jobject provider =
            provider_scope.Track(jni::ReadIndexedObject(env, providers, "get", provider_index));
        if (provider == nullptr) {
            continue;
        }

        jobject sensor_list = provider_scope.Track(jni::ReadObjectField(
            env, provider, kFingerprintSensorsField, kSensorListSignature));
        if (sensor_list == nullptr) {
            continue;
        }

        int sensor_count = 0;
        if (!jni::ReadCollectionSize(env, sensor_list, "size", sensor_count)) {
            continue;
        }

        for (int sensor_index = 0; sensor_index < sensor_count; ++sensor_index) {
            jni::LocalRefScope sensor_scope(env);
            jobject sensor = sensor_scope.Track(
                jni::ReadIndexedObject(env, sensor_list, "valueAt", sensor_index));
            ++result.inspected_sensors;
            if (sensor == nullptr) {
                continue;
            }

            jobject sensor_properties = sensor_scope.Track(jni::ReadObjectField(
                env, sensor, kSensorPropertiesField, kSensorPropertiesSignature));
            if (sensor_properties == nullptr) {
                continue;
            }

            int sensor_id = -1;
            if (!jni::ReadIntField(env, sensor_properties, kSensorIdField, sensor_id) ||
                sensor_id != config.target_sensor_id) {
                continue;
            }

            ++result.matched_sensors;
            PatchMatchedSensor(env, sensor, sensor_properties, config, result);
        }
    }

    if (result.matched_sensors == 0) {
        result.failure_stage = PatchFailureStage::kSensorSelection;
    }
#else
    static_cast<void>(env);
    static_cast<void>(service);
    static_cast<void>(config);
    result.failure_stage = PatchFailureStage::kServiceLookup;
#endif
    return result;
}

}  // namespace fod::udfps
