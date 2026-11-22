#!/usr/bin/env python3
"""Генерация из netlist.py: перечень элементов с ценами (docs/bom.md, out/bom.csv), таблица цепей (docs/netlist.md),
нетлист KiCad (out/pch1500.net). Чертежи по ЕСКД выпускает sch/docs.py, плату — pcb/make_pcb.py."""
import csv, os, collections
import netlist as N

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs")
os.makedirs(OUT, exist_ok=True); os.makedirs(DOCS, exist_ok=True)

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
        for r in rows: w.writerow(["Плата", *r])
        for ref, val, desc, q, p in N.OFFBOARD: w.writerow(["Прочее", ref, val, desc, q, p, q * p])
    md = ["# Перечень элементов и ориентировочная стоимость", "",
          "Файл создан автоматически (`hardware/gen.py`) из `hardware/netlist.py`. Цены — ориентировочные розничные оценки "
          "для российских маркетплейсов (Ozon, Wildberries, AliExpress) и магазинов «Чип и Дип», «Промэлектроника», «Платан»; "
          "перед закупкой их нужно уточнить. В «Чип и Дип» часть позиций заметно дороже указанного.", "",
          "## Плата PUFVFD.01", "", "| Поз. обозначение | Номинал / тип | Описание | Кол. | Цена, ₽ | Сумма, ₽ |", "|---|---|---|---:|---:|---:|"]
    md += [f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {r[4]} | {r[5]} |" for r in rows]
    md += [f"| | | **Итого по плате** | | | **{tot_a}** |", "", "## Изделия вне платы, конструктив, материалы", "",
           "| Поз. | Тип | Назначение | Кол. | Цена, ₽ | Сумма, ₽ |", "|---|---|---|---:|---:|---:|"]
    md += [f"| {ref} | {val} | {desc} | {q} | {p} | {q * p} |" for ref, val, desc, q, p in N.OFFBOARD]
    md += [f"| | | **Итого** | | | **{tot_b}** |", "", f"**Всего: ≈ {tot_a + tot_b} ₽** (включая опции: тормозной резистор, потенциометр, автомат).", ""]
    open(os.path.join(DOCS, "bom.md"), "w", encoding="utf-8").write("\n".join(md))
    nl = ["# Таблица цепей платы PUFVFD.01", "", "Создано автоматически из `hardware/netlist.py`. Формат: элемент.вывод.", "", "| Цепь | Подключённые выводы |", "|---|---|"]
    nl += [f"| `{net}` | {', '.join(r + '.' + p.partition(':')[0] for r, p in nodes)} |" for net, nodes in sorted(N.nets().items())]
    open(os.path.join(DOCS, "netlist.md"), "w", encoding="utf-8").write("\n".join(nl) + "\n")
    return tot_a, tot_b

if __name__ == "__main__":
    kicad_net(); a, b = bom()
    print("плата", a, "₽, прочее", b, "₽, всего", a + b, "₽")
