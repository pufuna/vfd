// Сборка всех схем в один PDF: node mkpdf.js  (нужен playwright)
const { chromium } = require('playwright'); const fs = require('fs'); const path = require('path');
(async () => {
  const out = path.join(__dirname, 'out');
  const files = ['structure.svg', 'sch_1.svg', 'sch_2.svg', 'sch_3.svg', 'sch_4.svg', 'sch_5.svg', 'wiring.svg'];
  const html = '<style>@page{size:A3 landscape;margin:8mm}body{margin:0}svg{width:100%;height:auto;max-height:270mm;page-break-after:always;display:block}</style>' +
    files.map(f => fs.readFileSync(path.join(out, f), 'utf8')).join('');
  const b = await chromium.launch(); const p = await b.newPage();
  await p.setContent(html); await p.pdf({ path: path.join(out, 'pch1500_schematics.pdf'), preferCSSPageSize: true });
  await b.close();
})();
