#!/usr/bin/env python3
"""Проверка посадки платы PUFVFD.01 в корпус: детали платы (габариты из проекта KiCad, высоты по таблице)
не должны пересекаться со стенками, стойками, крышкой и тем, что закреплено на крышке (дисплей, плата кнопок,
модуль PCF8574, модуль ESP32). Запуск: python3 enclosure/check_fit.py (нужны KiCad 7 с модулем pcbnew и OpenSCAD)."""
import os, re, subprocess, sys
import pcbnew
from pcbnew import ToMM

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "..", "hardware", "pcb", "pch1500_a1.kicad_pcb")
SCAD = os.path.join(HERE, "pch1500_case.scad")


def param(name):
    m = re.search(rf"(?<![\w]){name}\s*=\s*([-\d.]+)", open(SCAD, encoding="utf-8").read())
    return float(m.group(1))


# высота деталей над верхней стороной платы, мм (с запасом на выводы и панельки)
def height(ref, val, fp):
    if ref in ("C1", "C2", "C3"): return 50
    if ref == "C4": return 21
    if ref == "C53": return 14
    if ref in ("C12", "C16", "C27"): return 17
    if ref in ("R5", "R6"): return 7
    if ref == "R73": return 15
    if ref in ("R9", "R10", "R11", "C5"): return 1.5
    if ref == "RK1": return 26
    if ref == "RU1": return 20
    if ref == "K1": return 21
    if ref == "K2": return 16
    if ref == "A1": return 17
    if ref == "A2": return 19
    if ref == "A3": return 24
    if ref == "VT2": return 27
    if ref == "DA2": return 21
    if ref in ("DA7", "DA8"): return 2
    if ref == "DA5": return 8
    if ref.startswith(("DA", "DD", "U")): return 9
    if ref.startswith("XT"): return 14
    if ref.startswith("XP"): return 12
    if ref == "FU1": return 12
    if ref.startswith("VT"): return 8
    if ref.startswith(("VD", "HL")): return 9
    if ref.startswith("R"): return 12
    if ref.startswith("C"): return 8
    return 15


def main():
    pcb_w, pcb_top = param("pcb_w"), param("pcb_top")
    post_h = param("module_h") - param("wall")
    z0 = post_h + 1.6                                     # верхняя сторона платы
    b = pcbnew.LoadBoard(BOARD)
    boxes = []
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        if not ref or ref.startswith("H") or ref == "DA1": continue      # DA1 — под платой
        bb = fp.GetBoundingBox(False, False)
        x0, y0, x1, y1 = ToMM(bb.GetLeft()), ToMM(bb.GetTop()), ToMM(bb.GetRight()), ToMM(bb.GetBottom())
        boxes.append((ref, x0, y0, x1, y1, height(ref, fp.GetValue(), fp)))
    # зона радиатора тормозного ключа VT2
    boxes.append(("радиатор VT2", 10.0, 150.5, 38.0, 169.5, 30))
    lines = [f'use <{SCAD}>', "base_d = %g; lid_d = %g;" % (param("base_d"), param("lid_d"))]
    def cube(x0, y0, x1, y1, h):                         # координаты платы -> координаты корпуса
        return (f"translate([{x0 - pcb_w / 2:.2f}, {pcb_top - y1:.2f}, {z0:.2f}]) cube([{x1 - x0:.2f}, {y1 - y0:.2f}, {h:.2f}]);")
    lcd_y, btn_y = param("lcd_y"), param("btn_y")
    attached = f"""
module attached() {{       // элементы на крышке в собранном положении (крышка начинается на высоте base_d)
    translate([0, {lcd_y}, base_d + lid_d - 9 - 13 + 6.5]) cube([99, 61, 13], center = true);        // ЖКИ с адаптером I2C
    translate([-15, {btn_y + 12}, base_d + lid_d - 7 - 2.5]) cube([122, 32, 5], center = true);   // плата кнопок с пайкой
    translate([45, {btn_y + 12}, base_d + lid_d - 7 - 8]) cube([42, 18, 12], center = true);      // модуль PCF8574
    translate([62, {btn_y - 30}, base_d + lid_d - 2 - 7]) cube([53, 29, 14], center = true);      // модуль ESP32 с выводами
}}
module obstacles() {{ base(); translate([0, 0, base_d]) lid(); attached(); }}
"""
    lines.append(attached)
    for ref, x0, y0, x1, y1, h in boxes:
        lines.append(f"// {ref}\nmodule c_{re.sub(r'[^A-Za-z0-9]', '_', ref)}() {{ {cube(x0, y0, x1, y1, h)} }}")
    out = os.path.join(HERE, ".fit")
    os.makedirs(out, exist_ok=True)
    names = [re.sub(r'[^A-Za-z0-9]', '_', ref) for ref, *_ in boxes]
    lines.insert(1, "engrave = false; $fn = 24;")
    def collides(group):
        f = os.path.join(out, "t.scad")
        open(f, "w", encoding="utf-8").write("\n".join(lines) + "\nintersection() { union() { " + " ".join(f"c_{n}();" for n in group) + " } obstacles(); }\n")
        r = subprocess.run(["openscad", "-o", os.path.join(out, "t.stl"), f], capture_output=True, text=True)
        return "top level object is empty" not in r.stderr.lower()
    problems, stack = [], [names]
    while stack:                                          # поиск пересекающихся деталей делением пополам
        g = stack.pop()
        if not collides(g): continue
        if len(g) == 1: problems.append(boxes[names.index(g[0])][0]); continue
        stack += [g[:len(g) // 2], g[len(g) // 2:]]
    # плата целиком внутри полости с зазором не менее 3 мм до стенок
    W, H, wall = param("W"), param("H"), param("wall")
    gap = [(W / 2 - wall) - pcb_w / 2, (H / 2 - wall) - pcb_top, (H / 2 - wall) + (pcb_top - param("pcb_h"))]
    print(f"зазор от платы до стенок: по бокам {gap[0]:.1f} мм, сверху {gap[1]:.1f} мм, снизу (ввод кабелей) {gap[2]:.1f} мм")
    lid_inner = param("base_d") + param("lid_d")
    tallest = max(boxes, key=lambda b: b[5])
    print(f"самая высокая деталь: {tallest[0]}, верх на {z0 + tallest[5]:.1f} мм; внутренняя высота корпуса {lid_inner:.1f} мм")
    print("проверено деталей:", len(boxes))
    if problems: print("ПЕРЕСЕЧЕНИЯ:", ", ".join(problems)); return 1
    if min(gap) < 3: print("ПЛАТА БЛИЖЕ 3 мм К СТЕНКЕ"); return 1
    print("пересечений нет"); return 0


if __name__ == "__main__":
    sys.exit(main())
