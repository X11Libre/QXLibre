// Copyright (C) 2026 Joseph Crowell
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qxlibreanimationdriver.h"

#include <QtCore/qcoreevent.h>

#include <chrono>

QT_BEGIN_NAMESPACE

using namespace std::chrono_literals;

static constexpr auto FallbackInterval = 32ms;
// Several windows can deliver a frame at the same vblank.
static constexpr qint64 MinimumTickMs = 8;

QXLibreAnimationDriver::QXLibreAnimationDriver(QObject *parent) : QAnimationDriver(parent) { }

void QXLibreAnimationDriver::frameDelivered()
{
    if (isRunning() && (!m_sinceTick.isValid() || m_sinceTick.elapsed() >= MinimumTickMs))
        tick();
}

void QXLibreAnimationDriver::start()
{
    QAnimationDriver::start();
    m_fallback.start(FallbackInterval, Qt::PreciseTimer, this);
}

void QXLibreAnimationDriver::stop()
{
    m_fallback.stop();
    QAnimationDriver::stop();
}

void QXLibreAnimationDriver::timerEvent(QTimerEvent *event)
{
    if (event->id() == m_fallback.id())
        tick();
    else
        QAnimationDriver::timerEvent(event);
}

void QXLibreAnimationDriver::tick()
{
    m_sinceTick.start();
    advance();
    // Advancing can end the last animation, which stops the driver.
    if (isRunning())
        m_fallback.start(FallbackInterval, Qt::PreciseTimer, this);
}

QT_END_NAMESPACE
