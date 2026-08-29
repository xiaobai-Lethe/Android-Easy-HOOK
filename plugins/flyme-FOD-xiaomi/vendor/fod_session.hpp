#pragma once

#include "flyme-FOD-xiaomi/vendor/aidl_int_transaction.hpp"
#include "flyme-FOD-xiaomi/vendor/xiaomi_fingerprint.hpp"
#include "flyme-FOD-xiaomi/vendor/xiaomi_touch.hpp"

namespace fod::vendor {

/**
 * 2026年08月27日23时14分
 * FOD 会话的厂商命令序列。
 *
 * 指纹认证或录入开始时，内核需要被显式告知即将进入局部高亮状态，否则真实按压时不会点亮；
 * 任务结束时必须按相反方向完整清理，否则状态会残留到下一次任务。
 * 这里只负责下发命令序列，是否该下发由 HBM 监控依据调度器状态判断。
 * 2026 BY white
 */
class FodSession final {
public:
    explicit FodSession(AidlIntTransactionBackend& backend) noexcept;

    /**
     * 2026年08月27日23时14分
     * 准备或清理 FOD 会话。
     * 顺序固定：准备时先通知指纹 HAL 与触控固件，最后写显示节点的 fp_status；
     * 清理时反向进行，并额外多写一次中间态，确保驱动确实退出会话。
     * 2026 BY white
     */
    void Arm(bool arm) noexcept;

    [[nodiscard]] AidlIntTransactionResult SetTouchMode(bool enabled) noexcept;

private:
    XiaomiFingerprintExtension fingerprint_;
    XiaomiTouchFeature touch_;
};

}  // namespace fod::vendor
