#include "fod/core/process_context.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

namespace fod::core {
namespace {

std::string_view FirstToken(std::string_view command_line) noexcept {
    const auto first = command_line.find_first_not_of(" \t\r\n\0");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto end = command_line.find_first_of(" \t\r\n\0", first);
    return command_line.substr(first, end == std::string_view::npos ? end : end - first);
}

bool MatchesPackageProcess(std::string_view token, std::string_view package_name) noexcept {
    if (token == package_name) {
        return true;
    }
    return token.size() > package_name.size() && token.starts_with(package_name) &&
           token[package_name.size()] == ':';
}

bool HasSelinuxDomain(std::string_view context, std::string_view domain) noexcept {
    return context == domain ||
           (context.size() > domain.size() && context.starts_with(domain) &&
            context[domain.size()] == ':');
}

std::string ReadProcFile(const char* path, bool replace_nul) {
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return {};
    }

    std::array<char, 1024> buffer{};
    const ssize_t count = read(descriptor, buffer.data(), buffer.size() - 1);
    const int saved_errno = errno;
    close(descriptor);
    errno = saved_errno;
    if (count <= 0) {
        return {};
    }

    std::string result(buffer.data(), static_cast<std::size_t>(count));
    if (replace_nul) {
        std::replace(result.begin(), result.end(), '\0', ' ');
    }
    while (!result.empty() &&
           (result.back() == '\0' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

}  // namespace

ProcessRole ProcessContext::Classify(std::string_view command_line,
                                     std::string_view selinux_context) noexcept {
    const std::string_view token = FirstToken(command_line);

    if (token == "system_server" || HasSelinuxDomain(selinux_context, "u:r:system_server:s0")) {
        return ProcessRole::kSystemServer;
    }
    if (MatchesPackageProcess(token, "com.android.settings")) {
        return ProcessRole::kSettings;
    }
    if (MatchesPackageProcess(token, "com.android.systemui")) {
        return ProcessRole::kSystemUi;
    }
    if (token == "zygote" || token == "zygote64" ||
        HasSelinuxDomain(selinux_context, "u:r:zygote:s0")) {
        return ProcessRole::kZygote;
    }
    return token.empty() ? ProcessRole::kUnknown : ProcessRole::kOther;
}

ProcessSnapshot ProcessContext::ReadCurrent() {
    ProcessSnapshot snapshot;
    snapshot.command_line = ReadProcFile("/proc/self/cmdline", true);
    snapshot.selinux_context = ReadProcFile("/proc/self/attr/current", false);
    snapshot.role = Classify(snapshot.command_line, snapshot.selinux_context);
    return snapshot;
}

ProcessRole ProcessContext::CurrentRole() noexcept {
    static pid_t cached_pid = -1;
    static ProcessRole cached_role = ProcessRole::kUnknown;

    const pid_t current_pid = getpid();
    if (cached_pid == current_pid) {
        return cached_role;
    }

    try {
        cached_role = ReadCurrent().role;
    } catch (...) {
        cached_role = ProcessRole::kUnknown;
    }
    cached_pid = current_pid;
    return cached_role;
}

}  // namespace fod::core
