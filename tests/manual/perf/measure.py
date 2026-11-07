#!/usr/bin/env python3
"""Compares QXLibre's performance features on and off, using qxlibre_perf. See README.md."""

import argparse
import csv
import os
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

SCENARIOS = ["partial", "update", "resize", "fullrepaint1080p", "scrolltext", "scrolllist"]
SKIP = 77  # what ctest is told means "skipped"


def median(values):
    return statistics.median(values) if values else float("nan")


def percent(before, after):
    return (after - before) / before * 100 if before else float("nan")


def signed(value):
    return "-" if value != value else f"{value:+.0f}%".replace("+0%", "0%").replace("-0%", "0%")


class Compositor:
    """Suspends KWin's or sonic-win's compositing. Other window managers can't be switched from here."""

    def __init__(self):
        self.qdbus = shutil.which("qdbus6") or shutil.which("qdbus")
        self.original = self.active()

    def active(self):
        if not self.qdbus:
            return None
        result = subprocess.run(
            [self.qdbus, "org.kde.KWin", "/Compositor", "org.freedesktop.DBus.Properties.Get",
             "org.kde.kwin.Compositing", "active"],
            capture_output=True, text=True)
        return {"true": True, "false": False}.get(result.stdout.strip())

    def set(self, active):
        if self.active() in (None, active):
            return self.active() == active
        subprocess.run(
            [self.qdbus, "org.kde.kglobalaccel", "/component/kwin",
             "org.kde.kglobalaccel.Component.invokeShortcut", "Suspend Compositing"],
            capture_output=True)
        subprocess.run(["sleep", "2"])
        return self.active() == active

    def restore(self):
        if self.original is not None:
            self.set(self.original)


class PrivateServer:
    """An Xvfb nobody else uses, so that a run doesn't touch the desktop."""

    def __init__(self):
        read_end, write_end = os.pipe()
        self.process = subprocess.Popen(
            ["Xvfb", "-displayfd", str(write_end), "-noreset", "-screen", "0", "3840x2160x24"],
            pass_fds=[write_end], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(write_end)
        with os.fdopen(read_end) as pipe:
            self.display = ":" + pipe.readline().strip()

    def stop(self):
        self.process.kill()
        self.process.wait()


def find_server_pid():
    """The X server, for its CPU time. None if it can't be told apart from another one."""
    display = os.environ.get("DISPLAY", "")
    candidates = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            name = (entry / "comm").read_text().strip()
            arguments = (entry / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        if name in ("X", "Xorg", "Xlibre", "XLibre", "Xvfb"):
            if display.encode() in arguments:
                return int(entry.name)
            candidates.append(int(entry.name))
    return candidates[0] if len(candidates) == 1 else None


class Runner:
    def __init__(self, options, display, server_pid):
        self.tool = str(options.tool or options.build / "tests/manual/perf/qxlibre_perf")
        self.out = options.out
        self.failures = 0
        self.environment = dict(os.environ, DISPLAY=display, QT_QPA_PLATFORM="xlibre",
                                QT_PLUGIN_PATH=str(options.plugins or options.build / "plugins"),
                                QT_XLIBRE_DEBUG="1")
        self.environment.pop("QXLIBRE_PERF_PACE_MS", None)
        for variable in ("QT_XLIBRE_NO_VBLANK_UPDATES", "QT_XLIBRE_NO_SHM_COMPLETION"):
            self.environment.pop(variable, None)
        if server_pid:
            self.environment["QXLIBRE_PERF_SERVER_PID"] = str(server_pid)

    def run(self, arguments, extra):
        with open(self.out / "stderr.log", "a") as log:
            try:
                result = subprocess.run([self.tool, *arguments], env={**self.environment, **extra},
                                        stdout=subprocess.PIPE, stderr=log, text=True, timeout=120)
            except subprocess.TimeoutExpired:
                result = None
        lines = result.stdout.strip().splitlines() if result and result.returncode == 0 else []
        if not lines:
            self.failures += 1
            print(f"failed: {' '.join(arguments)} {extra}", file=sys.stderr)
            return None
        return lines[-1].split(",")


def write_csv(path, header, rows):
    with open(path, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(header)
        writer.writerows(rows)


def measure_vblank(runner, options, label):
    """rows: (mode, updates per second, interval deviation, client CPU, server CPU)"""
    rows = []
    for run in range(1, options.runs + 1):
        for mode, extra in (("timer", {"QT_XLIBRE_NO_VBLANK_UPDATES": "1"}), ("vblank", {})):
            fields = runner.run(["rasterwindow", str(options.seconds)], extra)
            if fields:
                rows.append([mode, run, *map(float, fields[3:7])])
    write_csv(options.out / f"vblank-{label}.csv",
              ["mode", "run", "updates_per_second", "interval_sd_ms", "client_cpu_ms_per_s",
               "server_cpu_ms_per_s"], rows)
    return rows


def measure_shm(runner, options, label):
    """rows: (mode, run, scenario, p50, p99, CPU, syncs, server CPU)"""
    rows = []
    extra_pace = {"QXLIBRE_PERF_PACE_MS": str(options.pace)}
    for run in range(1, options.runs + 1):
        for scenario in SCENARIOS:
            for mode, extra in (("sync", {"QT_XLIBRE_NO_SHM_COMPLETION": "1"}), ("events", {})):
                fields = runner.run([scenario, str(options.frames)], {**extra_pace, **extra})
                if fields:
                    rows.append([mode, run, scenario, float(fields[3]), float(fields[4]),
                                 float(fields[5]), float(fields[6]), float(fields[8])])
    write_csv(options.out / f"shm-{label}.csv",
              ["mode", "run", "scenario", "p50_ms", "p99_ms", "cpu_ms_per_frame", "syncs_per_frame",
               "server_cpu_ms_per_frame"], rows)
    return rows


def vblank_table(rows, title):
    def column(mode, index):
        return [row[index] for row in rows if row[0] == mode]

    def ranged(mode, index):
        values = column(mode, index)
        return f"{median(values):.1f} ({min(values):.1f}-{max(values):.1f})" if values else "-"

    def change(index):
        return signed(percent(median(column("timer", index)), median(column("vblank", index))))

    runs = len(column("vblank", 2))
    return [
        f"| {title}, {runs} runs | Timer | Vblank | Change |",
        "|---|---:|---:|---:|",
        f"| Updates per second | {median(column('timer', 2)):.1f} | {median(column('vblank', 2)):.1f} | |",
        f"| Client CPU, ms/s | {ranged('timer', 4)} | {ranged('vblank', 4)} | {change(4)} |",
        f"| Server CPU, ms/s | {ranged('timer', 5)} | {ranged('vblank', 5)} | {change(5)} |",
        f"| Interval standard deviation, ms | {median(column('timer', 3)):.3f} "
        f"| {median(column('vblank', 3)):.3f} | {change(3)} |",
    ]


def shm_table(rows, title):
    lines = [f"| {title} | Syncs | Median | Change | Lower | p99 | Change | Lower |",
             "|---|---:|---:|---:|---:|---:|---:|---:|"]
    cpu_notes = []
    for scenario in SCENARIOS:
        pairs = {}
        for row in rows:
            if row[2] == scenario:
                pairs.setdefault(row[1], {})[row[0]] = row
        pairs = [pair for pair in pairs.values() if len(pair) == 2]
        if not pairs:
            continue

        def before(index):
            return median([pair["sync"][index] for pair in pairs])

        def after(index):
            return median([pair["events"][index] for pair in pairs])

        def cells(index):
            changes = [percent(pair["sync"][index], pair["events"][index]) for pair in pairs]
            lower = sum(pair["events"][index] < pair["sync"][index] for pair in pairs)
            return (f"{before(index):.2f} to {after(index):.2f} | {signed(median(changes))} "
                    f"| {lower}/{len(pairs)}")

        lines.append(f"| `{scenario}` | {before(6):.2f} to {after(6):.2f} | {cells(3)} | {cells(4)} |")
        cpu_notes.append(f"`{scenario}` {before(5) + before(7):.2f} to {after(5) + after(7):.2f}")
    return lines + ["", "Client plus server CPU, ms per frame: " + ", ".join(cpu_notes) + "."]


def load_average():
    return Path("/proc/loadavg").read_text().split(" ")[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"),
                        help="the build directory, configured with -DQT_BUILD_TESTS=ON")
    parser.add_argument("--tool", type=Path, help="qxlibre_perf, if not in the build directory")
    parser.add_argument("--plugins", type=Path, help="Qt plugin directory, if not in the build directory")
    parser.add_argument("--out", type=Path, help="where results go; a new temporary directory by default")
    parser.add_argument("--runs", type=int, default=5, help="pairs of runs of each thing measured")
    parser.add_argument("--frames", type=int, default=100, help="frames per run of a scenario")
    parser.add_argument("--pace", type=int, default=20, help="milliseconds between frames")
    parser.add_argument("--seconds", type=int, default=5, help="seconds per rasterwindow run")
    parser.add_argument("--xvfb", action="store_true",
                        help="use a private Xvfb, which leaves the desktop alone but says nothing about a real GPU")
    parser.add_argument("--both-compositor-states", action="store_true",
                        help="measure with KWin or sonic-win compositing on, then off, then put it back")
    options = parser.parse_args()

    server = None
    compositor = None
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(1))
    try:
        if options.xvfb:
            if not shutil.which("Xvfb"):
                print("Needs Xvfb", file=sys.stderr)
                return SKIP
            server = PrivateServer()
            display, server_pid = server.display, server.process.pid
            states = [("none", None)]
        else:
            display = os.environ.get("DISPLAY")
            if not display:
                print("Needs DISPLAY, or --xvfb", file=sys.stderr)
                return SKIP
            server_pid = find_server_pid()
            if not server_pid:
                print("Can't tell which X server this is, so its CPU time will read 0", file=sys.stderr)
            compositor = Compositor()
            if options.both_compositor_states:
                if compositor.original is None:
                    print("--both-compositor-states needs KWin or sonic-win", file=sys.stderr)
                    return 2
                states = [("on", True), ("off", False)]
            elif compositor.original is None:
                states = [("unknown", None)]
            else:
                states = [("on" if compositor.original else "off", None)]

        options.out = options.out or Path(tempfile.mkdtemp(prefix="qxlibre-perf-"))
        options.out.mkdir(parents=True, exist_ok=True)
        runner = Runner(options, display, server_pid)

        report = []
        for label, wanted in states:
            if wanted is not None and not compositor.set(wanted):
                print(f"Couldn't turn the compositor {label}", file=sys.stderr)
                return 1
            title = "Compositor " + label
            start = load_average()
            vblank = measure_vblank(runner, options, label)
            shm = measure_shm(runner, options, label)
            report += [*vblank_table(vblank, title), "", *shm_table(shm, title), "",
                       f"Load average {start} at the start and {load_average()} at the end.", ""]
    finally:
        if compositor:
            compositor.restore()
        if server:
            server.stop()

    report = "\n".join(report)
    (options.out / "summary.md").write_text(report)
    print(report)
    print(f"Raw results and the summary are in {options.out}")
    if runner.failures:
        print(f"{runner.failures} runs failed, see {options.out / 'stderr.log'}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
