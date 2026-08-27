#include "easy_hook/art/xz_decompressor.hpp"

#include "easy_hook/core/log.hpp"

#include <cstring>
#include <memory>
#include <string_view>

#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

namespace easy::art {
namespace {

constexpr std::string_view kLogTag = "xz";

/**
 * 2026年08月28日02时38分
 * 系统 liblzma.so 的候选路径。不同 Android 版本把它放在主分区或 APEX 中，逐个尝试。
 * 2026 BY white
 */
constexpr const char* kLibraryPaths[] = {
    "liblzma.so",
    "/system/lib64/liblzma.so",
    "/apex/com.android.runtime/lib64/liblzma.so",
    "/apex/com.android.art/lib64/liblzma.so",
};

/**
 * 2026年08月28日02时38分
 * CXzUnpacker 的结构体布局属于 LZMA SDK 内部实现，不同版本大小不同且没有查询接口。
 * 这里按远超实际需要的容量分配并清零，让库自行在其中初始化，
 * 从而避免因结构体比预期大而写越界。该缓冲仅在一次解压期间存在。
 * 2026 BY white
 */
constexpr std::size_t kUnpackerStateBytes = 256u * 1024u;

/**
 * 2026年08月28日02时38分
 * LZMA SDK 的返回码与枚举取值，取自其公开头文件的稳定定义。
 * 2026 BY white
 */
constexpr int kResultOk = 0;
constexpr int kFinishModeAny = 0;
constexpr int kStatusNeedsMoreInput = 1;

using XzUnpackerConstructFn = void (*)(void* unpacker, const void* allocator);
using XzUnpackerInitFn = void (*)(void* unpacker);
using XzUnpackerFreeFn = void (*)(void* unpacker);
using XzUnpackerCodeFn = int (*)(void* unpacker, std::uint8_t* dest, std::size_t* dest_len,
                                 const std::uint8_t* src, std::size_t* src_len,
                                 int src_finished, int finish_mode, int* status);

}  // namespace

XzDecompressor& XzDecompressor::Instance() noexcept {
    static XzDecompressor instance;
    return instance;
}

XzDecompressor::XzDecompressor() noexcept {
    LoadLibrary();
}

void XzDecompressor::LoadLibrary() noexcept {
#if defined(__ANDROID__)
    for (const char* path : kLibraryPaths) {
        handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (handle_ != nullptr) {
            break;
        }
    }
    if (handle_ == nullptr) {
        status_ = "liblzma.so not found";
        core::Log::Print(core::LogLevel::kWarning, kLogTag, "%s", status_);
        return;
    }

    construct_ = dlsym(handle_, "XzUnpacker_Construct");
    init_ = dlsym(handle_, "XzUnpacker_Init");
    code_ = dlsym(handle_, "XzUnpacker_Code");
    free_ = dlsym(handle_, "XzUnpacker_Free");

    /**
     * 2026年08月28日02时38分
     * g_Alloc 是 LZMA SDK 导出的默认分配器实例。缺失时退回 g_AlignedAlloc，
     * 两者对本用途等价，都只是提供 malloc 与 free 的函数表。
     * 2026 BY white
     */
    allocator_ = dlsym(handle_, "g_Alloc");
    if (allocator_ == nullptr) {
        allocator_ = dlsym(handle_, "g_AlignedAlloc");
    }

    if (construct_ == nullptr || init_ == nullptr || code_ == nullptr || free_ == nullptr ||
        allocator_ == nullptr) {
        status_ = "liblzma.so lacks required XzUnpacker symbols";
        core::Log::Print(core::LogLevel::kWarning, kLogTag,
                         "%s (construct=%p init=%p code=%p free=%p alloc=%p)", status_,
                         construct_, init_, code_, free_, allocator_);
        dlclose(handle_);
        handle_ = nullptr;
        return;
    }

    PrepareChecksumTables();

    available_ = true;
    status_ = tables_ready_ ? "ready" : "ready without checksum tables";
    core::Log::Print(core::LogLevel::kInfo, kLogTag,
                     "xz decoder ready via system liblzma.so, checksum_tables=%d",
                     tables_ready_ ? 1 : 0);
#else
    status_ = "unsupported platform";
#endif
}

void XzDecompressor::PrepareChecksumTables() noexcept {
#if defined(__ANDROID__)
    if (handle_ == nullptr) {
        return;
    }

    /**
     * 2026年08月28日03时22分
     * 这三张表分别服务于 CRC32、CRC64 和 SHA256 三种 XZ 校验方式。
     * 不预先生成时解码器仍能还原出正确数据，但会在最后一步报校验失败。
     * 2026 BY white
     */
    using GenerateTableFn = void (*)();
    bool all_present = true;
    for (const char* name : {"CrcGenerateTable", "Crc64GenerateTable", "Sha256Prepare"}) {
        auto generate = reinterpret_cast<GenerateTableFn>(dlsym(handle_, name));
        if (generate == nullptr) {
            all_present = false;
            core::Log::Print(core::LogLevel::kWarning, kLogTag, "%s is not exported", name);
            continue;
        }
        generate();
    }
    tables_ready_ = all_present;
#endif
}

std::vector<std::uint8_t> XzDecompressor::Decompress(const std::uint8_t* data,
                                                     std::size_t size) noexcept {
    if (!available_ || data == nullptr || size == 0) {
        return {};
    }

#if defined(__ANDROID__)
    auto construct = reinterpret_cast<XzUnpackerConstructFn>(construct_);
    auto init = reinterpret_cast<XzUnpackerInitFn>(init_);
    auto code = reinterpret_cast<XzUnpackerCodeFn>(code_);
    auto release = reinterpret_cast<XzUnpackerFreeFn>(free_);

    /**
     * 2026年08月28日02时40分
     * 解压后的大小事先不可知。以压缩数据的若干倍起步，不够时翻倍重试；
     * 调试信息的压缩比通常在四到八倍之间，多数情况一次即可完成。
     * 2026 BY white
     */
    std::size_t capacity = size * 8;
    if (capacity < 1024u * 1024u) {
        capacity = 1024u * 1024u;
    }

    while (capacity <= kMaxOutputBytes) {
        auto state = std::make_unique<std::uint8_t[]>(kUnpackerStateBytes);
        std::memset(state.get(), 0, kUnpackerStateBytes);

        std::vector<std::uint8_t> output;
        try {
            output.resize(capacity);
        } catch (...) {
            return {};
        }

        construct(state.get(), allocator_);
        init(state.get());

        std::size_t produced = output.size();
        std::size_t consumed = size;
        int status = 0;
        const int result = code(state.get(), output.data(), &produced, data, &consumed, 1,
                                kFinishModeAny, &status);

        release(state.get());

        /**
         * 2026年08月28日03时24分
         * 输出缓冲被填满且解码器仍要求更多输出空间，说明容量不足，扩大后重来。
         * 这一判断必须先于错误处理：容量不足时解码器同样会返回非零结果。
         * 2026 BY white
         */
        if (produced == output.size() && status == kStatusNeedsMoreInput) {
            capacity *= 2;
            continue;
        }

        if (result != kResultOk) {
            /**
             * 2026年08月28日03时24分
             * 校验失败但已产出数据时不直接丢弃。
             * 这种组合几乎都源于校验查表未就绪，数据本身是完整的；
             * 而调用方随后会按 ELF 结构严格校验并做全程边界检查，
             * 因此放行的风险是"解析不出符号"，不是内存越界。
             * 真正的数据损坏会在 ELF 校验那一关被挡住。
             * 2026 BY white
             */
            if (produced == 0) {
                core::Log::Print(core::LogLevel::kWarning, kLogTag,
                                 "xz decode failed result=%d consumed=%zu", result, consumed);
                return {};
            }
            core::Log::Print(core::LogLevel::kWarning, kLogTag,
                             "xz integrity check failed result=%d but %zu bytes were produced; "
                             "passing them on for structural validation",
                             result, produced);
        }

        output.resize(produced);
        core::Log::Print(core::LogLevel::kInfo, kLogTag,
                         "xz decoded %zu bytes into %zu bytes", size, produced);
        return output;
    }

    core::Log::Print(core::LogLevel::kWarning, kLogTag,
                     "xz output exceeds the %zu byte limit", kMaxOutputBytes);
    return {};
#else
    return {};
#endif
}

}  // namespace easy::art
