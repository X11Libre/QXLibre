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
- Smooth animations now use far less processor time, because Qt windows that animate themselves draw in step with your display
  instead of drawing frames it can't show. On a 60 Hz display with an AMD Radeon graphics card, such a window went from
  187 to 60 updates a second, using 61% less processor time in the application and 49% less in the X server, with the
  desktop's compositing on. With compositing off, those savings were 54% and 37%, and updates were also 40% steadier.
  Windows built with Qt Widgets aren't affected. If you need to, you can turn this off with
  `QT_XLIBRE_NO_VBLANK_UPDATES=1`.
- Behind the scenes: new automated tests for containers, popups, animation timing and the reported platform name, and a
  new tool that measures drawing performance.
