#pragma once

#include "flyme-FOD-xiaomi/binder/fingerprint_transactions.hpp"
#include "flyme-FOD-xiaomi/vendor/aidl_int_transaction.hpp"
#include "flyme-FOD-xiaomi/vendor/fod_sysfs.hpp"

#include <array>
#include <cstdint>

namespace fod::vendor {

// 仅用于记录一次分发实际执行了哪些底层操作。
enum class FodVendorOperation : std::uint8_t {
    kNone,
    kDisplayFingerDown,
    kDisplayFingerUp,
    kFingerprintDown,
    kFingerprintUp,
    kTouchModeOn,
    kTouchModeOff,
};

struct FodVendorActionResult final {
    binder::FingerprintTransactionAction action{binder::FingerprintTransactionAction::kNone};
    std::array<FodVendorOperation, 2> operations{};
    int operations_attempted{};
    int operations_succeeded{};
    bool all_succeeded{true};
};

// 汇总显示节点、指纹扩展和触控扩展三条厂商通道。
class FodVendorActionBackend {
public:
    virtual ~FodVendorActionBackend() = default;

    [[nodiscard]] virtual FodSysfsResult ApplyDisplayState(
        FodDisplayState state) noexcept = 0;
    [[nodiscard]] virtual AidlIntTransactionResult SetFingerprintDown(
        bool enabled) noexcept = 0;
    [[nodiscard]] virtual AidlIntTransactionResult SetTouchMode(bool enabled) noexcept = 0;
};

// 将统一的指纹动作转换为有顺序要求的厂商命令。
class FodVendorActionDispatcher final {
public:
    explicit FodVendorActionDispatcher(FodVendorActionBackend& backend) noexcept;

    // 返回每一步的执行统计；部分成功不会被折叠成单一布尔值。
    [[nodiscard]] FodVendorActionResult Dispatch(
        binder::FingerprintTransactionAction action) noexcept;

private:
    FodVendorActionBackend& backend_;
};

}  // namespace fod::vendor
