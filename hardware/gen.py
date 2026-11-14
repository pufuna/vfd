#!/usr/bin/env python3
"""Генерация документов на аппаратную часть ПЧ-1500 из netlist.py:
   out/sch_*.svg — принципиальная схема по листам (связи показаны именами цепей),
   out/structure.svg — структурная схема, out/wiring.svg — схема соединений (монтажная),
   out/pch1500.net — нетлист KiCad, out/bom.csv, ../docs/bom.md, ../docs/netlist.md"""
import csv, html, os, collections
import netlist as N

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs")
os.makedirs(OUT, exist_ok=True); os.makedirs(DOCS, exist_ok=True)
HOT = {"L_IN", "N_IN", "L_AUX", "X2D", "BR+", "DCP", "GNDH", "+15VH", "+5VH", "NSH", "U_IPM", "U_OUT", "V_OUT", "W_OUT", "W_MOT", "BRK_C"}
PWR = {"+5V", "+3V3", "GND", "PE"}
def color(net):
    return "#b3261e" if net in HOT else ("#1a5fb4" if net in PWR else "#1b1b1b")
esc = html.escape
FONT = 'font-family="DejaVu Sans Mono,Consolas,monospace"'

class Svg:
    def __init__(s, w, h, title):
        s.w, s.h, s.b = w, h, []
        s.b.append(f'<rect width="{w}" height="{h}" fill="#fff"/><rect x="8" y="8" width="{w-16}" height="{h-16}" fill="none" stroke="#000" stroke-width="1.5"/>')
        s.text(20, 34, title, 18, bold=True)
    def text(s, x, y, t, size=12, anchor="start", fill="#1b1b1b", bold=False):
        s.b.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{fill}" {FONT}{" font-weight=\"bold\"" if bold else ""}>{esc(str(t))}</text>')
    def rect(s, x, y, w, h, fill="#fbfbf6", stroke="#000", sw=1.2, dash=None, rx=0):
        s.b.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{f" stroke-dasharray=\"{dash}\"" if dash else ""}/>')
    def line(s, pts, stroke="#000", sw=1.2, arrow=False, dash=None):
        d = " ".join(f"{x},{y}" for x, y in pts)
        s.b.append(f'<polyline points="{d}" fill="none" stroke="{stroke}" stroke-width="{sw}"{" marker-end=\"url(#ar)\"" if arrow else ""}{f" stroke-dasharray=\"{dash}\"" if dash else ""}/>')
    def save(s, name):
        defs = '<defs><marker id="ar" markerWidth="9" markerHeight="7" refX="8" refY="3.5" orient="auto"><path d="M0,0 L9,3.5 L0,7 z"/></marker></defs>'
        open(os.path.join(OUT, name), "w", encoding="utf-8").write(
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {s.w} {s.h}" width="{s.w}" height="{s.h}">{defs}{"".join(s.b)}</svg>')

# ---------- принципиальная схема: элементы-карточки с именами цепей ----------
PH, BW, LW = 17, 150, 96      # шаг выводов, ширина корпуса, ширина поля имени цепи
def comp_size(c):
    n = len(c["pins"])
    rows = 1 if n == 2 else (n + 1) // 2
    return BW + 2 * LW, 40 + rows * PH + 8
def draw_comp(s, c, x, y):
    pins = list(c["pins"].items()); n = len(pins)
    w, h = comp_size(c); bx = x + LW
    s.text(bx, y + 13, c["ref"], 13, bold=True); s.text(bx + 44, y + 13, c["val"], 11)
    s.rect(bx, y + 20, BW, h - 28)
    def pin(side, row, p, net):
        num, _, name = p.partition(":")
        py = y + 20 + 14 + row * PH
        col = color(net) if net else "#999"
        lab = net if net else "н/п"
        if side == 0:
            s.line([(bx - 14, py - 4), (bx, py - 4)], col); s.text(bx - 17, py, lab, 11, "end", col)
            s.text(bx + 4, py, (num + " " + name).strip() if n > 2 else "", 10, fill="#555")
        else:
            s.line([(bx + BW, py - 4), (bx + BW + 14, py - 4)], col); s.text(bx + BW + 17, py, lab, 11, "start", col)
            s.text(bx + BW - 4, py, (name + " " + num).strip() if n > 2 else "", 10, "end", "#555")
    if n == 2:
        pin(0, 0, *pins[0]); pin(1, 0, *pins[1])
        nm = [p.partition(":")[2] for p, _ in pins]
        if any(nm): s.text(bx + 4, y + 34, nm[0], 10, fill="#555"); s.text(bx + BW - 4, y + 34, nm[1], 10, "end", "#555")
    else:
        half = (n + 1) // 2
        for i in range(half): pin(0, i, *pins[i])
        for i in range(half, n): pin(1, n - 1 - i, *pins[i])      # правая сторона снизу вверх, как у DIP

def sheets():
    by = collections.OrderedDict()
    for c in N.C: by.setdefault(c["sheet"], []).append(c)
    files = []
    for k, (title, comps) in enumerate(by.items(), 1):
        comps = sorted(comps, key=lambda c: -len(c["pins"]))
        cw = BW + 2 * LW + 24; H = 1120; x, y, cols = 20, 56, 1
        pos = []
        for c in comps:
            w, h = comp_size(c)
            if y + h > H - 60: x += cw; y = 56; cols += 1
            pos.append((c, x, y)); y += h + 8
        s = Svg(max(1500, 40 + cols * cw), H, f"ПЧ-1500. Схема электрическая принципиальная. Лист {title}")
        for c, x, y in pos: draw_comp(s, c, x, y)
        s.text(20, H - 22, "Соединения показаны именами цепей: выводы с одинаковым именем соединены. Красные — цепи под сетевым потенциалом, синие — питание изолированной стороны, «н/п» — не подключён.", 11, fill="#444")
        name = f"sch_{k}.svg"; s.save(name); files.append(name)
    return files

# ---------- структурная схема ----------
def structure():
    s = Svg(1500, 900, "ПЧ-1500. Схема электрическая структурная")
    def box(x, y, w, h, lines, fill="#fbfbf6"):
        s.rect(x, y, w, h, fill, rx=4)
        for i, t in enumerate(lines): s.text(x + w / 2, y + h / 2 - (len(lines) - 1) * 8 + i * 16 + 4, t, 12, "middle", bold=(i == 0))
    s.rect(30, 60, 1440, 430, "#fff5f3", "#b3261e", 1.5, "8 4", 6); s.text(44, 82, "«Горячая» сторона — под потенциалом сети 220 В", 13, fill="#b3261e", bold=True)
    s.rect(30, 560, 1440, 310, "#f2f6ff", "#1a5fb4", 1.5, "8 4", 6); s.text(44, 582, "Изолированная сторона (безопасное сверхнизкое напряжение 5 В / 3,3 В)", 13, fill="#1a5fb4", bold=True)
    s.text(44, 530, "Барьер изоляции: оптроны 6N137, PC817, изолирующий усилитель HCPL-7840, датчики Холла ACS712, реле K1, модули питания Hi-Link", 12, fill="#444")
    P = [(50, "Сеть ~220 В", "QF1 C20, F1, RV1, CX1"), (250, "Выпрямитель", "VD1 KBPC3510"), (450, "Предзаряд", "R3 47 Ом + реле K1"),
         (650, "Шина DC 310 В", "C1–C3 3x470 мкФ"), (850, "Инвертор IGBT", "U1 IKCM30F60GA"), (1050, "Датчики тока", "U14, U15 ACS712"), (1250, "Двигатель", "3~ 220 В, Δ, 1,5 кВт")]
    for x, a, b in P: box(x, 120, 170, 70, [a, b])
    for i in range(len(P) - 1): s.line([(P[i][0] + 170, 155), (P[i + 1][0], 155)], "#b3261e", 3, True)
    box(650, 250, 170, 70, ["Тормозной ключ", "VT2, VD3, U10 (381 В)", "+ резистор RB1"]); s.line([(735, 190), (735, 250)], "#b3261e", 2, True)
    box(850, 250, 170, 70, ["Шунт и защита", "R9, R10 → ITRIP", "отключение при 23,5 А"]); s.line([(935, 250), (935, 190)], "#b3261e", 2, True)
    box(250, 250, 170, 70, ["Питание 15 В / 5 В", "PS1 HLK-5M15, U2"]); s.line([(135, 190), (135, 285), (250, 285)], "#b3261e", 2, True)
    box(850, 380, 170, 70, ["Развязка ШИМ", "U4–U9 6N137, U3 74HC14"]); s.line([(935, 380), (935, 320)], "#000", 1.5); s.line([(905, 380), (905, 190)], "#000", 2, True)
    box(650, 380, 170, 70, ["Измерение Udc", "R42–R45, U13 HCPL-7840"]); s.line([(700, 320), (700, 380)], "#000", 1.5); s.line([(690, 250), (690, 190)], "#000", 1)
    box(1050, 380, 170, 70, ["Сигнал аварии", "U10, U11 PC817"]); s.line([(1020, 285), (1135, 285), (1135, 380)], "#000", 1.5, True)
    box(600, 620, 420, 110, ["Контроллер A2: STM32F411 (Black Pill)", "ШИМ 6 каналов с мёртвым временем (TIM1), АЦП по таймеру", "U/f и бездатчиковое векторное управление", "защиты, параметры, Modbus RTU"])
    s.line([(935, 620), (935, 450)], "#000", 2, True); s.line([(735, 450), (735, 620)], "#000", 2, True)
    s.line([(1135, 450), (1135, 600), (1000, 600), (1000, 620)], "#000", 2, True); s.line([(1135, 190), (1135, 230), (1240, 230), (1240, 675), (1020, 675)], "#000", 2, True)
    s.text(1250, 500, "токи фаз U, W", 11); s.text(745, 545, "Udc", 11); s.text(945, 545, "ШИМ x6", 11); s.text(1145, 545, "авария", 11)
    box(50, 620, 190, 70, ["Питание 5 В", "PS2 HLK-10M05"]); s.line([(100, 190), (100, 620)], "#b3261e", 2, True); s.line([(240, 640), (600, 640)], "#1a5fb4", 2, True)
    box(300, 760, 190, 80, ["Пульт", "ЖКИ 20x4 (I2C)", "5 кнопок (PCF8574)"]); s.line([(395, 760), (395, 745), (620, 745), (620, 730)], "#000", 1.5, True)
    box(530, 760, 190, 80, ["RS-485", "U16 MAX485", "Modbus RTU"]); s.line([(680, 760), (680, 730)], "#000", 1.5, True)
    box(760, 760, 210, 80, ["Модуль связи A3: ESP32", "Wi-Fi: веб-интерфейс,", "Modbus TCP; Bluetooth"]); s.line([(860, 760), (860, 730)], "#000", 1.5, True)
    box(1010, 760, 210, 80, ["Входы / выходы", "DI1–DI3, AI 0–10 В,", "реле K2, NTC, вентилятор"]); s.line([(1010, 790), (990, 790), (990, 730)], "#000", 1.5, True)
    box(450, 670, 120, 50, ["Реле K1", "предзаряд"]); s.line([(600, 695), (570, 695)], "#000", 1.5, True); s.line([(510, 670), (510, 190)], "#000", 1.5, True, "5 3")
    s.save("structure.svg")

# ---------- схема соединений (монтажная) ----------
def wiring():
    s = Svg(1500, 1000, "ПЧ-1500. Схема электрическая соединений (монтажная)")
    s.rect(430, 90, 520, 830, "#f4f8f0", "#2e5d1e", 2, rx=6); s.text(690, 115, "Плата A1 (основная)", 14, "middle", bold=True)
    def conn(x, y, ref, pins, side):
        h = 22 * len(pins) + 26; w = 120
        s.rect(x, y, w, h, "#fff"); s.text(x + w / 2, y + 16, ref, 12, "middle", bold=True)
        pts = {}
        for i, p in enumerate(pins):
            py = y + 38 + i * 22; s.text(x + (w - 8 if side else 8), py, p, 11, "end" if side else "start")
            pts[p] = (x + (w if side else 0), py - 4)
        return pts
    def dev(x, y, w, h, lines):
        s.rect(x, y, w, h, "#fbfbf6", rx=4)
        for i, t in enumerate(lines): s.text(x + w / 2, y + 20 + i * 16, t, 12, "middle", bold=(i == 0))
    def wire(a, b, label, col="#000", sw=1.6, off=0):
        mx = (a[0] + b[0]) / 2 + off
        s.line([a, (mx, a[1]), (mx, b[1]), b], col, sw); s.text(mx + 4, min(a[1], b[1]) + abs(a[1] - b[1]) / 2 - 3, label, 10, fill="#444")
    R = "#b3261e"; G = "#2e7d32"
    # левая сторона — силовые цепи
    j1 = conn(440, 130, "J1 СЕТЬ", ["L", "N", "PE"], 0); j2 = conn(440, 250, "J2 ~", ["AC1", "AC2"], 0); j3 = conn(440, 350, "J3 =", ["+", "-"], 0)
    j5 = conn(440, 450, "J5 ДВИГ.", ["U", "V", "W", "PE"], 0); j4 = conn(440, 600, "J4 ТОРМ.", ["R+", "R-"], 0)
    dev(40, 110, 200, 110, ["Сеть ~220 В 50 Гц", "через автомат QF1 C20", "L — фаза", "N — нейтраль", "PE — защитное заземление"])
    wire((240, 150), j1["L"], "2,5 мм² корич.", R, 2.5); wire((240, 172), j1["N"], "2,5 мм² син.", "#1a5fb4", 2.5); wire((240, 194), j1["PE"], "2,5 мм² ж/з", G, 2.5)
    dev(40, 250, 200, 150, ["VD1 KBPC3510", "(на радиаторе)", "~  ~  +  −"]); 
    wire((240, 290), j2["AC1"], "2,5 мм²", R, 2.5, -30); wire((240, 312), j2["AC2"], "2,5 мм²", R, 2.5, -10)
    wire((240, 356), j3["+"], "2,5 мм² (+ красн., − чёрн.)", R, 2.5, 20); wire((240, 378), j3["-"], "", R, 2.5, 40)
    dev(40, 440, 200, 120, ["Двигатель M", "3~ 220 В, соединение Δ", "U1, V1, W1, корпус"])
    for i, p in enumerate(["U", "V", "W"]): wire((240, 478 + i * 22), j5[p], "2,5 мм²" if i == 0 else "", R, 2.5, -20 + i * 15)
    wire((240, 544), j5["PE"], "PE ж/з", G, 2.5, 30)
    dev(40, 600, 200, 70, ["RB1 100 Ом 100–200 Вт", "(опция)"]); wire((240, 630), j4["R+"], "1,5 мм², термост.", R, 2, -10); wire((240, 652), j4["R-"], "", R, 2, 10)
    dev(40, 720, 200, 90, ["Радиатор", "винт заземления М4", "U1, VD1, VT2 — через КПТ-8"]); s.line([(240, 765), (330, 765), (330, 194)], G, 2.5); s.text(250, 760, "PE ж/з 2,5 мм²", 10, fill="#444")
    # правая сторона — слаботочные цепи
    j7 = conn(820, 130, "J7 ПУЛЬТ", ["+5V", "GND", "SDA", "SCL"], 1); j8 = conn(820, 260, "J8 ESP32", ["+5V", "GND", "TX", "RX"], 1)
    j6 = conn(820, 390, "J6 RS-485", ["A", "B", "GND"], 1); j9 = conn(820, 495, "J9 ВХОДЫ", ["DI1", "DI2", "DI3"], 1)
    j10 = conn(820, 600, "J10 АНАЛОГ.", ["GND", "AI", "+3V3"], 1); j11 = conn(820, 705, "J11 РЕЛЕ", ["NO", "COM", "NC"], 1)
    j12 = conn(820, 810, "J12 J13", ["NTC", "GND", "FAN+", "FAN-"], 1)
    dev(1180, 110, 290, 130, ["Пульт (крышка корпуса)", "A4: ЖКИ 2004 + адаптер I2C (0x27)", "A5: PCF8574 (0x20): P0 ПУСК, P1 СТОП,", "P2 ▲, P3 ▼, P4 ВВОД; кнопки на GND", "VCC, GND, SDA, SCL — параллельно"])
    for i, p in enumerate(["+5V", "GND", "SDA", "SCL"]): wire(j7[p], (1180, 150 + i * 22), "шлейф 4x0,22 мм², до 30 см" if i == 0 else "", "#000", 1.4, -40 + i * 20)
    dev(1180, 270, 290, 90, ["A3: ESP32 DevKit", "VIN(5V), GND", "TX платы → GPIO16 (RX2)", "RX платы ← GPIO17 (TX2)"])
    for i, p in enumerate(["+5V", "GND", "TX", "RX"]): wire(j8[p], (1180, 294 + i * 18), "", "#000", 1.4, -40 + i * 20)
    dev(1180, 390, 290, 70, ["Сеть Modbus RTU", "витая пара A(+), B(−), общий", "JP2 — терминатор 120 Ом"])
    for i, p in enumerate(["A", "B", "GND"]): wire(j6[p], (1180, 414 + i * 16), "", "#000", 1.4, -20 + i * 20)
    dev(1180, 490, 290, 80, ["Внешнее управление («сухие» контакты)", "DI1 ВПЕРЁД, DI2 НАЗАД, DI3 функция", "вторые выводы контактов — на GND (J10.1)"])
    for i, p in enumerate(["DI1", "DI2", "DI3"]): wire(j9[p], (1180, 512 + i * 18), "", "#000", 1.4, -20 + i * 20)
    dev(1180, 600, 290, 80, ["RP1 10 кОм (JP1 разомкнута)", "или сигнал 0–10 В на AI–GND", "(JP1 замкнута, +3V3 не подключать)"])
    for i, p in enumerate(["GND", "AI", "+3V3"]): wire(j10[p], (1180, 622 + i * 18), "экранир.", "#000", 1.4, -20 + i * 20)
    dev(1180, 705, 290, 60, ["Цепь сигнализации", "до 250 В~ 5 А"])
    for i, p in enumerate(["NO", "COM", "NC"]): wire(j11[p], (1180, 722 + i * 14), "", "#000", 1.4, -20 + i * 20)
    dev(1180, 800, 290, 100, ["RK1: NTC 10 кОм — на радиаторе у U1", "M1: вентилятор 5 В (красный +)"])
    for i, p in enumerate(["NTC", "GND", "FAN+", "FAN-"]): wire(j12[p], (1180, 830 + i * 18), "", "#000", 1.4, -40 + i * 20)
    s.text(690, 200, "зона под сетевым", 12, "middle", R); s.text(690, 216, "потенциалом", 12, "middle", R)
    s.line([(690, 240), (690, 900)], "#888", 1.5, False, "10 6"); s.text(696, 895, "барьер изоляции ≥ 6,4 мм", 11, fill="#666")
    s.text(40, 960, "Силовые провода — ПуГВ 2,5 мм² с наконечниками НШВИ; провода двигателя и тормозного резистора прокладывать отдельно от слаботочных. Экран кабеля двигателя и корпус двигателя — на PE.", 11, fill="#444")
    s.save("wiring.svg")

# ---------- нетлист KiCad, перечень элементов ----------
def kicad_net():
    o = ['(export (version "E")', '  (components']
    for c in N.C:
        o.append(f'    (comp (ref "{c["ref"]}") (value "{c["val"]}") (footprint "{c["fp"]}"))')
    o.append('  )\n  (nets')
    for i, (net, nodes) in enumerate(sorted(N.nets().items()), 1):
        o.append(f'    (net (code "{i}") (name "{net}")')
        for ref, pin in nodes: o.append(f'      (node (ref "{ref}") (pin "{pin.partition(":")[0]}"))')
        o.append('    )')
    o.append('  )\n)')
    open(os.path.join(OUT, "pch1500.net"), "w", encoding="utf-8").write("\n".join(o) + "\n")

def bom():
    grp = collections.OrderedDict()
    for c in N.C: grp.setdefault((c["val"], c["desc"].split(" (")[0], c["price"]), []).append(c["ref"])
    rows = [(", ".join(refs), val, desc, len(refs), price, len(refs) * price) for (val, desc, price), refs in grp.items()]
    tot_a = sum(r[5] for r in rows); tot_b = sum(q * p for *_, q, p in N.OFFBOARD)
    with open(os.path.join(OUT, "bom.csv"), "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f, delimiter=";"); w.writerow(["Раздел", "Поз.", "Номинал / тип", "Описание", "Кол.", "Цена, руб", "Сумма, руб"])
        for r in rows: w.writerow(["Плата A1", *r])
        for ref, val, desc, q, p in N.OFFBOARD: w.writerow(["Прочее", ref, val, desc, q, p, q * p])
    md = ["# Перечень элементов и ориентировочная стоимость", "",
          "Файл создан автоматически (`hardware/gen.py`) из `hardware/netlist.py`. Цены — ориентировочные розничные оценки "
          "для российских маркетплейсов (Ozon, Wildberries, AliExpress) и магазинов «Чип и Дип», «Промэлектроника», «Платан»; "
          "перед закупкой их нужно уточнить. В «Чип и Дип» часть позиций заметно дороже указанного.", "",
          "## Плата A1", "", "| Поз. обозначение | Номинал / тип | Описание | Кол. | Цена, ₽ | Сумма, ₽ |", "|---|---|---|---:|---:|---:|"]
    md += [f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {r[4]} | {r[5]} |" for r in rows]
    md += [f"| | | **Итого по плате** | | | **{tot_a}** |", "", "## Изделия вне платы, конструктив, материалы", "",
           "| Поз. | Тип | Назначение | Кол. | Цена, ₽ | Сумма, ₽ |", "|---|---|---|---:|---:|---:|"]
    md += [f"| {ref} | {val} | {desc} | {q} | {p} | {q * p} |" for ref, val, desc, q, p in N.OFFBOARD]
    md += [f"| | | **Итого** | | | **{tot_b}** |", "", f"**Всего: ≈ {tot_a + tot_b} ₽** (включая опции: тормозной резистор, потенциометр, автомат).", ""]
    open(os.path.join(DOCS, "bom.md"), "w", encoding="utf-8").write("\n".join(md))
    nl = ["# Таблица цепей платы A1", "", "Создано автоматически из `hardware/netlist.py`. Формат: элемент.вывод.", "", "| Цепь | Подключённые выводы |", "|---|---|"]
    nl += [f"| `{net}` | {', '.join(r + '.' + p.partition(':')[0] for r, p in nodes)} |" for net, nodes in sorted(N.nets().items())]
    open(os.path.join(DOCS, "netlist.md"), "w", encoding="utf-8").write("\n".join(nl) + "\n")
    return tot_a, tot_b

if __name__ == "__main__":
    f = sheets(); structure(); wiring(); kicad_net(); a, b = bom()
    print("листы:", f, "| плата", a, "₽, прочее", b, "₽")
