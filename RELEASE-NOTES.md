# QXLibre Release Notes

QXLibre lets Qt applications open windows on X11, and it's developed and tested for XLibre. It's based on the X11
support in Qt, and it keeps working if Qt removes that support. These notes list only what differs from Qt 6.12.0.

## Unreleased

- Qt applications can keep running on X11 even with a copy of Qt built without any X11 support. There's nothing to set
  up: they find QXLibre by themselves.
- QXLibre installs alongside your existing Qt without replacing any of its files, so you can try it one application at
  a time. If it's missing or doesn't match your Qt, applications go back to Qt's own X11 support.
- Applications don't need any changes, because QXLibre tells them they're running on Qt's usual X11 platform.
- Applications now run inside XLibre's Xnamespace containers, where they crash at startup with Qt's own X11 support.
  Windows, see-through windows and Qt Quick all work, and the clipboard is shared within a container and kept private
  from other containers.
- Typing in an application inside a container works with XLibre's current development version. It needs a newer server
  than XLibre 25.1.11 or 25.2.3, which don't deliver key presses into containers.
- On a Qt without X11 support, menus and drop-down lists still tell your window manager they're popups, so they keep
  behaving like popups.
- Pasting very large clipboard contents is now safe. Another application can no longer crash yours by announcing a huge
  paste, or by sending more than 2 GiB.
- Animations use about half the processor time, because Qt windows that animate themselves now draw in step with your
  display, instead of drawing about two frames in three that it can't show. Windows built with Qt Widgets aren't
  affected, and neither are windows drawn by the graphics card, as Qt Quick windows usually are. If you need to, you can
  turn this off with `QT_XLIBRE_NO_VBLANK_UPDATES=1`.

  | Animated window, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Updates per second | 169 to 60 (-64%) | 166 to 60 (-64%) |
  | Application's processor use, % of one core | 7.0 to 3.1 (-55%) | 6.2 to 2.9 (-54%) |
  | X server's processor use, % of one core | 16.2 to 8.6 (-47%) | 12.2 to 5.1 (-58%) |
  | Variation in time between updates, ms | 0.355 to 0.089 (-75%) | 0.320 to 0.074 (-77%) |

  Measured on a 60 Hz display with an AMD Radeon graphics card and XLibre 25.1.9, over 5 runs each way with desktop effects
  on and 5 with them off. The desktop was in use during the runs.
- Windows built with Qt Widgets respond faster, because drawing usually no longer has to stop and wait for the X server.
  Small updates gain the most. Processor use stayed about the same, and scrolling didn't clearly change. If you need
  to, you can turn this off with `QT_XLIBRE_NO_SHM_COMPLETION=1`.

  | Time per update in ms, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Small update, such as a blinking text cursor | 0.86 to 0.67 (-24%) | 0.69 to 0.43 (-38%) |
  | Slowest 1% of small updates | 1.07 to 0.80 (-27%) | 0.76 to 0.52 (-38%) |
  | Repainting a window, as most applications do | 1.91 to 1.62 (-14%) | 1.65 to 1.39 (-15%) |
  | Slowest 1% of those repaints | 2.42 to 2.24 (-10%) | 2.26 to 1.64 (-20%) |
  | Resizing a window | 2.54 to 2.05 (-17%) | 2.46 to 2.09 (-17%) |
  | Repainting a full HD window | 5.01 to 4.77 (-4%) | 4.59 to 4.26 (-6%) |

  Measured on the same machine, over 5 pairs of runs of 100 updates each. Times include one trip to the X server that
  the measuring tool adds to every update. Each percentage is the typical difference between a pair of runs, so it can
  differ slightly from the two times beside it.
- Behind the scenes: new automated tests for containers, popups, animation timing, drawing without waiting for the X
  server and the reported platform name, and a new tool and script that anyone can use to measure drawing performance on
  their own machine.
