// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qset.h>
#include <QtCore/qtypes.h>

#include <xcb/xcb.h>

#include <memory>

QT_BEGIN_NAMESPACE

class QWindow;
class QXcbConnection;
class QXLibreAnimationDriver;

// Delivers QWindow::requestUpdate() at the display's vblank, using the Present extension's
// notifications. Null if the server lacks Present or QT_XLIBRE_NO_VBLANK_UPDATES is set.
class QXLibrePresent
{
public:
    static std::unique_ptr<QXLibrePresent> create(QXcbConnection *connection);
    ~QXLibrePresent();

    // Returns false if the caller has to schedule the update some other way.
    bool requestVblankUpdate(xcb_window_t window, const QWindow *qwindow);
    void forgetWindow(xcb_window_t window);

    void handleEvent(const xcb_generic_event_t *event);
    void handleError(const xcb_generic_error_t *error);

private:
    QXLibrePresent(QXcbConnection *connection, uint8_t majorOpcode);
    void useFrameDrivenAnimations(const QWindow *window);

    QXcbConnection *m_connection;
    QHash<xcb_window_t, uint> m_vblankUpdates;
    QSet<xcb_window_t> m_selectedWindows;
    std::unique_ptr<QXLibreAnimationDriver> m_animationDriver;
    bool m_animationDriverDecided = false;
    uint m_serial = 0;
    uint8_t m_majorOpcode;
    bool m_active = true;
};

QT_END_NAMESPACE
