# 构建与部署

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

## 环境要求

| 项目 | 要求 |
| --- | --- |
| 宿主系统 | Windows + WSL |
| CMake | 3.28+ |
| 构建器 | Ninja |
| Android NDK | r29 |
| 目标 ABI | `arm64-v8a` |
| 最低 API | Android 11 / API 30 |
| C++ 标准 | C++23 |

## 一键构建

```powershell
.\scripts\build-android.ps1

# 自定义路径与选项
.\scripts\build-android.ps1 `
  -NdkPath /opt/android-ndk-r29 `
  -BuildDir build-android-ninja `
  -BuildType Release `
  -DebugLogging ON
```

<details>
<summary>在 WSL 中手动构建</summary>

```bash
cmake -G Ninja -S . -B build-android-ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/opt/android-ndk-r29/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30 \
  -DANDROID_STL=c++_static \
  -DEASY_HOOK_DEBUG_DEFAULT=ON

cmake --build build-android-ninja --target easyhook --parallel
```

</details>

## 部署到 ROM

以 `app_process64` 的 `DT_NEEDED` 注入为例，解包后的分区需同时满足以下四项。
**缺任何一项都不会报错，只会静默不生效**，建议逐条核对：

```bash
# 1. 放入并剥离（剥离后从十余 MB 降到数百 KB，功能一致）
cp build-android-ninja/libeasyhook.so  $ROM/system/system/lib64/libeasyhook.so
llvm-strip --strip-all                 $ROM/system/system/lib64/libeasyhook.so

# 2. 替换 app_process64 原有的某个 NEEDED 项，使其指向 libeasyhook.so
patchelf --replace-needed <原依赖库>.so libeasyhook.so $ROM/system/system/bin/app_process64
patchelf --print-needed $ROM/system/system/bin/app_process64
```

```text
# 3. config/system_fs_config 增加权限条目
system/system/lib64/libeasyhook.so 0 0 0644

# 4. config/system_file_contexts 增加 SELinux 上下文（注意 . 要转义）
/system/system/lib64/libeasyhook\.so u:object_r:system_lib_file:s0
```

改完重新打包刷入。`/` 通常是只读 erofs，无法 remount，每次验证都需要完整打包刷机。
