#include "easy_hook/core/process_filter.hpp"

#include "easy_hook/core/registry.hpp"

#include <array>
#include <cstddef>
#include <string>

#if defined(__ANDROID__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace easy::core {
namespace {

/**
 * 2026年08月27日22时02分
 * 读取 /proc/self/cmdline 的首个字段作为进程名。
 * cmdline 以 NUL 分隔各参数，因此把缓冲区当作 C 字符串解释时天然只取到第一个参数。
 * 2026 BY white
 */
std::string ReadProcessNameFromProc() {
#if defined(__ANDROID__) || defined(__linux__)
    const int descriptor = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return {};
    }

    std::array<char, 256> buffer{};
    const ssize_t count = read(descriptor, buffer.data(), buffer.size() - 1);
    close(descriptor);
    if (count <= 0) {
        return {};
    }

    buffer[static_cast<std::size_t>(count)] = '\0';
    return std::string(buffer.data());
#else
    return {};
#endif
}

/**
 * 2026年08月27日22时02分
 * 进程名缓存。进程名在进程生命周期内不变，用函数内静态对象缓存，
 * 既避免重复读文件，也由 C++ 运行时保证初始化的线程安全。
 * 2026 BY white
 */
const std::string& CachedProcessName() {
    static const std::string process_name = ReadProcessNameFromProc();
    return process_name;
}

}  // namespace

ProcessFilter::ProcessFilter(const char* process_name) noexcept {
    Registry::Instance().AddAllowedProcess(process_name);
}

const char* CurrentProcessName() noexcept {
    return CachedProcessName().c_str();
}

}  // namespace easy::core
