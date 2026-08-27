#include "easy_hook/backend/dobby_backend.hpp"

#if defined(__ANDROID__)
#include "dobby.h"
#endif

namespace easy::backend {

bool DobbyBackend::Hook(std::uintptr_t target, void* replacement,
                        void** original_slot) noexcept {
    if (target == 0 || replacement == nullptr || original_slot == nullptr) {
        return false;
    }

#if defined(__ANDROID__)
    /**
     * 2026年08月27日21时48分
     * 先写入本地变量再回填调用方槽位，避免 Dobby 安装失败时把半成品地址留给用户，
     * 使 EASY_HAS_ORIGINAL 的语义始终与实际安装结果一致。
     * 2026 BY white
     */
    void* original = nullptr;
    const int status = DobbyHook(reinterpret_cast<void*>(target),
                                 reinterpret_cast<dobby_dummy_func_t>(replacement),
                                 reinterpret_cast<dobby_dummy_func_t*>(&original));
    if (status != 0 || original == nullptr) {
        return false;
    }
    *original_slot = original;
    return true;
#else
    return false;
#endif
}

bool DobbyBackend::Unhook(std::uintptr_t target) noexcept {
    if (target == 0) {
        return false;
    }

#if defined(__ANDROID__)
    return DobbyDestroy(reinterpret_cast<void*>(target)) == 0;
#else
    return false;
#endif
}

const char* DobbyBackend::name() const noexcept {
    return "dobby";
}

InlineHookBackend& DefaultInlineHookBackend() noexcept {
    static DobbyBackend backend;
    return backend;
}

}  // namespace easy::backend
