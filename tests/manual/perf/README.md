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

Two smaller tools show how animations meet the vblank. Neither is a ctest.

- `qxlibre_animation_probe <timer ms> <seconds>` logs each animation tick and frame of a Qt Quick scene, and sets Qt's
  animation timer to that interval. It prints frames per second, the variation between them, CPU, ticks per second and the
  late frames. Set `QT_QUICK_BACKEND=software` to use the software backend.
- `qxlibre_timer_window <timer ms>` times a window that requests updates from a timer rather than from its paint.

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
| Compositor on, 5 runs: updates per second | 168.2 | 60.0 | |
| Client CPU, ms/s | 70.8 (68.8-72.9) | 31.2 (28.1-31.6) | -56% |
| Server CPU, ms/s | 170.6 (166.3-175.3) | 92.7 (83.8-94.7) | -46% |
| Interval standard deviation, ms | 0.350 | 0.082 | -77% |
| Compositor off, 5 runs: updates per second | 164.2 | 60.0 | |
| Client CPU, ms/s | 65.9 (61.4-69.7) | 30.1 (29.2-30.4) | -54% |
| Server CPU, ms/s | 122.2 (120.0-127.6) | 53.3 (48.4-53.3) | -56% |
| Interval standard deviation, ms | 0.299 | 0.069 | -77% |

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
| `partial` | 1.01 to 0.01 | 0.93 to 0.68 | -24% | 5/5 | 1.11 to 0.81 | -25% | 5/5 |
| `update` | 1.01 to 0.01 | 1.94 to 1.68 | -13% | 5/5 | 2.47 to 2.19 | -13% | 5/5 |
| `resize` | 2.00 to 1.00 | 2.33 to 2.08 | -7% | 5/5 | 7.17 to 8.10 | +44% | 2/5 |
| `fullrepaint1080p` | 1.00 to 0.00 | 5.04 to 4.71 | -5% | 5/5 | 6.28 to 5.72 | -11% | 5/5 |
| `scrolltext` | 1.06 to 1.01 | 2.34 to 2.10 | -4% | 3/5 | 51.13 to 50.67 | -1% | 4/5 |
| `scrolllist` | 1.01 to 1.00 | 2.51 to 2.68 | +5% | 0/5 | 10.47 to 10.94 | +6% | 2/5 |

| Compositor off | Syncs | Median | Change | Lower | p99 | Change | Lower |
|---|---:|---:|---:|---:|---:|---:|---:|
| `partial` | 1.01 to 0.00 | 0.68 to 0.42 | -39% | 5/5 | 1.12 to 0.48 | -57% | 4/5 |
| `update` | 1.01 to 0.00 | 1.64 to 1.35 | -17% | 5/5 | 1.97 to 1.63 | -14% | 5/5 |
| `resize` | 2.00 to 0.99 | 2.44 to 2.19 | -11% | 5/5 | 10.94 to 8.38 | -18% | 5/5 |
| `fullrepaint1080p` | 1.00 to 0.00 | 4.54 to 4.26 | -6% | 5/5 | 6.13 to 5.40 | -10% | 4/5 |
| `scrolltext` | 1.04 to 1.00 | 2.36 to 2.21 | -7% | 3/5 | 51.66 to 50.69 | -2% | 3/5 |
| `scrolllist` | 1.00 to 0.99 | 2.66 to 4.98 | +97% | 2/5 | 10.44 to 10.28 | -2% | 3/5 |

- The tool makes its own round trip each frame, so these frame times still include one.
- Client plus server CPU per frame fell in the scenarios that lost their sync. `update` went from 2.90 to 2.83 ms with the
  compositor on and 2.15 to 1.96 ms with it off.
- `scrolltext` and `scrolllist` keep their sync, so differences there show the noise. Over 15 pairs with the compositor on,
  `scrolllist` client plus server CPU went from 5.44 to 5.39 ms and was lower in 7 pairs, so its +97% above isn't a stable result.
- `resize` p99 with the compositor on got worse in this run, by a median of 44% across pairs, and an earlier run the same day
  measured -26%. It hasn't been measured over more pairs.
- A frame painted before that event or a later one has been handled still calls `sync()`, as before.

### Kept: animations driven by the vblank

Qt Quick's software backend ticks animations from a 16 ms timer of its own, which made frames late once updates followed the
vblank. A late frame's gap contains two or more ticks, so the exact mechanism isn't known, but a faster timer removes the late
frames, and so does having no separate timer. With this feature, each delivered frame advances the animations, and a 32 ms
timer keeps them going when no frames come. `QT_XLIBRE_NO_VBLANK_ANIMATIONS=1` turns it off.

It's only used by the software backend, whose render loop installs no animation driver. Other render loops install their own,
including the software backend's `QSG_RENDER_LOOP=threaded`. Applications that animate from their own `QTimer` don't gain.

| Software backend, 5 s, compositor on, 10 runs | Vblank off | Vblank on, feature off | Vblank on, feature on |
|---|---:|---:|---:|
| Updates per second | 62.6 | 58.4 | 60.0 |
| Interval standard deviation, ms | 0.469 | 2.707 | 0.114 |
| Client CPU, ms/s | 31.2 (30.7-32.9) | 34.8 (33.3-36.2) | 32.7 (32.2-34.4) |
| Server CPU, ms/s | 82.0 (78.1-88.4) | 84.0 (80.0-87.6) | 82.2 (80.0-89.5) |
| Client plus server CPU, ms/s | 112.8 | 119.0 | 114.8 |

With the same scene, five 5 s runs each, the late frames per run were 7-9 with the 16 ms timer, and 0 with a 4 ms timer, which
used 43-46 ms/s of client CPU, or with this feature.

### Qt Quick: both features together

Off against on, over 5 runs each, with the feature above active on the software backend. The default backend used OpenGL on
`radeonsi`, with the threaded render loop.

| Qt Quick, compositor on | Off | On | Change |
|---|---:|---:|---:|
| Default backend: updates per second | 60.0 | 60.0 | |
| Client CPU, ms/s | 37.8 (36.0-38.8) | 38.2 (36.4-39.3) | +1% |
| Server CPU, ms/s | 59.2 (56.9-64.1) | 61.1 (55.2-64.8) | +3% |
| Interval standard deviation, ms | 0.119 | 0.109 | -8% |
| Software backend: updates per second | 62.6 | 60.0 | |
| Client CPU, ms/s | 31.7 (29.2-32.1) | 33.7 (33.0-35.2) | +6% |
| Server CPU, ms/s | 87.6 (80.6-90.5) | 86.3 (85.7-93.4) | -1% |
| Interval standard deviation, ms | 0.382 | 0.117 | -69% |

| Qt Quick, compositor off | Off | On | Change |
|---|---:|---:|---:|
| Default backend: updates per second | 60.0 | 60.0 | |
| Client CPU, ms/s | 37.0 (35.0-38.5) | 36.6 (35.7-38.0) | -1% |
| Server CPU, ms/s | 26.8 (24.8-26.9) | 22.9 (21.1-25.3) | -15% |
| Interval standard deviation, ms | 0.102 | 0.102 | 0% |
| Software backend: updates per second | 62.7 | 60.0 | |
| Client CPU, ms/s | 29.9 (29.6-32.0) | 30.8 (29.7-32.1) | +3% |
| Server CPU, ms/s | 36.2 (34.3-38.1) | 34.3 (33.7-35.8) | -5% |
| Interval standard deviation, ms | 0.398 | 0.126 | -68% |

- On the default backend neither feature is used, and the small differences are within the ranges, apart from the -15%, which
  hasn't been explained.
- On the software backend, client plus server CPU changed by +0.6% with the compositor on and -1.5% with it off, but client CPU
  alone is still over the "regresses none by more than 2%" rule above.
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
