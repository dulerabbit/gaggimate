from PIL import Image, ImageFilter, ImageChops
from pathlib import Path

root = Path(__file__).resolve().parents[1]
assets = root / "ui" / "assets"
out_dir = root / "src" / "display" / "ui" / "default" / "lvgl" / "images"

icon_map = {
    "play-40x40.png": "ui_img_glow_play_40x40",
    "settings-40x40.png": "ui_img_glow_settings_40x40",
    "dropdown-bar-40x40.png": "ui_img_glow_dropdown_bar_40x40",
    "mug-hot-alt-80x80.png": "ui_img_glow_mug_hot_alt_80x80",
    "wind-80x80.png": "ui_img_glow_wind_80x80",
    "raindrops-80x80.png": "ui_img_glow_raindrops_80x80",
    "coffee-bean-80x80.png": "ui_img_glow_coffee_bean_80x80",
    "check-40x40.png": "ui_img_glow_check_40x40",
    "pause-40x40.png": "ui_img_glow_pause_40x40",
    "disk-30x30.png": "ui_img_glow_disk_30x30",
    "floppy-disks-30x30.png": "ui_img_glow_floppy_disks_30x30",
    "plus-small-40x40.png": "ui_img_glow_plus_small_40x40",
    "minus-small-40x40.png": "ui_img_glow_minus_small_40x40",
}


def rgba_to_rgb565_le(r, g, b):
    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return v & 0xFF, (v >> 8) & 0xFF


for asset_name, symbol in icon_map.items():
    src_path = assets / asset_name
    img = Image.open(src_path).convert("RGBA")
    w0, h0 = img.size
    if max(w0, h0) >= 80:
        pad = 16
        blur_radius = 4.6
    elif max(w0, h0) >= 40:
        pad = 12
        blur_radius = 4.0
    else:
        pad = 10
        blur_radius = 3.6

    canvas = Image.new("RGBA", (w0 + pad * 2, h0 + pad * 2), (0, 0, 0, 0))
    canvas.paste(img, (pad, pad), img)
    alpha = canvas.getchannel("A")

    blur = alpha.filter(ImageFilter.GaussianBlur(radius=blur_radius))
    glow_alpha = ImageChops.lighter(blur, alpha)

    glow_rgba = Image.new("RGBA", canvas.size, (140, 255, 255, 0))
    glow_rgba.putalpha(glow_alpha)

    w, h = glow_rgba.size
    pix = glow_rgba.load()
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

    c_text = f"// Auto-generated glow variant for {asset_name}\n"
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

print(f"Generated {len(icon_map)} glow image C files in {out_dir}")
