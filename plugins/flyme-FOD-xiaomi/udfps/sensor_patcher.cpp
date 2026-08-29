#include "flyme-FOD-xiaomi/udfps/sensor_patcher.hpp"

#include "flyme-FOD-xiaomi/jni/class_resolver.hpp"
#include "flyme-FOD-xiaomi/jni/reflect.hpp"

namespace fod::udfps {
namespace {

/**
 * 2026年08月27日23时38分
 * 光学屏下指纹的传感器类型枚举值。Flyme 在小米硬件上给出的类型不是这个值，
 * 导致 Framework 走了非屏下指纹的分支，这正是需要修正的根源。
 * 2026 BY white
 */
constexpr int kSensorTypeUdfpsOptical = 3;

constexpr char kSensorTypeField[] = "sensorType";
constexpr char kIlluminationField[] = "halControlsIllumination";
constexpr char kLocationClass[] = "android.hardware.biometrics.SensorLocationInternal";
constexpr char kLocationConstructorSignature[] = "(Ljava/lang/String;III)V";
constexpr char kListSignature[] = "Ljava/util/List;";

/**
 * 2026年08月27日23时38分
 * 不同系统版本中传感器位置字段的命名不一致，按优先级依次尝试。
 * 2026 BY white
 */
constexpr const char* kLocationFieldCandidates[] = {
    "sensorLocations",
    "mSensorLocations",
    "sensorLocation",
};

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

#if defined(__ANDROID__)

[[nodiscard]] const char* FindLocationFieldName(JNIEnv* env, jobject sensor) noexcept {
    for (const char* candidate : kLocationFieldCandidates) {
        if (jni::FindFieldInHierarchy(env, sensor, candidate, kListSignature) != nullptr) {
            return candidate;
        }
    }
    return nullptr;
}

/**
 * 2026年08月27日23时38分
 * 构造一个描述 FOD 区域的 SensorLocationInternal 实例。
 * 显示器标识传空串表示主屏，坐标与半径来自属性配置换算出的几何信息。
 * 2026 BY white
 */
[[nodiscard]] jobject NewSensorLocation(JNIEnv* env, const Geometry& geometry) noexcept {
    jni::ClassResolver resolver(env);
    jclass location_class = resolver.Resolve(kLocationClass);
    if (location_class == nullptr) {
        return nullptr;
    }

    const jmethodID constructor =
        env->GetMethodID(location_class, "<init>", kLocationConstructorSignature);
    jni::ConsumeException(env);
    if (constructor == nullptr) {
        env->DeleteLocalRef(location_class);
        return nullptr;
    }

    jstring display = env->NewStringUTF("");
    if (jni::ConsumeException(env) || display == nullptr) {
        env->DeleteLocalRef(location_class);
        return nullptr;
    }

    jobject location = env->NewObject(location_class, constructor, display, geometry.center_x,
                                      geometry.center_y, geometry.radius);
    jni::ConsumeException(env);
    env->DeleteLocalRef(display);
    env->DeleteLocalRef(location_class);
    return location;
}

[[nodiscard]] jobject NewSingleItemArrayList(JNIEnv* env, jobject item) noexcept {
    if (item == nullptr) {
        return nullptr;
    }

    jclass list_class = env->FindClass("java/util/ArrayList");
    jni::ConsumeException(env);
    if (list_class == nullptr) {
        return nullptr;
    }

    const jmethodID constructor = env->GetMethodID(list_class, "<init>", "()V");
    const jmethodID add = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    jni::ConsumeException(env);
    if (constructor == nullptr || add == nullptr) {
        env->DeleteLocalRef(list_class);
        return nullptr;
    }

    jobject list = env->NewObject(list_class, constructor);
    jni::ConsumeException(env);
    if (list != nullptr) {
        static_cast<void>(env->CallBooleanMethod(list, add, item));
        jni::ConsumeException(env);
    }
    env->DeleteLocalRef(list_class);
    return list;
}

void PatchSensorType(JNIEnv* env, jobject sensor, PatchResult& result) noexcept {
    int sensor_type = 0;
    if (!jni::ReadIntField(env, sensor, kSensorTypeField, sensor_type)) {
        result.sensor_type = MergeState(result.sensor_type, FieldPatchState::kFailed);
        RecordFailure(result, PatchFailureStage::kSensorType);
        return;
    }

    if (sensor_type == kSensorTypeUdfpsOptical) {
        result.sensor_type = MergeState(result.sensor_type, FieldPatchState::kAlreadyCorrect);
        return;
    }

    const bool written = jni::WriteIntField(env, sensor, kSensorTypeField, kSensorTypeUdfpsOptical);
    result.sensor_type = MergeState(result.sensor_type, written ? FieldPatchState::kChanged
                                                                : FieldPatchState::kFailed);
    if (!written) {
        RecordFailure(result, PatchFailureStage::kSensorType);
    }
}

/**
 * 2026年08月27日23时38分
 * 关闭 halControlsIllumination。该字段为真时 Framework 认为高亮由 HAL 自行负责，
 * 于是不再下发点亮流程，而小米 HAL 在 Flyme 上并不会主动点亮。
 * 2026 BY white
 */
void PatchIllumination(JNIEnv* env, jobject sensor, PatchResult& result) noexcept {
    bool hal_controls = false;
    if (!jni::ReadBooleanField(env, sensor, kIlluminationField, hal_controls)) {
        result.illumination = MergeState(result.illumination, FieldPatchState::kFailed);
        RecordFailure(result, PatchFailureStage::kIllumination);
        return;
    }

    if (!hal_controls) {
        result.illumination = MergeState(result.illumination, FieldPatchState::kAlreadyCorrect);
        return;
    }

    const bool written = jni::WriteBooleanField(env, sensor, kIlluminationField, false);
    result.illumination = MergeState(result.illumination, written ? FieldPatchState::kChanged
                                                                  : FieldPatchState::kFailed);
    if (!written) {
        RecordFailure(result, PatchFailureStage::kIllumination);
    }
}

void PatchLocation(JNIEnv* env, jobject sensor, const Geometry& geometry,
                   PatchResult& result) noexcept {
    const char* field_name = FindLocationFieldName(env, sensor);
    if (field_name == nullptr) {
        result.location = MergeState(result.location, FieldPatchState::kFailed);
        RecordFailure(result, PatchFailureStage::kLocation);
        return;
    }

    jobject location = NewSensorLocation(env, geometry);
    jobject locations = NewSingleItemArrayList(env, location);
    bool written = false;
    if (locations != nullptr) {
        written = jni::WriteObjectField(env, sensor, field_name, kListSignature, locations);
        env->DeleteLocalRef(locations);
    }
    if (location != nullptr) {
        env->DeleteLocalRef(location);
    }

    result.location =
        MergeState(result.location, written ? FieldPatchState::kChanged : FieldPatchState::kFailed);
    if (!written) {
        RecordFailure(result, PatchFailureStage::kLocation);
    }
}

#endif

}  // namespace

PatchResult SensorPatcher::PatchJavaSensor(JNIEnv* env, jobject sensor,
                                           const SensorPatchConfig& config) noexcept {
    PatchResult result;
#if defined(__ANDROID__)
    if (env == nullptr || sensor == nullptr) {
        result.failure_stage = PatchFailureStage::kSensorSelection;
        return result;
    }

    result.inspected_sensors = 1;
    result.matched_sensors = 1;

    PatchSensorType(env, sensor, result);
    PatchIllumination(env, sensor, result);
    PatchLocation(env, sensor, config.geometry, result);
#else
    static_cast<void>(env);
    static_cast<void>(sensor);
    static_cast<void>(config);
    result.failure_stage = PatchFailureStage::kSensorSelection;
#endif
    return result;
}

}  // namespace fod::udfps
