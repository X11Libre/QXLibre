# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## XLibreQpaPrivate Module:
#####################################################################

if(GCC)
    # Work around GCC ABI issues
    add_compile_options(-Wno-psabi)
endif()

qt_internal_add_module(XLibreQpaPrivate
    CONFIG_MODULE_NAME xlibre_qpa_lib
    INTERNAL_MODULE
    NO_MODULE_HEADERS
    PLUGIN_TYPES
        xlibreglintegrations
    SOURCES
        gl_integrations/qxcbglintegration.cpp gl_integrations/qxcbglintegration.h
        gl_integrations/qxcbglintegrationfactory.cpp gl_integrations/qxcbglintegrationfactory.h
        gl_integrations/qxcbglintegrationplugin.h
        gl_integrations/qxcbnativeinterfacehandler.cpp gl_integrations/qxcbnativeinterfacehandler.h
        qxcbatom.cpp qxcbatom.h
        qxcbbackingstore.cpp qxcbbackingstore.h
        qxcbclipboard.cpp qxcbclipboard.h
        qxcbconnection.cpp qxcbconnection.h
        qxcbconnection_basic.cpp qxcbconnection_basic.h
        qxcbconnection_screens.cpp
        qxcbconnection_xi2.cpp
        qxcbcursor.cpp qxcbcursor.h
        qxcbeventdispatcher.cpp qxcbeventdispatcher.h
        qxcbeventqueue.cpp qxcbeventqueue.h
        qxcbimage.cpp qxcbimage.h
        qxcbintegration.cpp qxcbintegration.h
        qxcbkeyboard.cpp qxcbkeyboard.h
        qxcbscrollingdevice.cpp qxcbscrollingdevice_p.h
        qxcbmime.cpp qxcbmime.h
        qxcbnativeinterface.cpp qxcbnativeinterface.h
        qxcbobject.h
        qxcbscreen.cpp qxcbscreen.h
        qxcbsystemtraytracker.cpp qxcbsystemtraytracker.h
        qxcbwindow.cpp qxcbwindow.h
        qxcbwmsupport.cpp qxcbwmsupport.h
        qxcbxsettings.cpp qxcbxsettings.h
        xlibre/qxlibreanimationdriver.cpp xlibre/qxlibreanimationdriver.h
        xlibre/qxlibrepresent.cpp xlibre/qxlibrepresent.h
        xlibre/qxlibreserver.cpp xlibre/qxlibreserver.h
        xlibre/qxlibreshmuploads.cpp xlibre/qxlibreshmuploads.h
    DEFINES
        QT_BUILD_XCB_PLUGIN
    INCLUDE_DIRECTORIES
        gl_integrations
    PUBLIC_LIBRARIES
        PkgConfig::XKB_COMMON_X11
        Qt::CorePrivate
        Qt::GuiPrivate
        XCB::CURSOR
        XCB::ICCCM
        XCB::UTIL
        XCB::IMAGE
        XCB::KEYSYMS
        XCB::PRESENT
        XCB::RANDR
        XCB::RENDER
        XCB::RENDERUTIL
        XCB::SHAPE
        XCB::SHM
        XCB::SYNC
        XCB::XCB
        XCB::XFIXES
        XCB::XKB
        XKB::XKB
    NO_UNITY_BUILD # X11 define clashes
)

qt_disable_apple_app_extension_api_only(XLibreQpaPrivate)

## Scopes:
#####################################################################

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_glib
    LIBRARIES
        GLIB2::GLIB2
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_draganddrop
    SOURCES
        qxcbdrag.cpp qxcbdrag.h
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_xcb_xlib
    SOURCES
        qt_xlib_wrapper.cpp qt_xlib_wrapper.h
    PUBLIC_LIBRARIES
        X11::XCB
        X11::X11
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION NOT QT_FEATURE_xcb_xlib
    SOURCES
        qxcbcursorfont.h
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_xcb_sm
    SOURCES
        qxcbsessionmanager.cpp qxcbsessionmanager.h
    PUBLIC_LIBRARIES
        X11::SM
        X11::ICE
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_vulkan
    SOURCES
        qxcbvulkaninstance.cpp qxcbvulkaninstance.h
        qxcbvulkanwindow.cpp qxcbvulkanwindow.h
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION QT_FEATURE_dlopen
    PUBLIC_LIBRARIES
        ${CMAKE_DL_LIBS}
)

qt_internal_extend_target(XLibreQpaPrivate CONDITION CLANG
    COMPILE_OPTIONS
        -ftemplate-depth=1024
)

qt_internal_extend_target(XLibreQpaPrivate LIBRARIES XCB::XINPUT)

#####################################################################
## QXLibreIntegrationPlugin Plugin:
#####################################################################

qt_internal_add_plugin(QXLibreIntegrationPlugin
    PLUGIN_TYPE platforms
    DEFAULT_IF FALSE
    SOURCES
        qxcbmain.cpp
    LIBRARIES
        Qt::CorePrivate
        Qt::GuiPrivate
        Qt::XLibreQpaPrivate
)
# Not OUTPUT_NAME above: that creates a "qxlibre" target, which is the repo's own target name.
set_property(TARGET QXLibreIntegrationPlugin PROPERTY OUTPUT_NAME qxlibre)

if(QXLIBRE_WITHOUT_QT_XCB)
    qt_internal_extend_target(XLibreQpaPrivate
        SOURCES
            ../../../compat/qxlibrenativeinterfaces.cpp
    )
    qt_internal_extend_target(XLibreQpaPrivate CONDITION NOT QT_FEATURE_eglfs
        SOURCES
            ../../../compat/qedidparser.cpp
    )
    qt_internal_extend_target(QXLibreIntegrationPlugin
        DEFINES
            QXLIBRE_CLAIM_XCB_KEY
    )
endif()

add_subdirectory(gl_integrations)
if(OFF)
    add_subdirectory(xcb-static)
endif()
