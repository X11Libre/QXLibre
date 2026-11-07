// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qabstractanimation.h>
#include <QtCore/qbasictimer.h>
#include <QtCore/qelapsedtimer.h>

QT_BEGIN_NAMESPACE

// Advances animations once per frame, so that they and the vblank share one clock. Qt Quick's
// software backend otherwise ticks them from a 16 ms timer of its own, which makes frames late.
class QXLibreAnimationDriver : public QAnimationDriver
{
public:
    explicit QXLibreAnimationDriver(QObject *parent = nullptr);

    void frameDelivered();

protected:
    void start() override;
    void stop() override;
    void timerEvent(QTimerEvent *event) override;

private:
    void tick();

    // Keeps animations going when no frames come, as for a hidden window or a blanked display.
    QBasicTimer m_fallback;
    QElapsedTimer m_sinceTick;
};

QT_END_NAMESPACE
