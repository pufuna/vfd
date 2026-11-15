#!/bin/sh
# Растровый предпросмотр платы: render.sh файл.kicad_pcb выход.png [ширина]
S=$(mktemp -d)
kicad-cli pcb export svg -o $S/a.svg --layers B.Cu,F.Cu,Edge.Cuts --page-size-mode 2 --exclude-drawing-sheet "$1" >/dev/null 2>&1
node "$(dirname "$0")/svg2png.js" $S/a.svg "$2" ${3:-2000}
rm -rf $S
