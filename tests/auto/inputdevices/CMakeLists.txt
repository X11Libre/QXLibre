qt_internal_add_test(tst_inputdevices
    SOURCES
        tst_inputdevices.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
    LIBRARIES
        Qt::Gui
        Qt::GuiPrivate
        XCB::XINPUT
)
add_dependencies(tst_inputdevices QXLibreIntegrationPlugin)
