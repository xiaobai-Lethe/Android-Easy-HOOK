#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace easy::art {

/**
 * 2026年08月28日02时36分
 * XZ 解压器。用于还原 ELF 的 .gnu_debugdata 节区。
 *
 * Android 的 libart.so 被裁剪后不保留 .symtab，完整符号表被 XZ 压缩后放进 .gnu_debugdata，
 * 而 ART hook 所需的若干内部符号只存在于其中，因此必须先解压才能解析。
 *
 * 实现方式是运行时加载系统自带的 liblzma.so 并绑定其解码接口，
 * 不在框架内嵌入解码器：该库是 7-Zip LZMA SDK 构建，已导出完整的 XzUnpacker 接口。
 * 系统缺少该库或接口不全时，本类的 available() 返回 false，调用方应据此降级而不是崩溃。
 * 2026 BY white
 */
class XzDecompressor final {
public:
    /**
     * 2026年08月28日02时36分
     * 返回进程内唯一实例。首次访问时才加载系统库，未用到时不产生任何开销。
     * 2026 BY white
     */
    [[nodiscard]] static XzDecompressor& Instance() noexcept;

    XzDecompressor(const XzDecompressor&) = delete;
    XzDecompressor& operator=(const XzDecompressor&) = delete;

    /**
     * 2026年08月28日02时36分
     * 解码接口是否全部就绪。为 false 时 Decompress 必定失败。
     * 2026 BY white
     */
    [[nodiscard]] bool available() const noexcept { return available_; }

    /**
     * 2026年08月28日02时36分
     * 说明当前状态的可读原因，用于日志排查，例如系统库缺失或某个符号未导出。
     * 2026 BY white
     */
    [[nodiscard]] const char* status() const noexcept { return status_; }

    /**
     * 2026年08月28日02时36分
     * 解压一段完整的 XZ 数据。
     * 解压后的大小事先未知，内部按倍数扩张输出缓冲重试，上限由 kMaxOutputBytes 限制，
     * 避免异常输入导致内存被无限占用。失败时返回空 vector。
     * 2026 BY white
     */
    [[nodiscard]] std::vector<std::uint8_t> Decompress(const std::uint8_t* data,
                                                       std::size_t size) noexcept;

    static constexpr std::size_t kMaxOutputBytes = 64u * 1024u * 1024u;

private:
    XzDecompressor() noexcept;

    /**
     * 2026年08月28日02时36分
     * 尝试从系统各候选路径加载 liblzma.so 并绑定所需符号。
     * 2026 BY white
     */
    void LoadLibrary() noexcept;

    /**
     * 2026年08月28日03时22分
     * 生成 CRC 与摘要查表。LZMA SDK 要求在使用任何解码器之前先调用这些函数，
     * 否则数据能正确解出，但流尾部的完整性校验必定失败。
     * 2026 BY white
     */
    void PrepareChecksumTables() noexcept;

    void* handle_{nullptr};
    void* construct_{nullptr};
    void* init_{nullptr};
    void* code_{nullptr};
    void* free_{nullptr};
    void* allocator_{nullptr};
    bool available_{false};
    bool tables_ready_{false};
    const char* status_{"not initialized"};
};

}  // namespace easy::art
