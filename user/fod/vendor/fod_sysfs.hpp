#pragma once

#include <cstdint>
#include <mutex>
#include <string_view>

namespace fod::vendor {

// kUnknown 只表示尚未成功写入过状态，不能作为 Apply() 的目标值。
enum class FodDisplayState : std::uint8_t {
    kUnknown,
    kFingerUp,
    kFingerDown,
};

// kAlreadyApplied 是去重后的成功结果，不是写入失败。
enum class FodSysfsStatus : std::uint8_t {
    kApplied,
    kAlreadyApplied,
    kWriteFailed,
};

struct FodSysfsResult final {
    FodSysfsStatus status{FodSysfsStatus::kWriteFailed};
    FodDisplayState requested_state{FodDisplayState::kUnknown};
    int writes_attempted{};
    int writes_succeeded{};
};

// 真机使用 open/write，测试可替换为内存实现。
class FodSysfsWriter {
public:
    virtual ~FodSysfsWriter() = default;

    [[nodiscard]] virtual bool WriteText(
        std::string_view path, std::string_view value) noexcept = 0;
};

// 串行写入显示节点并缓存最后一次成功状态，避免重复触发驱动。
class FodSysfsController final {
public:
    explicit FodSysfsController(FodSysfsWriter& writer) noexcept;

    [[nodiscard]] FodDisplayState current_state() const noexcept;
    [[nodiscard]] FodSysfsResult Apply(FodDisplayState state) noexcept;

    // 写入 “12 <status>” 通知内核 FOD 线程：3 表示准备识别，关闭时依次写 1、0。
    // 这条命令只管理会话，不直接控制高亮区域。
    [[nodiscard]] FodSysfsResult WriteFpStatus(int status) noexcept;

private:
    FodSysfsWriter& writer_;
    mutable std::mutex mutex_;
    FodDisplayState current_state_{FodDisplayState::kUnknown};
};

// 全进程共用状态缓存和写锁。
[[nodiscard]] FodSysfsController& DefaultFodSysfsController() noexcept;

}  // namespace fod::vendor
