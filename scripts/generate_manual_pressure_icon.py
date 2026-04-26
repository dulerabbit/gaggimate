from pathlib import Path
from PIL import Image, ImageFilter, ImageChops

root = Path(__file__).resolve().parents[1]
assets = root / "ui" / "assets"
out_dir = root / "src" / "display" / "ui" / "default" / "lvgl" / "images"

SRC = assets / "tachometer-fast-40x40.png"
BASE_SYMBOL = "ui_img_manual_pressure_80x80"
GLOW_SYMBOL = "ui_img_glow_manual_pressure_80x80"


def rgba_to_rgb565_le(r, g, b):
    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return v & 0xFF, (v >> 8) & 0xFF


def encode_c_image(img: Image.Image, symbol: str, comment: str):
    img = img.convert("RGBA")
    w, h = img.size
    pix = img.load()

    data = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            lo, hi = rgba_to_rgb565_le(r, g, b)
            data.extend((lo, hi, a))

    hex_bytes = [f"0x{v:02X}" for v in data]
    lines = []
    row = []
    for token in hex_bytes:
        row.append(token)
        if len(row) >= 24:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")

    c_text = f"// Auto-generated {comment}\n"
    c_text += '#include "../ui.h"\n\n'
    c_text += "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n\n"
    c_text += f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {symbol}_data[] = {{\n"
    c_text += "\n".join(lines)
    c_text += "\n};\n\n"
    c_text += f"const lv_img_dsc_t {symbol} = {{\n"
    c_text += "    .header.always_zero = 0,\n"
    c_text += f"    .header.w = {w},\n"
    c_text += f"    .header.h = {h},\n"
    c_text += f"    .data_size = sizeof({symbol}_data),\n"
    c_text += "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n"
    c_text += f"    .data = {symbol}_data,\n"
    c_text += "};\n"

    out_file = out_dir / f"{symbol}.c"
    out_file.write_text(c_text, encoding="utf-8")
    print(f"generated {out_file}")


base = Image.open(SRC).convert("RGBA")
base_80 = base.resize((80, 80), Image.Resampling.LANCZOS)
encode_c_image(base_80, BASE_SYMBOL, "manual pressure icon")

pad = 16
canvas = Image.new("RGBA", (base_80.width + pad * 2, base_80.height + pad * 2), (0, 0, 0, 0))
canvas.paste(base_80, (pad, pad), base_80)
alpha = canvas.getchannel("A")
blur = alpha.filter(ImageFilter.GaussianBlur(radius=4.6))
glow_alpha = ImageChops.lighter(blur, alpha)
glow_rgba = Image.new("RGBA", canvas.size, (140, 255, 255, 0))
glow_rgba.putalpha(glow_alpha)
encode_c_image(glow_rgba, GLOW_SYMBOL, "manual pressure glow icon")
