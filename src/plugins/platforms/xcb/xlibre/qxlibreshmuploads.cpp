// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qxlibreshmuploads.h"

#include "qxcbconnection_basic.h"
#include "qxlibreserver.h"

#include <xcb/shm.h>

QT_BEGIN_NAMESPACE

std::unique_ptr<QXLibreShmUploads> QXLibreShmUploads::create(const QXcbBasicConnection *connection)
{
    if (!connection->hasShm() || QXLibreServer::isFeatureDisabled("SHM_COMPLETION"))
        return nullptr;

    const xcb_query_extension_reply_t *extension =
            xcb_get_extension_data(connection->xcb_connection(), &xcb_shm_id);
    if (!extension || !extension->present)
        return nullptr;

    return std::unique_ptr<QXLibreShmUploads>(
            new QXLibreShmUploads(extension->first_event + XCB_SHM_COMPLETION));
}

QT_END_NAMESPACE
