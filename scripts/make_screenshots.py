#!/usr/bin/env python3
"""Generate the README screenshots from the real application windows.

The app is C/GTK4, so there is no Rich/Textual console to record. A throwaway
instance runs on a private Xvfb display; ImageMagick captures the real
settings and setup windows, and a macOS-style title bar is added as window
decoration. The user's desktop is never captured.
"""

import base64
import os
import pathlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
APP = ROOT / "build" / "comebackwindow"
OUT = ROOT / "docs" / "img"
TMP = pathlib.Path(tempfile.mkdtemp(prefix="cbw-shot-"))
DISPLAY = ":97"
SCREEN = "1000x900x24"
SHOT_W, SHOT_H = 560, 820
FRAME_H = 36
RADIUS = 10

BASE_CONFIG = """[comebackwindow]
autostart=false
strip_enabled=true
strip_width=14
strip_style=text
strip_text=Show Desktop
strip_color=#1f80c4
strip_text_color=#ffffff
strip_opacity=0.900
strip_text_opacity=1.000
fallback_height=44
peek_enabled=true
peek_delay_ms=500
peek_opacity=0.080
shake_enabled=false
tray_enabled=false
language=en
"""


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=False, **kwargs)


def xvfb_binary() -> str | None:
    found = shutil.which("Xvfb")
    if found:
        return found
    local = pathlib.Path.home() / ".cache" / "cbw-deps" / "usr" / "bin" / "Xvfb"
    return str(local) if local.exists() else None


def magick(*args: str) -> None:
    tool = shutil.which("magick") or shutil.which("convert")
    if tool is None:
        die("ImageMagick is required (magick/convert)")
    if run([tool, *args]).returncode != 0:
        die("ImageMagick failed")


def png_size(path: pathlib.Path) -> tuple[int, int]:
    tool = shutil.which("magick") or shutil.which("identify")
    if tool is None:
        die("ImageMagick is required (magick/identify)")
    out = subprocess.check_output([tool, "identify", "-format", "%w %h", str(path)])
    width, height = out.decode().split()
    return int(width), int(height)


def framed_svg(png: pathlib.Path, out: pathlib.Path, title: str) -> None:
    content_w, content_h = png_size(png)
    width = content_w
    height = content_h + FRAME_H
    data = base64.b64encode(png.read_bytes()).decode("ascii")
    svg = (
        '<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
        f'<defs><clipPath id="win"><rect x="0" y="0" width="{width}" '
        f'height="{height}" rx="{RADIUS}"/></clipPath></defs>'
        '<g clip-path="url(#win)">'
        f'<rect width="{width}" height="{height}" fill="#1e1e1e"/>'
        f'<image x="0" y="{FRAME_H}" width="{content_w}" height="{content_h}" '
        f'href="data:image/png;base64,{data}"/>'
        f'<rect width="{width}" height="{FRAME_H}" fill="#2b2b2b"/>'
        '<circle cx="18" cy="18" r="6" fill="#ff5f57"/>'
        '<circle cx="38" cy="18" r="6" fill="#febc2e"/>'
        '<circle cx="58" cy="18" r="6" fill="#28c840"/>'
        f'<text x="{width / 2}" y="23" text-anchor="middle" '
        'font-family="DejaVu Sans, Arial, sans-serif" font-size="13" '
        f'fill="#e6e6e6">{title}</text>'
        '</g></svg>\n'
    )
    out.write_text(svg, encoding="utf-8")
    print(f"wrote {out.relative_to(ROOT)}")


def capture_root(dest: pathlib.Path) -> None:
    result = run(["import", "-display", DISPLAY, "-window", "root", str(dest)])
    if result.returncode != 0 or not dest.exists():
        die("screen capture failed (ImageMagick import + Xvfb required)")


def capture_window(config_text: str, args: list[str], name: str) -> pathlib.Path:
    config_dir = TMP / f"cfg-{name}"
    config_dir.mkdir()
    (config_dir / "config.ini").write_text(config_text, encoding="utf-8")

    env = os.environ.copy()
    env.pop("WAYLAND_DISPLAY", None)
    env["DISPLAY"] = DISPLAY
    env["GDK_BACKEND"] = "x11"
    env["GSK_RENDERER"] = "cairo"
    env["CBW_CONFIG_DIR"] = str(config_dir)
    env["CBW_DATA_DIR"] = str(ROOT)

    app = subprocess.Popen([str(APP), *args], env=env,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
    try:
        time.sleep(4.0)
        full = TMP / f"{name}-full.png"
        capture_root(full)
        shot = TMP / f"{name}.png"
        magick(str(full), "-crop", f"{SHOT_W}x{SHOT_H}+0+0", "+repage",
               str(shot))
    finally:
        app.terminate()
        try:
            app.wait(timeout=5)
        except subprocess.TimeoutExpired:
            app.kill()
    return shot


def main() -> int:
    if not APP.exists():
        die(f"build the app first: {APP} is missing (run ./comebackwindow once)")
    xvfb = xvfb_binary()
    if xvfb is None:
        die("Xvfb not found (install xorg-x11-server-Xvfb)")
    if run(["pgrep", "-x", "comebackwindow"], stdout=subprocess.DEVNULL).returncode == 0:
        die("close the running comebackwindow instance first (pkill -x comebackwindow)")
    if run(["make", "-s", "mo"], cwd=ROOT).returncode != 0:
        die("make mo failed (gettext/msgfmt required)")

    server = subprocess.Popen(
        [xvfb, DISPLAY, "-screen", "0", SCREEN],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    try:
        time.sleep(1.5)
        settings = capture_window(BASE_CONFIG + "first_run_done=true\n",
                                  ["--settings"], "settings")
        setup = capture_window(BASE_CONFIG + "first_run_done=false\n",
                               [], "setup")
        OUT.mkdir(parents=True, exist_ok=True)
        framed_svg(settings, OUT / "app-settings.svg",
                   "Come Back Window — Settings")
        framed_svg(setup, OUT / "app-setup.svg",
                   "Come Back Window — Setup")
    finally:
        server.send_signal(signal.SIGTERM)
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
        shutil.rmtree(TMP, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
