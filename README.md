# QXLibre

QXLibre is a Qt platform plugin for X11, forked from the `xcb` plugin in qtbase. It's developed and
tested for [XLibre](https://github.com/X11Libre/xserver), and it works with any X server.

Qt might remove X11 support. QXLibre installs alongside stock Qt today, and it also builds and runs
on a Qt with no X11 support, so Qt apps that only need a window on X11 can keep running there.

## Status

- A complete SonicDE session ran with every Qt process using QXLibre, including the window manager,
  panel and session manager.
- Qt's own `qscreen`, `qwindow`, `qclipboard`, `qx11info`, `qwidget`, `qquickpopup`, `qquickmenu`,
  `qguiapplication` and `qapplication` tests gave the same results under QXLibre and Qt's `xcb` plugin
  on Xvfb.
- On an AMD Radeon RX 9060 XT, Qt's OpenGL and Vulkan tests (`qrhi`, `qvulkan`) and nine of its Qt Quick
  tests pass under both plugins. `qvulkan` also crashes in some runs under both, in the driver's
  swapchain thread after its tests pass, and not when the driver library is kept loaded.
- Saving, shutting down and cancelling a shutdown work under both plugins on Xvfb, with `xsm`, a private
  `ksmserver` and a small test manager. `xsm` also restores a session under both.
- The plugin's own tests pass with stock Qt and with a Qt built without X11.

Not tested yet: restoring a session with `ksmserver` under QXLibre, and logging out of a real desktop
session.

## Features

- **Xnamespace:** applications run inside XLibre's containers, where Qt's own plugin crashes at startup.
  There's no monitor list or hot-plug there, GLX is off, and `QCursor::pos()` isn't expected to work,
  because the server hides those from a client. Typing needs server fix `31336c8a4b`, which isn't in
  XLibre 25.1.11 or 25.2.3.

## Branches and Qt versions

Qt only loads a platform plugin built for its own major.minor version, so each Qt minor has its own
branch.

| Branch | Qt | Source |
|---|---|---|
| `main` | 6.12 | qtbase `v6.12.0` (`025bdad181de241e81bf853c8a2d7bf3d19261a9`) |
| `6.11` | 6.11 | qtbase `v6.11.3` (`138f955f0b5e50e4a6c2ee1e78a2279db52ae9e3`) |

Qt 6.11 is end of life, so that branch gets no further testing. Its results come from before the
move to 6.12.

Both branches carry one fix not in those releases, from qtbase `dev`: INCR clipboard reads are
bounded by the data actually received (`d85a77f165390cc728033e301ca1dc2dd9c2dfb3`).

## Building

You need Qt's development files with private headers and `Qt6BuildInternals`, plus the xcb, X11,
xkbcommon and EGL libraries that Qt's own plugin uses (see
[Qt for X11 Requirements](https://doc.qt.io/qt-6/linux-requirements.html)). `xcb-xinput` must come
from your system, because QXLibre doesn't bundle a copy, and you also need `xcb-present`.

```sh
qt-cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
ninja -C build
sudo cmake --install build
```

Add `-DQT_BUILD_TESTS=ON` to build the tests, and run them with `xvfb-run ctest` from the build
directory.

To measure performance on your own machine, run `tests/manual/perf/measure.py --build build` from your
X session. See [its instructions and our results](tests/manual/perf/README.md).

Rebuild QXLibre on every Qt update, because it uses private Qt APIs.

These files don't overlap with any file from Qt:

- `libQt6XLibreQpa.so.6`
- `plugins/platforms/libqxlibre.so`
- `plugins/xlibreglintegrations/libqxlibre-glx-integration.so` and `libqxlibre-egl-integration.so`
- CMake, metatype and mkspec files under the `XLibreQpaPrivate`, `xlibre_qpa_lib` and
  `QXLibreIntegrationPlugin` names

## Using it

On stock Qt, select it with an environment variable:

```sh
QT_QPA_PLATFORM='xlibre;xcb' yourapp
```

Qt tries each platform in order, so this falls back to Qt's own plugin, with a warning, if QXLibre
isn't installed or doesn't match your Qt version. Quote the value, because of the `;`.

To use it for every app, add this line to `/etc/environment`, then log out and back in:

```sh
QT_QPA_PLATFORM='xlibre;xcb'
```

For one user, add `export QT_QPA_PLATFORM='xlibre;xcb'` to `~/.profile` instead, if your login path
reads that file. Don't set it this way if you also use Wayland sessions, because Qt apps there would
then run through XWayland or fail to start.

On a Qt built without X11, you don't need to set anything: the plugin also claims the `xcb` name, so
Qt's automatic selection finds it.

### Why apps see `xcb`

Qt and many apps check `QGuiApplication::platformName() == "xcb"` before using X11 code. QXLibre
loads as `xlibre` but reports `xcb`, so those checks keep working with no changes. It sets the name
during startup, which relies on an order Qt doesn't document. `tst_platformname` fails if that order
changes.

### Limitations on stock Qt

- Qt only selects `xcb` automatically, so QXLibre needs `QT_QPA_PLATFORM` or `-platform`.
- `-platform xcb`, or `QT_QPA_PLATFORM=xcb`, loads Qt's own plugin.
- `-geometry`, `-title` and `-icon` only work when the requested platform starts with `xcb`. Use
  `-qwindowgeometry`, `-qwindowtitle` and `-qwindowicon`.
- `qt.conf` platform arguments use the key `XlibreArguments`.
- The `QT_XCB_*` environment variables affect both plugins.

### Limitations on Qt without X11

QXLibre gives apps a working X11 platform there, but it can't restore what that Qt has compiled out:

- Apps using `QX11Info` or `QNativeInterface::QX11Application` won't compile.
- `Qt::WA_X11NetWmWindowType*` and `QWidget::setWindowRole()` don't reach X11.
- Qt Widgets has no XEmbed tray fallback.
- `Qt::Popup` windows without another type are labelled `_NET_WM_WINDOW_TYPE_POPUP_MENU`, so Qt Quick
  combo boxes and menu-bar drop-downs get that rather than their specific types. Qt Quick tooltips
  also gain the `_KDE_NET_WM_WINDOW_TYPE_OVERRIDE` hint.

## Updating to a new Qt version

1. Apply the changes from qtbase: `git diff vA vB -- src/plugins/platforms/xcb`.
2. Compare the copies in `src/compat/` with their sources in that version: `qunixnativeinterface.cpp`,
   `qglxconvenience.cpp` and `qedidparser.cpp`.
3. Read `init_platform()` in `qguiapplication.cpp` to confirm the startup order hasn't changed, then
   run the tests against both a stock Qt and a Qt built without `QT_FEATURE_xcb`.

If Qt deletes a declaration or private header that the plugin needs, move a copy into `src/compat/`.

## Licence

The files keep the licence terms in their headers from qtbase: `LGPL-3.0-only`, `GPL-2.0-only` or
`GPL-3.0-only` for the code, and `BSD-3-Clause` for the build files. The texts are in `LICENSES/`.
The commercial option named in those headers belongs to The Qt Company, and this project doesn't
offer it.

QXLibre isn't affiliated with The Qt Company.
