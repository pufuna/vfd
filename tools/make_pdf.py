#!/usr/bin/env python3
"""Сборка папки pdf/: схемы ЕСКД, чертежи слоёв платы, карта сверловки, документация (из Markdown).
Запуск: python3 tools/make_pdf.py   (нужны kicad-cli, node + playwright, пакет markdown)"""
import os, shutil, subprocess, sys, markdown

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PDF = os.path.join(ROOT, "pdf"); os.makedirs(PDF, exist_ok=True)
BOARD = os.path.join(ROOT, "hardware/pcb/pch1500_a1.kicad_pcb")

shutil.copy(os.path.join(ROOT, "hardware/out/pch1500_eskd.pdf"), os.path.join(PDF, "01_schematics_eskd.pdf"))
for n, (name, layers) in enumerate((("pcb_assembly", "F.SilkS,F.Fab,Edge.Cuts"), ("pcb_top_copper", "F.Cu,Edge.Cuts"), ("pcb_bottom_copper", "B.Cu,Edge.Cuts")), 2):
    subprocess.run(["kicad-cli", "pcb", "export", "pdf", "-o", os.path.join(PDF, f"0{n}_{name}.pdf"), "--layers", layers, "--black-and-white", BOARD],
                   check=True, capture_output=True)
shutil.copy(os.path.join(ROOT, "hardware/pcb/out/gerber/pch1500_a1-drl_map.pdf"), os.path.join(PDF, "05_pcb_drill_map.pdf"))

CSS = """@page{size:A4;margin:20mm 15mm 20mm 25mm}
body{font:11pt/1.4 'Liberation Serif','Times New Roman',serif;color:#000;text-align:justify;hyphens:auto}
h1{font-size:14pt;font-weight:bold;text-align:center;margin:0 0 14px}h2{font-size:13pt;margin:16px 0 6px;text-align:left}
h3{font-size:12pt;margin:12px 0 5px;text-align:left}h4{font-size:11pt;margin:10px 0 4px;text-align:left}
p{margin:0 0 6px;text-indent:12.5mm}li p,td p{text-indent:0}ul,ol{padding-left:12.5mm;margin:0 0 6px}
table{border-collapse:collapse;width:100%;font-size:10pt;margin:4px 0 10px;text-align:left}
td,th{border:0.6pt solid #000;padding:2px 5px;vertical-align:top}th{font-weight:normal;text-align:center}tr{page-break-inside:avoid}
code{font-family:'Liberation Mono',monospace;font-size:9.5pt}pre{font-size:9pt;margin:4px 0 8px 12.5mm;white-space:pre-wrap}
blockquote{margin:6px 0;padding:0}strong,b{font-weight:normal}a{color:#000;text-decoration:none}"""
DOCS = [("06_description", "docs/01_description.md"), ("07_assembly_and_commissioning", "docs/02_assembly.md"),
        ("09_modbus", "docs/04_modbus.md"), ("10_parameters", "docs/parameters.md"), ("11_bom", "docs/bom.md"), ("12_netlist", "docs/netlist.md"), ("14_enclosure_printing", "docs/05_printing.md")]
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
      footerTemplate: '<div style="font-family:Liberation Serif,serif;font-size:9px;width:100%;padding-left:10mm;text-align:center"><span class="pageNumber"></span></div>' }); }
  await b.close(); })();""")
subprocess.run(["node", js] + args, check=True)
shutil.rmtree(tmp)
# руководство по эксплуатации собирается отдельным скриптом (обложка, содержание, описания параметров)
subprocess.run([sys.executable, os.path.join(ROOT, "tools/make_manual.py")], check=True)
print("\n".join(sorted(os.listdir(PDF))))
