#include "easy_hook/art/elf_symbol_table.hpp"

#include "easy_hook/art/xz_decompressor.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/module/module_lookup.hpp"

#include <cstring>
#include <optional>

#if defined(__ANDROID__) || defined(__linux__)
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace easy::art {
namespace {

constexpr std::string_view kLogTag = "elf-symbols";

#if defined(__ANDROID__) || defined(__linux__)

/**
 * 2026年08月28日02时50分
 * 只读映射一个文件，析构时自动解除映射。
 * 符号表解析只需读取，映射比逐段读取更省事，也不必把整个文件读进堆内存。
 * 2026 BY white
 */
class MappedFile final {
public:
    explicit MappedFile(const std::string& path) noexcept {
        const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (descriptor < 0) {
            return;
        }

        struct stat info{};
        if (fstat(descriptor, &info) != 0 || info.st_size <= 0) {
            close(descriptor);
            return;
        }

        const auto length = static_cast<std::size_t>(info.st_size);
        void* mapped = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, descriptor, 0);
        close(descriptor);
        if (mapped == MAP_FAILED) {
            return;
        }
        data_ = static_cast<const std::uint8_t*>(mapped);
        size_ = length;
    }

    ~MappedFile() {
        if (data_ != nullptr) {
            munmap(const_cast<std::uint8_t*>(data_), size_);
        }
    }

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    const std::uint8_t* data_{nullptr};
    std::size_t size_{0};
};

/**
 * 2026年08月28日02时50分
 * 校验一段内存是否为可解析的 ELF 映像。
 * 输入可能来自解压结果，必须先确认头部与节区表都在范围内，
 * 否则后续按偏移访问会读到映射之外而导致崩溃。
 * 2026 BY white
 */
[[nodiscard]] bool IsUsableElf(const std::uint8_t* image, std::size_t size) noexcept {
    if (image == nullptr || size < sizeof(ElfW(Ehdr))) {
        return false;
    }
    if (std::memcmp(image, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    const auto* header = reinterpret_cast<const ElfW(Ehdr)*>(image);
    if (header->e_shoff == 0 || header->e_shnum == 0 || header->e_shentsize == 0) {
        return false;
    }
    if (header->e_shentsize != sizeof(ElfW(Shdr))) {
        return false;
    }

    const std::size_t table_bytes =
        static_cast<std::size_t>(header->e_shnum) * header->e_shentsize;
    return header->e_shoff < size && table_bytes <= size - header->e_shoff;
}

#endif

}  // namespace

bool ElfSymbolTable::Load(std::string_view module_name) noexcept {
#if defined(__ANDROID__) || defined(__linux__)
    const std::optional<module::ModuleInfo> module_info =
        module::ModuleLookup::Find(module_name);
    if (!module_info.has_value()) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "module %.*s is not loaded",
                         static_cast<int>(module_name.size()), module_name.data());
        return false;
    }

    load_bias_ = module_info->load_bias;
    return LoadFromPath(module_info->path);
#else
    static_cast<void>(module_name);
    return false;
#endif
}

bool ElfSymbolTable::LoadFromPath(const std::string& file_path) noexcept {
#if defined(__ANDROID__) || defined(__linux__)
    path_ = file_path;

    const MappedFile file(path_);
    if (!file.valid()) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "cannot map %s", path_.c_str());
        return false;
    }
    if (!IsUsableElf(file.data(), file.size())) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "%s is not a usable ELF image",
                         path_.c_str());
        return false;
    }

    dynamic_symbol_count_ = CollectSymbols(file.data(), file.size());
    CollectFromDebugData(file.data(), file.size());

    names_.reserve(symbols_.size());
    for (const auto& entry : symbols_) {
        names_.push_back(&entry.first);
    }

    loaded_ = !symbols_.empty();
    core::Log::Print(core::LogLevel::kInfo, kLogTag,
                     "%s load_bias=%p symbols file=%zu debugdata=%zu total=%zu", path_.c_str(),
                     reinterpret_cast<void*>(load_bias_), dynamic_symbol_count_,
                     debug_symbol_count_, symbols_.size());
    return loaded_;
#else
    static_cast<void>(file_path);
    return false;
#endif
}

std::size_t ElfSymbolTable::CollectSymbols(const std::uint8_t* image,
                                           std::size_t image_size) noexcept {
#if defined(__ANDROID__) || defined(__linux__)
    const auto* header = reinterpret_cast<const ElfW(Ehdr)*>(image);
    const auto* sections = reinterpret_cast<const ElfW(Shdr)*>(image + header->e_shoff);

    std::size_t collected = 0;
    for (ElfW(Half) index = 0; index < header->e_shnum; ++index) {
        const ElfW(Shdr)& section = sections[index];
        if (section.sh_type != SHT_SYMTAB && section.sh_type != SHT_DYNSYM) {
            continue;
        }
        if (section.sh_entsize == 0 || section.sh_link >= header->e_shnum) {
            continue;
        }

        const ElfW(Shdr)& strings = sections[section.sh_link];
        if (section.sh_offset > image_size || section.sh_size > image_size - section.sh_offset) {
            continue;
        }
        if (strings.sh_offset > image_size || strings.sh_size > image_size - strings.sh_offset) {
            continue;
        }

        const auto* entries = reinterpret_cast<const ElfW(Sym)*>(image + section.sh_offset);
        const char* string_table = reinterpret_cast<const char*>(image + strings.sh_offset);
        const std::size_t count = section.sh_size / section.sh_entsize;

        for (std::size_t entry_index = 0; entry_index < count; ++entry_index) {
            const ElfW(Sym)& symbol = entries[entry_index];

            /**
             * 2026年08月28日02时50分
             * 只收录有名字、有地址的已定义符号。
             * 未定义符号没有地址，收进来会在查询时给出错误结果。
             * 2026 BY white
             */
            if (symbol.st_name == 0 || symbol.st_value == 0 || symbol.st_shndx == SHN_UNDEF) {
                continue;
            }
            if (symbol.st_name >= strings.sh_size) {
                continue;
            }

            const char* name = string_table + symbol.st_name;
            if (name[0] == '\0') {
                continue;
            }

            /**
             * 2026年08月28日02时50分
             * 同名以先收录者为准。文件自带的符号表先于调试符号表处理，
             * 两者对同一符号给出的地址一致，重复时无需覆盖。
             * 2026 BY white
             */
            try {
                if (symbols_.try_emplace(name, static_cast<std::uintptr_t>(symbol.st_value))
                        .second) {
                    ++collected;
                }
            } catch (...) {
                return collected;
            }
        }
    }
    return collected;
#else
    static_cast<void>(image);
    static_cast<void>(image_size);
    return 0;
#endif
}

void ElfSymbolTable::CollectFromDebugData(const std::uint8_t* image,
                                          std::size_t image_size) noexcept {
#if defined(__ANDROID__) || defined(__linux__)
    const auto* header = reinterpret_cast<const ElfW(Ehdr)*>(image);
    if (header->e_shstrndx >= header->e_shnum) {
        return;
    }

    const auto* sections = reinterpret_cast<const ElfW(Shdr)*>(image + header->e_shoff);
    const ElfW(Shdr)& section_names = sections[header->e_shstrndx];
    if (section_names.sh_offset > image_size ||
        section_names.sh_size > image_size - section_names.sh_offset) {
        return;
    }
    const char* name_table = reinterpret_cast<const char*>(image + section_names.sh_offset);

    for (ElfW(Half) index = 0; index < header->e_shnum; ++index) {
        const ElfW(Shdr)& section = sections[index];
        if (section.sh_name >= section_names.sh_size) {
            continue;
        }
        if (std::strcmp(name_table + section.sh_name, ".gnu_debugdata") != 0) {
            continue;
        }
        if (section.sh_size == 0 || section.sh_offset > image_size ||
            section.sh_size > image_size - section.sh_offset) {
            return;
        }

        XzDecompressor& decompressor = XzDecompressor::Instance();
        if (!decompressor.available()) {
            core::Log::Print(core::LogLevel::kWarning, kLogTag,
                             "found .gnu_debugdata but the xz decoder is unavailable: %s",
                             decompressor.status());
            return;
        }

        const std::vector<std::uint8_t> decoded =
            decompressor.Decompress(image + section.sh_offset, section.sh_size);
        if (decoded.empty() || !IsUsableElf(decoded.data(), decoded.size())) {
            core::Log::Print(core::LogLevel::kWarning, kLogTag,
                             ".gnu_debugdata did not decode into a usable ELF image");
            return;
        }

        debug_symbol_count_ = CollectSymbols(decoded.data(), decoded.size());
        return;
    }
#else
    static_cast<void>(image);
    static_cast<void>(image_size);
#endif
}

void* ElfSymbolTable::Find(std::string_view name) const noexcept {
    if (name.empty() || symbols_.empty()) {
        return nullptr;
    }

    try {
        const auto found = symbols_.find(std::string(name));
        if (found == symbols_.end()) {
            return nullptr;
        }
        return reinterpret_cast<void*>(load_bias_ + found->second);
    } catch (...) {
        return nullptr;
    }
}

void* ElfSymbolTable::FindByPrefix(std::string_view prefix) const noexcept {
    if (prefix.empty() || symbols_.empty()) {
        return nullptr;
    }

    /**
     * 2026年08月28日02时50分
     * 前缀查询需要线性遍历。它只在初始化阶段被少量符号使用，
     * 相比之下精确查询才是高频路径，因此不为此额外维护有序索引。
     * 2026 BY white
     */
    for (const std::string* name : names_) {
        if (name == nullptr || name->size() < prefix.size()) {
            continue;
        }
        if (std::string_view(*name).substr(0, prefix.size()) != prefix) {
            continue;
        }
        const auto found = symbols_.find(*name);
        if (found != symbols_.end()) {
            return reinterpret_cast<void*>(load_bias_ + found->second);
        }
    }
    return nullptr;
}

}  // namespace easy::art
