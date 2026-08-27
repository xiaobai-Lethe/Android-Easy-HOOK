#include "fod/vendor/fod_session.hpp"

#include "fod/vendor/fod_sysfs.hpp"

namespace fod::vendor {
namespace {

/**
 * 2026年08月27日23时14分
 * 厂商扩展命令编号。数值与顺序来自实机验证，改动任何一个都会导致按压不亮或状态残留。
 * 2026 BY white
 */
constexpr int kExtCmdFodSession = 4;
constexpr int kExtCmdFodAuxiliary = 7;
constexpr int kFodSessionArmValue = 3;
constexpr int kFodSessionIdleValue = 0;

constexpr int kFpStatusArmed = 3;
constexpr int kFpStatusReleasing = 1;
constexpr int kFpStatusIdle = 0;

}  // namespace

FodSession::FodSession(AidlIntTransactionBackend& backend) noexcept
    : fingerprint_(backend), touch_(backend) {}

void FodSession::Arm(bool arm) noexcept {
    if (arm) {
        static_cast<void>(fingerprint_.ExtCmd(kExtCmdFodSession, kFodSessionArmValue));
        static_cast<void>(fingerprint_.ExtCmd(kExtCmdFodAuxiliary, kFodSessionIdleValue));
        static_cast<void>(touch_.SetTouchMode(0, XiaomiTouchFeature::kFodTouchMode,
                                              kFodSessionArmValue));
        static_cast<void>(DefaultFodSysfsController().WriteFpStatus(kFpStatusArmed));
        return;
    }

    static_cast<void>(fingerprint_.ExtCmd(kExtCmdFodSession, kFodSessionIdleValue));
    static_cast<void>(fingerprint_.ExtCmd(kExtCmdFodAuxiliary, kFodSessionIdleValue));
    static_cast<void>(
        touch_.SetTouchMode(0, XiaomiTouchFeature::kFodTouchMode, kFodSessionIdleValue));

    /**
     * 2026年08月27日23时14分
     * 清理时写两次 fp_status：先写释放态再写空闲态。
     * 只写空闲态时驱动偶尔不会真正退出会话，下一次按压就点不亮。
     * 2026 BY white
     */
    static_cast<void>(DefaultFodSysfsController().WriteFpStatus(kFpStatusReleasing));
    static_cast<void>(DefaultFodSysfsController().WriteFpStatus(kFpStatusIdle));
}

AidlIntTransactionResult FodSession::SetTouchMode(bool enabled) noexcept {
    return touch_.SetFodTouchMode(enabled);
}

}  // namespace fod::vendor
