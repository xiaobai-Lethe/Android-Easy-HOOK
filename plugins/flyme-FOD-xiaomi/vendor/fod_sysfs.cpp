#include "flyme-FOD-xiaomi/vendor/fod_sysfs.hpp"

#include <array>
#include <cstdio>
#include <span>
#include <string>

#if defined(__ANDROID__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fod::vendor {
namespace {

struct SysfsWrite final {
    std::string_view value;
};

constexpr std::array<std::string_view, 2> kDisplayParamPaths{{
    "/sys/class/mi_display/disp-DSI-0/disp_param",
    "/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/disp_param",
}};

constexpr std::array<SysfsWrite, 2> kFingerDownWrites{{
    SysfsWrite{.value = "9 1"},
    SysfsWrite{.value = "2 1"},
}};

constexpr std::array<SysfsWrite, 2> kFingerUpWrites{{
    SysfsWrite{.value = "2 0"},
    SysfsWrite{.value = "9 0"},
}};

std::span<const SysfsWrite> WritesForState(FodDisplayState state) noexcept {
    switch (state) {
        case FodDisplayState::kFingerDown:
            return kFingerDownWrites;
        case FodDisplayState::kFingerUp:
            return kFingerUpWrites;
        case FodDisplayState::kUnknown:
            return {};
    }
    return {};
}

bool WriteCommandToAnyDisplayPath(FodSysfsWriter& writer, std::string_view value,
                                  FodSysfsResult& result) noexcept {
    bool command_succeeded = false;
    for (std::string_view path : kDisplayParamPaths) {
        ++result.writes_attempted;
        if (writer.WriteText(path, value)) {
            ++result.writes_succeeded;
            command_succeeded = true;
            break;
        }
    }
    return command_succeeded;
}

#if defined(__ANDROID__)
class PosixFodSysfsWriter final : public FodSysfsWriter {
public:
    bool WriteText(std::string_view path, std::string_view value) noexcept override {
        if (path.empty() || value.empty()) {
            return false;
        }

        const int fd = open(path.data(), O_WRONLY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }

        const ssize_t written = write(fd, value.data(), value.size());
        const int close_result = close(fd);
        return written == static_cast<ssize_t>(value.size()) && close_result == 0;
    }
};
#else
class PosixFodSysfsWriter final : public FodSysfsWriter {
public:
    bool WriteText(std::string_view path, std::string_view value) noexcept override {
        (void)path;
        (void)value;
        return false;
    }
};
#endif

}  // namespace

FodSysfsController::FodSysfsController(FodSysfsWriter& writer) noexcept : writer_(writer) {}

FodDisplayState FodSysfsController::current_state() const noexcept {
    std::lock_guard lock(mutex_);
    return current_state_;
}

FodSysfsResult FodSysfsController::Apply(FodDisplayState state) noexcept {
    std::lock_guard lock(mutex_);

    FodSysfsResult result;
    result.requested_state = state;

    if (state == FodDisplayState::kUnknown) {
        result.status = FodSysfsStatus::kWriteFailed;
        return result;
    }
    if (state == current_state_) {
        result.status = FodSysfsStatus::kAlreadyApplied;
        return result;
    }

    const auto writes = WritesForState(state);
    int commands_succeeded = 0;
    for (const SysfsWrite& write : writes) {
        if (WriteCommandToAnyDisplayPath(writer_, write.value, result)) {
            ++commands_succeeded;
        }
    }

    if (commands_succeeded != static_cast<int>(writes.size())) {
        result.status = FodSysfsStatus::kWriteFailed;
        return result;
    }

    current_state_ = state;
    result.status = FodSysfsStatus::kApplied;
    return result;
}

FodSysfsResult FodSysfsController::WriteFpStatus(int status) noexcept {
    std::lock_guard lock(mutex_);

    FodSysfsResult result;
    // “12 <status>” 管理 FOD 会话，不是 9/2 亮度状态的一部分，
    // 因此不能用 current_state_ 去重，同一会话内允许重复写入。
    char buffer[16]{};
    const int length = std::snprintf(buffer, sizeof(buffer), "12 %d", status);
    if (length <= 0 || length >= static_cast<int>(sizeof(buffer))) {
        result.status = FodSysfsStatus::kWriteFailed;
        return result;
    }

    const std::string_view value(buffer, static_cast<std::size_t>(length));
    if (WriteCommandToAnyDisplayPath(writer_, value, result)) {
        result.status = FodSysfsStatus::kApplied;
    } else {
        result.status = FodSysfsStatus::kWriteFailed;
    }
    return result;
}

FodSysfsController& DefaultFodSysfsController() noexcept {
    static PosixFodSysfsWriter writer;
    static FodSysfsController controller(writer);
    return controller;
}

}  // namespace fod::vendor
