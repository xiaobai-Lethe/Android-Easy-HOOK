#include "fod/vendor/xiaomi_fingerprint.hpp"

namespace fod::vendor {

XiaomiFingerprintExtension::XiaomiFingerprintExtension(
    AidlIntTransactionBackend& backend) noexcept
    : backend_(backend) {}

AidlIntTransactionResult XiaomiFingerprintExtension::SetFingerDown(bool enabled) noexcept {
    return ExtCmd(kFodExtCmd, enabled ? 1 : 0);
}

AidlIntTransactionResult XiaomiFingerprintExtension::ExtCmd(int cmd, int value) noexcept {
    const AidlIntTransaction transaction{
        .service_name = kServiceName,
        .descriptor = kDescriptor,
        .code = kExtCmdCode,
        .arguments = {cmd, value, 0, 0},
        .argument_count = 2,
    };
    return backend_.Transact(transaction);
}

}  // namespace fod::vendor
