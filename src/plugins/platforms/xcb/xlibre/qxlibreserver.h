// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qtypes.h>

#include <xcb/xcb.h>

QT_BEGIN_NAMESPACE

// What QXLibre knows about the server, and its debugging switches. Features must check the
// extension and version they need: the vendor is only for diagnostics.
class QXLibreServer
{
public:
    explicit QXLibreServer(xcb_connection_t *connection);

    QByteArray vendor() const { return m_vendor; }
    bool isXLibre() const { return m_vendor == "XLibre"; }

    // True if QT_XLIBRE_NO_<feature> is set. Callers read this once at startup.
    static bool isFeatureDisabled(const char *feature);
    // True if QT_XLIBRE_DEBUG is set.
    static bool isDebugEnabled();

    // Round trips made through QXcbConnection::sync(), counted only with QT_XLIBRE_DEBUG.
    static void countSync();
    static quintptr syncCount();

private:
    QByteArray m_vendor;
};

QT_END_NAMESPACE
