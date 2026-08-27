#pragma once

#include <cstdint>

namespace fod::udfps {

// “原值正确”和“已经改写”都表示该字段最终可用。
enum class FieldPatchState : std::uint8_t {
    kNotAttempted,
    kAlreadyCorrect,
    kChanged,
    kFailed,
};

// 记录第一个失败步骤，避免日志只给出笼统的“补丁失败”。
enum class PatchFailureStage : std::uint8_t {
    kNone,
    kServiceLookup,
    kOriginalMethod,
    kSensorSelection,
    kSensorType,
    kIllumination,
    kLocation,
    kScheduler,
    kJniException,
};

// 分别记录传感器属性和调度器类型，允许调用方判断部分成功。
struct PatchResult final {
    int inspected_sensors{};
    int matched_sensors{};
    FieldPatchState sensor_type{FieldPatchState::kNotAttempted};
    FieldPatchState illumination{FieldPatchState::kNotAttempted};
    FieldPatchState location{FieldPatchState::kNotAttempted};
    FieldPatchState scheduler_type{FieldPatchState::kNotAttempted};
    PatchFailureStage failure_stage{PatchFailureStage::kNone};

    [[nodiscard]] bool SensorPropertiesSucceeded() const noexcept {
        return matched_sensors > 0 && IsSuccessful(sensor_type) && IsSuccessful(illumination) &&
               IsSuccessful(location);
    }

    [[nodiscard]] bool SchedulerSucceeded() const noexcept {
        return IsSuccessful(scheduler_type);
    }

private:
    [[nodiscard]] static bool IsSuccessful(FieldPatchState state) noexcept {
        return state == FieldPatchState::kAlreadyCorrect || state == FieldPatchState::kChanged;
    }
};

}  // namespace fod::udfps
