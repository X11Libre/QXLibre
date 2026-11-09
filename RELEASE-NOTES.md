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
- Plugging in or removing a mouse, keyboard or tablet no longer deletes input devices that an application set up for
  itself. Window managers such as SonicDE's crashed on exit or restart because of it.
- Applications no longer crash when an extra pointer and keyboard pair (an X master device) is removed, for example by a
  virtual machine or by `xinput remove-master`. Before, every running Qt application crashed at once, which could end
  the whole desktop session.
- Animations use about half the processor time, because Qt windows that animate themselves now draw in step with your
  display, instead of drawing about two frames in three that it can't show. Windows built with Qt Widgets aren't
  affected, and neither are windows drawn by the graphics card, as Qt Quick windows usually are. If you need to, you can
  turn this off with `QT_XLIBRE_NO_VBLANK_UPDATES=1`.

  | Animated window, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Updates per second | 170 to 60 (-65%) | 165 to 60 (-64%) |
  | Application's processor use, % of one core | 6.9 to 2.9 (-57%) | 6.3 to 2.9 (-55%) |
  | X server's processor use, % of one core | 15.4 to 7.8 (-50%) | 12.4 to 5.7 (-56%) |
  | Variation in time between updates, ms | 0.361 to 0.093 (-74%) | 0.305 to 0.083 (-73%) |

  Measured on a 60 Hz display with an AMD Radeon RX 9060 XT graphics card, XLibre 25.1.9 and Sonic Win, over 5 runs each
  way with desktop effects on and 5 with them off.
- Windows built with Qt Widgets respond faster, because drawing usually no longer has to stop and wait for the X server.
  Small updates gain the most. Processor use stayed about the same, and scrolling didn't clearly change. If you need
  to, you can turn this off with `QT_XLIBRE_NO_SHM_COMPLETION=1`.

  | Time per update in ms, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Small update, such as a blinking text cursor | 0.90 to 0.67 (-25%) | 0.67 to 0.42 (-39%) |
  | Slowest 1% of small updates | 1.27 to 0.79 (-34%) | 0.75 to 0.48 (-36%) |
  | Repainting a window, as most applications do | 1.87 to 1.65 (-13%) | 1.65 to 1.33 (-18%) |
  | Slowest 1% of those repaints | 2.38 to 2.12 (-12%) | 1.89 to 1.74 (-9%) |
  | Resizing a window | 2.44 to 2.05 (-16%) | 2.57 to 2.11 (-21%) |
  | Repainting a full HD window | 4.80 to 4.58 (-5%) | 4.45 to 4.11 (-6%) |

  Measured on the same machine, over 5 pairs of runs of 100 updates each. Times include one trip to the X server that
  the measuring tool adds to every update. Each percentage is the typical difference between a pair of runs, so it can
  differ slightly from the two times beside it.
- QML applications drawn by the graphics card, as most are, run as before. Those using Qt Quick's software drawing
  (`QT_QUICK_BACKEND=software`) now animate in step with your display, so the time between their updates is about three
  times more even. The application uses slightly more processor time and the X server about the same or slightly less.
  If you need to, you can turn this off with `QT_XLIBRE_NO_VBLANK_ANIMATIONS=1`.

  | Animated QML window, before to after | Desktop effects on | Desktop effects off |
  |---|---|---|
  | Drawn by the graphics card: updates per second | 60 to 60 | 60 to 60 |
  | Application's processor use, % of one core | 3.6 to 3.6 (+1%) | 3.5 to 3.5 (0%) |
  | X server's processor use, % of one core | 5.0 to 4.8 (-2%) | 2.7 to 2.7 (0%) |
  | Variation in time between updates, ms | 0.130 to 0.125 (-1%) | 0.115 to 0.124 (+8%) |
  | Software drawing: updates per second | 63 to 60 (-4%) | 63 to 60 (-4%) |
  | Application's processor use, % of one core | 3.0 to 3.2 (+7%) | 3.0 to 3.1 (+3%) |
  | X server's processor use, % of one core | 7.2 to 7.2 (-2%) | 4.0 to 3.8 (-5%) |
  | Variation in time between updates, ms | 0.390 to 0.127 (-67%) | 0.378 to 0.122 (-69%) |

  Measured on the same machine, over 5 runs each way. A 60 Hz display can't show the updates above 60 per second in the
  software drawing rows.
- Behind the scenes: new automated tests for containers, popups, animation timing, drawing without waiting for the X
  server, the reported platform name and input devices, new session management tests, and a new tool and script that anyone can use to
  measure drawing performance on their own machine.
