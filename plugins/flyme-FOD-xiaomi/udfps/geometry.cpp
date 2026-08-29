#include "flyme-FOD-xiaomi/udfps/geometry.hpp"

#include <charconv>
#include <string_view>

namespace fod::udfps {
namespace {

constexpr int kDefaultX = 597;
constexpr int kDefaultY = 2406;
constexpr int kDefaultWidth = 246;
constexpr int kDefaultHeight = 246;
constexpr int kMinimumRadius = 48;
constexpr int kDefaultRadius = 123;
constexpr int kMaximumCoordinate = 100000;
constexpr int kMaximumSize = 10000;

std::string_view Trim(std::string_view value) noexcept {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool ParseBoundedInteger(std::string_view text, int minimum, int maximum, int& output) noexcept {
    text = Trim(text);
    if (text.empty()) {
        return false;
    }

    int parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed < minimum ||
        parsed > maximum) {
        return false;
    }
    output = parsed;
    return true;
}

bool ParsePair(std::string_view text, int minimum, int maximum, int& first, int& second) noexcept {
    text = Trim(text);
    const auto comma = text.find(',');
    if (comma == std::string_view::npos || text.find(',', comma + 1) != std::string_view::npos) {
        return false;
    }
    return ParseBoundedInteger(text.substr(0, comma), minimum, maximum, first) &&
           ParseBoundedInteger(text.substr(comma + 1), minimum, maximum, second);
}

}  // namespace

Geometry Geometry::FromPropertyStrings(std::string_view location, std::string_view size) noexcept {
    Geometry geometry;

    if (!ParsePair(location, 0, kMaximumCoordinate, geometry.raw_x, geometry.raw_y)) {
        geometry.raw_x = kDefaultX;
        geometry.raw_y = kDefaultY;
        geometry.used_default_location = true;
    }
    if (!ParsePair(size, 1, kMaximumSize, geometry.width, geometry.height)) {
        geometry.width = kDefaultWidth;
        geometry.height = kDefaultHeight;
        geometry.used_default_size = true;
    }

    geometry.center_x = geometry.raw_x + geometry.width / 2;
    geometry.center_y = geometry.raw_y + geometry.height / 2;
    geometry.radius = (geometry.width + geometry.height) / 4;
    if (geometry.radius < kMinimumRadius) {
        geometry.radius = kDefaultRadius;
    }
    return geometry;
}

}  // namespace fod::udfps
