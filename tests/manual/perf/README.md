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

## Keeping a feature

A performance feature stays only if it improves p99 frame time or CPU per frame by at least 10% in
some scenario, and regresses none by more than 2%. Measure with a compositor on and off, and over
enough runs that the change is larger than the difference between runs below. Include a paced run: a
feature that throttles to the display otherwise looks faster because it drops frames.

`rasterwindow` prints updates per second, the standard deviation of the update interval in ms, client
and server CPU in ms per second, and the screen's refresh rate.

## Results on real hardware, 2026-11-05

An AMD Navi 44 on the `amdgpu` driver, with glamor, page flipping enabled and `TearFree` on `auto`, at 60 Hz,
with XLibre 25.1.9, sonic-win and qtbase 6.12.0. It was a desktop in use, with a load average of up to 11 during
the runs, so differences between runs are large. Figures are medians, and the ranges are the lowest and highest run.

### Kept: update requests at the vblank

`QWindow::requestUpdate()` is delivered at the next vblank using `xcb_present_notify_msc()`, instead of Qt's 5 ms
timer. `QT_XLIBRE_NO_VBLANK_UPDATES=1` turns it off.

| `rasterwindow`, 5 s | Timer | Vblank | Change |
|---|---:|---:|---:|
| Compositor on, 5 runs: updates per second | 187.4 | 60.0 | |
| Client CPU, ms/s | 42.8 (34.5-44.0) | 16.8 (15.7-19.0) | -61% |
| Server CPU, ms/s | 101.0 (86.3-106.8) | 51.4 (45.7-59.1) | -49% |
| Interval standard deviation, ms | 0.077 | 0.073 | -5% |
| Compositor off, 10 runs: updates per second | 178.5 | 60.0 | |
| Client CPU, ms/s | 67.7 (51.1-75.9) | 31.1 (21.2-32.9) | -54% |
| Server CPU, ms/s | 166.6 (116.5-195.8) | 105.2 (64.8-110.5) | -37% |
| Interval standard deviation, ms | 0.198 | 0.119 | -40% |

- The saving comes from no longer drawing about two frames in three that a 60 Hz display can't show.
- It helps windows that animate with `requestUpdate()`, such as `QRasterWindow`. Qt Widgets don't use it, so they
  neither gain nor lose.
- Not measured on this GPU: Qt Quick and OpenGL windows, which were only tested for correct behaviour on `llvmpipe`, and
  Vulkan. Displays other than 60 Hz weren't tried. A display that's blanked slows these windows to the server's 1 Hz.

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
