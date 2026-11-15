#!/usr/bin/env python3
"""ПЧ-1500 — выпуск комплекта схем по ЕСКД: Э1 (структурная), Э3 (принципиальная, 5 листов), ПЭ3 (перечень элементов),
Э4 (соединений); общий PDF. Запуск: python3 docs.py   (для PDF нужен node + playwright)"""
import os, re, subprocess, sys, collections
from gost import Sheet, verify, COMP
import netlist as N
import sheets as E3

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out")


def arrow(s, pts, w=0.5, head=True):
    for a, b in zip(pts, pts[1:]): s.line(a[0], a[1], b[0], b[1], w)
    if head:
        (x1, y1), (x2, y2) = pts[-2], pts[-1]
        import math
        L = math.hypot(x2 - x1, y2 - y1) or 1; ux, uy = (x2 - x1) / L, (y2 - y1) / L
        s.poly([(x2, y2), (x2 - 3 * ux - 1.1 * uy, y2 - 3 * uy + 1.1 * ux), (x2 - 3 * ux + 1.1 * uy, y2 - 3 * uy - 1.1 * ux)], 0.2, "#000", True)


def block(s, x, y, w, h, lines, sw=0.6):
    s.rect(x, y, w, h, sw)
    for i, t in enumerate(lines):
        s.text(x + w / 2, y + h / 2 - (len(lines) - 1) * 2.2 + i * 4.4 + 1, t, 3.0 if i == 0 else 2.5, "middle", bold=(i == 0))


def e1():
    s = Sheet("Схема электрическая структурная", "Э1", "Схема электрическая\nструктурная", 1, 1)
    s.rect(28, 22, 380, 100, 0.35, dash="8 3"); s.text(31, 27, "Цепи под потенциалом сети («горячая» сторона)", 3.0, bold=True)
    s.rect(28, 148, 380, 82, 0.35, dash="8 3"); s.text(31, 153, "Изолированная сторона: 5 В / 3,3 В (безопасное сверхнизкое напряжение)", 3.0, bold=True)
    s.text(31, 138, "Барьер изоляции: оптроны U1…U5, изолирующий усилитель DA6, датчики тока DA7, DA8, модули питания A2, A3", 2.8)
    s.line(28, 141, 408, 141, 0.8, "2 2")
    P = [(34, "Сеть ~220 В", "QF1, XT1, FU1, RU1, C53"), (88, "Выпрямитель", "VD1 (на радиаторе)"), (142, "Предзаряд", "RK1, реле K1"),
         (196, "Шина 310 В", "C1…C3"), (250, "Инвертор", "DA1"), (304, "Датчики тока", "DA7, DA8"), (358, "Двигатель", "3~ 220 В, 1,5 кВт")]
    for x, a, b in P: block(s, x, 36, 46, 18, [a, b])
    for i in range(len(P) - 1): arrow(s, [(P[i][0] + 46, 45), (P[i + 1][0], 45)], 0.9)
    block(s, 196, 66, 46, 20, ["Тормозной ключ", "DA3, VT2, VD3", "резистор RB1"]); arrow(s, [(219, 54), (219, 66)])
    block(s, 250, 66, 46, 20, ["Шунт, защита", "R9, R10 → ITRIP", "23,5 А"]); arrow(s, [(285, 66), (285, 54)])
    block(s, 88, 66, 46, 20, ["Питание 15 В, 5 В", "A2, DA2"]); arrow(s, [(50, 54), (50, 76), (88, 76)])
    block(s, 250, 96, 46, 20, ["Развязка ШИМ", "U1…U3, DD1"]); arrow(s, [(262, 96), (262, 54)])
    block(s, 196, 96, 46, 20, ["Измерение Udc", "R42…R45, DA6"]); arrow(s, [(206, 54), (206, 60), (192, 60), (192, 106), (196, 106)])
    block(s, 304, 96, 46, 20, ["Сигнал аварии", "DA4, U4"]); arrow(s, [(296, 80), (327, 80), (327, 96)])
    block(s, 142, 96, 46, 20, ["Управление K1", "U5, VT1"]); arrow(s, [(165, 96), (165, 54)])
    block(s, 190, 162, 112, 28, ["Контроллер A1 (STM32F411)", "ШИМ с мёртвым временем, АЦП по таймеру,", "U/f и векторное управление, защиты,", "параметры, Modbus RTU"])
    arrow(s, [(262, 162), (262, 116)]); s.text(264, 132, "ШИМ ×6", 2.5)
    arrow(s, [(219, 116), (219, 162)]); s.text(221, 132, "Udc", 2.5)
    arrow(s, [(327, 116), (327, 156), (290, 156), (290, 162)]); s.text(329, 132, "авария", 2.5)
    arrow(s, [(340, 54), (340, 60), (356, 60), (356, 176), (302, 176)]); s.text(358, 132, "токи фаз", 2.5)
    arrow(s, [(200, 162), (200, 156), (165, 156), (165, 116)]); s.text(167, 132, "предзаряд", 2.5)
    block(s, 34, 162, 46, 18, ["Питание 5 В", "A3"]); arrow(s, [(40, 54), (40, 162)]); arrow(s, [(80, 171), (190, 171)])
    block(s, 60, 200, 52, 22, ["Пульт", "A5: ЖКИ 20×4", "A6, SB1…SB5: кнопки"]); arrow(s, [(112, 211), (150, 211), (150, 186), (190, 186)])
    block(s, 122, 200, 52, 22, ["RS-485", "DD2, XT6", "Modbus RTU"]); arrow(s, [(174, 206), (182, 206), (182, 181), (190, 181)])
    block(s, 220, 200, 52, 22, ["Модуль связи A4", "ESP32: Wi-Fi,", "Bluetooth, Modbus TCP"]); arrow(s, [(246, 200), (246, 190)])
    block(s, 282, 200, 62, 22, ["Входы и выходы", "DI1…DI3, AI, реле K2,", "вентилятор, термистор"]); arrow(s, [(300, 200), (300, 196), (290, 196), (290, 190)])
    return s


def e4():
    s = Sheet("Схема электрическая соединений", "Э4", "Схема электрическая\nсоединений", 1, 1)
    s.rect(150, 28, 130, 206, 0.8); s.text(215, 34, "Плата основная ПЧ1500.01.000", 3.2, "middle", bold=True)
    s.line(215, 40, 215, 232, 0.4, "6 3"); s.text(183, 231, "барьер изоляции", 2.3)
    def tb(ref, title, x, y, pins, side):
        w = 26; s.rect(x, y, w, 5 * len(pins) + 6, 0.6); s.text(x + w / 2, y + 4.2, f"{ref} {title}", 2.5, "middle", bold=True)
        s.line(x, y + 6, x + w, y + 6, 0.35); out = {}
        for i, p in enumerate(pins):
            py = y + 6 + 5 * i + 2.5
            s.text(x + w / 2, py + 1, p, 2.4, "middle")
            if i: s.line(x, py - 2.5, x + w, py - 2.5, 0.2)
            out[p] = (x if side == "l" else x + w, py)
        return out
    def dev(x, y, w, h, lines):
        s.rect(x, y, w, h, 0.6)
        for i, t in enumerate(lines): s.text(x + w / 2, y + 5 + i * 4.2, t, 2.9 if i == 0 else 2.4, "middle", bold=(i == 0))
    def wire(a, b, label="", off=0.0, ly=-1.0):
        mx = (a[0] + b[0]) / 2 + off
        s.poly([a, (mx, a[1]), (mx, b[1]), b], 0.45)
        if label: s.text(min(a[0], b[0]) + 3 if a[0] < b[0] else b[0] + 3, (a[1] if a[0] < b[0] else b[1]) + ly, label, 2.2)
    x1 = tb("XT1", "сеть", 152, 42, ["L", "N"], "l"); x2 = tb("XT2", "~", 152, 64, ["AC1", "AC2"], "l"); x3 = tb("XT3", "=", 152, 86, ["+", "−"], "l")
    x4 = tb("XT4", "торм.", 152, 196, ["R+", "R−"], "l"); x5 = tb("XT5", "двиг.", 152, 150, ["U", "V", "W"], "l")
    dev(30, 36, 62, 30, ["Сеть ~220 В, 50 Гц", "через автомат QF1 (C20)", "L — фаза, N — нейтраль", "PE — защитный проводник"])
    wire((92, 46), x1["L"], "1: ПуГВ 2,5 мм², коричневый", -6); wire((92, 52), x1["N"], "2: ПуГВ 2,5 мм², синий", 0, 4.0)
    dev(30, 70, 62, 32, ["VD1 KBPC3510", "диодный мост на радиаторе", "выводы: ~  ~  +  −"])
    wire((92, 76), x2["AC1"], "3, 4: ПуГВ 2,5 мм²", -10); wire((92, 81), x2["AC2"], "", -4)
    wire((92, 92), x3["+"], "5: 2,5 мм², красный (+)", 4, 3.8); wire((92, 97), x3["−"], "6: 2,5 мм², чёрный (−)", 10, 3.8)
    dev(30, 144, 62, 28, ["Двигатель M1", "3~ 220 В, обмотки — Δ", "выводы U1, V1, W1"])
    for i, p in enumerate(["U", "V", "W"]): wire((92, 152 + 5 * i), x5[p], "7–9: кабель 3×1,5…2,5 мм² (желательно экранированный)" if i == 0 else "", -8 + 5 * i)
    dev(30, 190, 62, 22, ["RB1 100 Ом 100…200 Вт", "тормозной резистор (опция)", "вне корпуса"])
    wire((92, 198), x4["R+"], "10, 11: термостойкий 1,5 мм²", -4); wire((92, 205), x4["R−"], "", 2)
    dev(30, 110, 62, 24, ["Шпилька заземления М4", "на радиаторе", "PE сети, PE двигателя, экран"])
    s.poly([(92, 58), (100, 58), (100, 118), (92, 118)], 0.45); s.text(101, 112, "12: ПуГВ 2,5 мм², жёлто-зелёный", 2.2)
    s.poly([(92, 168), (104, 168), (104, 126), (92, 126)], 0.45); s.text(105, 132, "13: PE двигателя", 2.2)
    # правая сторона
    p1 = tb("XP1", "пульт", 252, 42, ["+5V", "GND", "SDA", "SCL"], "r"); p2 = tb("XP2", "ESP32", 252, 72, ["+5V", "GND", "TX", "RX"], "r")
    t6 = tb("XT6", "RS-485", 252, 102, ["A", "B", "GND"], "r"); t7 = tb("XT7", "входы", 252, 126, ["DI1", "DI2", "DI3"], "r")
    t8 = tb("XT8", "аналог.", 252, 150, ["GND", "AI", "+3V3"], "r"); t9 = tb("XT9", "реле", 252, 174, ["NO", "COM", "NC"], "r")
    p3 = tb("XP3", "NTC", 252, 198, ["NTC", "GND"], "r"); p4 = tb("XP4", "вент.", 252, 216, ["+", "−"], "r")
    dev(330, 36, 76, 30, ["Пульт (на крышке корпуса)", "A5: ЖКИ 2004 с адаптером I2C (0x27)", "A6: PCF8574 (0x20) + кнопки SB1…SB5:", "P0 ПУСК, P1 СТОП, P2 ▲, P3 ▼, P4 ВВОД → GND"])
    for i, p in enumerate(["+5V", "GND", "SDA", "SCL"]): wire(p1[p], (330, 42 + 6 * i), "14: шлейф 4×0,22 мм², до 300 мм" if i == 0 else "", -14 + 6 * i)
    dev(330, 70, 76, 26, ["A4: модуль ESP32 DevKit", "+5V → VIN, GND → GND", "TX платы → GPIO16, RX платы ← GPIO17"])
    for i, p in enumerate(["+5V", "GND", "TX", "RX"]): wire(p2[p], (330, 76 + 5 * i), "15: шлейф 4×0,22 мм²" if i == 0 else "", -14 + 6 * i)
    dev(330, 100, 76, 20, ["Сеть Modbus RTU", "витая пара: A (+), B (−), общий", "XP6 — терминатор 120 Ом"])
    for i, p in enumerate(["A", "B", "GND"]): wire(t6[p], (330, 105 + 5 * i), "", -8 + 6 * i)
    dev(330, 124, 76, 20, ["Внешние контакты управления", "DI1, DI2, DI3 замыкаются на GND (XT8.1)", "назначение — параметры P00, P39, P46"])
    for i, p in enumerate(["DI1", "DI2", "DI3"]): wire(t7[p], (330, 129 + 5 * i), "", -8 + 6 * i)
    dev(330, 148, 76, 20, ["RP1 10 кОм (XP5 снята)", "или сигнал 0–10 В на AI–GND", "(XP5 установлена, +3V3 не подключать)"])
    for i, p in enumerate(["GND", "AI", "+3V3"]): wire(t8[p], (330, 153 + 5 * i), "экранированный" if i == 0 else "", -8 + 6 * i)
    dev(330, 172, 76, 20, ["Цепь сигнализации", "до 250 В~, 5 А"])
    for i, p in enumerate(["NO", "COM", "NC"]): wire(t9[p], (330, 177 + 5 * i), "", -8 + 6 * i)
    dev(330, 196, 76, 14, ["RK2: NTC 10 кОм B3950", "на радиаторе рядом с DA1"])
    for i, p in enumerate(["NTC", "GND"]): wire(p3[p], (330, 201 + 5 * i), "", -4 + 6 * i)
    dev(330, 215, 76, 14, ["M2: вентилятор 5 В, 50×50 мм", "красный — «+»"])
    for i, p in enumerate(["+", "−"]): wire(p4[p], (330, 222 + 4 * i), "", 2 + 6 * i)
    s.note(30, 242, ["1. Силовые провода (поз. 1–13) — с наконечниками НШВИ; прокладывать отдельно от проводов управления.",
                     "2. Провода PE сети и двигателя, экран кабеля двигателя соединяются на шпильке заземления радиатора.",
                     "3. Радиатор, шпилька заземления и диодный мост VD1 находятся вне платы (см. сборочный чертёж)."])
    return s


SECTIONS = [("A", "Устройства (модули)"), ("C", "Конденсаторы"), ("DA", "Микросхемы аналоговые"), ("DD", "Микросхемы цифровые"), ("FU", "Предохранители"),
            ("HL", "Индикаторы"), ("K", "Реле"), ("R", "Резисторы"), ("RK", "Терморезисторы"), ("RU", "Варисторы"), ("U", "Оптроны"),
            ("VD", "Диоды"), ("VT", "Транзисторы"), ("XP", "Соединители штыревые"), ("XT", "Клеммники")]


def pe3_rows():
    def key(ref): m = re.match(r"([A-Z]+)(\d+)", ref); return m.group(1), int(m.group(2))
    comps = sorted(N.C, key=lambda c: key(c["ref"]))
    rows = []
    for pref, title in SECTIONS:
        grp = [c for c in comps if key(c["ref"])[0] == pref]
        if not grp: continue
        rows.append(("", title, "", "", True))
        i = 0
        while i < len(grp):
            j = i
            while j + 1 < len(grp) and (grp[j + 1]["val"], grp[j + 1]["desc"].split(" (")[0]) == (grp[i]["val"], grp[i]["desc"].split(" (")[0]) \
                    and key(grp[j + 1]["ref"])[1] == key(grp[j]["ref"])[1] + 1: j += 1
            n = j - i + 1
            ref = grp[i]["ref"] if n == 1 else f'{grp[i]["ref"]}, {grp[j]["ref"]}' if n == 2 else f'{grp[i]["ref"]}…{grp[j]["ref"]}'
            full = grp[i]["desc"]
            d = full.split(" — ")[0].split(" (")[0]
            if len(d) > 46: d = d.split(",")[0]
            m = re.search(r"\(([^()]*)\)\s*$", full)
            note = (m.group(1) if m else full.split(" — ", 1)[1] if " — " in full else "") if n == 1 else ""
            rows.append((ref, f'{d}; {grp[i]["val"]}', str(n), note, False))
            i = j + 1
    rows.append(("", "Изделия вне платы", "", "", True))
    for ref, val, desc, q, _ in N.OFFBOARD:
        if ref != "—": rows.append((ref, val, str(q), desc, False))
    return rows


def pe3():
    rows = pe3_rows(); per = 30; pages = [rows[i:i + per] for i in range(0, len(rows), per)]
    out = []
    for pi, page in enumerate(pages, 1):
        s = Sheet("", "ПЭ3", "Перечень элементов", pi, len(pages), fmt="A4")
        x0, y0 = 20, 5; cols = [(0, 22, "Поз. обозн."), (22, 108, "Наименование"), (130, 10, "Кол."), (140, 45, "Примечание")]
        s.rect(x0, y0, 185, 15, 0.6)
        for cx, cw, t in cols:
            s.line(x0 + cx, y0, x0 + cx, y0 + 15 + 7 * per, 0.6); s.text(x0 + cx + cw / 2, y0 + 9.5, t, 2.8, "middle")
        s.line(x0, y0 + 15 + 7 * per, x0 + 185, y0 + 15 + 7 * per, 0.6)
        for i in range(per):
            y = y0 + 15 + 7 * i; s.line(x0, y + 7, x0 + 185, y + 7, 0.25)
            if i >= len(page): continue
            ref, name, q, note, hdr = page[i]
            def fit(t, w, size):
                mx = int(w / (size * 0.53)); return t if len(t) <= mx else t[:mx - 1] + "…"
            s.text(x0 + 1.5, y + 4.9, fit(ref, 21, 2.5), 2.5)
            s.text(x0 + (22 + 54 if hdr else 23.5), y + 4.9, fit(name, 106, 2.5), 2.7 if hdr else 2.5, "middle" if hdr else "start", bold=hdr)
            s.text(x0 + 135, y + 4.9, q, 2.5, "middle"); s.text(x0 + 141.5, y + 4.9, fit(note, 44, 2.1), 2.1)
        out.append(s)
    return out


def main():
    e3 = [f() for f in E3.SHEETS]
    errs = verify(e3)
    print(f"Э3: листов {len(e3)}, элементов {len({p.ref for s in e3 for p in s.pins})} из {len(COMP)}, расхождений с нетлистом: {len(errs)}")
    for e in errs[:30]: print("   ", e)
    docs = [("e1_structure", [e1()]), ("e3_sheet", e3), ("pe3_list", pe3()), ("e4_wiring", [e4()])]
    files = []
    for name, sheets in docs:
        for i, s in enumerate(sheets, 1):
            fn = f"{name}{i if len(sheets) > 1 else ''}.svg"; open(os.path.join(OUT, fn), "w", encoding="utf-8").write(s.svg()); files.append((fn, s.W, s.H))
    js = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mkpdf.js")
    r = subprocess.run(["node", js, OUT, "pch1500_eskd.pdf"] + [f"{f}:{w}:{h}" for f, w, h in files], capture_output=True, text=True)
    print("PDF:", (r.stdout + r.stderr).strip() or "готов", "| файлов", len(files))
    return len(errs)


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
