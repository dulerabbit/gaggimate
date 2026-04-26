import os
import subprocess
import sys
import shutil
from pathlib import Path

ROOT = Path(r"C:/Users/Administrator/Desktop/gaggimate-master")
TTF = ROOT / ".pio/libdeps/elecrow_rotary_21/lvgl/scripts/built_in_font/Montserrat-Medium.ttf"
OUT = ROOT / "src/display/ui/default/lvgl/images/ui_font_manual_pressure_56.c"

if not TTF.exists():
    print(f"TTF not found: {TTF}")
    sys.exit(1)

npx = shutil.which("npx") or shutil.which("npx.cmd") or r"C:\Program Files\nodejs\npx.cmd"

cmd = [
    npx,
    "lv_font_conv",
    "--font", str(TTF),
    "--size", "56",
    "--bpp", "4",
    "--format", "lvgl",
    "--no-compress",
    "--symbols", "0123456789",
    "--lv-font-name", "ui_font_manual_pressure_56",
    "-o", str(OUT),
]

print("Running:", " ".join(cmd))
subprocess.check_call(cmd)
print(f"Generated: {OUT}")
