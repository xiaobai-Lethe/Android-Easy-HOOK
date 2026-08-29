#!/usr/bin/env bash
#
# Easy Hook 插件编译选择器（menuconfig 风格，*nix 版）。
#
#   ↑ / ↓ 或 k / j   移动光标
#   空格             切换开/关
#   a                全开
#   n                全关
#   回车             保存并退出
#   q / Esc          取消退出（不改动）
#
# 选择结果写入 <repo>/plugins.config（每行一个启用的插件文件夹名，# 开头为注释）。
# CMake 配置时自动读取，保存后正常运行 cmake 即可生效。

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
plugins_dir="$repo_root/plugins"
config_file="$repo_root/plugins.config"

if [[ ! -d "$plugins_dir" ]]; then
    echo "找不到插件目录: $plugins_dir" >&2
    exit 1
fi

# 发现插件：plugins/ 下的每个子目录
names=()
counts=()
while IFS= read -r dir; do
    [[ -z "$dir" ]] && continue
    name="$(basename "$dir")"
    count="$(find "$dir" -type f -name '*.cpp' | wc -l | tr -d ' ')"
    names+=("$name")
    counts+=("$count")
done < <(find "$plugins_dir" -mindepth 1 -maxdepth 1 -type d | sort)

if [[ ${#names[@]} -eq 0 ]]; then
    echo "plugins/ 下没有任何插件。"
    exit 0
fi

# 载入已有选择
declare -a enabled
for ((i = 0; i < ${#names[@]}; i++)); do enabled[i]=0; done
if [[ -f "$config_file" ]]; then
    while IFS= read -r line; do
        line="${line#"${line%%[![:space:]]*}"}"   # 去左空白
        line="${line%"${line##*[![:space:]]}"}"    # 去右空白
        [[ -z "$line" || "$line" == \#* ]] && continue
        for ((i = 0; i < ${#names[@]}; i++)); do
            [[ "${names[i]}" == "$line" ]] && enabled[i]=1
        done
    done < "$config_file"
fi

cursor=0

render() {
    clear
    printf '\033[36mEasy Hook 插件编译选择\033[0m\n'
    printf '\033[90m↑/↓(或 k/j) 移动   空格 开关   a 全开   n 全关   回车 保存   q 取消\033[0m\n\n'
    for ((i = 0; i < ${#names[@]}; i++)); do
        if [[ ${enabled[i]} -eq 1 ]]; then mark='[x]'; else mark='[ ]'; fi
        line="$mark ${names[i]}  (${counts[i]} 源文件)"
        if [[ $i -eq $cursor ]]; then
            printf '\033[30;42m > %s\033[0m\n' "$line"
        else
            printf '   %s\n' "$line"
        fi
    done
    printf '\n'
}

save_config() {
    {
        echo '# Easy Hook 启用的插件清单，由 tools/plugin_config.sh 生成。'
        echo '# 每行一个 plugins/ 下的文件夹名；# 开头为注释；留空表示全部关闭。'
        for ((i = 0; i < ${#names[@]}; i++)); do
            [[ ${enabled[i]} -eq 1 ]] && echo "${names[i]}"
        done
    } > "$config_file"
}

count=${#names[@]}
while true; do
    render
    IFS= read -rsn1 key
    if [[ "$key" == $'\033' ]]; then
        # 可能是方向键的转义序列
        read -rsn2 -t 0.001 rest || rest=''
        case "$rest" in
            '[A') cursor=$(((cursor - 1 + count) % count)); continue ;;
            '[B') cursor=$(((cursor + 1) % count)); continue ;;
            '')   clear; echo '已取消，未改动。'; exit 0 ;;   # 单独 Esc
            *)    continue ;;
        esac
    fi
    case "$key" in
        k|K) cursor=$(((cursor - 1 + count) % count)) ;;
        j|J) cursor=$(((cursor + 1) % count)) ;;
        ' ') enabled[cursor]=$((1 - enabled[cursor])) ;;
        a|A) for ((i = 0; i < count; i++)); do enabled[i]=1; done ;;
        n|N) for ((i = 0; i < count; i++)); do enabled[i]=0; done ;;
        q|Q) clear; echo '已取消，未改动。'; exit 0 ;;
        '')  # 回车
            save_config
            clear
            sel=()
            for ((i = 0; i < count; i++)); do
                [[ ${enabled[i]} -eq 1 ]] && sel+=("${names[i]}")
            done
            if [[ ${#sel[@]} -gt 0 ]]; then
                echo "已保存，启用: ${sel[*]}"
            else
                echo '已保存，未启用任何插件。'
            fi
            echo "写入: $config_file"
            echo '现在重新运行 cmake 配置即可生效。'
            exit 0
            ;;
    esac
done
