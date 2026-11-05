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
  | Updates per second | 187 to 60 (-68%) | 179 to 60 (-66%) |
  | Application's processor use, % of one core | 4.3 to 1.7 (-61%) | 6.8 to 3.1 (-54%) |
  | X server's processor use, % of one core | 10.1 to 5.1 (-49%) | 16.7 to 10.5 (-37%) |
  | Variation in time between updates, ms | 0.077 to 0.073 (-5%) | 0.198 to 0.119 (-40%) |

  Measured on a 60 Hz display with an AMD Radeon graphics card and XLibre 25.1.9, over 5 runs with desktop effects on and
  10 with them off. The desktop was in use during the runs.
- Windows built with Qt Widgets respond faster, because drawing usually no longer has to stop and wait for the X server.
  Small updates gain the most. Processor use stayed about the same, and scrolling didn't clearly change. If you need
  to, you can turn this off with `QT_XLIBRE_NO_SHM_COMPLETION=1`.

  | Time per update in ms, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Small update, such as a blinking text cursor | 0.92 to 0.71 (-22%) | 0.69 to 0.42 (-35%) |
  | Slowest 1% of small updates | 1.34 to 0.97 (-26%) | 1.34 to 0.68 (-57%) |
  | Repainting a window, as most applications do | 1.90 to 1.72 (-10%) | 1.63 to 1.38 (-16%) |
  | Slowest 1% of those repaints | 2.94 to 2.20 (-24%) | 2.61 to 2.02 (-23%) |
  | Resizing a window | 2.63 to 2.13 (-19%) | 2.62 to 2.19 (-15%) |
  | Repainting a full HD window | 5.28 to 5.08 (-3%) | 4.81 to 4.49 (-7%) |

  Measured on the same machine, over 5 pairs of runs of 100 updates each. Times include one trip to the X server that
  the measuring tool adds to every update. Each percentage is the typical difference between a pair of runs, so it can
  differ slightly from the two times beside it.
- Behind the scenes: new automated tests for containers, popups, animation timing, drawing without waiting for the X
  server and the reported platform name, and a new tool that measures drawing performance.
