#pragma once

#include "flyme-FOD-xiaomi/core/process_context.hpp"

#include <cstdint>
#include <string_view>

namespace fod::binder {

// 指纹事务可能来自会话接口，也可能来自系统服务接口。
enum class DescriptorKind : std::uint8_t {
    kUnknown,
    kSession,
    kService,
};

// 上层只处理业务动作，不依赖各 Android 版本不同的事务码。
enum class FingerprintTransactionAction : std::uint8_t {
    kNone,
    kStartHal,
    kPointerDown,
    kPointerUp,
    kSessionClose,
    kUdfpsUiEvent,
};

struct FingerprintTransactionDecision final {
    FingerprintTransactionAction action{FingerprintTransactionAction::kNone};
};

class FingerprintTransactions final {
public:
    static constexpr std::string_view kSessionDescriptor =
        "android.hardware.biometrics.fingerprint.ISession";
    static constexpr std::string_view kServiceDescriptor =
        "android.hardware.fingerprint.IFingerprintService";

    FingerprintTransactions() = delete;

    [[nodiscard]] static DescriptorKind ClassifyDescriptor(
        std::string_view descriptor) noexcept;
    [[nodiscard]] static bool IsCandidateSessionCode(int code) noexcept;
    [[nodiscard]] static bool IsCandidateServiceCode(int code) noexcept;
    [[nodiscard]] static FingerprintTransactionDecision Classify(
        core::ProcessRole role, int code, std::string_view descriptor) noexcept;
};

}  // namespace fod::binder
