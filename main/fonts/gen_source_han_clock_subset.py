#!/usr/bin/env python3
"""Generate a Source Han clock subset font (0-9, ':') at arbitrary pixel size.

Usage:
  python gen_source_han_clock_subset.py 150 lv_font_source_han_ambient_time_150
  python gen_source_han_clock_subset.py 100 lv_font_source_han_screen1_time_100

Requires: SourceHanSansSC-Regular.otf in this directory, Node.js, tar (Windows 10+).
"""
import os
import shutil
import subprocess
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_OTF = os.path.join(HERE, "SourceHanSansSC-Regular.otf")
TGZ = os.path.join(HERE, "lv_font_conv.tgz")
CLI = os.path.join(HERE, "package", "lv_font_conv.js")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    size_s, base = sys.argv[1], sys.argv[2]
    try:
        size = int(size_s)
    except ValueError:
        print("size must be int", file=sys.stderr)
        return 2
    if size < 8 or size > 200:
        print("size out of range 8..200", file=sys.stderr)
        return 2
    out_c = os.path.join(HERE, base + ".c")
    if not base.startswith("lv_font_") or ".." in base or "/" in base or "\\" in base:
        print("basename must be like lv_font_... (no path)", file=sys.stderr)
        return 2

    if not os.path.isfile(FONT_OTF):
        print("Missing", FONT_OTF, file=sys.stderr)
        return 1

    node = shutil.which("node")
    if node is None:
        print("node not found in PATH.", file=sys.stderr)
        return 1

    if not os.path.isfile(CLI):
        url = "https://registry.npmjs.org/lv_font_conv/-/lv_font_conv-1.5.2.tgz"
        print("Fetching lv_font_conv...", flush=True)
        urllib.request.urlretrieve(url, TGZ)
        r = subprocess.run(["tar", "-xzf", TGZ], cwd=HERE)
        if r.returncode != 0 or not os.path.isfile(CLI):
            print("extract failed", file=sys.stderr)
            return 1
        try:
            os.remove(TGZ)
        except OSError:
            pass

    cmd = [
        node,
        CLI,
        "--no-compress",
        "--no-prefilter",
        "--bpp",
        "4",
        "--size",
        str(size),
        "--font",
        FONT_OTF,
        "-r",
        "0x30-0x3A",
        "--format",
        "lvgl",
        "-o",
        out_c,
        "--force-fast-kern-format",
    ]
    print("Running lv_font_conv ->", out_c, flush=True)
    return subprocess.run(cmd, cwd=HERE).returncode


if __name__ == "__main__":
    raise SystemExit(main())
