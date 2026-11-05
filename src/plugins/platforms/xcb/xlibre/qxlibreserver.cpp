// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qxlibreserver.h"

#include <QtCore/qdebug.h>

#include <atomic>
#include <cstdlib>

QT_BEGIN_NAMESPACE

static std::atomic<quintptr> s_syncCount = 0;

QXLibreServer::QXLibreServer(xcb_connection_t *connection)
{
    const xcb_setup_t *setup = xcb_get_setup(connection);
    m_vendor = QByteArray(xcb_setup_vendor(setup), xcb_setup_vendor_length(setup));

    const xcb_window_t root = xcb_setup_roots_iterator(setup).data->root;
    auto cookie = xcb_get_window_attributes(connection, root);
    auto *reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
    m_rootRestricted = !reply;
    free(reply);

    if (isDebugEnabled()) {
        qDebug() << "QXLibre: server vendor" << m_vendor << "release" << setup->release_number
                 << "root restricted" << m_rootRestricted;
    }
}

bool QXLibreServer::isFeatureDisabled(const char *feature)
{
    const QByteArray name = QByteArray("QT_XLIBRE_NO_").append(feature);
    const bool disabled = qEnvironmentVariableIsSet(name.constData());
    if (disabled && isDebugEnabled())
        qDebug() << "QXLibre: feature" << feature << "turned off";
    return disabled;
}

bool QXLibreServer::isDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("QT_XLIBRE_DEBUG");
    return enabled;
}

void QXLibreServer::countSync()
{
    if (isDebugEnabled())
        s_syncCount.fetch_add(1, std::memory_order_relaxed);
}

quintptr QXLibreServer::syncCount()
{
    return s_syncCount.load(std::memory_order_relaxed);
}

QT_END_NAMESPACE
