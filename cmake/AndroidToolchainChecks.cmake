#[[
2026年08月27日21时54分
校验 Android 工具链配置是否满足框架要求，在配置阶段尽早失败，
避免用错 ABI 或过低的 API 级别时到链接阶段才暴露问题。
2026 BY white
]]

function(easy_hook_validate_android_toolchain)
    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "Easy Hook currently targets arm64-v8a only, got '${CMAKE_ANDROID_ARCH_ABI}'")
    endif()

    if(DEFINED ANDROID_PLATFORM_LEVEL)
        set(platform_level "${ANDROID_PLATFORM_LEVEL}")
    elseif(DEFINED CMAKE_ANDROID_API)
        set(platform_level "${CMAKE_ANDROID_API}")
    elseif(ANDROID_PLATFORM MATCHES "^android-([0-9]+)$")
        set(platform_level "${CMAKE_MATCH_1}")
    else()
        message(FATAL_ERROR "Unable to determine the Android API level")
    endif()

    if(platform_level VERSION_LESS 30)
        message(FATAL_ERROR "Easy Hook requires Android API level 30 or newer, got ${platform_level}")
    endif()
endfunction()
