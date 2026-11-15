// Сборка листов SVG в один PDF (каждый лист — страница своего формата): node mkpdf.js <каталог> <выход.pdf> файл:ширина:высота ...
const { chromium } = require('playwright'); const fs = require('fs'); const path = require('path');
(async () => {
  const [dir, out, ...items] = process.argv.slice(2);
  const b = await chromium.launch(); const bufs = [];
  let html = '<style>body{margin:0}.p{page-break-after:always;overflow:hidden}svg{display:block}</style>';
  // страницы разного формата: именованные @page
  let css = '';
  items.forEach((it, i) => { const [f, w, h] = it.split(':'); css += `@page p${i}{size:${w}mm ${h}mm;margin:0} .p${i}{page:p${i};width:${w}mm;height:${h}mm}`;
    html += `<div class="p p${i}">${fs.readFileSync(path.join(dir, f), 'utf8')}</div>`; });
  const p = await b.newPage(); await p.setContent(`<style>${css}</style>` + html);
  await p.pdf({ path: path.join(dir, out), preferCSSPageSize: true, printBackground: true });
  await b.close();
})();
