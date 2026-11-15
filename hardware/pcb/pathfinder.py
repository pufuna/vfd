"""Согласованная трассировка сигнальных цепей и цепей питания логики (алгоритм PathFinder).
Цепям разрешено временно проходить через области, занятые другими цепями, со штрафом; штраф растёт
с каждой итерацией, пока конфликты не исчезнут. Неснимаемые препятствия (площадки, силовые дорожки,
запретные зоны) берутся из снимка Router.fix. Состояние сохраняется в файл и подхватывается при повторном запуске."""
import heapq, math, os, pickle, time
import numpy as np
from router import G, MARGIN, VIA_COST

WIDTHS = (0.3, 0.6)


def clr(wa, wb):
    return 0.25 if (wa == 0.3 and wb == 0.3) else 0.3


class PF:
    def __init__(self, R, nets, signature, ckpt):
        self.R, self.nets, self.sig, self.ckpt = R, nets, signature, ckpt
        self.ny, self.nx = R.ny, R.nx
        self.U = {w: np.zeros((2, R.ny, R.nx), dtype=np.int16) for w in WIDTHS}
        self.H = np.zeros((2, R.ny, R.nx), dtype=np.float32)
        self.state, self.it = {}, 0
        self.halo = np.where(R.halo, 2.0, 1.0).astype(np.float32)
        if os.path.exists(ckpt):
            try:
                d = pickle.load(open(ckpt, "rb"))
                if d["sig"] == signature:
                    self.state, self.H, self.it = d["state"], d["H"], d["it"]
                    for net in nets:
                        if net["name"] in self.state: self.stamp(net, 1)
                    print(f"  продолжение с итерации {self.it}", flush=True)
            except Exception as e:
                print("  контрольная точка не прочитана:", e)

    def save(self):
        pickle.dump({"sig": self.sig, "state": self.state, "H": self.H, "it": self.it}, open(self.ckpt, "wb"))

    # ---- учёт занятости ----
    def stamp(self, net, sign):
        st = self.state[net["name"]]; wm = net["w"]; R = self.R
        for (ax, ay, bx, by, w, layer) in st["segs"]:
            dx, dy = bx - ax, by - ay; L2 = dx * dx + dy * dy
            for wr, U in self.U.items():
                r = w / 2 + clr(wm, wr) + wr / 2 + MARGIN
                i0, i1, j0, j1, X, Y = R._window(min(ax, bx), min(ay, by), max(ax, bx), max(ay, by), r)
                t = np.clip(((X - ax) * dx + (Y - ay) * dy) / L2, 0, 1) if L2 > 0 else 0
                d = np.hypot(X - (ax + t * dx), Y - (ay + t * dy))
                sub = U[layer, j0:j1, i0:i1]; sub[d <= r] += sign
        for (x, y, dia, drill) in st["vias"]:
            for wr, U in self.U.items():
                r = dia / 2 + clr(wm, wr) + wr / 2 + MARGIN
                i0, i1, j0, j1, X, Y = R._window(x, y, x, y, r)
                m = np.hypot(X - x, Y - y) <= r
                for l in (0, 1):
                    sub = U[l, j0:j1, i0:i1]; sub[m] += sign

    # ---- прокладка одной цепи ----
    def route_net(self, net, pf):
        R = self.R; name = net["name"]; n = R.netid[name]; c = R.cls[net["key"]]; nx = self.nx
        fix = R.fix[net["key"]]; ok = (fix == 0) | (fix == n)
        passb = [ok[0].tobytes(), ok[1].tobytes()]
        cost = (1.0 + self.H + pf * self.U[net["w"]]) * self.halo
        costl = [cost[0].ravel().tolist(), cost[1].ravel().tolist()]
        vr = max(0, int(math.ceil((c.via_d - c.w) / 2 / G - 0.01)))
        tht = R.tht.get(n, set())
        pads = net["pads"]
        cx = sum(p["x"] for p in pads) / len(pads); cy = sum(p["y"] for p in pads) / len(pads)
        pads = sorted(pads, key=lambda p: abs(p["x"] - cx) + abs(p["y"] - cy))
        cells = [R.pad_cells(p) for p in pads]
        tree = set(cells[0]); done = [True] + [False] * (len(pads) - 1)
        st = {"segs": [], "stubs": [], "vias": [], "cells": set(), "fail": []}
        padcell = R.padcell.get(n, {})
        while not all(done):
            conn = [pads[i] for i in range(len(pads)) if done[i]]
            cand = [i for i in range(len(pads)) if not done[i]]
            t = min(cand, key=lambda i: min(abs(pads[i]["x"] - p["x"]) + abs(pads[i]["y"] - p["y"]) for p in conn)); done[t] = True
            if any(cc in tree for cc in cells[t]): continue
            targets = {cc for cc in cells[t] if passb[cc[0]][cc[1] * nx + cc[2]]}
            path = self.astar(tree, targets, pads[t], passb, costl, vr, tht, net["bias"]) if targets else None
            if path is None: st["fail"].append(pads[t]["ref"]); continue
            self.commit(path, c, tht, pads[t], padcell, st, vr)
            tree.update(path); tree.update(cells[t])
        self.state[name] = st

    def astar(self, tree, targets, pad, passb, costl, vr, tht, bias):
        ny, nx = self.ny, self.nx
        tx, ty = pad["x"] / G, pad["y"] / G
        heap, best, prev = [], {}, {}
        for node in tree:
            l, j, i = node
            if passb[l][j * nx + i]:
                dx, dy = abs(i - tx), abs(j - ty)
                best[node] = 0.0; heapq.heappush(heap, (max(dx, dy) + 0.5 * min(dx, dy), 0.0, node))
        moves = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.5), (1, -1, 1.5), (-1, 1, 1.5), (-1, -1, 1.5))
        it = 0
        while heap:
            f, gc, node = heapq.heappop(heap)
            if gc > best.get(node, 1e18): continue
            if node in targets:
                path = [node]
                while node in prev: node = prev[node]; path.append(node)
                return path[::-1]
            it += 1
            if it > 600000: return None
            l, j, i = node; pb = passb[l]; cl = costl[l]; lb = bias[l]
            for di, dj, cost in moves:
                i2, j2 = i + di, j + dj
                if i2 < 0 or j2 < 0 or i2 >= nx or j2 >= ny: continue
                q = j2 * nx + i2
                if not pb[q]: continue
                if di and dj and not (pb[j * nx + i2] and pb[j2 * nx + i]): continue
                nn = (l, j2, i2); ng = gc + cost * lb * cl[q]
                if ng < best.get(nn, 1e18):
                    best[nn] = ng; prev[nn] = node
                    dx, dy = abs(i2 - tx), abs(j2 - ty)
                    heapq.heappush(heap, (ng + max(dx, dy) + 0.5 * min(dx, dy), ng, nn))
            l2 = 1 - l; po = passb[l2]; q0 = j * nx + i
            if po[q0]:
                free = (j, i) in tht
                extra = 0.0
                if not free:
                    if not (vr < j < ny - vr - 1 and vr < i < nx - vr - 1): continue
                    okv = True; co = costl[l2]
                    for dj in range(-vr, vr + 1):
                        for di in range(-vr, vr + 1):
                            q = (j + dj) * nx + i + di
                            if not (pb[q] and po[q]): okv = False; break
                            extra += cl[q] + co[q] - 2.0
                        if not okv: break
                    if not okv: continue
                nn = (l2, j, i); ng = gc + (0.0 if free else VIA_COST + 4.0 * extra)
                if ng < best.get(nn, 1e18):
                    best[nn] = ng; prev[nn] = node
                    dx, dy = abs(i - tx), abs(j - ty)
                    heapq.heappush(heap, (ng + max(dx, dy) + 0.5 * min(dx, dy), ng, nn))
        return None

    def commit(self, path, c, tht, pad, padcell, st, vr):
        def emit(a, b):
            if a[0] == b[0] and (a[1], a[2]) != (b[1], b[2]): st["segs"].append((a[2] * G, a[1] * G, b[2] * G, b[1] * G, c.w, a[0]))
        pts = [path[0]]
        for a, b, nxt in zip(path, path[1:], path[2:] + [None]):
            if a[0] != b[0]:
                if (a[1], a[2]) not in tht:
                    st["vias"].append((a[2] * G, a[1] * G, c.via_d, c.via_drill))
                    for dj in range(-vr, vr + 1):
                        for di in range(-vr, vr + 1):
                            st["cells"].add((0, a[1] + dj, a[2] + di)); st["cells"].add((1, a[1] + dj, a[2] + di))
                if pts[-1] != a: emit(pts[-1], a)
                pts.append(b); continue
            if nxt is None or nxt[0] != b[0] or (nxt[1] - b[1], nxt[2] - b[2]) != (b[1] - a[1], b[2] - a[2]):
                emit(pts[-1], b); pts.append(b)
        s0 = path[0]; sp = padcell.get(s0)
        if sp is not None and (abs(s0[2] * G - sp["x"]) > 1e-6 or abs(s0[1] * G - sp["y"]) > 1e-6):
            st["stubs"].append((s0[2] * G, s0[1] * G, sp["x"], sp["y"], min(c.w, sp["minsize"]), s0[0]))
        e = path[-1]
        if abs(e[2] * G - pad["x"]) > 1e-6 or abs(e[1] * G - pad["y"]) > 1e-6:
            st["stubs"].append((e[2] * G, e[1] * G, pad["x"], pad["y"], min(c.w, pad["minsize"]), e[0]))
        st["cells"].update(cc for cc in path if cc not in padcell)      # узлы внутри своих площадок конфликтом не считаются

    # ---- основной цикл ----
    def conflicts(self, bump):
        conf = []; extra = []
        for net in self.nets:
            st = self.state.get(net["name"])
            if not st or not st["cells"]: continue
            self.stamp(net, -1)
            U = self.U[net["w"]]
            bad = [cc for cc in st["cells"] if U[cc] > 0]
            if bad:
                conf.append(net)
                if bump:
                    for cc in bad: self.H[cc] += bump
                    # соседние цепи, чьи дорожки создают конфликт, тоже перекладываются
                    for other in self.nets:
                        if other is net or other in extra: continue
                        hit = False
                        for (ax, ay, bx, by, w, layer) in self.state.get(other["name"], {}).get("segs", []):
                            for (l, j, i) in bad[:30]:
                                if layer == l and min(ax, bx) - 1.0 <= i * G <= max(ax, bx) + 1.0 and min(ay, by) - 1.0 <= j * G <= max(ay, by) + 1.0: hit = True; break
                            if hit: break
                        if hit: extra.append(other)
            self.stamp(net, 1)
        self.extra = [n for n in extra if n not in conf]
        return conf

    def run(self, budget):
        t0 = time.time()
        todo = [n for n in self.nets if n["name"] not in self.state] if self.state else list(self.nets)
        if self.state and not todo: todo = self.conflicts(0.0)
        while True:
            pf = min(0.5 * 1.5 ** self.it, 3000.0)
            for net in todo:
                if net["name"] in self.state: self.stamp(net, -1)
                self.route_net(net, pf); self.stamp(net, 1)
                if time.time() - t0 > budget: break
            conf = self.conflicts(0.4)
            self.it += 1; self.save()
            nf = sum(len(s["fail"]) for s in self.state.values())
            print(f"  итерация {self.it}: штраф {pf:.1f}, цепей с конфликтами {len(conf)}, без пути {nf}, {time.time() - t0:.0f} с", flush=True)
            if not conf: return True
            if time.time() - t0 > budget: return False
            todo = conf + self.extra

    def drop_conflicts(self):
        """Снять цепи, оставшиеся в конфликте (по одной, начиная с самых коротких), чтобы результат был корректным."""
        dropped = []
        while True:
            conf = self.conflicts(0.0)
            if not conf: break
            for cn in conf:                                   # диагностика: где и с кем конфликт
                self.stamp(cn, -1); U = self.U[cn["w"]]
                bad = [cc for cc in self.state[cn["name"]]["cells"] if U[cc] > 0]
                self.stamp(cn, 1)
                near = set()
                for (l, j, i) in bad[:40]:
                    for other in self.nets:
                        if other is cn: continue
                        for (ax, ay, bx, by, w, layer) in self.state.get(other["name"], {}).get("segs", []):
                            if layer == l and min(ax, bx) - 1.2 <= i * G <= max(ax, bx) + 1.2 and min(ay, by) - 1.2 <= j * G <= max(ay, by) + 1.2: near.add(other["name"])
                xs = [c[2] * G for c in bad]; ys = [c[1] * G for c in bad]
                print(f"    конфликт {cn['name']}: {len(bad)} узлов, x {min(xs):.1f}..{max(xs):.1f}, y {min(ys):.1f}..{max(ys):.1f}, слой {sorted({c[0] for c in bad})}, рядом: {sorted(near)}")
            net = min(conf, key=lambda n: len(self.state[n["name"]]["cells"]))
            self.stamp(net, -1)
            self.state[net["name"]] = {"segs": [], "stubs": [], "vias": [], "cells": set(), "fail": [p["ref"] for p in net["pads"][1:]]}
            dropped.append(net["name"])
        return dropped
