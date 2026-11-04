// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// From qtbase src/gui/platform/unix/qunixnativeinterface.cpp, which compiles these only with xcb.

#include <QtGui/private/qguiapplication_p.h>
#if QT_CONFIG(opengl)
#  include <QtGui/private/qopenglcontext_p.h>
#endif

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformopenglcontext.h>
#include <qpa/qplatformscreen_p.h>
#include <qpa/qplatformwindow_p.h>

QT_BEGIN_NAMESPACE

using namespace QNativeInterface::Private;

#if QT_CONFIG(xcb_glx_plugin)
QT_DEFINE_NATIVE_INTERFACE(QGLXContext);
QT_DEFINE_PRIVATE_NATIVE_INTERFACE(QGLXIntegration);

QOpenGLContext *QNativeInterface::QGLXContext::fromNative(GLXContext configBasedContext, QOpenGLContext *shareContext)
{
    return QGuiApplicationPrivate::platformIntegration()->call<
        &QGLXIntegration::createOpenGLContext>(configBasedContext, nullptr, shareContext);
}

QOpenGLContext *QNativeInterface::QGLXContext::fromNative(GLXContext visualBasedContext, void *visualInfo, QOpenGLContext *shareContext)
{
    return QGuiApplicationPrivate::platformIntegration()->call<
        &QGLXIntegration::createOpenGLContext>(visualBasedContext, visualInfo, shareContext);
}
#endif // QT_CONFIG(xcb_glx_plugin)

QT_DEFINE_PRIVATE_NATIVE_INTERFACE(QXcbScreen);
QT_DEFINE_PRIVATE_NATIVE_INTERFACE(QXcbWindow);
QT_DEFINE_NATIVE_INTERFACE(QX11Application);

QT_END_NAMESPACE
