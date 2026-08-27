#include "easy_hook/core/process_filter.hpp"

#include "easy_hook/core/registry.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>

#if defined(__ANDROID__) || defined(__linux__)
#include <fcntl.h>
#include <sys/types.h>
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
 * 2026年08月28日01时22分
 * 进程名缓存，按 pid 失效。
 *
 * 早先这里用的是"函数内静态对象只初始化一次"的写法，在 Android 上是错的：
 * 应用进程由 zygote fork 而来，fork 会连同这份已初始化的缓存一起继承下去。
 * 只要 zygote 在 fork 之前读过一次进程名，之后所有应用进程拿到的都是 zygote 的名字，
 * 依赖进程名的判断(包括 EASY_HOOK_ONLY_IN)在应用进程中会全部失效。
 *
 * 因此缓存必须与 pid 绑定：pid 变了说明已经 fork 到新进程，需要重新读取。
 * 存储用固定缓冲区而不是 std::string，是为了让返回的指针地址始终稳定，
 * 不会因为重新读取而失效。
 * 2026 BY white
 */
constexpr std::size_t kProcessNameCapacity = 256;

std::mutex process_name_mutex;
std::array<char, kProcessNameCapacity> process_name_buffer{};
pid_t process_name_pid = -1;

const char* CachedProcessName() noexcept {
    const pid_t current_pid = getpid();

    const std::lock_guard lock(process_name_mutex);
    if (process_name_pid != current_pid) {
        const std::string name = ReadProcessNameFromProc();
        const std::size_t length = name.size() < kProcessNameCapacity - 1
                                       ? name.size()
                                       : kProcessNameCapacity - 1;
        std::memcpy(process_name_buffer.data(), name.data(), length);
        process_name_buffer[length] = '\0';
        process_name_pid = current_pid;
    }
    return process_name_buffer.data();
}

}  // namespace

ProcessFilter::ProcessFilter(const char* process_name) noexcept {
    Registry::Instance().AddAllowedProcess(process_name);
}

const char* CurrentProcessName() noexcept {
    return CachedProcessName();
}

}  // namespace easy::core
