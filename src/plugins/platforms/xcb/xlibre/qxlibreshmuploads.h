// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qtypes.h>

#include <xcb/xcb.h>

#include <memory>

QT_BEGIN_NAMESPACE

class QXcbBasicConnection;

// Knows when the server has finished an upload, from its completion events, so painting needn't
// block on a round trip to find out. Null without MIT-SHM, or if QT_XLIBRE_NO_SHM_COMPLETION is
// set.
class QXLibreShmUploads
{
public:
    static std::unique_ptr<QXLibreShmUploads> create(const QXcbBasicConnection *connection);

    bool isCompletion(uint responseType) const { return responseType == m_completionType; }
    void handleCompletion(const xcb_generic_event_t *event) { m_lastCompletion = event->full_sequence; }

    // Requests run in order, so any later completion means this upload has finished too.
    bool isFinished(uint sequence) const { return qint32(m_lastCompletion - sequence) >= 0; }

private:
    explicit QXLibreShmUploads(uint8_t completionType) : m_completionType(completionType) { }

    uint m_lastCompletion = 0;
    const uint8_t m_completionType;
};

QT_END_NAMESPACE
