#!/bin/sh
# Выпуск STL корпуса PUFVFD для печати. Детали уже развёрнуты так, как их надо класть на стол.
#   stl/whole/  — цельные основание и крышка (стол не меньше 235 × 215 мм)
#   stl/split/  — половинки и стяжные пластины (стол 220 × 220 мм)
#   stl/common/ — ушки крепления и ножки (нужны в обоих вариантах)
set -e
cd "$(dirname "$0")"
rm -rf stl && mkdir -p stl/whole stl/split stl/common
one() { openscad -q -D "part=\"$1\"" -o "stl/$2/$1.stl" pch1500_case.scad 2>&1 | grep -vi fontconfig || true; echo "  $2/$1.stl"; }
one base whole; one lid whole
one base_a split; one base_b split; one lid_a split; one lid_b split
one splice_wall split; one splice_back split; one splice_lid split
one ear common; one leg common
