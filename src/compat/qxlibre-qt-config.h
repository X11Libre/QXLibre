// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Force-included when Qt is built without xcb: Qt's headers then declare the X11 interfaces, and
// src/compat provides what Qt no longer compiles. Keep the list in sync with src/CMakeLists.txt.
#pragma once

#ifdef __cplusplus
#include <QtGui/private/qtguiglobal_p.h>

#if !QT_CONFIG(xcb)
#define QXLIBRE_QT_WITHOUT_XCB 1

#undef QT_FEATURE_xcb
#define QT_FEATURE_xcb 1
#undef QT_FEATURE_xlib
#define QT_FEATURE_xlib 1
#undef QT_FEATURE_xcb_xlib
#define QT_FEATURE_xcb_xlib 1
#undef QT_FEATURE_xkbcommon_x11
#define QT_FEATURE_xkbcommon_x11 1
#undef QT_FEATURE_system_xcb_xinput
#define QT_FEATURE_system_xcb_xinput 1

#if QT_CONFIG(sessionmanager)
#undef QT_FEATURE_xcb_sm
#define QT_FEATURE_xcb_sm 1
#endif

#if QT_CONFIG(opengl) && !QT_CONFIG(opengles2)
#undef QT_FEATURE_xcb_glx
#define QT_FEATURE_xcb_glx 1
#undef QT_FEATURE_xcb_glx_plugin
#define QT_FEATURE_xcb_glx_plugin 1
#endif

#if QT_CONFIG(egl) && QT_CONFIG(opengl)
#undef QT_FEATURE_xcb_egl_plugin
#define QT_FEATURE_xcb_egl_plugin 1
#undef QT_FEATURE_egl_x11
#define QT_FEATURE_egl_x11 1
#endif
#endif // !QT_CONFIG(xcb)
#endif // __cplusplus
