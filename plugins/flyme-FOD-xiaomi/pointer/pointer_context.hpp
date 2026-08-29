#pragma once

#include <string_view>

namespace fod::pointer {

// 会话关闭兜底只需要区分按下和抬起。
enum class PointerEventKind {
    kDown,
    kUp,
};

struct PointerContext final {
    int pointer_id{};
    float x{};
    float y{};
    float minor{};
    float major{};
};

// 一次直接发往 ISession 的指针事务。
struct SessionPointerFallbackRequest final {
    static constexpr std::string_view kDescriptor =
        "android.hardware.biometrics.fingerprint.ISession";

    int transaction_code{};
    int pointer_id{};
    int x{};
    int y{};
    float minor{};
    float major{};
    bool write_axes{};
    std::string_view descriptor{kDescriptor};
};

class SessionPointerFallback final {
public:
    SessionPointerFallback() = delete;

    [[nodiscard]] static SessionPointerFallbackRequest Build(
        PointerEventKind event_kind, const PointerContext& context) noexcept;
};

}  // namespace fod::pointer
