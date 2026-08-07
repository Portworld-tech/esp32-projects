#!/usr/bin/env python3
"""Export SPIFFS PNG files from SquareLine ui_img_*_png.c (TRUE_COLOR_ALPHA).

Also builds Screen10 theme thumbnails sc1_t.png .. sc4_t.png from spiffs_image/sc*.png
(or freshly exported sc*.png) so Screen10 does not decode full-res wallpapers.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SMARTHOME = ROOT / "esp32-smarthome-4inch"
UI_IMG_DIR = SMARTHOME / "ui" / "images"
if not UI_IMG_DIR.is_dir():
    UI_IMG_DIR = ROOT / "ui" / "images"
OUT_DIR = ROOT / "spiffs_image"

# Screen10 tile ~163x112; former zoom=100 on 480px ≈ 188px on the long side.
THEME_THUMB_MAX = 188

# ui_img_src.h SPIFFS basename -> source stem (without ui_img_ / _png)
# Theme wallpapers: sc1.png..sc4.png (+ sc*_t.png thumbs for Screen10)
NAME_MAP = {
    "wificlose.png": "ui_img_wificlose_png",
    "airConditioner.png": "ui_img_airconditioner_png",
    "heating.png": "ui_img_heat_png",
    "newtrend.png": "ui_img_newtrend_png",
    "underfloorHeating.png": "ui_img_underfloorheating_png",
    "antifreezing.png": "ui_img_antifreezing_png",
    "home.png": "ui_img_home_png",
    "openTheDoor.png": "ui_img_openthedoor_png",
    "safa.png": "ui_img_safa_png",
    "read_mode.png": "ui_img_read_png",
    "music_mode.png": "ui_img_music_png",
    "movie_mode.png": "ui_img_movie_png",
    "mood_mode.png": "ui_img_mood_png",
    "computer.png": "ui_img_computer_png",
    "open_img.png": "ui_img_open_png",
    "auto_mode.png": "ui_img_auto_png",
    "indoorConditioner.png": "ui_img_indoorconditioner_png",
    "return_img.png": "ui_img_return_png",
    "wifiopen.png": "ui_img_wifi_png",
    "icon.png": "ui_img_icon_png",
    "agreement.png": "ui_img_agreement_png",
    "link_img.png": "ui_img_link_png",
    "set_img.png": "ui_img_set_png",
    "soundClose.png": "ui_img_soundclose_png",
    "sun.png": "ui_img_sun_png",
    "soundopen.png": "ui_img_soundopen_png",
}

THEME_FULL = {
    "sc1.png": "ui_img_sc1_png",
    "sc2.png": "ui_img_sc2_png",
    "sc3.png": "ui_img_sc3_png",
    "sc4.png": "ui_img_sc4_png",
}


def parse_c_array(text: str, stem: str) -> bytes:
    marker = f"{stem}_data"
    start = text.find(marker)
    if start < 0:
        raise ValueError(f"{marker} not found")
    brace = text.find("{", start)
    end = text.find("};", brace)
    if brace < 0 or end < 0:
        raise ValueError("data array not found")
    hex_bytes = re.findall(r"0x[0-9A-Fa-f]{2}", text[brace : end + 1])
    if not hex_bytes:
        raise ValueError("data array empty")
    return bytes(int(h, 16) for h in hex_bytes)


def lvgl_rgb565_alpha_to_rgba(lo: int, hi: int, alpha: int) -> tuple[int, int, int, int]:
    """LVGL 16-bit TRUE_COLOR(_ALPHA): color.full LE + alpha byte."""
    v = lo | (hi << 8)
    r5 = (v >> 11) & 0x1F
    g6 = (v >> 5) & 0x3F
    b5 = v & 0x1F
    r = (r5 * 255 + 15) // 31
    g = (g6 * 255 + 31) // 63
    b = (b5 * 255 + 15) // 31
    return r, g, b, alpha


def parse_img_meta(text: str, data: bytes) -> tuple[int, int, bytes]:
    wm = re.search(r"\.header\.w\s*=\s*(\d+)", text)
    hm = re.search(r"\.header\.h\s*=\s*(\d+)", text)
    cf_m = re.search(r"\.header\.cf\s*=\s*(LV_IMG_CF_\w+)", text)
    if not wm or not hm:
        raise ValueError("width/height not found")
    w, h = int(wm.group(1)), int(hm.group(1))
    cf = cf_m.group(1) if cf_m else "LV_IMG_CF_TRUE_COLOR_ALPHA"
    use_len = len(data)
    px_cnt = w * h
    if px_cnt <= 0:
        raise ValueError("invalid image dimensions")
    if cf == "LV_IMG_CF_TRUE_COLOR":
        bpp = 3
    elif use_len >= px_cnt * 4:
        bpp = 4
    elif use_len >= px_cnt * 3:
        bpp = 3
    else:
        raise ValueError(f"{w}x{h} cf={cf}: data {use_len} bytes, need {px_cnt * 3} or {px_cnt * 4}")
    if use_len < px_cnt * bpp:
        raise ValueError(f"incomplete pixel data: got {use_len}, need {px_cnt * bpp}")
    rgba = bytearray(px_cnt * 4)
    if bpp == 3:
        for i in range(px_cnt):
            off = i * 3
            lo, hi, a = data[off], data[off + 1], data[off + 2]
            if cf == "LV_IMG_CF_TRUE_COLOR":
                r, g, b, _a = lvgl_rgb565_alpha_to_rgba(lo, hi, 255)
                a = 255
            else:
                r, g, b, a = lvgl_rgb565_alpha_to_rgba(lo, hi, a)
            rgba[i * 4 + 0] = r
            rgba[i * 4 + 1] = g
            rgba[i * 4 + 2] = b
            rgba[i * 4 + 3] = a
    else:
        # 32-bit embedded B,G,R,A -> PNG R,G,B,A
        for i in range(px_cnt):
            off = i * 4
            rgba[i * 4 + 0] = data[off + 2]
            rgba[i * 4 + 1] = data[off + 1]
            rgba[i * 4 + 2] = data[off + 0]
            rgba[i * 4 + 3] = data[off + 3]
    return w, h, bytes(rgba)


def write_png(path: Path, width: int, height: int, rgba: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    row_bytes = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        off = y * row_bytes
        raw.extend(rgba[off : off + row_bytes])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def scale_rgba_nearest(w: int, h: int, rgba: bytes, tw: int, th: int) -> bytes:
    out = bytearray(tw * th * 4)
    for y in range(th):
        sy = y * h // th
        for x in range(tw):
            sx = x * w // tw
            si = (sy * w + sx) * 4
            di = (y * tw + x) * 4
            out[di : di + 4] = rgba[si : si + 4]
    return bytes(out)


def thumb_size(w: int, h: int, max_side: int) -> tuple[int, int]:
    m = max(w, h)
    if m <= max_side:
        return w, h
    tw = max(1, (w * max_side + m // 2) // m)
    th = max(1, (h * max_side + m // 2) // m)
    return tw, th


def export_one(stem: str, out_name: str) -> tuple[int, int]:
    src = UI_IMG_DIR / f"{stem}.c"
    if not src.exists():
        raise FileNotFoundError(src)
    text = src.read_text(encoding="utf-8", errors="ignore")
    data = parse_c_array(text, stem)
    w, h, rgba = parse_img_meta(text, data)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUT_DIR / out_name
    write_png(out, w, h, rgba)
    print(f"  {out_name} ({w}x{h}) {out.stat().st_size} B")
    return w, h


def write_thumb_from_rgba(out_name: str, w: int, h: int, rgba: bytes, max_side: int) -> None:
    tw, th = thumb_size(w, h, max_side)
    trgba = scale_rgba_nearest(w, h, rgba, tw, th) if (tw, th) != (w, h) else rgba
    out = OUT_DIR / out_name
    write_png(out, tw, th, trgba)
    print(f"  {out_name} ({tw}x{th}) {out.stat().st_size} B  [from {w}x{h}]")


def gen_thumbs_from_spiffs(max_side: int) -> int:
    """Build sc*_t.png from existing / freshly written sc*.png (preferred runtime source)."""
    try:
        from PIL import Image
    except ImportError as e:
        raise SystemExit("Pillow required for thumbs-from-PNG: pip install Pillow") from e

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    n = 0
    for i in range(1, 5):
        src = OUT_DIR / f"sc{i}.png"
        if not src.exists():
            print(f"  skip sc{i}_t.png (missing {src.name})")
            continue
        im = Image.open(src).convert("RGBA")
        w, h = im.size
        tw, th = thumb_size(w, h, max_side)
        if (tw, th) != (w, h):
            im = im.resize((tw, th), Image.Resampling.LANCZOS)
        out = OUT_DIR / f"sc{i}_t.png"
        # Themes are opaque; palette PNG matches full sc*.png and stays small on SPIFFS.
        rgb = im.convert("RGB")
        try:
            q = rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
            q.save(out, format="PNG", optimize=True)
        except Exception:
            rgb.save(out, format="PNG", optimize=True)
        print(f"  sc{i}_t.png ({tw}x{th}) {out.stat().st_size} B  [from {src.name} {w}x{h}]")
        n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--icons", action="store_true", help="export icon NAME_MAP from C arrays")
    ap.add_argument("--themes-from-c", action="store_true", help="export sc1..sc4.png from C arrays")
    ap.add_argument("--thumbs", action="store_true", help="generate sc1_t..sc4_t from spiffs_image/sc*.png")
    ap.add_argument("--thumb-max", type=int, default=THEME_THUMB_MAX, help="thumb long-side px")
    ap.add_argument("--all", action="store_true", help="icons + themes-from-c + thumbs")
    args = ap.parse_args()

    if not (args.icons or args.themes_from_c or args.thumbs or args.all):
        args.thumbs = True

    print(f"SPIFFS out -> {OUT_DIR}")
    if args.all or args.icons:
        print("Icons:")
        for out_name, stem in sorted(NAME_MAP.items()):
            export_one(stem, out_name)
        print(f"Done icons: {len(NAME_MAP)}")

    if args.all or args.themes_from_c:
        print("Theme full (from C):")
        for out_name, stem in sorted(THEME_FULL.items()):
            export_one(stem, out_name)

    if args.all or args.thumbs:
        print(f"Theme thumbs (max_side={args.thumb_max}):")
        n = gen_thumbs_from_spiffs(args.thumb_max)
        print(f"Done thumbs: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
