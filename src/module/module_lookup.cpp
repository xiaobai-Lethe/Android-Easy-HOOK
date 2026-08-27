#include "easy_hook/module/module_lookup.hpp"

#include <cstddef>

#if defined(__ANDROID__) || defined(__linux__)
#include <elf.h>
#include <link.h>
#endif

namespace easy::module {
namespace {

/**
 * 2026年08月27日21时48分
 * 取路径的文件名部分。用于把用户填写的 "libart.so" 与链接器给出的完整路径做比对。
 * 2026 BY white
 */
std::string_view BaseName(std::string_view path) noexcept {
    const auto separator = path.find_last_of('/');
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

/**
 * 2026年08月27日21时48分
 * 判断链接器给出的模块路径是否与用户填写的模块名匹配。
 * 优先按文件名精确相等判断，其次允许路径以该名称结尾，以兼容
 * "/data/app/xxx/base.apk!/lib/arm64-v8a/libfoo.so" 这类带压缩包前缀的路径。
 * 2026 BY white
 */
bool PathMatchesModuleName(std::string_view path, std::string_view module_name) noexcept {
    if (path.empty() || module_name.empty()) {
        return false;
    }
    if (BaseName(path) == BaseName(module_name)) {
        return true;
    }
    return path.size() >= module_name.size() && path.ends_with(module_name);
}

#if defined(__ANDROID__) || defined(__linux__)

/**
 * 2026年08月27日21时48分
 * dl_iterate_phdr 的遍历上下文。found 置位后回调返回非零以提前终止遍历。
 * 2026 BY white
 */
struct LookupContext final {
    std::string_view module_name;
    ModuleInfo* result{nullptr};
    bool found{false};
};

int CollectModule(struct dl_phdr_info* info, std::size_t /*size*/, void* data) noexcept {
    auto* context = static_cast<LookupContext*>(data);
    if (context == nullptr || info == nullptr || info->dlpi_name == nullptr) {
        return 0;
    }

    const std::string_view path(info->dlpi_name);
    if (!PathMatchesModuleName(path, context->module_name)) {
        return 0;
    }

    ModuleInfo& module_info = *context->result;
    module_info.path.assign(path);
    module_info.load_bias = static_cast<std::uintptr_t>(info->dlpi_addr);
    module_info.segments.clear();

    for (ElfW(Half) index = 0; index < info->dlpi_phnum; ++index) {
        const ElfW(Phdr)& header = info->dlpi_phdr[index];
        if (header.p_type != PT_LOAD) {
            continue;
        }
        module_info.segments.push_back(ModuleSegment{
            .start = module_info.load_bias + static_cast<std::uintptr_t>(header.p_vaddr),
            .mapped_size = static_cast<std::size_t>(header.p_memsz),
            .file_size = static_cast<std::size_t>(header.p_filesz),
            .executable = (header.p_flags & PF_X) != 0,
        });
    }

    context->found = true;
    return 1;
}

#endif

}  // namespace

std::uintptr_t ModuleInfo::ToRuntimeAddress(std::uintptr_t relative_address) const noexcept {
    return load_bias + relative_address;
}

bool ModuleInfo::ContainsRelativeAddress(std::uintptr_t relative_address) const noexcept {
    const std::uintptr_t absolute = ToRuntimeAddress(relative_address);
    for (const ModuleSegment& segment : segments) {
        if (segment.mapped_size == 0) {
            continue;
        }
        if (absolute >= segment.start && absolute < segment.start + segment.mapped_size) {
            return true;
        }
    }
    return false;
}

std::optional<ModuleInfo> ModuleLookup::Find(std::string_view module_name) noexcept {
    if (module_name.empty()) {
        return std::nullopt;
    }

#if defined(__ANDROID__) || defined(__linux__)
    ModuleInfo module_info;
    LookupContext context{
        .module_name = module_name,
        .result = &module_info,
        .found = false,
    };
    dl_iterate_phdr(&CollectModule, &context);
    if (!context.found || module_info.segments.empty()) {
        return std::nullopt;
    }
    return module_info;
#else
    return std::nullopt;
#endif
}

bool ModuleLookup::IsLoaded(std::string_view module_name) noexcept {
    return Find(module_name).has_value();
}

}  // namespace easy::module
