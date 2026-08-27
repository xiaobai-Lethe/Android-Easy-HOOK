<#
2026年08月27日21时56分
在 WSL 中用 Android NDK 交叉编译 libeasyhook.so，并输出产物路径与 SHA-256。
脚本只做环境预检和命令转发，所有构建参数与 CMakeLists 中的约定保持一致。
2026 BY white
#>

param(
    [string]$NdkPath = "/opt/android-ndk-r29",
    [string]$BuildDir = "build-android-ninja",
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$BuildType = "Release",
    [ValidateSet("ON", "OFF")]
    [string]$DebugLogging = "ON"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Quote-Bash {
    param([Parameter(Mandatory = $true)][string]$Value)
    if ($Value.Contains("'")) {
        throw "Bash argument contains an unsupported single quote: ${Value}"
    }
    return "'" + $Value + "'"
}

function Invoke-CheckedWsl {
    param([Parameter(Mandatory = $true)][string]$Command)

    & wsl bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed with exit code ${LASTEXITCODE}: ${Command}"
    }
}

function Convert-ToWslPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)

    $resolved = (Resolve-Path -LiteralPath $WindowsPath).Path
    if ($resolved -notmatch "^([A-Za-z]):\\(.*)$") {
        throw "Only drive-letter Windows paths are supported: ${resolved}"
    }

    $drive = $Matches[1].ToLowerInvariant()
    $tail = $Matches[2].Replace('\', '/')
    return "/mnt/${drive}/${tail}"
}

function Convert-ToWindowsPath {
    param([Parameter(Mandatory = $true)][string]$WslPath)

    if ($WslPath -notmatch "^/mnt/([A-Za-z])/(.*)$") {
        throw "Only /mnt/<drive> WSL paths are supported: ${WslPath}"
    }

    $drive = $Matches[1].ToUpperInvariant()
    $tail = $Matches[2].Replace('/', '\')
    return "${drive}:\${tail}"
}

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$repoWsl = Convert-ToWslPath -WindowsPath $repoRoot.Path
$toolchainFile = "${NdkPath}/build/cmake/android.toolchain.cmake"

Invoke-CheckedWsl -Command "command -v cmake >/dev/null && command -v ninja >/dev/null"
Invoke-CheckedWsl -Command "test -f $(Quote-Bash $toolchainFile)"
Invoke-CheckedWsl -Command "test -d $(Quote-Bash "${repoWsl}/dobby")"

$configureCommand = @(
    "cd $(Quote-Bash $repoWsl)",
    ("cmake -G Ninja -S . -B $(Quote-Bash $BuildDir)" +
     " -DCMAKE_BUILD_TYPE=${BuildType}" +
     " -DCMAKE_TOOLCHAIN_FILE=$(Quote-Bash $toolchainFile)" +
     " -DANDROID_ABI=arm64-v8a" +
     " -DANDROID_PLATFORM=android-30" +
     " -DANDROID_STL=c++_static" +
     " -DEASY_HOOK_DEBUG_DEFAULT=${DebugLogging}")
) -join " && "
Invoke-CheckedWsl -Command $configureCommand

$buildCommand = @(
    "cd $(Quote-Bash $repoWsl)",
    "cmake --build $(Quote-Bash $BuildDir) --target easyhook --parallel"
) -join " && "
Invoke-CheckedWsl -Command $buildCommand

$artifactWsl = "${repoWsl}/${BuildDir}/libeasyhook.so"
Invoke-CheckedWsl -Command "test -f $(Quote-Bash $artifactWsl)"
$artifactWindows = Convert-ToWindowsPath -WslPath $artifactWsl
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $artifactWindows

Write-Host "Artifact: $artifactWindows"
Write-Host "SHA256: $($hash.Hash)"
