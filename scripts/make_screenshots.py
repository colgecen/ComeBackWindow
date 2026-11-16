#!/usr/bin/env python3
"""Generate the README screenshots from the real application UI.

The application is C/GTK4, so there is no Rich/Textual console to record.
Instead this script starts a throwaway instance on a private Xvfb display,
captures the real windows with ImageMagick, and embeds the PNG bytes in an
SVG wrapper. Nothing is drawn by hand, and the user's desktop is never
captured.
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
SCREEN = "1920x1080x24"

CONFIG = """[comebackwindow]
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
shake_enabled=true
tray_enabled=false
language=en
first_run_done=true
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


def wrap_svg(png: pathlib.Path, out: pathlib.Path) -> None:
    width, height = png_size(png)
    data = base64.b64encode(png.read_bytes()).decode("ascii")
    out.write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
        f'<image width="{width}" height="{height}" '
        f'href="data:image/png;base64,{data}"/></svg>\n',
        encoding="utf-8",
    )
    print(f"wrote {out.relative_to(ROOT)}")


def capture_root(dest: pathlib.Path) -> None:
    result = run(["import", "-display", DISPLAY, "-window", "root", str(dest)])
    if result.returncode != 0 or not dest.exists():
        die("screen capture failed (ImageMagick import + Xvfb required)")


def main() -> int:
    if not APP.exists():
        die(f"build the app first: {APP} is missing (run ./comebackwindow once)")
    xvfb = xvfb_binary()
    if xvfb is None:
        die("Xvfb not found (install xorg-x11-server-Xvfb)")

    # English UI needs the compiled catalogs next to the sources.
    if run(["make", "-s", "mo"], cwd=ROOT).returncode != 0:
        die("make mo failed (gettext/msgfmt required)")

    config_dir = TMP / "config"
    config_dir.mkdir()
    (config_dir / "config.ini").write_text(CONFIG, encoding="utf-8")

    env = os.environ.copy()
    env.pop("WAYLAND_DISPLAY", None)
    env["DISPLAY"] = DISPLAY
    env["GDK_BACKEND"] = "x11"
    env["GSK_RENDERER"] = "cairo"
    env["CBW_CONFIG_DIR"] = str(config_dir)
    env["CBW_DATA_DIR"] = str(ROOT)

    server = subprocess.Popen(
        [xvfb, DISPLAY, "-screen", "0", SCREEN, "-wr"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    app = None
    try:
        time.sleep(1.5)
        app = subprocess.Popen([str(APP)], env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
        time.sleep(3.0)

        # Ask the running instance to present the real settings window.
        run([str(APP), "--settings"], env=env, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        time.sleep(2.0)

        full = TMP / "full.png"
        capture_root(full)
        # The settings window is mapped at 0,0 on the bare Xvfb screen.
        magick(str(full), "-crop", "560x880+0+0", "+repage",
               str(TMP / "settings.png"))
        # Strip sits in the bottom-right corner of the virtual screen.
        magick(str(full), "-gravity", "SouthEast", "-crop", "260x170+0+0",
               "+repage", str(TMP / "strip.png"))

        OUT.mkdir(parents=True, exist_ok=True)
        wrap_svg(TMP / "strip.png", OUT / "app-strip.svg")
        wrap_svg(TMP / "settings.png", OUT / "app-settings.svg")
    finally:
        if app is not None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
        server.send_signal(signal.SIGTERM)
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
        shutil.rmtree(TMP, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
