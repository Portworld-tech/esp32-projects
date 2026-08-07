#!/usr/bin/env python3
"""Generate hub SPIFFS icon masks from lvgl-front/lvgl-icons.jsx (Smart-LVGL nt style).

White-on-transparent 96×96 PNGs for LVGL img_recolor + zoom.
Usage:  pip install pillow svg.path
        python tools/gen_hub_icons.py
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

try:
    from svg.path import Close, Move, parse_path
except ImportError as e:
    raise SystemExit("Run: pip install svg.path pillow") from e

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
JSX = ROOT / "lvgl-front" / "lvgl-icons.jsx"
NT_DIR = ROOT / "spiffs_image" / "icons" / "nt"
SIZE = 96
VIEW = 24.0
MASK = (0xFF, 0xFF, 0xFF, 0xFF)

# hub_ico_t → FE LIco name (must exist in JSX)
HUB_NAMES = [
    "home", "bulb", "curtain", "snow", "shield", "gauge", "layers",
    "wifi", "wifiOff", "cog", "grid", "clock", "power", "moon", "moonSleep",
    "away", "bus", "drop", "heat", "plus", "minus", "up", "down", "stop",
    "back", "left", "right", "chevron", "link", "bright", "info", "lock",
    "plug2",
]


def _scale(x: float, y: float, view: float = VIEW) -> tuple[float, float]:
    pad = SIZE * 0.10
    inner = SIZE - 2 * pad
    return pad + x * inner / view, pad + y * inner / view


def _sample_subpaths(d: str, steps_per_unit: float = 5.0) -> list[list[tuple[float, float]]]:
    path = parse_path(d)
    subpaths: list[list[tuple[float, float]]] = []
    current: list[tuple[float, float]] = []

    for seg in path:
        if isinstance(seg, Move):
            if len(current) >= 2:
                subpaths.append(current)
            current = [(seg.end.real, seg.end.imag)]
            continue

        length = max(seg.length(error=1e-3), 1e-3)
        n = max(int(length * steps_per_unit), 6)
        for i in range(1, n + 1):
            p = seg.point(i / n)
            current.append((p.real, p.imag))

        if isinstance(seg, Close) and current:
            current.append(current[0])

    if len(current) >= 2:
        subpaths.append(current)

    return subpaths


def _draw_stroke(draw: ImageDraw.ImageDraw, d: str, color: tuple[int, int, int, int], sw: float) -> None:
    width = max(int(round(sw * SIZE / VIEW)), 2)
    for subpath in _sample_subpaths(d):
        pts = [_scale(x, y) for x, y in subpath]
        if len(pts) >= 2:
            draw.line(pts, fill=color, width=width, joint="curve")


def _draw_fill(draw: ImageDraw.ImageDraw, d: str, color: tuple[int, int, int, int]) -> None:
    for subpath in _sample_subpaths(d):
        pts = [_scale(x, y) for x, y in subpath]
        if len(pts) >= 3:
            draw.polygon(pts, fill=color)


def _draw_rect(draw: ImageDraw.ImageDraw, x: float, y: float, w: float, h: float,
               color: tuple[int, int, int, int], sw: float, rx: float = 0.0, filled: bool = False) -> None:
    x1, y1 = _scale(x, y)
    x2, y2 = _scale(x + w, y + h)
    if x1 > x2:
        x1, x2 = x2, x1
    if y1 > y2:
        y1, y2 = y2, y1
    radius = max(int(round(rx * (SIZE / VIEW))), 0)
    if filled:
        draw.rounded_rectangle([x1, y1, x2, y2], radius=radius, fill=color)
    else:
        width = max(int(round(sw * SIZE / VIEW)), 2)
        draw.rounded_rectangle([x1, y1, x2, y2], radius=radius, outline=color, width=width)


def _draw_circle(draw: ImageDraw.ImageDraw, cx: float, cy: float, r: float,
                 color: tuple[int, int, int, int], sw: float, filled: bool = False) -> None:
    x1, y1 = _scale(cx - r, cy - r)
    x2, y2 = _scale(cx + r, cy + r)
    if filled:
        draw.ellipse([x1, y1, x2, y2], fill=color)
    else:
        width = max(int(round(sw * SIZE / VIEW)), 2)
        draw.ellipse([x1, y1, x2, y2], outline=color, width=width)


def _parse_svg_fragment(body: str, default_sw: float, svg_fill: str | None, svg_stroke: str | None) -> dict:
    spec: dict = {"fills": [], "strokes": [], "rects": [], "circles": [], "sw": default_sw}
    wrapped = f"<svg xmlns='http://www.w3.org/2000/svg'>{body}</svg>"
    root = ET.fromstring(wrapped)

    def tag_local(el: ET.Element) -> str:
        return el.tag.split("}")[-1]

    for el in root.iter():
        t = tag_local(el)
        if t == "path":
            d = el.get("d")
            if not d:
                continue
            fill = el.get("fill")
            stroke = el.get("stroke")
            if fill and fill not in ("none", "transparent"):
                # currentColor fill on child — treat as fill
                if fill == "currentColor" or fill not in ("none",):
                    spec["fills"].append(d)
            elif stroke and stroke not in ("none", "transparent"):
                spec["strokes"].append(d)
            elif svg_fill and svg_fill not in ("none", "transparent"):
                spec["fills"].append(d)
            elif svg_stroke and svg_stroke not in ("none", "transparent"):
                spec["strokes"].append(d)
            else:
                spec["strokes"].append(d)
        elif t == "rect":
            fill = el.get("fill")
            filled = fill not in (None, "none", "transparent")
            # inherit svg fill=currentColor on stop icon
            if not filled and svg_fill and svg_fill not in ("none", "transparent"):
                filled = True
            spec["rects"].append({
                "x": float(el.get("x", 0)),
                "y": float(el.get("y", 0)),
                "w": float(el.get("width", 0)),
                "h": float(el.get("height", 0)),
                "rx": float(el.get("rx", 0) or 0),
                "filled": filled,
            })
        elif t == "circle":
            fill = el.get("fill")
            filled = fill not in (None, "none", "transparent")
            if fill == "currentColor":
                filled = True
            spec["circles"].append({
                "cx": float(el.get("cx", 0)),
                "cy": float(el.get("cy", 0)),
                "r": float(el.get("r", 0)),
                "filled": filled,
            })
    return spec


def parse_icons_jsx(path: Path) -> dict[str, dict]:
    text = path.read_text(encoding="utf-8")
    icons: dict[str, dict] = {}
    # Accept (p={}), (p = {}), (props = {})
    pattern = re.compile(
        r"(\w+)\s*:\s*\([^)]*\)\s*=>\s*\(\s*<svg([^>]*)>(.*?)</svg>\s*\)",
        re.DOTALL,
    )
    for name, svg_attrs, body in pattern.findall(text):
        if name.startswith("wx"):
            continue
        sw_m = re.search(r'strokeWidth="([^"]+)"', svg_attrs)
        default_sw = float(sw_m.group(1)) if sw_m else 1.8
        svg_fill = None
        svg_stroke = None
        fill_m = re.search(r'\bfill="([^"]+)"', svg_attrs)
        if fill_m:
            svg_fill = fill_m.group(1)
        stroke_m = re.search(r'\bstroke="([^"]+)"', svg_attrs)
        if stroke_m:
            svg_stroke = stroke_m.group(1)
        icons[name] = _parse_svg_fragment(body, default_sw, svg_fill, svg_stroke)
    return icons


def render_icon(spec: dict, color: tuple[int, int, int, int]) -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    sw = spec.get("sw", 1.8)

    for d in spec.get("fills", []):
        _draw_fill(draw, d, color)

    for d in spec.get("strokes", []):
        _draw_stroke(draw, d, color, sw)

    for rc in spec.get("rects", []):
        _draw_rect(draw, rc["x"], rc["y"], rc["w"], rc["h"], color, sw, rc.get("rx", 0), rc["filled"])

    for cc in spec.get("circles", []):
        _draw_circle(draw, cc["cx"], cc["cy"], cc["r"], color, sw, cc["filled"])

    return img


def main() -> None:
    if not JSX.is_file():
        raise SystemExit(f"Missing {JSX}")

    icons = parse_icons_jsx(JSX)
    missing = [n for n in HUB_NAMES if n not in icons]
    if missing:
        raise SystemExit(f"Icons missing from JSX parse: {missing} (parsed={sorted(icons)})")

    NT_DIR.mkdir(parents=True, exist_ok=True)
    # Also emit alias plug.png for HUB_ICO_PLUG
    aliases = {"plug2": "plug"}

    count = 0
    for name in sorted(set(HUB_NAMES) | set(aliases.values())):
        src = name
        if name == "plug" and "plug" not in icons:
            src = "plug2"
        if src not in icons:
            continue
        out = NT_DIR / f"{name}.png"
        render_icon(icons[src], MASK).save(out, "PNG")
        count += 1
        print(f"  {out.name}")

    # Ensure every hub name exists (plug2.png + plug.png)
    for name in HUB_NAMES:
        if not (NT_DIR / f"{name}.png").is_file():
            raise SystemExit(f"Failed to write {name}.png")

    print(f"Parsed {len(icons)} icons, wrote {count} masks → {NT_DIR}")


if __name__ == "__main__":
    main()
