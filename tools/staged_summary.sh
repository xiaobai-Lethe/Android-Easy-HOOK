#!/bin/bash
#[[
# 2026年08月28日01时08分
# 汇总本次暂存内容，确认忽略规则生效且关键源码全部纳入。
# 2026 BY white
#]]
cd /mnt/d/PROJECT/Easy-Framework/main || exit 1

git diff --cached --name-only > /tmp/eh_staged.txt

echo "=== 各顶层目录暂存文件数 ==="
for dir in dobby include src user tools cmake scripts; do
    count=$(grep -c "^${dir}/" /tmp/eh_staged.txt)
    printf "  %-10s %s\n" "${dir}/" "$count"
done
printf "  %-10s %s\n" "(根目录)" "$(grep -vc '/' /tmp/eh_staged.txt)"
echo "  ------------------"
printf "  %-10s %s\n" "合计" "$(wc -l < /tmp/eh_staged.txt)"

echo
echo "=== 自研代码（不含 dobby）==="
grep -v '^dobby/' /tmp/eh_staged.txt | wc -l | sed 's/^/  /'

echo
echo "=== 根目录文件 ==="
grep -v '/' /tmp/eh_staged.txt | sed 's/^/  /'

echo
echo "=== 确认被忽略 ==="
git status --ignored --short | grep '^!!' | sed 's/^/  /'

echo
echo "=== 工作区是否干净（除忽略项外应无未跟踪文件）==="
untracked=$(git status --short | grep -c '^??')
echo "  未跟踪文件数: $untracked"
