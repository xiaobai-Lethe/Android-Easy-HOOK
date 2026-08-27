#pragma once

#include "fod/vendor/aidl_int_transaction.hpp"

namespace fod::vendor {

// 将 IXiaomiFingerprint.extCmd 封装为统一的整数 AIDL 事务。
class XiaomiFingerprintExtension final {
public:
    static constexpr std::string_view kServiceName =
        "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint/default";
    static constexpr std::string_view kDescriptor =
        "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint";
    static constexpr int kExtCmdCode = 1;
    static constexpr int kFodExtCmd = 4;

    explicit XiaomiFingerprintExtension(AidlIntTransactionBackend& backend) noexcept;

    [[nodiscard]] AidlIntTransactionResult SetFingerDown(bool enabled) noexcept;

    // 原始 extCmd 入口。会话准备阶段使用特定值，让内核在真实按压时点亮 LHBM。
    [[nodiscard]] AidlIntTransactionResult ExtCmd(int cmd, int value) noexcept;

private:
    AidlIntTransactionBackend& backend_;
};

}  // namespace fod::vendor
