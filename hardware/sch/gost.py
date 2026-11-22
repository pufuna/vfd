"""Мини-САПР для оформления схем PUFVFD по ЕСКД (ГОСТ 2.104, 2.701, 2.702, 2.710, 2.721–2.759):
лист A3 с рамкой и основной надписью, условные графические обозначения, линии связи, обозначения цепей.
Номиналы и цепи берутся из netlist.py; после отрисовки связность чертежа сверяется с нетлистом (verify)."""
import html, math, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import netlist as N

COMP = {c["ref"]: c for c in N.C}
FONT = 'font-family="DejaVu Sans,Arial,sans-serif" font-style="italic"'
DOC = {"code": "PUFVFD.00.000", "name": "Преобразователь частоты PUFVFD", "date": "11.2026"}


def pin_net(ref, num):
    for k, net in COMP[ref]["pins"].items():
        if k.partition(":")[0] == str(num): return net
    raise KeyError((ref, num))


class P:
    """Точка подключения вывода элемента."""
    def __init__(self, x, y, ref=None, num=None):
        self.x, self.y, self.ref, self.num = x, y, ref, num

    @property
    def net(self): return pin_net(self.ref, self.num)

    def xy(self): return (round(self.x, 3), round(self.y, 3))


def _xy(p): return p.xy() if isinstance(p, P) else (round(p[0], 3), round(p[1], 3))


class Comp:
    def __init__(self, ref): self.ref, self.p = ref, {}

    def __getitem__(self, num): return self.p[str(num)]


class Sheet:
    W, H = 420.0, 297.0

    def __init__(self, title, doc_suffix, doc_name, num, total, fmt="A3"):
        if fmt == "A4": self.W, self.H = 210.0, 297.0
        self.title, self.num, self.total = title, num, total
        self.b, self.wires, self.labels, self.pins, self.ncs = [], [], [], [], []
        self.comps = {}
        self._frame(doc_suffix, doc_name)

    # ---------- примитивы ----------
    def line(self, x1, y1, x2, y2, w=0.35, dash=None):
        self.b.append(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="#000" stroke-width="{w}" stroke-linecap="round"'
                      + (f' stroke-dasharray="{dash}"' if dash else "") + "/>")

    def poly(self, pts, w=0.35, fill="none", close=False):
        d = " ".join(f"{x:.2f},{y:.2f}" for x, y in pts)
        tag = "polygon" if close else "polyline"
        self.b.append(f'<{tag} points="{d}" fill="{fill}" stroke="#000" stroke-width="{w}" stroke-linejoin="round" stroke-linecap="round"/>')

    def rect(self, x, y, w, h, sw=0.5, fill="none", dash=None):
        self.b.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{fill}" stroke="#000" stroke-width="{sw}"'
                      + (f' stroke-dasharray="{dash}"' if dash else "") + "/>")

    def circle(self, x, y, r, sw=0.5, fill="none"):
        self.b.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{fill}" stroke="#000" stroke-width="{sw}"/>')

    def text(self, x, y, t, size=3.0, anchor="start", rot=0, bold=False):
        tr = f' transform="rotate({rot} {x:.2f} {y:.2f})"' if rot else ""
        self.b.append(f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" text-anchor="{anchor}" {FONT}'
                      + (' font-weight="bold"' if bold else "") + f'{tr}>{html.escape(str(t))}</text>')

    # ---------- рамка и основная надпись (ГОСТ 2.104, форма 1) ----------
    def _frame(self, suffix, doc_name):
        W, H = self.W, self.H
        self.b.append(f'<rect width="{W}" height="{H}" fill="#fff"/>')
        self.rect(20, 5, W - 25, H - 10, 0.7)
        x0, y0 = W - 5 - 185, H - 5 - 55
        self.rect(x0, y0, 185, 55, 0.7)
        for dx in (7, 17, 40, 55, 65): self.line(x0 + dx, y0, x0 + dx, y0 + 55, 0.5 if dx in (65,) else 0.35)
        self.line(x0 + 65, y0, x0 + 65, y0 + 55, 0.7)
        for k in range(1, 11): self.line(x0, y0 + 5 * k, x0 + 65, y0 + 5 * k, 0.25 if k not in (2, 3) else 0.5)
        self.line(x0 + 65, y0 + 15, x0 + 185, y0 + 15, 0.7); self.line(x0 + 65, y0 + 40, x0 + 185, y0 + 40, 0.7)
        self.line(x0 + 135, y0 + 15, x0 + 135, y0 + 55, 0.7)
        self.line(x0 + 135, y0 + 20, x0 + 185, y0 + 20, 0.5); self.line(x0 + 135, y0 + 35, x0 + 185, y0 + 35, 0.5)
        for dx in (150, 167): self.line(x0 + dx, y0 + 15, x0 + dx, y0 + 35, 0.5)
        for dx in (140, 145): self.line(x0 + dx, y0 + 20, x0 + dx, y0 + 35, 0.35)
        self.line(x0 + 155, y0 + 35, x0 + 155, y0 + 40, 0.5)
        for i, t in enumerate(("Изм.", "Лист", "№ докум.", "Подп.", "Дата")):
            self.text(x0 + (3.5, 12, 28.5, 47.5, 60)[i], y0 + 14, t, 2.2, "middle")
        for i, (t, who) in enumerate((("Разраб.", "Claude"), ("Пров.", ""), ("Т. контр.", ""), ("", ""), ("Н. контр.", ""), ("Утв.", ""))):
            self.text(x0 + 1, y0 + 19 + 5 * i, t, 2.2); self.text(x0 + 18, y0 + 19 + 5 * i, who, 2.2)
        self.text(x0 + 60, y0 + 19, DOC["date"], 1.8, "middle")
        self.text(x0 + 125, y0 + 10, f'{DOC["code"]} {suffix}', 6, "middle")
        self.text(x0 + 100, y0 + 24, DOC["name"], 3.4, "middle")
        for i, t in enumerate(doc_name.split("\n")): self.text(x0 + 100, y0 + 30 + 4.5 * i, t, 3.2, "middle")
        self.text(x0 + 142.5, y0 + 19, "Лит.", 2.2, "middle"); self.text(x0 + 158.5, y0 + 19, "Масса", 2.2, "middle"); self.text(x0 + 176, y0 + 19, "Масштаб", 2.2, "middle")
        self.text(x0 + 176, y0 + 29, "—", 3, "middle")
        self.text(x0 + 137, y0 + 39, f"Лист {self.num}", 2.6); self.text(x0 + 157, y0 + 39, f"Листов {self.total}", 2.6)
        self.text(x0 + 160, y0 + 49, "Проект PUFVFD", 3, "middle")
        self.text(25, 12, self.title, 4.2, bold=True)

    # ---------- элементы ----------
    def _reg(self, ref, pts):
        c = self.comps.setdefault(ref, Comp(ref))
        for num, (x, y) in pts.items():
            pt = P(x, y, ref, str(num)); c.p[str(num)] = pt; self.pins.append(pt)
        return c

    def _val(self, ref):
        return COMP[ref]["val"]

    def _two(self, ref, x, y, o, half):
        """Координаты двух выводов: o = 'h' (1 слева), 'h-' (1 справа), 'v' (1 сверху), 'v-' (1 снизу)."""
        a, b = ((x - half, y), (x + half, y)) if o[0] == "h" else ((x, y - half), (x, y + half))
        if o.endswith("-"): a, b = b, a
        return a, b

    def _lbl(self, ref, x, y, o, off=3.2, val=True, size=2.8):
        v = self._val(ref) if val else ""
        if o[0] == "h":
            self.text(x, y - off, ref, size, "middle"); self.text(x, y + off + 2.4, v, 2.3, "middle")
        else:
            self.text(x + off, y - 0.6, ref, size); self.text(x + off, y + 3.0, v, 2.3)

    def res(self, ref, x, y, o="h", kind=None, val=True):
        a, b = self._two(ref, x, y, o, 7.5)
        if o[0] == "h":
            self.rect(x - 5, y - 2, 10, 4); self.line(a[0], y, x - 5 if a[0] < x else x + 5, y); self.line(b[0], y, x + 5 if b[0] > x else x - 5, y)
        else:
            self.rect(x - 2, y - 5, 4, 10); self.line(x, a[1], x, y - 5 if a[1] < y else y + 5); self.line(x, b[1], x, y + 5 if b[1] > y else y - 5)
        if kind in ("U", "t"):          # варистор / терморезистор: наклонная черта с изломом
            self.poly([(x - 5.5, y + 4), (x - 3.5, y + 4), (x + 4.5, y - 4)], 0.3)
            self.text(x - 6.5, y + 3.2, "U" if kind == "U" else "t°", 2.2, "end")
        if kind == "fuse":
            if o[0] == "h": self.line(x - 5, y, x + 5, y)
            else: self.line(x, y - 5, x, y + 5)
        self._lbl(ref, x, y, o, val=val)
        return self._reg(ref, {1: a, 2: b})

    def cap(self, ref, x, y, o="v", polar=False, val=True):
        a, b = self._two(ref, x, y, o, 5)
        if o[0] == "h":
            self.line(x - 0.75, y - 4, x - 0.75, y + 4, 0.6); self.line(x + 0.75, y - 4, x + 0.75, y + 4, 0.6)
            self.line(a[0], y, x - 0.75 if a[0] < x else x + 0.75, y); self.line(b[0], y, x + 0.75 if b[0] > x else x - 0.75, y)
            if polar: self.text((a[0] + x) / 2, y - 1.2, "+", 2.6, "middle")
        else:
            self.line(x - 4, y - 0.75, x + 4, y - 0.75, 0.6); self.line(x - 4, y + 0.75, x + 4, y + 0.75, 0.6)
            self.line(x, a[1], x, y - 0.75 if a[1] < y else y + 0.75); self.line(x, b[1], x, y + 0.75 if b[1] > y else y - 0.75)
            if polar: self.text(x - 2.6, (a[1] + y) / 2 + (0.2 if a[1] < y else 1.6), "+", 2.6, "middle")
        if o[0] == "h": self._lbl(ref, x, y, o, off=5.2, val=val)
        else: self._lbl(ref, x, y, o, off=5.0, val=val)
        return self._reg(ref, {1: a, 2: b})

    def diode(self, ref, x, y, o="h", led=False, val=True):
        """Вывод A — анод, K — катод; o задаёт положение анода: 'h' — слева, 'h-' — справа, 'v' — сверху, 'v-' — снизу."""
        a, k = self._two(ref, x, y, o, 7.5)
        dx, dy = (k[0] - a[0]) / 15, (k[1] - a[1]) / 15          # единичный вектор анод -> катод
        px, py = -dy, dx
        t0 = (x - 2.5 * dx, y - 2.5 * dy); t1 = (x + 2.5 * dx, y + 2.5 * dy)
        self.poly([(t0[0] + 2.5 * px, t0[1] + 2.5 * py), (t0[0] - 2.5 * px, t0[1] - 2.5 * py), t1], 0.4, close=True)
        self.line(t1[0] + 2.5 * px, t1[1] + 2.5 * py, t1[0] - 2.5 * px, t1[1] - 2.5 * py, 0.5)
        self.line(a[0], a[1], t0[0], t0[1]); self.line(t1[0], t1[1], k[0], k[1])
        if led:
            for s in (-1.2, 1.2):
                bx, by = x + s * dx - 3.2 * px, y + s * dy - 3.2 * py
                ex, ey = bx - 2.6 * px + 1.2 * dx, by - 2.6 * py + 1.2 * dy
                self.line(bx, by, ex, ey, 0.3); self.poly([(ex, ey), (ex + 1.1 * px - 0.1 * dx, ey + 1.1 * py - 0.1 * dy), (ex + 0.4 * px - 0.9 * dx, ey + 0.4 * py - 0.9 * dy)], 0.2, "#000", True)
        self._lbl(ref, x, y, o, off=3.6 if o[0] == "h" else 3.8, val=val)
        pins = {}
        for key in COMP[ref]["pins"]:
            num, _, nm = key.partition(":"); pins[num] = a if nm == "A" else k
        return self._reg(ref, pins)

    def npn(self, ref, x, y, mirror=False, igbt=False, val=True):
        """Транзистор: база (затвор) слева, коллектор сверху, эмиттер снизу (mirror — зеркально)."""
        m = -1 if mirror else 1
        self.circle(x, y, 6)
        self.line(x - 2.5 * m, y - 4, x - 2.5 * m, y + 4, 0.6)
        if igbt: self.line(x - 3.6 * m, y - 3.2, x - 3.6 * m, y + 3.2, 0.5); self.line(x - 10 * m, y, x - 3.6 * m, y)
        else: self.line(x - 10 * m, y, x - 2.5 * m, y)
        self.line(x - 2.5 * m, y - 1.5, x + 2.5 * m, y - 4.5); self.line(x + 2.5 * m, y - 4.5, x + 2.5 * m, y - 10)
        self.line(x - 2.5 * m, y + 1.5, x + 2.5 * m, y + 4.5); self.line(x + 2.5 * m, y + 4.5, x + 2.5 * m, y + 10)
        ex, ey = x + 2.5 * m, y + 4.5
        self.poly([(ex, ey), (ex - 2.0 * m, ey - 0.1), (ex - 1.0 * m, ey - 1.7)], 0.2, "#000", True)
        self.text(x + 7.5 * m, y - 1.5, ref, 2.8, "start" if m > 0 else "end")
        if val: self.text(x + 7.5 * m, y + 2.2, self._val(ref), 2.3, "start" if m > 0 else "end")
        pos = {"B": (x - 10 * m, y), "G": (x - 10 * m, y), "C": (x + 2.5 * m, y - 10), "E": (x + 2.5 * m, y + 10)}
        return self._reg(ref, {k.partition(":")[0]: pos[k.partition(":")[2]] for k in COMP[ref]["pins"]})

    def box(self, ref, x, y, w, left, right, title=None, pitch=5.0, top=5.0, val=True, hdr=True):
        """Микросхема / модуль: прямоугольник, выводы слева и справа. left/right — списки номеров выводов (None — пропуск шага)."""
        n = max(len(left), len(right)); h = top + pitch * (n - 1) + 5
        self.rect(x, y, w, h, 0.6)
        names = {k.partition(":")[0]: k.partition(":")[2] for k in COMP[ref]["pins"]}
        pins = {}
        for side, lst in ((0, left), (1, right)):
            for i, num in enumerate(lst):
                if num is None: continue
                py = y + top + pitch * i
                if side == 0:
                    self.line(x - 5, py, x, py); self.text(x - 1, py - 0.8, num, 2.0, "end"); self.text(x + 1, py + 1, names[str(num)], 2.3)
                    pins[str(num)] = (x - 5, py)
                else:
                    self.line(x + w, py, x + w + 5, py); self.text(x + w + 1, py - 0.8, num, 2.0); self.text(x + w - 1, py + 1, names[str(num)], 2.3, "end")
                    pins[str(num)] = (x + w + 5, py)
        if hdr:
            self.text(x + w / 2, y - 4.4, ref, 3.0, "middle")
            if val: self.text(x + w / 2, y - 1.2, title or self._val(ref), 2.3, "middle")
        return self._reg(ref, pins)

    def conn(self, ref, x, y, pins, side="r", title=None, w=22):
        """Соединитель в виде таблицы «Конт. / Цепь»; выводы со стороны side ('l' или 'r')."""
        names = {k.partition(":")[0]: k.partition(":")[2] for k in COMP[ref]["pins"]}
        n = len(pins); self.rect(x, y, w, 5 * (n + 1), 0.6)
        cx = x + 7 if side == "l" else x + w - 7
        self.line(cx, y, cx, y + 5 * (n + 1)); self.line(x, y + 5, x + w, y + 5)
        self.text((x + cx) / 2 if side == "l" else (cx + x + w) / 2, y + 3.6, "Конт.", 2.0, "middle")
        self.text((cx + x + w) / 2 if side == "l" else (x + cx) / 2, y + 3.6, "Цепь", 2.0, "middle")
        out = {}
        for i, num in enumerate(pins):
            py = y + 5 * (i + 1) + 2.5
            if i: self.line(x, py - 2.5, x + w, py - 2.5, 0.25)
            self.text((x + cx) / 2 if side == "l" else (cx + x + w) / 2, py + 1, num, 2.3, "middle")
            self.text((cx + x + w) / 2 if side == "l" else (x + cx) / 2, py + 1, names.get(str(num), "") or str(num), 2.3, "middle")
            if side == "l": self.line(x - 5, py, x, py); out[str(num)] = (x - 5, py)
            else: self.line(x + w, py, x + w + 5, py); out[str(num)] = (x + w + 5, py)
        self.text(x + w / 2, y - 4.4, ref, 3.0, "middle")
        self.text(x + w / 2, y - 1.2, title if title is not None else self._val(ref), 2.3, "middle")
        return self._reg(ref, out)

    def relay(self, ref, x, y, coil, val=True):
        """Реле: обмотка (прямоугольник) с выводами сверху/снизу. coil = (вывод сверху, вывод снизу)."""
        self.rect(x - 5, y - 3, 10, 6, 0.5); self.line(x, y - 7.5, x, y - 3); self.line(x, y + 3, x, y + 7.5)
        self.text(x + 6.5, y - 0.5, ref, 2.8)
        if val: self.text(x + 6.5, y + 3.0, self._val(ref), 2.3)
        return self._reg(ref, {coil[0]: (x, y - 7.5), coil[1]: (x, y + 7.5)})

    def jumper(self, ref, x, y, o="h", val=True):
        """Перемычка (два штыря с замыкателем)."""
        a, b = self._two(ref, x, y, o, 7.5)
        if o[0] == "h":
            self.line(a[0], y, x - 2.5 if a[0] < x else x + 2.5, y); self.line(b[0], y, x + 2.5 if b[0] > x else x - 2.5, y)
            self.circle(x - 2.5, y, 0.9, 0.4, "#fff"); self.circle(x + 2.5, y, 0.9, 0.4, "#fff"); self.line(x - 2.5, y - 2.2, x + 2.5, y - 2.2, 0.4, "1.2 0.8")
        else:
            self.line(x, a[1], x, y - 2.5 if a[1] < y else y + 2.5); self.line(x, b[1], x, y + 2.5 if b[1] > y else y - 2.5)
            self.circle(x, y - 2.5, 0.9, 0.4, "#fff"); self.circle(x, y + 2.5, 0.9, 0.4, "#fff"); self.line(x - 2.2, y - 2.5, x - 2.2, y + 2.5, 0.4, "1.2 0.8")
        self._lbl(ref, x, y, o, off=3.6, val=val)
        return self._reg(ref, {1: a, 2: b})

    def contact(self, ref, tag, x, y, com, no, nc=None):
        """Контакт реле (горизонтально): общий вывод слева, замыкающий справа (и размыкающий — справа ниже)."""
        self.line(x - 7.5, y, x - 3, y); self.line(x - 3, y, x + 3.2, y - 2.6, 0.5); self.line(x + 3, y, x + 7.5, y)
        self.text(x, y - 4.2, tag, 2.6, "middle")
        pts = {com: (x - 7.5, y), no: (x + 7.5, y)}
        if nc is not None:
            self.line(x + 3, y + 5, x + 7.5, y + 5); self.line(x + 3, y + 5, x + 3, y + 1.2); pts[nc] = (x + 7.5, y + 5)
        c = self.comps.setdefault(ref, Comp(ref))
        for num, (px, py) in pts.items():
            pt = P(px, py, ref, str(num)); c.p[str(num)] = pt; self.pins.append(pt)
        return c

    # ---------- линии связи ----------
    def wire(self, *pts):
        q = [_xy(p) for p in pts]
        for a, b in zip(q, q[1:]):
            assert abs(a[0] - b[0]) < 1e-6 or abs(a[1] - b[1]) < 1e-6, f"наклонная линия связи {a}-{b} ({self.title})"
            if a != b: self.wires.append((a, b)); self.line(a[0], a[1], b[0], b[1], 0.35)

    def w(self, a, b, mode="hv"):
        a, b = _xy(a), _xy(b)
        self.wire(a, (b[0], a[1]) if mode == "hv" else (a[0], b[1]), b)

    def flag(self, p, d="r", net=None, length=5.0):
        """Обрыв линии связи с обозначением цепи."""
        x, y = _xy(p); name = net or p.net
        ex, ey = {"r": (x + length, y), "l": (x - length, y), "u": (x, y - length), "d": (x, y + length)}[d]
        if length: self.wires.append(((x, y), (ex, ey))); self.line(x, y, ex, ey, 0.35)
        s = 2.4; wd = len(name) * 1.55 + 2.5
        if d in "rl":
            sg = 1 if d == "r" else -1
            self.poly([(ex, ey), (ex + sg * 1.5, ey - 1.9), (ex + sg * (wd + 1.5), ey - 1.9), (ex + sg * (wd + 1.5), ey + 1.9), (ex + sg * 1.5, ey + 1.9)], 0.3, close=True)
            self.text(ex + sg * (wd / 2 + 1.6), ey + 0.9, name, s, "middle")
        else:
            sg = 1 if d == "d" else -1
            self.poly([(ex, ey), (ex - 1.9, ey + sg * 1.5), (ex - 1.9, ey + sg * (wd + 1.5)), (ex + 1.9, ey + sg * (wd + 1.5)), (ex + 1.9, ey + sg * 1.5)], 0.3, close=True)
            self.text(ex + 0.9, ey + sg * (wd / 2 + 1.6), name, s, "middle", rot=-90)
        self.labels.append(((ex, ey), name))

    def pwr(self, p, d=None, net=None, length=4.0):
        """Обозначение цепи питания или общего провода."""
        x, y = _xy(p); name = net or p.net
        gnd = name in ("GND", "GNDH", "PE")
        d = d or ("d" if gnd else "u")
        ex, ey = {"r": (x + length, y), "l": (x - length, y), "u": (x, y - length), "d": (x, y + length)}[d]
        if length: self.wires.append(((x, y), (ex, ey))); self.line(x, y, ex, ey, 0.35)
        vx, vy = {"r": (1, 0), "l": (-1, 0), "u": (0, -1), "d": (0, 1)}[d]
        px, py = -vy, vx
        if name == "GND":
            for i, hw in enumerate((3.0, 2.0, 1.0)):
                cx, cy = ex + vx * i * 1.0, ey + vy * i * 1.0
                self.line(cx - px * hw, cy - py * hw, cx + px * hw, cy + py * hw, 0.5)
        elif name == "GNDH":
            self.line(ex - px * 3, ey - py * 3, ex + px * 3, ey + py * 3, 0.7)
            for i in (-2, 0, 2): self.line(ex + px * i, ey + py * i, ex + px * (i - 1.2) + vx * 1.6, ey + py * (i - 1.2) + vy * 1.6, 0.3)
            self.text(ex + (4 if d in "ud" else vx * 3.4), ey + (1 if d in "ud" else 4.2), "H", 2.2, "middle" if d in "rl" else "start")
        elif name == "PE":
            self.circle(ex + vx * 2.6, ey + vy * 2.6, 2.6, 0.4)
            for i, hw in enumerate((1.8, 1.2, 0.6)):
                cx, cy = ex + vx * (1.6 + i * 0.9), ey + vy * (1.6 + i * 0.9)
                self.line(cx - px * hw, cy - py * hw, cx + px * hw, cy + py * hw, 0.4)
        else:
            self.line(ex - px * 2.2, ey - py * 2.2, ex + px * 2.2, ey + py * 2.2, 0.6)
            if d == "u": self.text(ex, ey - 1.2, name, 2.4, "middle")
            elif d == "d": self.text(ex, ey + 3.4, name, 2.4, "middle")
            elif d == "r": self.text(ex + 1.5, ey + 0.9, name, 2.4)
            else: self.text(ex - 1.5, ey + 0.9, name, 2.4, "end")
        self.labels.append(((ex, ey), name))

    def nc(self, p):
        x, y = _xy(p); self.line(x - 1.2, y - 1.2, x + 1.2, y + 1.2, 0.4); self.line(x - 1.2, y + 1.2, x + 1.2, y - 1.2, 0.4); self.ncs.append(_xy(p))

    def note(self, x, y, lines, size=2.6):
        for i, t in enumerate(lines): self.text(x, y + i * (size + 1.4), t, size)

    def group(self, x, y, w, h, title):
        self.rect(x, y, w, h, 0.3, dash="6 2 1 2"); self.text(x + 2, y + 4, title, 2.8)

    # ---------- вывод ----------
    def junctions(self):
        ends = {}
        for a, b in self.wires:
            ends[a] = ends.get(a, 0) + 1; ends[b] = ends.get(b, 0) + 1
        for p in self.pins: ends[p.xy()] = ends.get(p.xy(), 0) + 1
        dots = set()
        for pt, cnt in ends.items():
            on = sum(1 for a, b in self.wires if _between(pt, a, b))
            if cnt >= 3 or (on and cnt >= 1): dots.add(pt)
        return dots

    def svg(self):
        body = "".join(self.b) + "".join(f'<circle cx="{x}" cy="{y}" r="0.75" fill="#000"/>' for x, y in self.junctions())
        return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.W} {self.H}" width="{self.W}mm" height="{self.H}mm">{body}</svg>'


def _between(p, a, b):
    """p лежит строго внутри отрезка a-b."""
    if p == a or p == b: return False
    if abs(a[0] - b[0]) < 1e-6: return abs(p[0] - a[0]) < 1e-6 and min(a[1], b[1]) < p[1] < max(a[1], b[1])
    return abs(p[1] - a[1]) < 1e-6 and min(a[0], b[0]) < p[0] < max(a[0], b[0])


def verify(sheets):
    """Сверка чертежа с нетлистом: каждый вывод изображён ровно один раз, связность совпадает."""
    parent = {}
    def find(a):
        parent.setdefault(a, a)
        while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
        return a
    def union(a, b): parent[find(a)] = find(b)
    errors, seen = [], {}
    for si, s in enumerate(sheets):
        pts = set()
        for a, b in s.wires: union((si, a), (si, b)); pts.add(a); pts.add(b)
        for p in s.pins: pts.add(p.xy())
        for (pt, name) in s.labels: union((si, pt), ("net", name)); pts.add(pt)
        for pt in pts:
            for a, b in s.wires:
                if _between(pt, a, b): union((si, pt), (si, a))
        for p in s.pins:
            key = (p.ref, p.num)
            if key in seen: errors.append(f"вывод {p.ref}.{p.num} изображён дважды")
            seen[key] = (si, p.xy())
    netof, rootnet = {}, {}
    for c in N.C:
        for k, net in c["pins"].items():
            num = k.partition(":")[0]; key = (c["ref"], num)
            if key not in seen: errors.append(f"вывод {c['ref']}.{num} ({net}) не изображён"); continue
            si, xy = seen[key]
            if net is None:
                if xy not in sheets[si].ncs and find((si, xy)) != (si, xy): errors.append(f"неподключаемый вывод {c['ref']}.{num} соединён с цепью")
                continue
            r = find((si, xy))
            if r in rootnet and rootnet[r] != net: errors.append(f"замыкание цепей {rootnet[r]} и {net} (вывод {c['ref']}.{num})")
            rootnet[r] = net
            netof.setdefault(net, set()).add(r)
    for net, roots in netof.items():
        if len(roots) > 1: errors.append(f"цепь {net} разорвана на {len(roots)} части")
    for si, s in enumerate(sheets):
        for (pt, name) in s.labels:
            r = find((si, pt))
            if r in rootnet and rootnet[r] != name: errors.append(f"обозначение цепи {name} стоит на цепи {rootnet[r]} (лист {si + 1})")
    return errors
