#!/usr/bin/env python3
"""Сборка руководства по эксплуатации PUFVFD.
Источники: tools/manual/manual_template.md (текст), tools/manual/params_desc.py (описания параметров и аварий),
таблица параметров прошивки (firmware/sim/dump_params json), docs/04_modbus.md (протокол).
Результат: docs/03_user_manual.md и pdf/08_user_manual.pdf.  Запуск: python3 tools/make_manual.py"""
import html, json, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "manual"))
import params_desc as PD

UNITS = {"Hz": "Гц", "kHz": "кГц", "s": "с", "ms": "мс", "V": "В", "A": "А", "rpm": "об/мин", "Ohm": "Ом", "mH": "мГн", "C": "°C", "%": "%", "": ""}


def load_params():
    subprocess.run(["make", "-s", "-C", os.path.join(ROOT, "firmware/sim"), "dump_params"], check=True)
    out = subprocess.run([os.path.join(ROOT, "firmware/sim/dump_params"), "json"], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def fmt(v, dec):
    if not dec: return str(v)
    s = f"{abs(v) / 10 ** dec:.{dec}f}".replace(".", ",")
    return ("-" if v < 0 else "") + s


def value_text(p, v):
    if p["opts"]: return p["opts"][v - p["min"]]
    u = UNITS.get(p["unit"], p["unit"])
    return f"{fmt(v, p['dec'])} {u}".strip()


def range_text(p):
    if p["opts"]: return "; ".join(f"{i + p['min']} — {o}" for i, o in enumerate(p["opts"]))
    u = UNITS.get(p["unit"], p["unit"])
    return f"{fmt(p['min'], p['dec'])} … {fmt(p['max'], p['dec'])} {u}".strip()


def scale_text(p):
    return "×1" if not p["dec"] else f"×{10 ** p['dec']}"


def param_groups(data):
    P = {p["n"]: p for p in data["params"]}
    used = [n for _, lst in PD.GROUPS for n in lst]
    assert sorted(used) == sorted(P), f"параметры без группы или повторы: {sorted(set(P) ^ set(used))}"
    missing = [n for n in P if n not in PD.D]
    assert not missing, f"нет описания для P{missing}"
    out = []
    for gi, (title, lst) in enumerate(PD.GROUPS, 1):
        out.append(f"#### 2.5.{gi} {title}\n")
        for n in lst:
            p = P[n]
            out.append(f"##### P{n:02d} {p['ru']}\n")
            change = "только при останове" if p["stop"] else "в любое время"
            if p["cmd"]: change += "; команда, не сохраняется"
            out.append("| Диапазон | По умолчанию | Регистр Modbus | Изменение |\n|---|---|---|---|")
            reg = f"{n} (0x{n:04X}), {scale_text(p)}" if not p["opts"] else f"{n} (0x{n:04X})"
            out.append(f"| {range_text(p)} | {value_text(p, p['def'])} | {reg} | {change} |\n")
            out.append(PD.D[n].strip().replace("**", "") + "\n")
    return "\n".join(out)


def faults(data):
    rows = ["| Код | Сообщение на дисплее | Причина | Что делать |", "|---|---|---|---|"]
    for i, text in enumerate(data["faults"], 1):
        cause, action = PD.FAULTS[i]
        rows.append(f"| E{i:02d} | {text} | {cause} | {action} |")
    return "\n".join(rows)


def param_table(data):
    rows = ["| № | Название на дисплее | Диапазон | По умолч. | Стоп |", "|---|---|---|---|---|"]
    for p in data["params"]:
        rows.append(f"| P{p['n']:02d} | {p['ru']} | {range_text(p)} | {value_text(p, p['def'])} | {'да' if p['stop'] else ''} |")
    return "\n".join(rows)


def settings_sheet(data):
    rows = ["| № | Параметр | По умолч. | Значение на объекте | Примечание |", "|---|---|---|---|---|"]
    for p in data["params"]:
        if p["cmd"]: continue
        rows.append(f"| P{p['n']:02d} | {p['ru']} | {value_text(p, p['def'])} | | |")
    return "\n".join(rows)


def include(path):
    s = open(os.path.join(ROOT, path), encoding="utf-8").read()
    s = re.sub(r"^# .*\n", "", s, count=1)                       # заголовок документа не нужен
    s = s.replace("**", "")
    s = s.replace("(описание параметров — в руководстве по эксплуатации, подраздел 2.5; сводная таблица — `parameters.md`)", "(см. 2.5 и приложение А)")
    s = re.sub(r"^(#{2,4}) ", lambda m: "##" + m.group(1) + " ", s, flags=re.M)
    n = [0]
    def num(m):
        n[0] += 1; return f"#### 2.7.{n[0]} {m.group(1)}"
    return re.sub(r"^#### (.*)$", num, s, flags=re.M).strip()


def build_md(data):
    t = open(os.path.join(ROOT, "tools/manual/manual_template.md"), encoding="utf-8").read()
    t = t.replace("{{VERSION}}", f"{data['version'] // 100}.{data['version'] % 100:02d}")
    t = t.replace("{{PARAM_GROUPS}}", param_groups(data)).replace("{{FAULTS}}", faults(data))
    t = t.replace("{{PARAM_TABLE}}", param_table(data)).replace("{{SETTINGS_SHEET}}", settings_sheet(data))
    t = re.sub(r"\{\{INCLUDE:([^}]+)\}\}", lambda m: include(m.group(1)), t)
    note = "<!-- Файл создаётся автоматически: python3 tools/make_manual.py. Правьте tools/manual/*. -->\n"
    return note + t


CSS = """
@page { size: A4; margin: 20mm 15mm 20mm 25mm; }
body { font: 11pt/1.4 'Liberation Serif', 'Times New Roman', serif; color: #000; text-align: justify; hyphens: auto; }
h1 { display: none; }
h2 { font-size: 13pt; font-weight: bold; margin: 18px 0 8px; page-break-after: avoid; text-align: left; }
h2.np { page-break-before: always; margin-top: 0; }
h3 { font-size: 12pt; font-weight: bold; margin: 14px 0 6px; page-break-after: avoid; text-align: left; }
h4 { font-size: 11pt; font-weight: bold; margin: 12px 0 5px; page-break-after: avoid; text-align: left; }
h5 { font-size: 11pt; font-weight: bold; margin: 12px 0 3px; page-break-after: avoid; text-align: left; }
p { margin: 0 0 6px; text-indent: 12.5mm; orphans: 3; widows: 3; }
li p, td p, .cover p, .toc p { text-indent: 0; }
ul, ol { margin: 0 0 6px; padding-left: 12.5mm; }
li { margin: 0 0 2px; }
table { border-collapse: collapse; width: 100%; font-size: 10pt; margin: 4px 0 10px; text-align: left; }
td, th { border: 0.6pt solid #000; padding: 2px 5px; vertical-align: top; }
th { font-weight: normal; text-align: center; }
tr { page-break-inside: avoid; }
code { font-family: 'Liberation Mono', monospace; font-size: 9.5pt; }
pre { font-size: 9pt; line-height: 1.2; margin: 4px 0 8px 12.5mm; page-break-inside: avoid; }
em { font-style: normal; }
img { max-width: 100%; display: block; margin: 6px auto 2px; }
.cover { height: 247mm; display: flex; flex-direction: column; justify-content: space-between; text-align: center; page-break-after: always; }
.cover .c1 { margin-top: 70mm; font-size: 16pt; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; }
.cover .c2 { margin-top: 8mm; font-size: 14pt; }
.cover .c3 { margin-top: 6mm; font-size: 14pt; }
.cover .c4 { margin-top: 18mm; font-size: 12pt; }
.cover .c5 { font-size: 12pt; }
table.pt { table-layout: fixed; }
table.pt th:nth-child(1) { width: 44%; }
table.pt th:nth-child(2) { width: 17%; }
table.pt th:nth-child(3) { width: 19%; }
.toc { page-break-after: always; }
.toc h2 { text-align: center; }
.toc ul { list-style: none; padding-left: 0; margin: 0; }
.toc ul ul { padding-left: 7mm; }
.toc li { margin: 1px 0; }
.toc a { color: #000; text-decoration: none; }
"""


def build_pdf(md_text, out_pdf):
    import markdown
    body = re.sub(r"^<!--.*?-->\n", "", md_text, flags=re.S)
    body = re.sub(r"^# .*\n", "", body, count=1)
    m = markdown.Markdown(extensions=["tables", "fenced_code", "toc"], extension_configs={"toc": {"toc_depth": "2-4"}})
    body = body.split("\n## ", 1)[1]                    # строка-аннотация перед «Введением» в PDF не нужна (есть титульный лист)
    h = m.convert("## " + body)
    h = re.sub(r'<h2 id="([^"]+)">', lambda mm: f'<h2 class="np" id="{mm.group(1)}">', h)      # каждый раздел — с новой страницы
    h = h.replace('<table>\n<thead>\n<tr>\n<th>Диапазон</th>', '<table class="pt">\n<thead>\n<tr>\n<th>Диапазон</th>')
    h = re.sub(r'<p>(Таблица [^<]+|Рисунок [^<]+)</p>', r'<p style="text-indent:0;margin:8px 0 2px">\1</p>', h)
    h = h.replace('src="../', 'src="file://' + ROOT + '/')
    ver = re.search(r"Версия программного обеспечения ([\d.]+)", md_text).group(1)
    cover = f"""<div class="cover"><div><div class="c1">Преобразователь частоты PUFVFD</div>
<div class="c2">Руководство по эксплуатации</div><div class="c3">PUFVFD.00.000 РЭ</div>
<div class="c4">Версия программного обеспечения {ver}</div></div><div class="c5">2026</div></div>"""
    toc = f'<div class="toc"><h2>Содержание</h2>{m.toc}</div>'
    page = f'<!doctype html><html lang="ru"><head><meta charset="utf-8"><style>{CSS}</style></head><body>{cover}{toc}{h}</body></html>'
    tmp = os.path.join(ROOT, "pdf", "_manual.html")
    open(tmp, "w", encoding="utf-8").write(page)
    js = os.path.join(ROOT, "pdf", "_manual.js")
    open(js, "w").write("""const { chromium } = require('playwright');
(async () => { const [src, dst] = process.argv.slice(2); const b = await chromium.launch(); const p = await b.newPage();
  await p.goto('file://' + src, { waitUntil: 'load' });
  await p.pdf({ path: dst, preferCSSPageSize: true, printBackground: true, displayHeaderFooter: true,
    headerTemplate: '<div style="font-family:Liberation Serif,serif;font-size:9px;width:100%;padding:0 15mm 0 25mm;text-align:right">PUFVFD.00.000 РЭ</div>',
    footerTemplate: '<div style="font-family:Liberation Serif,serif;font-size:9px;width:100%;padding-left:10mm;text-align:center"><span class="pageNumber"></span></div>' });
  await b.close(); })();""")
    subprocess.run(["node", js, tmp, out_pdf], check=True)
    os.remove(tmp); os.remove(js)


def main():
    data = load_params()
    md_text = build_md(data)
    open(os.path.join(ROOT, "docs/03_user_manual.md"), "w", encoding="utf-8").write(md_text)
    os.makedirs(os.path.join(ROOT, "pdf"), exist_ok=True)
    out = os.path.join(ROOT, "pdf/08_user_manual.pdf")
    build_pdf(md_text, out)
    pages = subprocess.run(["pdfinfo", out], capture_output=True, text=True).stdout
    print("docs/03_user_manual.md,", out, re.search(r"Pages:\s+(\d+)", pages).group(1) + " стр.")


if __name__ == "__main__":
    main()
