// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qxlibrepresent.h"

#include "qxcbconnection.h"
#include "qxcbwindow.h"
#include "qxlibreanimationdriver.h"
#include "qxlibreserver.h"

#include <QtCore/qdebug.h>

#include <xcb/present.h>

QT_BEGIN_NAMESPACE

std::unique_ptr<QXLibrePresent> QXLibrePresent::create(QXcbConnection *connection)
{
    if (QXLibreServer::isFeatureDisabled("VBLANK_UPDATES"))
        return nullptr;

    xcb_connection_t *xcbConnection = connection->xcb_connection();
    const xcb_query_extension_reply_t *extension =
            xcb_get_extension_data(xcbConnection, &xcb_present_id);
    if (!extension || !extension->present) {
        if (QXLibreServer::isDebugEnabled())
            qDebug("QXLibre: this server has no Present extension");
        return nullptr;
    }

    auto *version = xcb_present_query_version_reply(
            xcbConnection, xcb_present_query_version(xcbConnection, XCB_PRESENT_MAJOR_VERSION,
                                                     XCB_PRESENT_MINOR_VERSION),
            nullptr);
    if (!version)
        return nullptr;
    if (QXLibreServer::isDebugEnabled()) {
        qDebug("QXLibre: using Present %u.%u for vblank updates", version->major_version,
               version->minor_version);
    }
    free(version);

    return std::unique_ptr<QXLibrePresent>(new QXLibrePresent(connection, extension->major_opcode));
}

QXLibrePresent::QXLibrePresent(QXcbConnection *connection, uint8_t majorOpcode)
    : m_connection(connection), m_majorOpcode(majorOpcode)
{
}

// The plugin is destroyed after the event dispatcher, which is too late for Qt to restart its own
// animation timer, as uninstalling a running driver does.
static std::unique_ptr<QXLibreAnimationDriver> *s_animationDriver = nullptr;

static void uninstallAnimationDriver()
{
    if (s_animationDriver)
        s_animationDriver->reset();
}

QXLibrePresent::~QXLibrePresent()
{
    if (s_animationDriver == &m_animationDriver) {
        s_animationDriver = nullptr;
        qRemovePostRoutine(uninstallAnimationDriver);
    }
}

void QXLibrePresent::useFrameDrivenAnimations(const QWindow *window)
{
    if (!window->inherits("QQuickWindow"))
        return;
    m_animationDriverDecided = true;
    // A raster QQuickWindow means the software backend, which applies to the whole process, so no
    // other render loop can want to install a driver. Its threaded loop does install one.
    if (qgetenv("QSG_RENDER_LOOP") == "threaded" || QXLibreServer::isFeatureDisabled("VBLANK_ANIMATIONS"))
        return;
    // This can be running inside an animation tick, which is no place to change drivers.
    QMetaObject::invokeMethod(m_connection, [this] {
        if (!m_active)
            return;
        m_animationDriver = std::make_unique<QXLibreAnimationDriver>();
        m_animationDriver->install();
        s_animationDriver = &m_animationDriver;
        qAddPostRoutine(uninstallAnimationDriver);
    }, Qt::QueuedConnection);
}

bool QXLibrePresent::requestVblankUpdate(xcb_window_t window, const QWindow *qwindow)
{
    if (!m_active || !window)
        return false;

    if (!m_animationDriverDecided)
        useFrameDrivenAnimations(qwindow);

    xcb_connection_t *xcbConnection = m_connection->xcb_connection();
    // This ends when the window does. Selecting again would deliver each event twice.
    if (!m_selectedWindows.contains(window)) {
        m_selectedWindows.insert(window);
        xcb_present_select_input(xcbConnection, xcb_generate_id(xcbConnection), window,
                                 XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);
    }

    const uint serial = ++m_serial;
    m_vblankUpdates.insert(window, serial);
    // A divisor of 0 would ask for the notification right away instead of at the next vblank.
    xcb_present_notify_msc(xcbConnection, window, serial, 0, 1, 0);
    xcb_flush(xcbConnection);
    return true;
}

void QXLibrePresent::forgetWindow(xcb_window_t window)
{
    m_selectedWindows.remove(window);
    m_vblankUpdates.remove(window);
}

void QXLibrePresent::handleEvent(const xcb_generic_event_t *event)
{
    auto *generic = reinterpret_cast<const xcb_ge_generic_event_t *>(event);
    if (generic->extension != m_majorOpcode || generic->event_type != XCB_PRESENT_EVENT_COMPLETE_NOTIFY)
        return;

    auto *complete = reinterpret_cast<const xcb_present_complete_notify_event_t *>(event);
    if (complete->kind != XCB_PRESENT_COMPLETE_KIND_NOTIFY_MSC)
        return;
    const auto update = m_vblankUpdates.constFind(complete->window);
    if (update == m_vblankUpdates.cend() || *update != complete->serial)
        return;
    m_vblankUpdates.erase(update);

    const xcb_window_t windowId = complete->window;
    QMetaObject::invokeMethod(m_connection, [this, windowId] {
        QXcbWindow *window = m_connection->platformWindowFromId(windowId);
        if (window && window->hasPendingUpdateRequest())
            window->deliverUpdateRequest();
        if (m_animationDriver)
            m_animationDriver->frameDelivered();
    }, Qt::QueuedConnection);
}

void QXLibrePresent::handleError(const xcb_generic_error_t *error)
{
    if (!m_active || error->major_code != m_majorOpcode)
        return;

    // Another client can destroy our window. That says nothing about Present itself.
    if (error->error_code == XCB_WINDOW) {
        if (m_vblankUpdates.remove(error->resource_id)) {
            QXcbWindow *window = m_connection->platformWindowFromId(error->resource_id);
            if (window && window->hasPendingUpdateRequest())
                window->QPlatformWindow::requestUpdate();
        }
        return;
    }

    qWarning("QXLibre: turning vblank updates off after X error %d on request %d",
             error->error_code, error->minor_code);
    m_active = false;
    // Without vblanks to follow, Qt's own timer drives animations again.
    m_animationDriver.reset();

    // Whatever was waiting for a notification still has to be delivered.
    const auto windows = m_vblankUpdates.keys();
    m_vblankUpdates.clear();
    for (xcb_window_t id : windows) {
        QXcbWindow *window = m_connection->platformWindowFromId(id);
        if (window && window->hasPendingUpdateRequest())
            window->QPlatformWindow::requestUpdate();
    }
}

QT_END_NAMESPACE
