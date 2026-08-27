#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace easy::art {

/**
 * 2026年08月28日02时44分
 * 单个模块的符号表。
 *
 * 面向 ART hook 的符号解析有三点是系统 dlsym 做不到的：
 *   其一，需要未导出的内部符号，dlsym 只能看到 .dynsym 中的导出项；
 *   其二，Android 的 libart.so 被裁剪，完整符号表压缩在 .gnu_debugdata 中；
 *   其三，部分符号的完整修饰名随版本变化，只能按前缀匹配。
 * 本类把这三件事一次做完：映射模块文件，收集 .dynsym、.symtab 与解压后的调试符号表，
 * 统一换算成运行时绝对地址，并同时提供精确与前缀查询。
 * 2026 BY white
 */
class ElfSymbolTable final {
public:
    ElfSymbolTable() = default;

    ElfSymbolTable(const ElfSymbolTable&) = delete;
    ElfSymbolTable& operator=(const ElfSymbolTable&) = delete;
    ElfSymbolTable(ElfSymbolTable&&) = default;
    ElfSymbolTable& operator=(ElfSymbolTable&&) = default;

    /**
     * 2026年08月28日02时44分
     * 载入指定模块的符号表。模块须已加载到本进程，
     * 因为需要它的运行时基址把符号相对地址换算成可调用的绝对地址。
     * 2026 BY white
     */
    [[nodiscard]] bool Load(std::string_view module_name) noexcept;

    /**
     * 2026年08月28日03时06分
     * 直接按文件路径载入符号表，用于目标模块尚未映射进本进程的场合，
     * 例如脱离 ART 运行的诊断探针。此时解析出的是文件内相对地址（load_bias 记为 0），
     * 只可用于核对符号存在性与相对偏移，不可当作运行时可调用地址。
     * 2026 BY white
     */
    [[nodiscard]] bool LoadFromPath(const std::string& file_path) noexcept;

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] std::uintptr_t load_bias() const noexcept { return load_bias_; }

    [[nodiscard]] std::size_t dynamic_symbol_count() const noexcept {
        return dynamic_symbol_count_;
    }
    [[nodiscard]] std::size_t debug_symbol_count() const noexcept { return debug_symbol_count_; }
    [[nodiscard]] std::size_t total_symbol_count() const noexcept { return symbols_.size(); }

    /**
     * 2026年08月28日02时44分
     * 按完整符号名查询运行时绝对地址，未命中返回空指针。
     * 2026 BY white
     */
    [[nodiscard]] void* Find(std::string_view name) const noexcept;

    /**
     * 2026年08月28日02时44分
     * 按前缀查询，返回第一个匹配项。
     * 用于那些完整修饰名随版本变化、但前缀稳定的符号。
     * 命中多个时不保证顺序，因此前缀应当足够长以确保唯一。
     * 2026 BY white
     */
    [[nodiscard]] void* FindByPrefix(std::string_view prefix) const noexcept;

private:
    /**
     * 2026年08月28日02时44分
     * 收集一段 ELF 映像中全部符号表的符号，返回本次新增的条目数。
     * 收进来的是符号的相对虚拟地址，查询时再叠加 load_bias 得到运行时地址。
     * 2026 BY white
     */
    std::size_t CollectSymbols(const std::uint8_t* image, std::size_t image_size) noexcept;

    /**
     * 2026年08月28日02时44分
     * 抽取 .gnu_debugdata 并解压，把其中的符号一并收入。
     * 该节区缺失属于正常情况，此时静默返回。
     * 2026 BY white
     */
    void CollectFromDebugData(const std::uint8_t* image, std::size_t image_size) noexcept;

    bool loaded_{false};
    std::string path_;
    std::uintptr_t load_bias_{0};
    std::size_t dynamic_symbol_count_{0};
    std::size_t debug_symbol_count_{0};

    /**
     * 2026年08月28日02时44分
     * 名称到相对虚拟地址的映射。名称自带存储，因为解压出的映像在构造结束后即被释放。
     * 2026 BY white
     */
    std::unordered_map<std::string, std::uintptr_t> symbols_;

    /**
     * 2026年08月28日02时44分
     * 前缀查询需要遍历，单独保存一份键的视图避免每次都遍历哈希表的节点。
     * 2026 BY white
     */
    std::vector<const std::string*> names_;
};

}  // namespace easy::art
