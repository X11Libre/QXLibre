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
  | Updates per second | 168 to 60 (-64%) | 164 to 60 (-63%) |
  | Application's processor use, % of one core | 7.2 to 3.1 (-57%) | 6.7 to 3.0 (-56%) |
  | X server's processor use, % of one core | 16.2 to 8.4 (-48%) | 12.6 to 5.3 (-58%) |
  | Variation in time between updates, ms | 0.352 to 0.078 (-78%) | 0.295 to 0.070 (-76%) |

  Measured on a 60 Hz display with an AMD Radeon graphics card and XLibre 25.1.9, over 5 runs each way with desktop effects
  on and 5 with them off. The desktop was in use during the runs.
- Windows built with Qt Widgets respond faster, because drawing usually no longer has to stop and wait for the X server.
  Small updates gain the most. Processor use stayed about the same, and scrolling didn't clearly change. If you need
  to, you can turn this off with `QT_XLIBRE_NO_SHM_COMPLETION=1`.

  | Time per update in ms, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Small update, such as a blinking text cursor | 0.93 to 0.69 (-27%) | 0.68 to 0.43 (-38%) |
  | Slowest 1% of small updates | 1.09 to 0.78 (-29%) | 0.76 to 0.48 (-35%) |
  | Repainting a window, as most applications do | 1.90 to 1.66 (-13%) | 1.63 to 1.38 (-15%) |
  | Slowest 1% of those repaints | 2.52 to 2.17 (-16%) | 2.05 to 1.67 (-25%) |
  | Resizing a window | 2.43 to 2.11 (-13%) | 2.49 to 2.16 (-15%) |
  | Repainting a full HD window | 4.86 to 4.65 (-5%) | 4.50 to 4.21 (-7%) |

  Measured on the same machine, over 5 pairs of runs of 100 updates each. Times include one trip to the X server that
  the measuring tool adds to every update. Each percentage is the typical difference between a pair of runs, so it can
  differ slightly from the two times beside it.
- QML applications don't gain from these two changes. Those drawn by the graphics card, as most are, run as before. Those
  using Qt Quick's software drawing (`QT_QUICK_BACKEND=software`) use a little more processor time, and the time between
  their updates becomes less even.

  | Animated QML window, both changes off to on | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Drawn by the graphics card: updates per second | 60 to 60 | 60 to 60 |
  | Application's processor use, % of one core | 3.7 to 3.8 (+3%) | 3.6 to 3.6 (+1%) |
  | X server's processor use, % of one core | 5.1 to 5.3 (+4%) | 2.5 to 2.5 (+2%) |
  | Variation in time between updates, ms | 0.129 to 0.111 (-14%) | 0.104 to 0.101 (-3%) |
  | Software drawing: updates per second | 63 to 58 (-7%) | 63 to 60 (-5%) |
  | Application's processor use, % of one core | 3.2 to 3.6 (+11%) | 3.1 to 3.5 (+14%) |
  | X server's processor use, % of one core | 8.0 to 7.8 (-2%) | 3.6 to 3.8 (+5%) |
  | Variation in time between updates, ms | 0.369 to 2.826 (+666%) | 0.380 to 1.391 (+266%) |

  Measured on the same machine, over 5 runs each way. A 60 Hz display can't show the updates above 60 per second in the
  software drawing rows.
- Behind the scenes: new automated tests for containers, popups, animation timing, drawing without waiting for the X
  server and the reported platform name, and a new tool and script that anyone can use to measure drawing performance on
  their own machine.
