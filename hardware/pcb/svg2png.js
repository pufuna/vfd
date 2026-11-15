const { chromium } = require('playwright'); const fs = require('fs');
(async () => { const [src, dst, w] = process.argv.slice(2);
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: +w || 1600, height: 1200 } });
  await p.setContent('<body style="margin:0;background:#fff"><img style="width:100%" src="data:image/svg+xml;base64,' + fs.readFileSync(src).toString('base64') + '"></body>');
  await p.screenshot({ path: dst, fullPage: true }); await b.close(); })();
