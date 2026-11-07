# Performance baseline

`qxlibre_perf` measures software-rendered window updates. It prints one CSV line per run, and it's
built with `-DQT_BUILD_TESTS=ON`.

```sh
QT_XLIBRE_DEBUG=1 QT_QPA_PLATFORM=xlibre qxlibre_perf <scenario> [frames]
```

| Scenario | What it does |
|---|---|
| `fullrepaint1080p`, `fullrepaint4k` | Repaints a whole window of that size |
| `scrolltext` | Scrolls a long `QTextEdit`, which uses the scrolled-region path |
| `scrolllist` | Scrolls a long `QListView`, which uses the scrolled-region path |
| `partial` | Repaints a rectangle the size of a text cursor |
| `resize` | Resizes and repaints a window |
| `update` | Repaints a window with `QWidget::update()`, as most applications do |
| `rasterwindow` | Animates a `QRasterWindow` with `requestUpdate()`. Its second argument is in seconds, and its own columns are below |
| `quick` | Animates a Qt Quick scene, drawn as `QT_QUICK_BACKEND` chooses. It takes seconds and has the same columns as `rasterwindow`. Only built if Qt Quick is found |

The columns are:

- `p50_ms`, `p99_ms`: time per frame, including a round trip so the server's work counts.
- `cpu_ms_per_frame`: this process's CPU time. The X server's isn't included.
- `syncs_per_frame`: the plugin's blocking `QXcbConnection::sync()` calls. Only QXLibre can count
  these, and only with `QT_XLIBRE_DEBUG` set, so stock Qt's plugin shows `-`.
- `bytes_per_frame`: bytes sent to the X server, from `xcb_total_written()`. MIT-SHM image data isn't
  included, because it's shared rather than sent.
- `server_cpu_ms_per_frame`: the X server's CPU time. Set `QXLIBRE_PERF_SERVER_PID` to its process ID,
  or this column is meaningless. Work moved into the server can look like a saving without it.

Set `QXLIBRE_PERF_PACE_MS` to wait that long between frames, from an event loop. Without it, the tool draws far faster than
the display refreshes.

## Measuring a feature on and off

`measure.py` runs interleaved pairs of runs with each feature on and off, and prints the tables below. It needs Python 3
and a build with `-DQT_BUILD_TESTS=ON`. Run it from your X session, with your Qt's libraries findable:

```sh
tests/manual/perf/measure.py --build build
```

It opens windows on your desktop for about 6 minutes, without taking the keyboard focus. Leave the machine alone and close
anything busy such as videos, because other work shows up in the figures. If the tool was built with Qt Quick, it also
measures Qt Quick on the default backend and on the software backend, with both features together on and off.

| Option | What it does |
|---|---|
| `--both-compositor-states` | Measures with compositing on, then off, then puts it back, taking about 12 minutes. Needs KWin or sonic-win. With another window manager, switch it yourself and run the script twice |
| `--xvfb` | Uses a private Xvfb, which leaves your desktop alone but says nothing about a real GPU. Its vblank is faked, so an animation driven by a timer can look far slower with vblank updates than it does on a real display |
| `--runs`, `--frames`, `--pace`, `--seconds` | The pairs of runs, frames per scenario run, milliseconds between frames and seconds per `rasterwindow` run. The defaults are 5, 100, 20 and 5 |
| `--out` | Where the raw CSV files, `stderr.log` and `summary.md` go. By default, a new temporary directory |

It finds the X server to read its CPU time, and warns if it can't. It exits with an error if any run failed. `ctest` runs it
briefly on a private Xvfb as `qxlibre_perf_script`, only to check that it still works.

## Keeping a feature

A performance feature stays only if it improves p99 frame time or CPU per frame by at least 10% in
some scenario, and regresses none by more than 2%. Measure with a compositor on and off, and over
enough runs that the change is larger than the difference between runs below. Include a paced run: a
feature that throttles to the display otherwise looks faster because it drops frames.

`rasterwindow` prints updates per second, the standard deviation of the update interval in ms, client
and server CPU in ms per second, and the screen's refresh rate.

## Results on real hardware, 2026-11-07

An AMD Navi 44 on the `amdgpu` driver, with glamor, page flipping enabled and `TearFree` on `auto`, at 60 Hz,
with XLibre 25.1.9, sonic-win and Qt 6.12.0, using `measure.py --both-compositor-states`. It was a quiet desktop, with a load
average of up to 1.8 during the runs. Figures are medians, and the ranges are the lowest and highest run.

### Kept: update requests at the vblank

On raster windows, `QWindow::requestUpdate()` is delivered at the next vblank using `xcb_present_notify_msc()`, instead
of Qt's 5 ms timer. `QT_XLIBRE_NO_VBLANK_UPDATES=1` turns it off.

| `rasterwindow`, 5 s | Timer | Vblank | Change |
|---|---:|---:|---:|
| Compositor on, 5 runs: updates per second | 168.0 | 60.0 | |
| Client CPU, ms/s | 72.1 (70.9-73.6) | 31.3 (30.5-32.0) | -57% |
| Server CPU, ms/s | 162.1 (158.1-168.5) | 83.8 (81.9-84.2) | -48% |
| Interval standard deviation, ms | 0.352 | 0.078 | -78% |
| Compositor off, 5 runs: updates per second | 163.9 | 60.0 | |
| Client CPU, ms/s | 66.8 (65.6-68.9) | 29.5 (29.0-31.6) | -56% |
| Server CPU, ms/s | 126.3 (123.8-128.4) | 52.6 (51.4-53.7) | -58% |
| Interval standard deviation, ms | 0.295 | 0.070 | -76% |

- The saving comes from no longer drawing about two frames in three that a 60 Hz display can't show.
- It helps windows that animate with `requestUpdate()`, such as `QRasterWindow`. Qt Widgets don't use it, so they
  neither gain nor lose.
- Not measured on this GPU: Vulkan. Displays other than 60 Hz weren't tried. A display that's blanked slows these windows to the server's 1 Hz.

### Kept: fewer round trips

Each upload's last `xcb_shm_put_image()` asks for a completion event. Painting over memory the server was reading calls
`sync()` only if neither that upload's event nor a later one has arrived. `QT_XLIBRE_NO_SHM_COMPLETION=1` turns it off.

Each row compares 5 interleaved pairs of runs of 100 frames at `QXLIBRE_PERF_PACE_MS=20`, with vblank updates on. It shows
blocking syncs per frame and median frame time and p99 in ms, off against on. `lower` counts the pairs where the frame time
was lower with completion events. Percentages are the median difference between pairs.

| Compositor on | Syncs | Median | Change | Lower | p99 | Change | Lower |
|---|---:|---:|---:|---:|---:|---:|---:|
| `partial` | 1.01 to 0.01 | 0.93 to 0.69 | -27% | 5/5 | 1.09 to 0.78 | -29% | 4/5 |
| `update` | 1.01 to 0.01 | 1.90 to 1.66 | -13% | 5/5 | 2.52 to 2.17 | -16% | 5/5 |
| `resize` | 2.00 to 1.00 | 2.43 to 2.11 | -13% | 5/5 | 8.77 to 7.97 | -26% | 4/5 |
| `fullrepaint1080p` | 1.00 to 0.00 | 4.86 to 4.65 | -5% | 5/5 | 6.56 to 6.09 | -8% | 5/5 |
| `scrolltext` | 1.04 to 1.00 | 2.66 to 2.00 | -22% | 4/5 | 51.45 to 51.63 | 0% | 2/5 |
| `scrolllist` | 1.00 to 0.99 | 2.50 to 2.55 | +5% | 1/5 | 10.70 to 10.61 | +2% | 2/5 |

| Compositor off | Syncs | Median | Change | Lower | p99 | Change | Lower |
|---|---:|---:|---:|---:|---:|---:|---:|
| `partial` | 1.01 to 0.00 | 0.68 to 0.43 | -38% | 5/5 | 0.76 to 0.48 | -35% | 5/5 |
| `update` | 1.01 to 0.00 | 1.63 to 1.38 | -15% | 5/5 | 2.05 to 1.67 | -25% | 5/5 |
| `resize` | 2.00 to 0.99 | 2.49 to 2.16 | -15% | 5/5 | 9.61 to 8.60 | -11% | 4/5 |
| `fullrepaint1080p` | 1.00 to 0.00 | 4.50 to 4.21 | -7% | 5/5 | 6.05 to 5.06 | -17% | 5/5 |
| `scrolltext` | 1.05 to 1.00 | 4.82 to 2.50 | -6% | 4/5 | 50.87 to 51.00 | 0% | 3/5 |
| `scrolllist` | 1.00 to 0.99 | 2.63 to 2.98 | -1% | 3/5 | 10.24 to 10.14 | -1% | 3/5 |

- The tool makes its own round trip each frame, so these frame times still include one.
- Client plus server CPU per frame fell in the scenarios that lost their sync. `update` went from 2.67 to 2.48 ms with the
  compositor on and 2.14 to 1.98 ms with it off.
- `scrolltext` and `scrolllist` keep their sync, so differences there show the noise. `scrolltext` CPU fell by 15% in both
  compositor states. `scrolllist` CPU rose by 18% and 21%, but an earlier run the same day measured +4% and -20%, so that isn't
  a stable result.
- A frame painted before that event or a later one has been handled still calls `sync()`, as before.

### Qt Quick: no gain

Both features together, off against on, over 5 runs each. The default backend used OpenGL on `radeonsi`, with the threaded
render loop.

| Qt Quick, compositor on | Off | On | Change |
|---|---:|---:|---:|
| Default backend: updates per second | 60.0 | 60.0 | |
| Client CPU, ms/s | 37.0 (34.7-38.4) | 38.0 (36.9-39.6) | +3% |
| Server CPU, ms/s | 51.4 (47.6-80.0) | 53.3 (51.4-55.2) | +4% |
| Interval standard deviation, ms | 0.129 | 0.111 | -14% |
| Software backend: updates per second | 62.7 | 58.3 | |
| Client CPU, ms/s | 31.9 (30.9-32.7) | 35.5 (33.0-36.9) | +11% |
| Server CPU, ms/s | 80.0 (77.9-81.9) | 78.1 (73.7-80.0) | -2% |
| Interval standard deviation, ms | 0.369 | 2.826 | +666% |

| Qt Quick, compositor off | Off | On | Change |
|---|---:|---:|---:|
| Default backend: updates per second | 60.0 | 60.0 | |
| Client CPU, ms/s | 35.9 (35.3-36.7) | 36.4 (20.0-37.5) | +1% |
| Server CPU, ms/s | 24.8 (23.2-35.8) | 25.2 (24.8-34.4) | +2% |
| Interval standard deviation, ms | 0.104 | 0.101 | -3% |
| Software backend: updates per second | 62.7 | 59.6 | |
| Client CPU, ms/s | 30.7 (29.7-31.8) | 34.9 (31.5-35.2) | +14% |
| Server CPU, ms/s | 36.2 (35.8-38.1) | 37.9 (36.2-41.9) | +5% |
| Interval standard deviation, ms | 0.380 | 1.391 | +266% |

- On the default backend neither feature is used, and the small differences are within the ranges.
- The software backend does use vblank updates, and its result breaks the "regresses none by more than 2%" rule above. It asks
  for updates from Qt's 16 ms animation timer rather than from its paint, which may be why, but that hasn't been confirmed.
- On Xvfb a window driven by a 16 ms timer fell to about 43 updates per second with vblank updates, while on this display it
  reached 56-59, so use a real display for this.

## Baseline, 2026-11-05

QXLibre at Qt 6.12.0, in a CachyOS VM with 8 vCPUs and 8 GB, XLibre 25.1.9 and `llvmpipe`. Each row is
one run of 300 frames, except that real-display figures show two runs. Nothing here measures a real
GPU, vblank timing or page flips.

### Xvfb at 3840x2160

| Scenario | Plugin | p50 ms | p99 ms | CPU ms | Syncs | Bytes |
|---|---|---:|---:|---:|---:|---:|
| `fullrepaint1080p` | `xcb` | 2.717 | 3.271 | 1.241 | - | 84 |
| `fullrepaint1080p` | `xlibre` | 1.524 | 2.029 | 0.692 | 1.00 | 84 |
| `fullrepaint4k` | `xcb` | 10.924 | 12.076 | 6.626 | - | 84 |
| `fullrepaint4k` | `xlibre` | 11.184 | 12.819 | 6.759 | 1.00 | 84 |
| `scrolltext` | `xcb` | 1.283 | 37.589 | 1.855 | - | 208 |
| `scrolltext` | `xlibre` | 1.361 | 39.327 | 1.976 | 1.00 | 208 |
| `scrolllist` | `xcb` | 1.495 | 1.704 | 1.352 | - | 216 |
| `scrolllist` | `xlibre` | 1.662 | 2.285 | 1.380 | 1.00 | 216 |
| `partial` | `xcb` | 0.047 | 0.058 | 0.030 | - | 84 |
| `partial` | `xlibre` | 0.064 | 0.121 | 0.044 | 1.00 | 84 |
| `resize` | `xcb` | 0.674 | 1.967 | 0.583 | - | 519 |
| `resize` | `xlibre` | 0.648 | 1.773 | 0.566 | 2.00 | 519 |

### Real display at 1280x800, with the compositor on

| Scenario | Plugin | p50 ms | p99 ms | CPU ms | Syncs |
|---|---|---:|---:|---:|---:|
| `fullrepaint1080p` | `xcb` | 1.039 | 13.461 | 0.537 | - |
| `fullrepaint1080p` | `xlibre` | 0.632, 1.120 | 13.807, 12.932 | 0.303, 0.556 | 0.66, 0.86 |
| `scrolltext` | `xcb` | 3.488, 2.319 | 79.867, 80.004 | 3.078, 2.051 | - |
| `scrolltext` | `xlibre` | 2.705, 3.165 | 80.019, 93.226 | 2.462, 3.353 | 0.89, 0.96 |
| `scrolllist` | `xcb` | 3.564, 6.501 | 79.161, 79.154 | 2.470, 5.469 | - |
| `scrolllist` | `xlibre` | 3.852, 4.570 | 70.203, 77.211 | 2.478, 3.262 | 1.00, 1.00 |
| `partial` | `xcb` | 0.046, 0.048 | 1.228, 1.018 | 0.033, 0.035 | - |
| `partial` | `xlibre` | 0.047, 0.140 | 1.742, 3.602 | 0.035, 0.077 | 0.99, 0.53 |
| `resize` | `xcb` | 0.794, 2.421 | 27.162, 42.749 | 0.672, 0.894 | - |
| `resize` | `xlibre` | 1.308, 2.071 | 39.831, 44.583 | 0.687, 0.822 | 1.86, 2.62 |

The first `fullrepaint1080p` run under `xcb` is left out: it recorded 0.014 ms and 4 bytes per frame,
so the window wasn't being drawn.

### What this shows

- QXLibre and Qt's `xcb` plugin perform about the same. Most differences are within the variation
  between runs of the same plugin, which on the real display can be a factor of two.
- Software-rendered frames usually make one blocking `sync()` round trip each, and resizing makes two.
- These figures are too noisy to judge a 10% change from single runs, so compare several runs of each.
