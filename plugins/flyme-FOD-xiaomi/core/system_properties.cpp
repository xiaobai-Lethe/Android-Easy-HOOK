#include "flyme-FOD-xiaomi/core/system_properties.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <string>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace fod::core {
namespace {

std::string LowerAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        lowered.push_back(static_cast<char>(std::tolower(byte)));
    }
    return lowered;
}

}  // namespace

bool SystemProperties::ParseBool(std::string_view value, bool default_value) noexcept {
    const std::string lowered = LowerAscii(value);
    if (lowered == "1" || lowered == "true" || lowered == "y" || lowered == "yes" ||
        lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "n" || lowered == "no" ||
        lowered == "off") {
        return false;
    }
    return default_value;
}

int SystemProperties::ParseInt(std::string_view value, int default_value) noexcept {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    if (value.empty()) {
        return default_value;
    }

    int parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return default_value;
    }
    return parsed;
}

std::string SystemProperties::GetString(std::string_view name,
                                        std::string_view default_value) {
    try {
#if defined(__ANDROID__)
        std::array<char, PROP_VALUE_MAX> value{};
        const std::string property_name(name);
        const int length = __system_property_get(property_name.c_str(), value.data());
        if (length <= 0) {
            return std::string(default_value);
        }
        return std::string(value.data(), static_cast<std::size_t>(length));
#else
        (void)name;
        return std::string(default_value);
#endif
    } catch (...) {
        return {};
    }
}

bool SystemProperties::GetBool(std::string_view name, bool default_value) noexcept {
    try {
        return ParseBool(GetString(name, default_value ? "1" : "0"), default_value);
    } catch (...) {
        return default_value;
    }
}

int SystemProperties::GetInt(std::string_view name, int default_value) noexcept {
    try {
        return ParseInt(GetString(name, {}), default_value);
    } catch (...) {
        return default_value;
    }
}

}  // namespace fod::core
