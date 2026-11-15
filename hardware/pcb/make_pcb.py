#!/usr/bin/env python3
"""ПЧ-1500 — генерация печатной платы A1 (KiCad 7, pcbnew).
Этапы:  place  — размещение элементов и контур платы -> pch1500.kicad_pcb, pch1500.dsn
        (внешний шаг) автотрассировка Freerouting: pch1500.dsn -> pch1500.ses
        finish — импорт трасс, полигоны, проверка DRC, отчёт
Запуск:  python3 make_pcb.py place | finish     (см. build.sh)"""
import json, os, sys
import pcbnew
from pcbnew import FromMM, ToMM, VECTOR2I

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import netlist as N

PCB = os.path.join(HERE, "pch1500.kicad_pcb")
PRO = os.path.join(HERE, "pch1500.kicad_pro")
W, H = 180.0, 150.0
NOTCH = (34.0, 36.0)                 # вырез под диодный мост VD1 (левый верхний угол)
BARRIER_X = 120.0                    # ось барьера изоляции

# ---------------- классы цепей ----------------
HVPWR = ["L_IN", "N_IN", "BR+", "DCP", "U_IPM", "U_OUT", "V_OUT", "W_OUT", "W_MOT", "BRK_C"]
HVSIG = ["L_AUX", "X2D", "BLD", "LEDA", "LEDB", "VD_A", "VD_B", "BRK_D", "VSU", "VBU", "VSV", "VBV", "VSW", "VBW"]
LVPWR = ["GNDH", "NSH", "SHM"]
PWR5 = ["+5V", "GND", "+15VH", "+5VH", "+3V3", "K1P", "K1C", "K2C", "FAN_N", "PE"]
CLASSES = {  # имя: (зазор, ширина дорожки, диаметр/сверло перехода)
    "Default": (0.25, 0.4, 0.9, 0.5),
    "HVPWR": (2.0, 2.5, 1.6, 0.8),
    "HVSIG": (2.0, 0.6, 1.2, 0.6),
    "LVPWR": (0.4, 1.5, 1.4, 0.7),
    "PWR5": (0.3, 0.8, 1.2, 0.6),
}

# ---------------- размещение ----------------
# крупные элементы: поз. обозначение -> (угол, x, y) — координаты левого верхнего угла габарита
BIG = {
    "J1": (270, 35.5, 2.0), "J2": (270, 35.5, 19.5), "J3": (270, 50.0, 2.0), "RT1": (0, 64.5, 2.5),
    "K1": (0, 50.0, 15.5), "PS2": (0, 100.0, 2.3), "F1": (0, 56.0, 30.3),
    "PS1": (270, 2.0, 38.0), "CX1": (90, 2.0, 82.0), "RV1": (90, 15.0, 82.0),
    "R5": (90, 22.0, 82.0), "R6": (90, 28.5, 82.0), "R7": (0, 15.0, 101.5), "R8": (0, 15.0, 107.5), "HL1": (90, 5.0, 111.0),
    "J4": (0, 2.5, 137.0), "VT2": (0, 17.5, 142.0), "VD3": (0, 17.5, 134.0),
    "K2": (0, 157.0, 108.5), "J11": (270, 169.0, 91.0), "J9": (0, 125.0, 137.5), "J10": (0, 142.5, 137.5), "J6": (0, 160.0, 137.5),
    "J7": (270, 175.5, 3.0), "J8": (270, 175.5, 15.5), "J12": (270, 170.5, 3.0), "J13": (270, 170.5, 10.5),
}
# элементы с заданным положением первого вывода (угол, x, y)
PIN1 = {
    "C1": (0, 47.0, 52.0), "C2": (0, 47.0, 84.0), "C3": (0, 47.0, 116.0),
    "U1": (0, 88.0, 111.0),                      # начало координат посадочного места — центр корпуса
    "C4": (0, 76.2, 129.5), "J5": (0, 82.0, 142.5),
    "R9": (90, 103.5, 130.0), "R10": (90, 109.0, 130.0),
    "U4": (180, 123.81, 46.6), "U5": (180, 123.81, 58.0), "U6": (180, 123.81, 69.4),
    "U17": (180, 123.81, 77.5), "U11": (0, 116.19, 88.0), "U13": (0, 116.19, 96.0),
    "U14": (0, 120.0, 114.0), "U15": (0, 120.0, 124.0),
    "R13": (0, 127.0, 46.6), "R14": (0, 127.0, 38.98), "R15": (0, 127.0, 58.0), "R16": (0, 127.0, 50.38),
    "R17": (0, 127.0, 69.4), "R18": (0, 127.0, 61.78), "R4": (0, 127.0, 77.5),
    "A2": (0, 158.0, 40.0),
    "C6": (90, 73.2, 91.5), "C7": (90, 79.4, 91.5), "C8": (90, 85.6, 91.5),
    "U3": (0, 98.0, 48.0), "U18": (0, 98.0, 70.0), "U10": (0, 3.6, 120.6),
}
# мелкие элементы: области автоматической раскладки (x0, y0, x1, y1, угол, [позиции])
PACK = [
    (82.0, 15.0, 99.5, 36.0, 0, ["R72", "VT1", "VD2", "R70", "R71"]),
    (68.0, 37.0, 96.5, 46.0, 0, ["U2", "C15", "C16", "C12", "C13"]),
    (67.5, 46.5, 96.5, 62.0, 0, ["R19", "R20", "C18", "R21", "R22", "C19", "R23", "R24", "C20", "R25", "R26", "R27", "R28", "R29", "R30"]),
    (67.5, 62.0, 96.5, 86.0, 0, ["C46", "C47", "C48", "C49", "C50", "C51", "C17", "C52", "R12", "C14", "R11", "C5", "R31", "R32", "R33",
                                  "C9", "C10", "C11"]),
    (106.5, 66.0, 114.6, 108.0, 0, ["R42", "R43", "R44", "R45", "C24", "C25"]),
    (13.5, 118.5, 37.6, 133.9, 0, ["C30", "U12", "R41", "R35", "R36", "R37", "R55", "R38", "C31", "R39", "R40"]),
    (21.0, 113.4, 37.5, 117.6, 0, ["R1", "R2"]),
    (136.0, 36.0, 155.5, 108.0, 0, ["R34", "R46", "R47", "C42", "R48", "R49", "C43", "R56", "R57", "C40", "R58", "R59", "C41", "C26",
                                     "C37", "C36", "C39", "C38", "C28", "R52", "C45", "C27", "R50", "R51", "JP1", "C29", "R68"]),
    (125.0, 108.5, 156.6, 136.5, 0, ["R62", "R65", "C33", "R63", "R66", "C34", "R64", "R67", "C35", "U16", "C32", "R60", "JP2", "R61"]),
    (156.8, 125.0, 180.0, 137.4, 0, ["VD4", "VT3", "VT4", "VD5", "R53", "R54"]),
]
SLOTS = [  # фрезерованные прорези под элементами барьера (x, y0, y1), ширина 1,2 мм
    (BARRIER_X, 37.5, 48.0), (BARRIER_X, 49.0, 59.4), (BARRIER_X, 60.4, 70.8), (BARRIER_X, 73.5, 78.9),
    (BARRIER_X, 86.6, 91.9), (BARRIER_X, 94.6, 105.0), (BARRIER_X, 111.0, 117.0), (BARRIER_X, 121.0, 127.0),
]
HOLES = [(38.0, 146.0), (176.0, 146.5), (176.0, 34.0), (31.5, 60.0), (113.0, 146.0), (97.5, 33.5)]   # крепёжные отверстия 3,2 мм


def load_fp(c):
    lib, _, name = c["fp"].partition(":")
    path = os.path.join(HERE, "VFD.pretty") if lib == "VFD" else f"/usr/share/kicad/footprints/{lib}.pretty"
    fp = pcbnew.FootprintLoad(path, name)
    assert fp is not None, c["fp"]
    return fp


def bbox(fp):
    b = fp.GetBoundingBox(False, False)
    return ToMM(b.GetLeft()), ToMM(b.GetTop()), ToMM(b.GetRight()), ToMM(b.GetBottom())


def write_project():
    def cls(name, v):
        c, w, vd, vdr = v
        return {"name": name, "clearance": c, "track_width": w, "via_diameter": vd, "via_drill": vdr, "bus_width": 12, "wire_width": 6,
                "diff_pair_gap": 0.25, "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2, "line_style": 0,
                "microvia_diameter": 0.3, "microvia_drill": 0.1, "pcb_color": "rgba(0, 0, 0, 0.000)", "schematic_color": "rgba(0, 0, 0, 0.000)"}
    pats = [{"netclass": k, "pattern": n} for k, lst in (("HVPWR", HVPWR), ("HVSIG", HVSIG), ("LVPWR", LVPWR), ("PWR5", PWR5)) for n in lst]
    pro = {"board": {"design_settings": {"defaults": {}, "rules": {"min_clearance": 0.2, "min_track_width": 0.25, "min_via_diameter": 0.6,
            "min_through_hole_diameter": 0.3, "min_hole_to_hole": 0.25, "min_copper_edge_clearance": 0.3, "min_hole_clearance": 0.25,
            "solder_mask_to_copper_clearance": 0.0, "min_silk_clearance": 0.0},
            "rule_severities": {"silk_overlap": "ignore", "silk_over_copper": "ignore", "silk_edge_clearance": "ignore", "courtyards_overlap": "ignore",
                                "lib_footprint_issues": "ignore", "lib_footprint_mismatch": "ignore", "text_height": "ignore", "text_thickness": "ignore",
                                "missing_courtyard": "ignore", "npth_inside_courtyard": "ignore", "pth_inside_courtyard": "ignore",
                                "solder_mask_bridge": "ignore", "footprint_type_mismatch": "ignore"}}},
           "net_settings": {"classes": [cls(k, v) for k, v in CLASSES.items()], "meta": {"version": 3}, "net_colors": None, "netclass_assignments": None,
                            "netclass_patterns": pats},
           "meta": {"filename": "pch1500.kicad_pro", "version": 1}}
    json.dump(pro, open(PRO, "w"), indent=1)
    # пользовательские правила: бутстрепные цепи одной фазы и вход сети/варистор — малый зазор между собой
    rules = ['(version 1)']
    for ph in "UVW":
        rules.append(f'(rule "bootstrap_{ph}" (condition "A.NetName == \'/VB{ph}\' && B.NetName == \'/VS{ph}\' || A.NetName == \'VB{ph}\' && B.NetName == \'VS{ph}\'") (constraint clearance (min 0.4mm)))')
    rules.append('(rule "isolation_barrier" (condition "A.NetClass != B.NetClass && (A.NetName == \'GND\' || A.NetName == \'+5V\' || A.NetName == \'+3V3\') && (B.NetClass == \'HVPWR\' || B.NetClass == \'HVSIG\' || B.NetClass == \'LVPWR\')") (constraint clearance (min 3.0mm)))')
    open(os.path.join(HERE, "pch1500.kicad_dru"), "w").write("\n".join(rules) + "\n")


def edge(board, pts, closed=True):
    n = len(pts)
    for i in range(n if closed else n - 1):
        s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        a, b = pts[i], pts[(i + 1) % n]
        s.SetStart(VECTOR2I(FromMM(a[0]), FromMM(a[1]))); s.SetEnd(VECTOR2I(FromMM(b[0]), FromMM(b[1])))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(FromMM(0.1)); board.Add(s)


def text(board, t, x, y, size=1.5, layer=pcbnew.F_SilkS, rot=0):
    s = pcbnew.PCB_TEXT(board); s.SetText(t); s.SetPosition(VECTOR2I(FromMM(x), FromMM(y))); s.SetLayer(layer)
    s.SetTextSize(VECTOR2I(FromMM(size), FromMM(size))); s.SetTextThickness(FromMM(size * 0.15)); s.SetTextAngleDegrees(rot)
    if layer == pcbnew.B_SilkS: s.SetMirrored(True)
    board.Add(s)


def place():
    if os.path.exists(PCB): os.remove(PCB)
    board = pcbnew.BOARD()
    nets = {}
    for name in sorted(N.nets()):
        ni = pcbnew.NETINFO_ITEM(board, name); board.Add(ni); nets[name] = ni
    fps = {}
    for c in N.C:
        fp = load_fp(c); fp.SetReference(c["ref"]); fp.SetValue(c["val"])
        for pad in fp.Pads():
            for k, net in c["pins"].items():
                if k.partition(":")[0] == pad.GetNumber() and net: pad.SetNet(nets[net])
        fp.Value().SetVisible(False)
        fp.Reference().SetTextSize(VECTOR2I(FromMM(0.9), FromMM(0.9))); fp.Reference().SetTextThickness(FromMM(0.13))
        board.Add(fp); fps[c["old"]] = fp
    placed = set()

    def put_tl(ref, rot, x, y):
        fp = fps[ref]; fp.SetPosition(VECTOR2I(0, 0)); fp.SetOrientationDegrees(rot)
        l, t, r, b = bbox(fp)
        fp.SetPosition(VECTOR2I(FromMM(x - l), FromMM(y - t))); placed.add(ref)
        return r - l, b - t

    for ref, (rot, x, y) in BIG.items(): put_tl(ref, rot, x, y)
    for ref, (rot, x, y) in PIN1.items():
        fps[ref].SetOrientationDegrees(rot); fps[ref].SetPosition(VECTOR2I(FromMM(x), FromMM(y))); placed.add(ref)
    for x0, y0, x1, y1, rot, refs in PACK:
        x, y, rowh = x0, y0, 0.0
        for ref in refs:
            fp = fps[ref]; fp.SetPosition(VECTOR2I(0, 0)); fp.SetOrientationDegrees(rot)
            l, t, r, b = bbox(fp); w, h = r - l + 0.7, b - t + 0.7
            if x + w > x1 + 0.01: x = x0; y += rowh; rowh = 0.0
            if y + h > y1 + 0.01: print(f"ВНИМАНИЕ: {ref} не помещается в область ({x0},{y0})-({x1},{y1})")
            put_tl(ref, rot, x, y); x += w; rowh = max(rowh, h)
    missing = [c["old"] for c in N.C if c["old"] not in placed]
    assert not missing, f"не размещены: {missing}"

    # контур платы с вырезом под диодный мост, прорези барьера, крепёжные отверстия
    nx, ny = NOTCH
    edge(board, [(nx, 0), (W, 0), (W, H), (0, H), (0, ny), (nx, ny)])
    for x, y0, y1 in SLOTS: edge(board, [(x - 0.6, y0), (x + 0.6, y0), (x + 0.6, y1), (x - 0.6, y1)])
    for x, y in HOLES:
        fp = pcbnew.FootprintLoad("/usr/share/kicad/footprints/MountingHole.pretty", "MountingHole_3.2mm_M3")
        fp.SetReference(f"H{HOLES.index((x, y)) + 1}"); fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
        fp.SetPosition(VECTOR2I(FromMM(x), FromMM(y))); board.Add(fp)
    # запретная зона трассировки в зазоре барьера изоляции
    for (x0, y0, x1, y1) in ((BARRIER_X - 2.9, 34.5, BARRIER_X + 2.9, H), (108.5, 0.0, 142.0, 34.5)):
        z = pcbnew.ZONE(board); z.SetIsRuleArea(True); z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowCopperPour(True)
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
        ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu); z.SetLayerSet(ls)
        o = z.Outline(); o.NewOutline()
        for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)): o.Append(FromMM(px), FromMM(py))
        board.Add(z)
    text(board, "ПЧ-1500 A1 v1.0", 162, 36.2, 1.2)
    text(board, "ОПАСНО 400 В", 52, 147.5, 1.8)
    text(board, "ИЗОЛИРОВАНО", 138, 148.8, 1.0)
    board.GetDesignSettings().SetCopperLayerCount(2)
    pcbnew.SaveBoard(PCB, board)
    write_project()            # после SaveBoard: он перезаписывает файл проекта
    board = pcbnew.LoadBoard(PCB)
    ok = pcbnew.ExportSpecctraDSN(board, os.path.join(HERE, "pch1500.dsn"))
    fix_dsn(os.path.join(HERE, "pch1500.dsn"))
    print("размещено", len(fps), "элементов; DSN:", ok)


def net_class(name):
    for k, lst in (("HVPWR", HVPWR), ("HVSIG", HVSIG), ("LVPWR", LVPWR), ("PWR5", PWR5)):
        if name in lst: return k
    return "Default"


def fix_dsn(path):
    """Экспорт KiCad 7 записывает все цепи в один класс; заменяем его нашими классами (зазоры, ширины, переходы)."""
    import re
    s = open(path, encoding="utf-8").read()
    m = re.search(r'\(padstack "(Via\[0-1\]_\d+:\d+_um)"\n(.*?)\n    \)\n', s, re.S)
    vname, vbody = m.group(1), m.group(0)
    extra, vias = "", {}
    for k, (cl, w, vd, vdr) in CLASSES.items():
        nm = f"Via[0-1]_{int(vd * 1000)}:{int(vdr * 1000)}_um"; vias[k] = nm
        if nm != vname and nm not in extra:
            extra += re.sub(r"\(circle (\S+) [\d.]+\)", lambda mm: f"(circle {mm.group(1)} {int(vd * 1000)})", vbody).replace(vname, nm)
    s = s.replace(vbody, vbody + extra)
    s = re.sub(r'\(via "' + re.escape(vname) + r'"\)', "(via " + " ".join(f'"{v}"' for v in dict.fromkeys([vname] + list(vias.values()))) + ")", s, count=1)
    a = s.index("    (class kicad_default"); b = s.index("  (wiring")
    out = ""
    for k, (cl, w, vd, vdr) in CLASSES.items():
        nets = [n for n in sorted(N.nets()) if net_class(n) == k]
        out += f'    (class {k} {" ".join(nets)}\n      (circuit\n        (use_via "{vias[k]}")\n      )\n      (rule\n        (width {int(w * 1000)})\n        (clearance {cl * 1000 + 0.1:.1f})\n      )\n    )\n'
    s = s[:a] + out + "  )\n" + s[b:]
    open(path, "w", encoding="utf-8").write(s)


# ---------------- трассировка ----------------
MAINS = ["L_IN", "N_IN", "L_AUX", "X2D"]
HV = ["BR+", "DCP", "U_IPM", "U_OUT", "V_OUT", "W_OUT", "W_MOT", "BRK_C", "BLD", "LEDA", "LEDB", "VD_A", "VD_B", "BRK_D",
      "VSU", "VBU", "VSV", "VBV", "VSW", "VBW", "PE"]
LV = ["GNDH", "NSH", "SHM"]
P5 = ["+5V", "GND", "+15VH", "+5VH", "+3V3", "K1P", "K1C", "K2C", "FAN_N"]
GROUP = {"VBU": "BSU", "VSU": "BSU", "VBV": "BSV", "VSV": "BSV", "VBW": "BSW", "VSW": "BSW",
         "U_IPM": "PHU", "U_OUT": "PHU", "W_OUT": "PHW", "W_MOT": "PHW"}


def net_cl(name):
    return 2.5 if name in MAINS else 2.0 if name in HV else 0.4 if name in LV else 0.3 if name in P5 else 0.25


def via_for(w):
    return (1.6, 0.8) if w >= 1.5 else (1.2, 0.6) if w >= 0.6 else (0.9, 0.5)


# задания трассировки по порядку: (цепь, ширина, [выводы] или None — все оставшиеся)
JOBS = [
    ("L_IN", 3.0, ["J1.1", "J2.1"]), ("N_IN", 3.0, ["J1.2", "J2.2"]), ("BR+", 3.0, ["J3.1", "RT1.1", "K1.11"]),
    ("DCP", 3.0, ["RT1.2", "K1.14", "C1.1", "C2.1", "C3.1", "U1.23", "C4.1"]),
    ("GNDH", 3.0, ["J3.2", "C1.2", "C2.2", "C3.2", "R10.2"]),
    ("NSH", 2.5, ["U1.19", "U1.18", "U1.17", "R9.1", "C4.2"]), ("SHM", 2.5, None),
    ("U_IPM", 1.8, ["U1.22", "U14.1"]), ("U_OUT", 1.8, ["U14.4", "J5.1"]), ("V_OUT", 1.8, None),
    ("W_OUT", 1.8, ["U1.20", "U15.1"]), ("W_MOT", 1.8, ["U15.4", "J5.3"]), ("PE", 1.8, None),
    ("BRK_C", 1.5, None), ("DCP", 1.5, ["C3.1", "J4.1", "VD3.1"]),
    ("VBU", 0.6, None), ("VSU", 0.6, None), ("VBV", 0.6, None), ("VSV", 0.6, None), ("VBW", 0.6, None), ("VSW", 0.6, None),
    ("L_IN", 1.0, None), ("N_IN", 1.0, None), ("L_AUX", 0.6, None), ("X2D", 0.6, None),
    ("DCP", 0.6, None), ("BLD", 0.6, None), ("LEDA", 0.6, None), ("LEDB", 0.6, None), ("VD_A", 0.6, None), ("VD_B", 0.6, None), ("BRK_D", 0.6, None),
    ("*", 0.4, None),
    ("+15VH", 0.8, None), ("+5VH", 0.8, None), ("K1P", 0.8, None), ("K1C", 0.8, None), ("+3V3", 0.8, None), ("+5V", 0.8, None),
    ("K2C", 0.8, None), ("FAN_N", 0.8, None), ("GNDH", 0.8, None), ("GND", 0.8, None),
]
PRETIES = [("U14", "1", "2", "U_IPM"), ("U14", "3", "4", "U_OUT"), ("U15", "1", "2", "W_OUT"), ("U15", "3", "4", "W_MOT")]


def route():
    import time
    from router import Router, JobClass, G
    board = pcbnew.LoadBoard(PCB)
    for t in list(board.GetTracks()): board.Remove(t)
    allnets = sorted(N.nets())
    special = {j[0] for j in JOBS if j[0] != "*"}
    jobs = []
    for name, w, pins in JOBS:
        if name == "*": jobs += [(n, w, None) for n in allnets if n not in special]
        else: jobs.append((name, w, pins))
    classes = {}
    for name, w, _ in jobs:
        k = (net_cl(name), GROUP.get(name), w)
        if k not in classes: classes[k] = JobClass(k[0], k[1], w, *via_for(w))
    R = Router(W, H, list(classes.values()))
    for n in allnets: R.net(n, net_cl(n), GROUP.get(n))
    # край платы, вырез, прорези, отверстия, запретные зоны
    nx, ny = NOTCH
    R.stamp_rect(0, 0, nx, ny, 0, -1, (0, 1))
    for (ax, ay, bx, by) in ((0, 0, W, 0), (W, 0, W, H), (W, H, 0, H), (0, H, 0, 0)):
        for l in (0, 1): R.stamp_seg(ax, ay, bx, by, 0, -1, l)
    for x, y0, y1 in SLOTS: R.stamp_rect(x - 0.6, y0, x + 0.6, y1, 0, -1, (0, 1))
    for z in board.Zones():
        if z.GetIsRuleArea():
            b = z.GetBoundingBox()
            R.stamp_rect(ToMM(b.GetLeft()), ToMM(b.GetTop()), ToMM(b.GetRight()), ToMM(b.GetBottom()), 0, -1, (0, 1), extra=0.0)
    pads, anon = {}, 0
    for fp in board.GetFootprints():
        for p in fp.Pads():
            pos = p.GetPosition(); x, y = ToMM(pos.x), ToMM(pos.y)
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                R.stamp_rect(x, y, x, y, ToMM(p.GetDrillSize().x) / 2, -1, (0, 1)); continue
            b = p.GetBoundingBox()
            d = dict(ref=f"{fp.GetReference()}.{p.GetNumber()}", x=x, y=y, rect=(ToMM(b.GetLeft()), ToMM(b.GetTop()), ToMM(b.GetRight()), ToMM(b.GetBottom())),
                     layers=(0, 1) if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else (0,),
                     minsize=min(ToMM(p.GetSize().x), ToMM(p.GetSize().y)))
            if p.GetShape() == pcbnew.PAD_SHAPE_CIRCLE: d["round"] = ToMM(p.GetSize().x) / 2
            name = p.GetNetname()
            if not name:
                anon += 1; name = f"~nc{anon}"; R.net(name, 2.0 if fp.GetReference() == "U1" else 0.25)
            d["net"] = name
            pads.setdefault(name, []).append(d)
            R.add_pad(d, R.netid[name])
    def find(name, spec):          # spec — «рабочее обозначение.вывод», например "J1.1"
        r, _, pn = spec.partition(".")
        return [p for p in pads[name] if p["ref"] == f"{N.new_ref(r)}.{pn}"]
    # перемычки между сдвоенными выводами датчиков тока
    for ref, a, b, name in PRETIES:
        pa, pb = find(name, f"{ref}.{a}")[0], find(name, f"{ref}.{b}")[0]
        R.add_track(pa["x"], pa["y"], pb["x"], pb["y"], 0.5, R.netid[name], 0)
        R.tree.setdefault(R.netid[name], set())
    def hpwl(n):
        xs = [p["x"] for p in pads[n]]; ys = [p["y"] for p in pads[n]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))
    star = [j for j in jobs if j[0] not in special]
    star.sort(key=lambda j: hpwl(j[0]))
    it = iter(star)
    ordered = [next(it) if j[0] not in special else j for j in jobs]
    t0 = time.time()
    for name, w, pins in ordered:
        if pins is None: pl = list(pads[name])
        else: pl = [p for s in pins for p in find(name, s)]
        if len(pl) < 1: continue
        # площадки, уже связанные перемычкой с соседней
        k = (net_cl(name), GROUP.get(name), w)
        nf = R.route(name, k, pl, f"w={w}")
        if nf: print(f"  не разведено: {name} ({nf} из {len(pl)})")
    print(f"трассировка: {time.time() - t0:.0f} с, отрезков {len(R.segs)}, переходов {len(R.vias)}, неразведённых соединений {len(R.fail)}")
    for f in R.fail: print("   ", f)
    for ax, ay, bx, by, w, n, layer in R.segs:
        t = pcbnew.PCB_TRACK(board); t.SetStart(VECTOR2I(FromMM(ax), FromMM(ay))); t.SetEnd(VECTOR2I(FromMM(bx), FromMM(by)))
        t.SetWidth(FromMM(w)); t.SetLayer(pcbnew.F_Cu if layer == 0 else pcbnew.B_Cu)
        t.SetNet(board.FindNet([k for k, v in R.netid.items() if v == n][0])); board.Add(t)
    for x, y, d, dr, n in R.vias:
        v = pcbnew.PCB_VIA(board); v.SetPosition(VECTOR2I(FromMM(x), FromMM(y))); v.SetWidth(FromMM(d)); v.SetDrill(FromMM(dr))
        v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNet(board.FindNet([k for k, vv in R.netid.items() if vv == n][0])); board.Add(v)
    pcbnew.SaveBoard(os.path.join(HERE, "pch1500_routed.kicad_pcb"), board)


if __name__ == "__main__":
    {"place": place, "route": route}[sys.argv[1]]()
