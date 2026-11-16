#!/usr/bin/env python3
"""Сборка папки pdf/: схемы ЕСКД, чертежи слоёв платы, карта сверловки, документация (из Markdown).
Запуск: python3 tools/make_pdf.py   (нужны kicad-cli, node + playwright, пакет markdown)"""
import os, shutil, subprocess, markdown

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PDF = os.path.join(ROOT, "pdf"); os.makedirs(PDF, exist_ok=True)
BOARD = os.path.join(ROOT, "hardware/pcb/pch1500_a1.kicad_pcb")

shutil.copy(os.path.join(ROOT, "hardware/out/pch1500_eskd.pdf"), os.path.join(PDF, "01_schematics_eskd.pdf"))
for n, (name, layers) in enumerate((("pcb_assembly", "F.SilkS,F.Fab,Edge.Cuts"), ("pcb_top_copper", "F.Cu,Edge.Cuts"), ("pcb_bottom_copper", "B.Cu,Edge.Cuts")), 2):
    subprocess.run(["kicad-cli", "pcb", "export", "pdf", "-o", os.path.join(PDF, f"0{n}_{name}.pdf"), "--layers", layers, "--black-and-white", BOARD],
                   check=True, capture_output=True)
shutil.copy(os.path.join(ROOT, "hardware/pcb/out/gerber/pch1500_a1-drl_map.pdf"), os.path.join(PDF, "05_pcb_drill_map.pdf"))

CSS = """@page{size:A4;margin:16mm 14mm}body{font:10.5pt/1.45 'DejaVu Sans',Arial,sans-serif;color:#111}
h1{font-size:18pt;border-bottom:1.5px solid #333;padding-bottom:4px}h2{font-size:13.5pt;margin-top:18px}h3{font-size:11.5pt}
table{border-collapse:collapse;width:100%;font-size:8.8pt;margin:8px 0}td,th{border:1px solid #888;padding:3px 5px;vertical-align:top}
th{background:#eee}code{font-family:'DejaVu Sans Mono',monospace;font-size:9pt;background:#f2f2f2;padding:0 2px}
pre{background:#f2f2f2;padding:8px;font-size:8.5pt;white-space:pre-wrap}pre code{background:none}blockquote{border-left:3px solid #999;margin:8px 0;padding:2px 10px;color:#333}
tr{page-break-inside:avoid}"""
DOCS = [("06_description", "docs/01_description.md"), ("07_assembly_and_commissioning", "docs/02_assembly.md"), ("08_user_manual", "docs/03_user_manual.md"),
        ("09_modbus", "docs/04_modbus.md"), ("10_parameters", "docs/parameters.md"), ("11_bom", "docs/bom.md"), ("12_netlist", "docs/netlist.md")]
tmp = os.path.join(PDF, "_tmp"); os.makedirs(tmp, exist_ok=True); args = []
for name, src in DOCS:
    html = markdown.markdown(open(os.path.join(ROOT, src), encoding="utf-8").read(), extensions=["tables", "fenced_code"])
    h = os.path.join(tmp, name + ".html")
    open(h, "w", encoding="utf-8").write(f'<!doctype html><meta charset="utf-8"><style>{CSS}</style>{html}')
    args += [h, os.path.join(PDF, name + ".pdf")]
js = os.path.join(tmp, "p.js")
open(js, "w").write("""const { chromium } = require('playwright');
(async () => { const a = process.argv.slice(2); const b = await chromium.launch();
  for (let i = 0; i < a.length; i += 2) { const p = await b.newPage(); await p.goto('file://' + a[i]);
    await p.pdf({ path: a[i + 1], preferCSSPageSize: true, printBackground: true,
      displayHeaderFooter: true, headerTemplate: '<span></span>',
      footerTemplate: '<div style="font-size:8px;width:100%;text-align:center;color:#666">ПЧ-1500 — стр. <span class="pageNumber"></span> из <span class="totalPages"></span></div>' }); }
  await b.close(); })();""")
subprocess.run(["node", js] + args, check=True)
shutil.rmtree(tmp)
print("\n".join(sorted(os.listdir(PDF))))
