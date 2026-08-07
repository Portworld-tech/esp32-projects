#!/usr/bin/env python3
"""Generate a small supplemental CJK/punct font for hub UI missing glyphs."""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_OTF = os.path.join(HERE, "SourceHanSansSC-Regular.otf")
OUT_C = os.path.join(HERE, "lv_font_hub_cjk_extra.c")
OUT_H = os.path.join(HERE, "lv_font_hub_cjk_extra.h")

# Glyphs missing from lv_font_source_han_14_cjk3500 + hub punctuation
# Keep in sync with hub_ui_extra.txt — glyphs missing from cjk3500 used by hub UI.
SYMBOLS = (
    "主置整总制·°→—…「」" "冷暖正障柔耗计量附近编辑暂添隐阀亮扫密热刷链断存输错败启选景卷帘床头厨台排顶桌插"
    "值执止照至足钟"  # 分钟 / 停止 / 执行 / 峰值 / 至 / 足 — dropdown + labels
)


def main() -> int:
    if not os.path.isfile(FONT_OTF):
        print("Missing", FONT_OTF, file=sys.stderr)
        return 1

    npx = shutil.which("npx")
    if npx is None:
        cand = os.path.join(os.environ.get("TEMP", ""), "node-v20.18.1-win-x64", "npx.cmd")
        npx = cand if os.path.isfile(cand) else None

    local_js = os.path.join(HERE, "node_modules", "lv_font_conv", "lv_font_conv.js")
    node = shutil.which("node")

    if os.path.isfile(local_js) and node:
        cmd_prefix = [node, local_js]
    elif npx is not None:
        # Windows npx shims are flaky; prefer local install when available.
        cmd_prefix = [npx, "--yes", "lv_font_conv@1.5.2"]
    else:
        print("Need Node.js + fonts/node_modules/lv_font_conv (npm i lv_font_conv@1.5.2)", file=sys.stderr)
        return 1

    cmd = cmd_prefix + [
        "--no-compress", "--no-prefilter",
        "--bpp", "4", "--size", "14",
        "--font", FONT_OTF,
        "-r", "0xB0", "-r", "0xB7",
        "-r", "0x2014", "-r", "0x2192",
        "--symbols", SYMBOLS,
        "--format", "lvgl",
        "-o", OUT_C,
        "--lv-include", "lvgl.h",
        "--force-fast-kern-format",
    ]
    print("Generating", OUT_C, "symbols=", SYMBOLS, flush=True)
    env = os.environ.copy()
    if npx:
        env["PATH"] = os.path.dirname(npx) + os.pathsep + env.get("PATH", "")
    r = subprocess.run(cmd, cwd=HERE, env=env)
    if r.returncode != 0:
        return r.returncode

    # Rename public symbol to avoid clash with default lv_font_conv name
    text = open(OUT_C, "r", encoding="utf-8").read()
    # lv_font_conv names font from size: often lv_font_montserrat_... or based on file
    # Force a stable name:
    import re
    text2, n = re.subn(
        r"const lv_font_t lv_font_\w+",
        "const lv_font_t lv_font_hub_cjk_extra",
        text,
        count=1,
    )
    if n == 0:
        text2, n = re.subn(
            r"lv_font_t lv_font_\w+",
            "lv_font_t lv_font_hub_cjk_extra",
            text,
            count=1,
        )
    open(OUT_C, "w", encoding="utf-8", newline="\n").write(text2)
    open(OUT_H, "w", encoding="utf-8", newline="\n").write(
        "#pragma once\n#include \"lvgl.h\"\n"
        "extern const lv_font_t lv_font_hub_cjk_extra;\n"
    )
    print("Wrote", OUT_C, OUT_H, "renames=", n, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
