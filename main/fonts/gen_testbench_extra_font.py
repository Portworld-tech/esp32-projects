#!/usr/bin/env python3
"""Generate lv_font_source_han_14_tb_extra.c — testbench punctuation supplement."""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_OTF = os.path.join(HERE, "SourceHanSansSC-Regular.otf")
EXTRA = os.path.join(HERE, "testbench_ui_extra.txt")
OUT_C = os.path.join(HERE, "lv_font_source_han_14_tb_extra.c")
OUT_H = os.path.join(HERE, "lv_font_source_han_14_tb_extra.h")


def main() -> int:
    if not os.path.isfile(FONT_OTF) or not os.path.isfile(EXTRA):
        print("Missing font or testbench_ui_extra.txt", file=sys.stderr)
        return 1

    symbols = open(EXTRA, "r", encoding="utf-8").read().replace("\n", "").replace("\r", "")

    npx = shutil.which("npx")
    if npx is None:
        cand = os.path.join(os.environ.get("ProgramFiles", ""), "nodejs", "npx.cmd")
        npx = cand if os.path.isfile(cand) else None
    if npx is None:
        print("npx not found", file=sys.stderr)
        return 1

    cmd = [
        npx,
        "--yes",
        "lv_font_conv@1.5.2",
        "--no-compress",
        "--no-prefilter",
        "--bpp",
        "4",
        "--size",
        "14",
        "--font",
        FONT_OTF,
        "-r",
        "0xA0-0xFF",
        "--symbols",
        symbols,
        "--format",
        "lvgl",
        "-o",
        OUT_C,
        "--force-fast-kern-format",
    ]
    print("Generating testbench extra font...", flush=True)
    env = os.environ.copy()
    node_dir = os.path.dirname(npx)
    env["PATH"] = node_dir + os.pathsep + env.get("PATH", "")
    r = subprocess.run(cmd, cwd=HERE, env=env)
    if r.returncode != 0:
        return r.returncode

    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write(
            "#pragma once\n\n#include \"lvgl.h\"\n\n"
            "/* Testbench UI punctuation supplement (14px Source Han). */\n"
            "extern const lv_font_t lv_font_source_han_14_tb_extra;\n"
        )
    print("Wrote", OUT_C, "and", OUT_H)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
