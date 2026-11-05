#!/usr/bin/env python3
"""PPSSPP OwnShell smoke: launch, screenshot, assert UI is not a black stub."""
from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SHOT = Path(__file__).resolve().parents[1] / "ppsspp" / "screenshots" / "smoke_ui.png"
EBOOT = Path.home() / ".config/ppsspp/PSP/GAME/OwnShell/EBOOT.PBP"
APPIMAGE = Path(__file__).resolve().parents[1] / "ppsspp" / "PPSSPP.AppImage"
LOG = Path(__file__).resolve().parents[1] / "ppsspp" / "stdout.log"


def run(cmd, **kw):
    return subprocess.run(cmd, check=False, **kw)


def ppsspp_geo():
    out = subprocess.check_output(["hyprctl", "clients", "-j"], text=True)
    for c in json.loads(out):
        if c.get("class") == "PPSSPPSDL":
            x, y = c["at"]
            w, h = c["size"]
            return f"{x},{y} {w}x{h}", c.get("title", "")
    return None, None


def main() -> int:
    from PIL import Image
    import numpy as np

    src = ROOT / "shell" / "EBOOT.PBP"
    if not src.is_file():
        print("FAIL: missing shell/EBOOT.PBP — build first", file=sys.stderr)
        return 2
    EBOOT.parent.mkdir(parents=True, exist_ok=True)
    EBOOT.write_bytes(src.read_bytes())

    run(["pkill", "-f", "PPSSPPSDL"])
    time.sleep(0.4)
    with LOG.open("w") as log:
        subprocess.Popen(
            [str(APPIMAGE), str(EBOOT)],
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )

    geo = title = None
    for _ in range(40):
        time.sleep(0.25)
        geo, title = ppsspp_geo()
        if geo and title and "OwnShell" in title:
            break
    if not geo:
        print("FAIL: PPSSPPSDL window not found", file=sys.stderr)
        return 1
    if "OwnShell" not in (title or ""):
        print(f"FAIL: unexpected title {title!r} (crashed to menu?)", file=sys.stderr)
        return 1

    time.sleep(1.0)
    run(["hyprctl", "dispatch", "focuswindow", "class:PPSSPPSDL"])
    time.sleep(0.3)
    SHOT.parent.mkdir(parents=True, exist_ok=True)
    r = run(["grim", "-g", geo, str(SHOT)])
    if r.returncode != 0 or not SHOT.is_file():
        print("FAIL: grim screenshot", file=sys.stderr)
        return 1

    im = Image.open(SHOT).convert("RGB")
    a = np.asarray(im)
    mean = a.mean(axis=(0, 1))
    # Accent cyan-ish pixels
    cyan = int(((a[:, :, 2] > 140) & (a[:, :, 1] > 140) & (a[:, :, 0] < 120)).sum())
    # Content not nearly pitch-black
    bright = float((a.sum(axis=2) > 40).mean())
    print(f"title={title!r} size={im.size} mean={mean} cyan={cyan} bright={bright:.3f}")

    fails = 0
    if bright < 0.08:
        print("FAIL: frame mostly black (UI not drawn)", file=sys.stderr)
        fails += 1
    if cyan < 200:
        print("FAIL: too few cyan accent pixels", file=sys.stderr)
        fails += 1
    if mean.sum() < 15:
        print("FAIL: mean luminance too low", file=sys.stderr)
        fails += 1

    # Crop letterbox-ish content and require multiple bright bands (nav + body + footer)
    lum = a.astype(np.int16).sum(axis=2)
    rows = np.where((lum > 40).mean(axis=1) > 0.02)[0]
    if len(rows) < 40:
        print("FAIL: content vertical span too small", file=sys.stderr)
        fails += 1
    else:
        span = int(rows[-1] - rows[0])
        if span < 200:
            print(f"FAIL: content height span {span} < 200", file=sys.stderr)
            fails += 1

    if fails:
        return 1
    print("ok: smoke_ui passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
