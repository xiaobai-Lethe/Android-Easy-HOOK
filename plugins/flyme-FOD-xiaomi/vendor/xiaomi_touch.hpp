#pragma once

#include "flyme-FOD-xiaomi/vendor/aidl_int_transaction.hpp"

namespace fod::vendor {

// 将 ITouchFeature.setModeValue 封装为统一的整数 AIDL 事务。
class XiaomiTouchFeature final {
public:
    static constexpr std::string_view kServiceName =
        "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";
    static constexpr std::string_view kDescriptor =
        "vendor.xiaomi.hw.touchfeature.ITouchFeature";
    static constexpr int kSetModeValueCode = 9;
    static constexpr int kFodTouchMode = 10;

    explicit XiaomiTouchFeature(AidlIntTransactionBackend& backend) noexcept;

    [[nodiscard]] AidlIntTransactionResult SetFodTouchMode(bool enabled) noexcept;

    // 保留显式 value 入口；准备识别时需要发送 3，而普通开关只发送 0/1。
    [[nodiscard]] AidlIntTransactionResult SetTouchMode(int touch_id, int mode,
                                                        int value) noexcept;

private:
    AidlIntTransactionBackend& backend_;
};

}  // namespace fod::vendor
