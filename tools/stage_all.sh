#!/bin/bash
#[[
# 2026年08月28日01时12分
# 暂存仓库全部改动并复核结果。
# 2026 BY white
#]]
cd /mnt/d/PROJECT/Easy-Framework/main || exit 1

git add -A

untracked=$(git status --short | grep -c '^??')
staged=$(git diff --cached --name-only | wc -l)
own=$(git diff --cached --name-only | grep -vc '^dobby/')

echo "暂存文件总数 : $staged"
echo "其中自研代码 : $own"
echo "其中 dobby   : $((staged - own))"
echo "未跟踪文件数 : $untracked"
echo
echo "=== 被忽略的内容 ==="
git status --ignored --short | grep '^!!' | sed 's/^/  /'
