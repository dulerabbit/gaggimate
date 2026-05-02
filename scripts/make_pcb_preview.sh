#!/usr/bin/env bash
cd pcb
pcbdraw plot --side front --components -l KiCAD-base,custom -m remap.json -s set-black-hasl Gaggimate.kicad_pcb ../docs/assets/pcb_top.png
# sips -r 180 ../docs/assets/pcb_bottom.png
cd ..
rm -f docs/assets/pinout.png
python3 -m pinout.manager --export scripts/pinout_diagram.py docs/assets/pinout.png