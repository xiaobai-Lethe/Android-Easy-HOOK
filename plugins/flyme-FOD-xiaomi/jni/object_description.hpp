#pragma once

#include <cstdint>
#include <string>

namespace fod::jni {

/**
 * 2026年08月27日22时58分
 * 区分对象为空与 toString() 调用失败。
 * 监控逻辑据此决定是重试还是把当前轮次判定为"没有正在执行的任务"，两者含义完全不同。
 * 2026 BY white
 */
enum class ObjectDescriptionStatus : std::uint8_t {
    kNull,
    kDescribed,
    kFailed,
};

struct ObjectDescription final {
    ObjectDescriptionStatus status{ObjectDescriptionStatus::kNull};
    std::string text;
};

}  // namespace fod::jni
