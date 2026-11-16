#!/bin/sh
# Выпуск производственных файлов платы ПЧ1500.01: Gerber, сверловка, координаты, чертежи слоёв, изображения.
set -e
cd "$(dirname "$0")"
B=pch1500_a1.kicad_pcb
rm -rf out/gerber && mkdir -p out/gerber
kicad-cli pcb export gerbers -o out/gerber/ --layers F.Cu,B.Cu,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts --subtract-soldermask $B >/dev/null
kicad-cli pcb export drill -o out/gerber/ --format excellon --drill-origin absolute --excellon-units mm --generate-map --map-format pdf $B >/dev/null
kicad-cli pcb export pos -o out/pch1500_a1_pos.csv --format csv --units mm $B >/dev/null
(cd out && rm -f pch1500_a1_gerber.zip && zip -q -r pch1500_a1_gerber.zip gerber)
for L in "F.Cu,Edge.Cuts:top_copper" "B.Cu,Edge.Cuts:bottom_copper" "F.SilkS,F.Fab,Edge.Cuts:assembly"; do
  layers=${L%%:*}; name=${L##*:}
  kicad-cli pcb export svg -o out/pcb_$name.svg --layers $layers --black-and-white --page-size-mode 2 --exclude-drawing-sheet $B >/dev/null
done
kicad-cli pcb export svg -o out/pcb_both.svg --layers B.Cu,F.Cu,Edge.Cuts --page-size-mode 2 --exclude-drawing-sheet $B >/dev/null
for n in top_copper bottom_copper assembly both; do node svg2png.js out/pcb_$n.svg out/pcb_$n.png 2400; done
ls out out/gerber
