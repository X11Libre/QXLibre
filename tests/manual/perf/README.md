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

The columns are:

- `p50_ms`, `p99_ms`: time per frame, including a round trip so the server's work counts.
- `cpu_ms_per_frame`: this process's CPU time. The X server's isn't included.
- `syncs_per_frame`: the plugin's blocking `QXcbConnection::sync()` calls. Only QXLibre can count
  these, and only with `QT_XLIBRE_DEBUG` set, so stock Qt's plugin shows `-`.
- `bytes_per_frame`: bytes sent to the X server, from `xcb_total_written()`. MIT-SHM image data isn't
  included, because it's shared rather than sent.

## Keeping a feature

A performance feature stays only if it improves p99 frame time or CPU per frame by at least 10% in
some scenario, and regresses none by more than 2%. Measure with a compositor on and off, and over
enough runs that the change is larger than the difference between runs below.

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
