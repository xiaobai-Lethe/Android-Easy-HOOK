#include "flyme-FOD-xiaomi/vendor/xiaomi_touch.hpp"

namespace fod::vendor {

XiaomiTouchFeature::XiaomiTouchFeature(AidlIntTransactionBackend& backend) noexcept
    : backend_(backend) {}

AidlIntTransactionResult XiaomiTouchFeature::SetFodTouchMode(bool enabled) noexcept {
    return SetTouchMode(0, kFodTouchMode, enabled ? 1 : 0);
}

AidlIntTransactionResult XiaomiTouchFeature::SetTouchMode(int touch_id, int mode,
                                                          int value) noexcept {
    const AidlIntTransaction transaction{
        .service_name = kServiceName,
        .descriptor = kDescriptor,
        .code = kSetModeValueCode,
        .arguments = {touch_id, mode, value, 0},
        .argument_count = 3,
    };
    return backend_.Transact(transaction);
}

}  // namespace fod::vendor
