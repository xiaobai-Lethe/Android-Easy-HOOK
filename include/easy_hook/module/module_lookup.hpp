#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace easy::module {

/**
 * 2026年08月27日21时46分
 * 一个已映射的 PT_LOAD 段在运行时的地址范围。mapped_size 取 p_memsz 用于地址范围校验，
 * file_size 取 p_filesz 用于特征码搜索，因为搜索不能进入 p_memsz 覆盖的零填充区。
 * 2026 BY white
 */
struct ModuleSegment final {
    std::uintptr_t start{0};
    std::size_t mapped_size{0};
    std::size_t file_size{0};
    bool executable{false};
};

/**
 * 2026年08月27日21时46分
 * 一个已加载模块的运行时信息。load_bias 即 dl_phdr_info::dlpi_addr，
 * 是把 ELF 中的相对虚拟地址换算成运行时绝对地址所需的唯一加数，
 * 因此按 IDA 地址定位时必须用它而不是 /proc/self/maps 的首行基址。
 * 2026 BY white
 */
struct ModuleInfo final {
    std::string path;
    std::uintptr_t load_bias{0};
    std::vector<ModuleSegment> segments;

    /**
     * 2026年08月27日21时46分
     * 把 ELF 相对虚拟地址换算为运行时绝对地址，不校验范围。
     * 2026 BY white
     */
    [[nodiscard]] std::uintptr_t ToRuntimeAddress(std::uintptr_t relative_address) const noexcept;

    /**
     * 2026年08月27日21时46分
     * 判断给定相对虚拟地址是否落在本模块任一已映射段内，用于拦截明显写错的偏移。
     * 2026 BY white
     */
    [[nodiscard]] bool ContainsRelativeAddress(std::uintptr_t relative_address) const noexcept;
};

/**
 * 2026年08月27日21时46分
 * 已加载模块查询器。基于 dl_iterate_phdr 直接读取链接器维护的程序头，
 * 相比解析 /proc/self/maps 更准确，且能一次拿到 load_bias 与全部段范围。
 * 2026 BY white
 */
class ModuleLookup final {
public:
    ModuleLookup() = delete;

    /**
     * 2026年08月27日21时46分
     * 按模块名查找已加载模块。匹配规则为路径的文件名部分相等，或路径以该名称结尾，
     * 允许用户只填 "libart.so" 而不必写出完整路径。
     * 2026 BY white
     */
    [[nodiscard]] static std::optional<ModuleInfo> Find(std::string_view module_name) noexcept;

    /**
     * 2026年08月27日21时46分
     * 判断模块是否已加载，供引擎区分"模块尚未加载"与"模块已加载但目标找不到"。
     * 2026 BY white
     */
    [[nodiscard]] static bool IsLoaded(std::string_view module_name) noexcept;
};

}  // namespace easy::module
