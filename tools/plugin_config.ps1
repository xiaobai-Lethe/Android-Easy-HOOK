<#
.SYNOPSIS
    Easy Hook 插件编译选择器（menuconfig 风格）。

.DESCRIPTION
    扫描 plugins/ 下的所有插件，在终端里用键盘勾选要编译哪些：
        上 / 下      移动光标
        空格         切换开/关
        a            全开
        n            全关
        回车         保存并退出
        q / Esc      取消退出（不改动）

    选择结果写入 <repo>/plugins.config（每行一个启用的插件文件夹名，# 开头为注释）。
    CMake 配置时会自动读取该文件，因此保存后正常运行 cmake 即可生效。
#>

$ErrorActionPreference = 'Stop'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot   = Split-Path -Parent $scriptDir          # tools/ 的上级即工程根（main/）
$pluginsDir = Join-Path $repoRoot 'plugins'
$configFile = Join-Path $repoRoot 'plugins.config'

if (-not (Test-Path -LiteralPath $pluginsDir)) {
    Write-Host "找不到插件目录: $pluginsDir" -ForegroundColor Red
    exit 1
}

# 发现插件：plugins/ 下的每个子目录
$plugins = @(
    Get-ChildItem -LiteralPath $pluginsDir -Directory | Sort-Object Name | ForEach-Object {
        $cpp = @(Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Filter *.cpp)
        [pscustomobject]@{
            Name    = $_.Name
            Count   = $cpp.Count
            Enabled = $false
        }
    }
)

if ($plugins.Count -eq 0) {
    Write-Host "plugins/ 下没有任何插件。" -ForegroundColor Yellow
    exit 0
}

# 载入已有选择
if (Test-Path -LiteralPath $configFile) {
    $enabledSet = @(
        Get-Content -LiteralPath $configFile |
            ForEach-Object { $_.Trim() } |
            Where-Object { $_ -ne '' -and -not $_.StartsWith('#') }
    )
    foreach ($p in $plugins) {
        if ($enabledSet -contains $p.Name) { $p.Enabled = $true }
    }
}

$cursor = 0

function Show-Menu {
    Clear-Host
    Write-Host "Easy Hook 插件编译选择" -ForegroundColor Cyan
    Write-Host "↑/↓ 移动   空格 开关   a 全开   n 全关   回车 保存   q 取消" -ForegroundColor DarkGray
    Write-Host ""
    for ($i = 0; $i -lt $plugins.Count; $i++) {
        $p = $plugins[$i]
        $mark = if ($p.Enabled) { '[x]' } else { '[ ]' }
        $line = '{0} {1}  ({2} 源文件)' -f $mark, $p.Name, $p.Count
        if ($i -eq $cursor) {
            Write-Host (' > ' + $line) -ForegroundColor Black -BackgroundColor Green
        }
        else {
            Write-Host ('   ' + $line)
        }
    }
    Write-Host ""
}

function Save-Config {
    $enabled = @($plugins | Where-Object { $_.Enabled } | ForEach-Object { $_.Name })
    $lines = @(
        '# Easy Hook 启用的插件清单，由 tools/plugin_config.ps1 生成。',
        '# 每行一个 plugins/ 下的文件夹名；# 开头为注释；留空表示全部关闭。'
    ) + $enabled
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines($configFile, $lines, $utf8NoBom)
    return $enabled
}

while ($true) {
    Show-Menu
    $key = [System.Console]::ReadKey($true)

    switch ($key.Key) {
        'UpArrow'   { $cursor = ($cursor - 1 + $plugins.Count) % $plugins.Count; continue }
        'DownArrow' { $cursor = ($cursor + 1) % $plugins.Count; continue }
        'Spacebar'  { $plugins[$cursor].Enabled = -not $plugins[$cursor].Enabled; continue }
        'Enter' {
            $enabled = Save-Config
            Clear-Host
            if ($enabled.Count -gt 0) {
                Write-Host ('已保存，启用: ' + ($enabled -join ', ')) -ForegroundColor Green
            }
            else {
                Write-Host '已保存，未启用任何插件。' -ForegroundColor Yellow
            }
            Write-Host "写入: $configFile"
            Write-Host '现在重新运行 cmake 配置即可生效。'
            exit 0
        }
        'Escape' { Clear-Host; Write-Host '已取消，未改动。' -ForegroundColor DarkGray; exit 0 }
    }

    switch ($key.KeyChar) {
        'a' { foreach ($p in $plugins) { $p.Enabled = $true } }
        'n' { foreach ($p in $plugins) { $p.Enabled = $false } }
        'q' { Clear-Host; Write-Host '已取消，未改动。' -ForegroundColor DarkGray; exit 0 }
    }
}
