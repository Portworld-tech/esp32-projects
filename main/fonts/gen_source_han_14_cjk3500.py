#!/usr/bin/env python3
"""Generate lv_font_source_han_14_cjk3500.c using lv_font_conv (requires Node.js npx)."""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_OTF = os.path.join(HERE, "SourceHanSansSC-Regular.otf")
COMMON = os.path.join(HERE, "common3500.txt")
OUT_C = os.path.join(HERE, "lv_font_source_han_14_cjk3500.c")


def main() -> int:
    if not os.path.isfile(FONT_OTF):
        print("Missing font. Download:", file=sys.stderr)
        print(
            "  https://github.com/adobe-fonts/source-han-sans/raw/release/OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf",
            file=sys.stderr,
        )
        print("  Save as:", FONT_OTF, file=sys.stderr)
        return 1
    if not os.path.isfile(COMMON):
        print("Missing", COMMON, file=sys.stderr)
        return 1

    symbols = open(COMMON, "r", encoding="utf-8").read()
    for extra_name in ("testbench_ui_extra.txt", "hub_ui_extra.txt"):
        extra_path = os.path.join(HERE, extra_name)
        if os.path.isfile(extra_path):
            extra = open(extra_path, "r", encoding="utf-8").read()
            added = 0
            for ch in extra:
                if ch not in symbols and not ch.isspace():
                    symbols += ch
                    added += 1
            print(f"Appended {extra_name}: +{added} symbols", flush=True)
    # Strip whitespace so Windows cmdline / argparse does not break on newlines
    symbols = "".join(ch for ch in symbols if not ch.isspace())
    # Deduplicate while preserving order
    seen = set()
    deduped = []
    for ch in symbols:
        if ch not in seen:
            seen.add(ch)
            deduped.append(ch)
    symbols = "".join(deduped)
    print(f"Symbol count: {len(symbols)}", flush=True)

    if len(symbols) < 3000:
        print("common3500.txt seems too short:", len(symbols), file=sys.stderr)
        return 1

    # Prefer writing symbols to a temp file to avoid Windows CreateProcess limits
    sym_file = os.path.join(HERE, "_symbols_tmp.txt")
    with open(sym_file, "w", encoding="utf-8", newline="") as f:
        f.write(symbols)

    npx = shutil.which("npx")
    if npx is None:
        # Windows portable Node: e.g. %TEMP%\node-v20.18.1-win-x64\npx.cmd
        cand = os.path.join(os.environ.get("TEMP", ""), "node-v20.18.1-win-x64", "npx.cmd")
        npx = cand if os.path.isfile(cand) else None
    if npx is None:
        print("npx not found. Install Node.js or extract portable Node and add to PATH.", file=sys.stderr)
        return 1

    # Read symbols back for --symbols (lv_font_conv has no --symbols-file);
    # keep as one argv element without newlines.
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
        "0x20-0x7F",
        "-r",
        "0xA0-0xFF",
        "-r",
        "0x2013-0x2026",
        "-r",
        "0x2190-0x2193",
        "--symbols",
        symbols,
        "--format",
        "lvgl",
        "-o",
        OUT_C,
        "--force-fast-kern-format",
    ]
    print("Running lv_font_conv (may take a few minutes)...", flush=True)
    env = os.environ.copy()
    node_dir = os.path.dirname(npx)
    env["PATH"] = node_dir + os.pathsep + env.get("PATH", "")
    r = subprocess.run(cmd, cwd=HERE, env=env, shell=False)
    return r.returncode


if __name__ == "__main__":
    raise SystemExit(main())
