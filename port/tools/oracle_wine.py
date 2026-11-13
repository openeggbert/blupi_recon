#!/usr/bin/env python3
"""Cross-build display oracle on Linux: a rebuilt historical program under Wine versus the SDL3 port.

The Linux counterpart of oracle_capture.ps1 (port/docs/ORACLE.md). It runs a historical *rebuild* (never an
original executable: its SHA-256 must not be one of targets.json's originals) under Wine, full screen on a
640x480 virtual X display (Xvfb), with the frame-capture proxy port/oracle/ddraw_proxy.cpp as its ddraw.dll
in front of Wine's own DirectDraw. At every CPixmap::Display the proxy hashes the back buffer's raw rows;
the port computes the same hash with --display-log. Both runs read the same config.def (Benchmark=0, the
requested TrueColor, MouseType=2: the graphic cursor would follow the X pointer, not the game) and the same
scripted input; frames are compared by display index.

    python3 port/tools/oracle_wine.py --rebuilt build/oracle-editions/rebuilt/sb1-blupi-us/BLUPI.EXE \\
        --data <game folder> --port build/port-linux/blupi-sdl3 --frames 2200 --color 16 \\
        --work build/oracle-editions/runs

Wine draws through its own ddraw and GDI, not Windows': a difference can be Wine's. Calibrate with a
program whose oracle matched on Windows (SB1 US, 2026-10-21) before reading other results.
Needs wine (32-bit), Xvfb, xdotool and i686-w64-mingw32-g++. Output: <work>/<name>-<depth>-<time>/ with
hist.log, port.log and compare.json.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

PORT = Path(__file__).resolve().parents[1]
ROOT = PORT.parent


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def originals():
    return set(re.findall(r"[0-9a-f]{64}", (ROOT / "targets.json").read_text(encoding="utf-8")))


def build_proxy(out):
    source = PORT / "oracle" / "ddraw_proxy.cpp"
    if out.exists() and out.stat().st_mtime >= source.stat().st_mtime:
        return
    out.parent.mkdir(parents=True, exist_ok=True)
    launcher = ["ccache"] if shutil.which("ccache") else []
    subprocess.run(launcher + ["i686-w64-mingw32-g++", "-std=c++17", "-O2", "-shared", "-static", "-Wl,--kill-at",
                    "-o", str(out), str(source), "-ldxguid", "-luuid"], check=True)


def find_config(game):
    for folder in game.iterdir():
        if folder.is_dir() and folder.name.lower() == "data":
            for f in folder.iterdir():
                if f.name.lower() == "config.def":
                    return f
    raise SystemExit("no data/config.def in the game folder")


def edit_config(path, color):
    text = path.read_bytes().decode("latin-1")
    lines = [l for l in text.replace("\r\n", "\n").split("\n") if l]
    def put(key, value):
        for i, line in enumerate(lines):
            if line.split("=", 1)[0].lower() == key.lower():
                lines[i] = f"{line.split('=', 1)[0]}={value}"
                return
        lines.append(f"{key}={value}")
    put("Benchmark", "0")
    put("TrueColor", str(color))
    put("MouseType", "2")
    path.write_bytes(("\r\n".join(lines) + "\r\n").encode("latin-1"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rebuilt", type=Path, required=True, help="the historical rebuild (built from source)")
    ap.add_argument("--data", type=Path, required=True, help="the game folder (only its subfolders are copied)")
    ap.add_argument("--port", type=Path, required=True, help="the port program of the same edition")
    ap.add_argument("--frames", type=int, default=2200, help="displays to compare (the port runs N - 11 frames)")
    ap.add_argument("--color", type=int, choices=(8, 16), default=16)
    ap.add_argument("--input", type=Path, help="scripted input, as the port's --input")
    ap.add_argument("--dump-every", type=int, default=0)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--display", default=":78")
    a = ap.parse_args()

    rebuilt = a.rebuilt.resolve()
    if sha256(rebuilt) in originals():
        raise SystemExit(f"refusing an original executable: {rebuilt}")
    for f in a.data.iterdir():
        if f.is_file() and f.stat().st_size == rebuilt.stat().st_size and sha256(f) == sha256(rebuilt):
            raise SystemExit(f"the rebuild equals the data folder's {f.name}: refusing")
    proxy = a.work.resolve() / "ddraw.dll"
    build_proxy(proxy)
    name = rebuilt.parent.name
    run = a.work.resolve() / f"{name}-{a.color}-{time.strftime('%Y%m%d-%H%M%S')}"
    game = run / "game"
    game.mkdir(parents=True)
    for f in a.data.iterdir():   # the data folders only: no program of the data folder is copied
        if f.is_dir():
            shutil.copytree(f, game / f.name)
    exe = game / (rebuilt.stem + ".EXE")
    shutil.copy(rebuilt, exe)
    shutil.copy(proxy, game / "ddraw.dll")
    config = find_config(game)
    edit_config(config, a.color)
    user = run / "port-user"
    (user / "data").mkdir(parents=True)
    shutil.copy(config, user / "data" / "config.def")
    for d in ("hist-frames", "port-frames"):
        (run / d).mkdir()

    prefix = a.work.resolve() / "wine"
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG="-all", DISPLAY=a.display,
               WINEDLLOVERRIDES="ddraw=n,b;mscoree,mshtml=",
               BLUPI_ORACLE_DDRAW="system", BLUPI_ORACLE_LOG=str(run / "hist.log"),
               BLUPI_ORACLE_MAX_FRAMES=str(a.frames), BLUPI_ORACLE_DEBUG=str(run / "hist-debug.txt"))
    if a.dump_every:
        env.update(BLUPI_ORACLE_DUMP_DIR=str(run / "hist-frames"), BLUPI_ORACLE_DUMP_EVERY=str(a.dump_every))
    if a.input:
        env["BLUPI_ORACLE_INPUT"] = str(a.input.resolve())
    receipt = {"executable": str(rebuilt), "sha256": sha256(rebuilt), "proxy_sha256": sha256(proxy),
               "data": str(a.data.resolve()), "frames": a.frames, "color_depth": a.color,
               "input": str(a.input) if a.input else None, "config": config.read_bytes().decode("latin-1"),
               "host": "Wine " + subprocess.run(["wine", "--version"], capture_output=True, text=True).stdout.strip(),
               "scope": "Runtime evidence under Wine's DirectDraw and GDI, not a strict/equivalence proof"}

    xvfb = subprocess.Popen(["Xvfb", a.display, "-screen", "0", "640x480x24", "-nolisten", "tcp"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.0)
        if not (prefix / "system.reg").exists():
            subprocess.run(["wineboot", "-i"], env=env, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run(["wineserver", "-w"], env=env, check=False)
        subprocess.run(["xdotool", "mousemove", "0", "0"], env=env, check=False)
        limit = int(a.frames * 0.05 * 2) + 60
        log = open(run / "hist-wine.log", "w")
        game_proc = subprocess.Popen(["wine", exe.name, "-nocd"], cwd=game, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            game_proc.wait(timeout=limit)
            receipt["forced_termination"] = False
        except subprocess.TimeoutExpired:
            receipt["forced_termination"] = True
            game_proc.kill()
            game_proc.wait()
        receipt["exit_code"] = game_proc.returncode
        subprocess.run(["wineserver", "-k"], env=env, check=False)
        log.close()
    finally:
        xvfb.terminate()
        xvfb.wait()

    command = [str(a.port.resolve()), "--headless", "--data", str(a.data.resolve()), "--user", str(user),
               "--frames", str(a.frames - 11), "--display-log", str(run / "port.log")]
    if a.dump_every:
        command += ["--display-dump", f"{run / 'port-frames'}:{a.dump_every}"]
    if a.input:
        command += ["--input", str(a.input.resolve())]
    receipt["port_exit_code"] = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    hist = (run / "hist.log").read_text().splitlines() if (run / "hist.log").exists() else []
    mine = (run / "port.log").read_text().splitlines() if (run / "port.log").exists() else []
    n = min(len(hist), len(mine))
    differ = [i for i in range(n) if hist[i] != mine[i]]
    receipt.update(hist_frames=len(hist), port_frames=len(mine), compared=n, identical_frames=n - len(differ),
                   first_difference=differ[0] if differ else -1, differing=differ[:50])
    receipt["status"] = "MATCH" if n == a.frames and not differ else ("DIFFERENT" if differ else "INCOMPLETE")
    (run / "compare.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(run)
    print(json.dumps({k: receipt[k] for k in ("status", "hist_frames", "port_frames", "identical_frames",
                                               "first_difference", "exit_code", "port_exit_code")}))
    return 0 if receipt["status"] == "MATCH" else 1


if __name__ == "__main__":
    sys.exit(main())
