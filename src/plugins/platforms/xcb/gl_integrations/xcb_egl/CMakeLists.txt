# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## QXLibreEglIntegrationPlugin Plugin:
#####################################################################

qt_find_package(EGL MODULE)

qt_internal_add_plugin(QXLibreEglIntegrationPlugin
    OUTPUT_NAME qxlibre-egl-integration
    PLUGIN_TYPE xlibreglintegrations
    DEFAULT_IF FALSE
    SOURCES
        qxcbeglcontext.h
        qxcbeglintegration.cpp qxcbeglintegration.h
        qxcbeglmain.cpp
        qxcbeglnativeinterfacehandler.cpp qxcbeglnativeinterfacehandler.h
        qxcbeglwindow.cpp qxcbeglwindow.h
    INCLUDE_DIRECTORIES
        ..
        ../..
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::GuiPrivate
        Qt::XLibreQpaPrivate
        EGL::EGL
    NO_UNITY_BUILD # X11 define clashes
)
