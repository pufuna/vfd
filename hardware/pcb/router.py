"""Простой сеточный трассировщик для платы ПЧ-1500 (A* на сетке 0,25 мм, 2 слоя, 8 направлений).
Зазоры учитываются «раздуванием» чужой меди: для каждого класса трассировки (зазор цепи, группа, ширина)
ведётся своя карта занятости. Результат проверяется штатным DRC KiCad (см. make_pcb.py)."""
import heapq, math
import numpy as np

G = 0.254           # шаг сетки, мм (1/10 шага выводов 2,54 мм — дорожка проходит точно между выводами DIP)
MARGIN = 0.04       # запас на дискретизацию
VIA_COST = 60       # цена перехода в шагах сетки
EDGE_CLR = 0.5


class JobClass:
    def __init__(self, cl, grp, w, via_d, via_drill):
        self.cl, self.grp, self.w, self.via_d, self.via_drill = cl, grp, w, via_d, via_drill
        self.key = (cl, grp, w)


class Router:
    def __init__(self, W, H, classes):
        self.nx, self.ny = int(round(W / G)) + 1, int(round(H / G)) + 1
        self.cls = {c.key: c for c in classes}
        self.grid = {k: np.zeros((2, self.ny, self.nx), dtype=np.int16) for k in self.cls}
        self.netid, self.netinfo = {}, {}          # имя -> номер; номер -> (зазор, группа)
        self.tree = {}                             # номер цепи -> множество узлов (l, j, i), уже соединённых
        self.tht = {}                              # номер цепи -> множество (j, i) внутри своих сквозных площадок
        self.segs, self.vias, self.fail = [], [], []
        self.padcell = {}                          # номер цепи -> {узел: площадка}
        self.halo = np.zeros((2, self.ny, self.nx), dtype=bool)   # окрестности площадок: проход дороже (чтобы не «запирать» выводы)

    def net(self, name, cl, grp=None):
        if name not in self.netid:
            n = len(self.netid) + 1; self.netid[name] = n; self.netinfo[n] = (cl, grp)
        return self.netid[name]

    def pair(self, k, n):
        """Требуемый зазор между классом трассировки k и чужой цепью n (n = -1 — край платы, отверстие)."""
        if n < 0: return EDGE_CLR
        cl, grp = self.netinfo[n]
        if grp is not None and grp == k.grp: return 0.3
        return max(cl, k.cl)

    def _window(self, x0, y0, x1, y1, r):
        i0, i1 = max(0, int(math.floor((x0 - r) / G))), min(self.nx, int(math.ceil((x1 + r) / G)) + 1)
        j0, j1 = max(0, int(math.floor((y0 - r) / G))), min(self.ny, int(math.ceil((y1 + r) / G)) + 1)
        X, Y = np.meshgrid(np.arange(i0, i1) * G, np.arange(j0, j1) * G)
        return i0, i1, j0, j1, X, Y

    def _apply(self, g, mask, n):
        if n < 0: g[mask] = -1
        else:
            conflict = mask & (g != 0) & (g != n)
            g[mask & (g == 0)] = n
            g[conflict] = -1

    def stamp_rect(self, x0, y0, x1, y1, rad, n, layers, extra=None):
        """Прямоугольник (при x0==x1, y0==y1 — круг радиуса rad) цепи n."""
        for k, c in self.cls.items():
            r = rad + (self.pair(c, n) if extra is None else extra) + c.w / 2 + MARGIN
            i0, i1, j0, j1, X, Y = self._window(x0, y0, x1, y1, r)
            d = np.hypot(np.maximum(np.maximum(x0 - X, X - x1), 0), np.maximum(np.maximum(y0 - Y, Y - y1), 0))
            for l in layers: self._apply(self.grid[k][l, j0:j1, i0:i1], d <= r, n)

    def stamp_seg(self, ax, ay, bx, by, hw, n, layer):
        dx, dy = bx - ax, by - ay; L2 = dx * dx + dy * dy
        for k, c in self.cls.items():
            r = hw + self.pair(c, n) + c.w / 2 + MARGIN
            i0, i1, j0, j1, X, Y = self._window(min(ax, bx), min(ay, by), max(ax, bx), max(ay, by), r)
            t = np.clip(((X - ax) * dx + (Y - ay) * dy) / L2, 0, 1) if L2 > 0 else 0
            d = np.hypot(X - (ax + t * dx), Y - (ay + t * dy))
            self._apply(self.grid[k][layer, j0:j1, i0:i1], d <= r, n)

    def pad_cells(self, pad):
        """Узлы сетки внутри площадки."""
        x0, y0, x1, y1 = pad["rect"]
        out = set()
        for j in range(int(math.ceil((y0 + 0.05) / G)), int(math.floor((y1 - 0.05) / G)) + 1):
            for i in range(int(math.ceil((x0 + 0.05) / G)), int(math.floor((x1 - 0.05) / G)) + 1):
                if pad.get("round") and math.hypot(i * G - pad["x"], j * G - pad["y"]) > pad["round"] - 0.05: continue
                if 0 <= j < self.ny and 0 <= i < self.nx:
                    for l in pad["layers"]: out.add((l, j, i))
        return out

    def add_pad(self, pad, n):
        x0, y0, x1, y1 = pad["rect"]
        if pad.get("round"): self.stamp_rect(pad["x"], pad["y"], pad["x"], pad["y"], pad["round"], n, pad["layers"])
        else: self.stamp_rect(x0, y0, x1, y1, 0, n, pad["layers"])
        i0, i1, j0, j1, X, Y = self._window(x0, y0, x1, y1, 1.1)
        d = np.hypot(np.maximum(np.maximum(x0 - X, X - x1), 0), np.maximum(np.maximum(y0 - Y, Y - y1), 0))
        for l in pad["layers"]: self.halo[l, j0:j1, i0:i1] |= (d <= 1.1)
        if n > 0:
            pc = self.padcell.setdefault(n, {})
            for cc in self.pad_cells(pad): pc[cc] = pad
        if len(pad["layers"]) == 2 and n > 0:
            self.tht.setdefault(n, set()).update((j, i) for (_, j, i) in self.pad_cells(pad))

    def add_track(self, ax, ay, bx, by, w, n, layer):
        self.segs.append((ax, ay, bx, by, w, n, layer)); self.stamp_seg(ax, ay, bx, by, w / 2, n, layer)

    # ------------------------------------------------------------------
    def route(self, name, key, pads, label="", bias=(1.0, 1.0)):
        """Соединить площадки pads цепи name дорожкой класса key. Возвращает число несоединённых площадок."""
        n = self.netid[name]; c = self.cls[key]; g = self.grid[key]
        ny, nx = self.ny, self.nx
        ok = ((g == 0) | (g == n))
        passb = [ok[0].tobytes(), ok[1].tobytes()]
        self._halob = [self.halo[0].tobytes(), self.halo[1].tobytes()]
        vr = max(0, int(math.ceil((c.via_d - c.w) / 2 / G - 0.01)))
        tht = self.tht.get(n, set())
        tree = self.tree.setdefault(n, set())
        if not tree and len(pads) > 2:                 # начинать с площадки, ближайшей к центру масс цепи
            cx = sum(p["x"] for p in pads) / len(pads); cy = sum(p["y"] for p in pads) / len(pads)
            pads = sorted(pads, key=lambda p: abs(p["x"] - cx) + abs(p["y"] - cy))
        cells = [self.pad_cells(p) for p in pads]
        done = [bool(tree) and any(cc in tree for cc in cells[i]) for i in range(len(pads))]
        if not tree:
            done[0] = True; tree.update(cells[0]); self._stub_all(cells[0], pads[0], c, n)
        nfail = 0
        while not all(done):
            # ближайшая к дереву несоединённая площадка
            cand = [i for i in range(len(pads)) if not done[i]]
            conn = [pads[i] for i in range(len(pads)) if done[i]] or None
            def dist(i):
                if conn: return min(abs(pads[i]["x"] - p["x"]) + abs(pads[i]["y"] - p["y"]) for p in conn)
                return 0
            t = min(cand, key=dist); done[t] = True
            targets = {cc for cc in cells[t] if passb[cc[0]][cc[1] * nx + cc[2]]}
            path = self._astar(tree, targets, pads[t], passb, vr, tht, bias) if targets else None
            if path is None and len(tree) == len(cells[0]) and self.last_it < 3000 and len(pads) > 2 and not hasattr(self, "_retry"):
                # стартовая площадка «заперта»: начать с другой площадки, запертую присоединить позже
                self._retry = True; self.tree[n] = set()
                r = self.route(name, key, pads[1:] + pads[:1], label, bias); del self._retry
                return r
            if path is None:
                nsrc = sum(1 for (l, j, i) in tree if passb[l][j * nx + i])
                nfail += 1; self.fail.append((name, pads[t]["ref"], label, f"src={nsrc}/{len(tree)} tgt={len(targets)}/{len(cells[t])} it={self.last_it}")); continue
            self._commit(path, c, n, tht, pads[t], tree)
            tree.update(cells[t])
        return nfail

    def _stub_all(self, cells, pad, c, n):
        pass

    def _astar(self, tree, targets, pad, passb, vr, tht, bias=(1.0, 1.0)):
        ny, nx = self.ny, self.nx
        tx, ty = pad["x"] / G, pad["y"] / G
        def h(j, i):
            dx, dy = abs(i - tx), abs(j - ty)
            return 1.15 * (max(dx, dy) + 0.5 * min(dx, dy))
        heap, best, prev = [], {}, {}
        for node in tree:
            l, j, i = node
            if passb[l][j * nx + i]:
                best[node] = 0.0; heapq.heappush(heap, (h(j, i), 0.0, node))
        moves = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.5), (1, -1, 1.5), (-1, 1, 1.5), (-1, -1, 1.5))
        limit = 500000; it = 0; self.last_it = 0
        while heap:
            f, gc, node = heapq.heappop(heap)
            if gc > best.get(node, 1e18): continue
            if node in targets:
                path = [node]
                while node in prev: node = prev[node]; path.append(node)
                return path[::-1]
            it += 1; self.last_it = it
            if it > limit: return None
            l, j, i = node; pb = passb[l]; lb = bias[l]; hb = self._halob[l]
            for di, dj, cost in moves:
                i2, j2 = i + di, j + dj
                if i2 < 0 or j2 < 0 or i2 >= nx or j2 >= ny or not pb[j2 * nx + i2]: continue
                if di and dj and not (pb[j * nx + i2] and pb[j2 * nx + i]): continue
                nn = (l, j2, i2); ng = gc + cost * lb * (3.0 if hb[j2 * nx + i2] else 1.0)
                if ng < best.get(nn, 1e18):
                    best[nn] = ng; prev[nn] = node; heapq.heappush(heap, (ng + h(j2, i2), ng, nn))
            # смена слоя: бесплатно в своей сквозной площадке, иначе переходное отверстие
            l2 = 1 - l; po = passb[l2]
            if po[j * nx + i]:
                free = (j, i) in tht
                if not free:
                    okv = vr < j < ny - vr - 1 and vr < i < nx - vr - 1
                    if okv:
                        for dj in range(-vr, vr + 1):
                            for di in range(-vr, vr + 1):
                                q = (j + dj) * nx + i + di
                                if not (pb[q] and po[q]): okv = False; break
                            if not okv: break
                    if not okv: continue
                nn = (l2, j, i); ng = gc + (0.0 if free else VIA_COST)
                if ng < best.get(nn, 1e18):
                    best[nn] = ng; prev[nn] = node; heapq.heappush(heap, (ng + h(j, i), ng, nn))
        return None

    def _commit(self, path, c, n, tht, pad, tree):
        # разбиение пути на прямые отрезки
        pts = [path[0]]
        for a, b, nxt in zip(path, path[1:], path[2:] + [None]):
            if a[0] != b[0]:
                if (a[1], a[2]) not in tht: self.vias.append((a[2] * G, a[1] * G, c.via_d, c.via_drill, n)); self._stamp_via(a, c, n)
                if pts[-1] != a: self._emit(pts[-1], a, c, n)
                pts.append(b); continue
            if nxt is None or nxt[0] != b[0] or (nxt[1] - b[1], nxt[2] - b[2]) != (b[1] - a[1], b[2] - a[2]):
                self._emit(pts[-1], b, c, n); pts.append(b)
        # короткие отводы к центрам площадок на концах пути
        st = path[0]; sp = self.padcell.get(n, {}).get(st)
        if sp is not None and (abs(st[2] * G - sp["x"]) > 1e-6 or abs(st[1] * G - sp["y"]) > 1e-6):
            self.add_track(st[2] * G, st[1] * G, sp["x"], sp["y"], min(c.w, sp["minsize"]), n, st[0])
        end = path[-1]
        if abs(end[2] * G - pad["x"]) > 1e-6 or abs(end[1] * G - pad["y"]) > 1e-6:
            self.add_track(end[2] * G, end[1] * G, pad["x"], pad["y"], min(c.w, pad["minsize"]), n, end[0])
        tree.update(path)

    def _emit(self, a, b, c, n):
        if a[0] == b[0] and (a[1], a[2]) != (b[1], b[2]):
            self.add_track(a[2] * G, a[1] * G, b[2] * G, b[1] * G, c.w, n, a[0])

    def _stamp_via(self, node, c, n):
        x, y = node[2] * G, node[1] * G
        self.stamp_rect(x, y, x, y, c.via_d / 2, n, (0, 1))

    def connect_pad_center(self, pad, name, key):
        """Отвод от центра первой площадки цепи до ближайшего узла сетки (чтобы дерево начиналось в центре)."""
        n = self.netid[name]; c = self.cls[key]
        i, j = int(round(pad["x"] / G)), int(round(pad["y"] / G))
        for l in pad["layers"]:
            if abs(i * G - pad["x"]) > 1e-6 or abs(j * G - pad["y"]) > 1e-6:
                self.add_track(pad["x"], pad["y"], i * G, j * G, min(c.w, pad["minsize"]), n, l)


# ---------------- снятие и повторная прокладка мешающих цепей (rip-up) ----------------
def _snapshot(self):
    """Запомнить состояние после прокладки неснимаемых (силовых) цепей."""
    self.fix = {k: g.copy() for k, g in self.grid.items()}
    self.fix_nseg, self.fix_nvia = len(self.segs), len(self.vias)
    self.fix_tree = {n: set(t) for n, t in self.tree.items()}


def _rip(self, nets):
    """Снять все дорожки указанных цепей, проложенные после snapshot, и восстановить карты занятости."""
    nets = set(nets)
    keep_s = self.segs[:self.fix_nseg] + [sg for sg in self.segs[self.fix_nseg:] if sg[5] not in nets]
    keep_v = self.vias[:self.fix_nvia] + [v for v in self.vias[self.fix_nvia:] if v[4] not in nets]
    for k in self.grid: self.grid[k][...] = self.fix[k]
    redo_s, redo_v = keep_s[self.fix_nseg:], keep_v[self.fix_nvia:]
    self.segs, self.vias = keep_s[:self.fix_nseg], keep_v[:self.fix_nvia]
    for (ax, ay, bx, by, w, n, layer) in redo_s: self.add_track(ax, ay, bx, by, w, n, layer)
    for (x, y, d, dr, n) in redo_v:
        self.vias.append((x, y, d, dr, n)); self.stamp_rect(x, y, x, y, d / 2, n, (0, 1))
    for n in nets: self.tree[n] = set(self.fix_tree.get(n, set()))
    self.fail = [f for f in self.fail if self.netid[f[0]] not in nets]


def _probe(self, name, key, pad, bias=(1.0, 1.0)):
    """Путь от дерева цепи до площадки с учётом только неснимаемых препятствий."""
    n = self.netid[name]; c = self.cls[key]; g = self.fix[key]; nx = self.nx
    ok = ((g == 0) | (g == n)); passb = [ok[0].tobytes(), ok[1].tobytes()]
    self._halob = [self.halo[0].tobytes(), self.halo[1].tobytes()]
    vr = max(0, int(math.ceil((c.via_d - c.w) / 2 / G - 0.01)))
    tree = self.tree.get(n) or self.pad_cells(pad)
    targets = {cc for cc in self.pad_cells(pad) if passb[cc[0]][cc[1] * nx + cc[2]]}
    return self._astar(tree, targets, pad, passb, vr, self.tht.get(n, set()), bias) if targets else None


def _blockers(self, path, name, key):
    """Цепи, дорожки которых мешают пройти по пути path."""
    n = self.netid[name]; c = self.cls[key]; g = self.grid[key]; out = set()
    pts = [(l, j, i) for (l, j, i) in path if g[l, j, i] not in (0, n)]
    vr = max(1, int(math.ceil((c.via_d - c.w) / 2 / G - 0.01)))
    for a, b in zip(path, path[1:]):                      # окрестности переходных отверстий
        if a[0] != b[0]:
            for dj in range(-vr, vr + 1):
                for di in range(-vr, vr + 1):
                    for l in (0, 1):
                        if g[l, a[1] + dj, a[2] + di] not in (0, n): pts.append((l, a[1] + dj, a[2] + di))
    for (l, j, i) in pts:
        v = int(g[l, j, i])
        if v > 0: out.add(v); continue
        x, y = i * G, j * G
        for (ax, ay, bx, by, w, m, layer) in self.segs[self.fix_nseg:]:
            if layer != l or m == n: continue
            r = w / 2 + self.pair(c, m) + c.w / 2 + MARGIN
            if x < min(ax, bx) - r or x > max(ax, bx) + r or y < min(ay, by) - r or y > max(ay, by) + r: continue
            dx, dy = bx - ax, by - ay; L2 = dx * dx + dy * dy
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2)) if L2 > 0 else 0.0
            if math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= r: out.add(m)
        for (vx, vy, d, dr, m) in self.vias[self.fix_nvia:]:
            if m != n and math.hypot(x - vx, y - vy) <= d / 2 + self.pair(c, m) + c.w / 2 + MARGIN: out.add(m)
    return out


Router.snapshot, Router.rip, Router.probe, Router.blockers = _snapshot, _rip, _probe, _blockers
