# Runs in its own process because the platform plugin is chosen once per process.
add_executable(quickpopuptypes_helper helper.cpp)
target_link_libraries(quickpopuptypes_helper PRIVATE
    Qt::GuiPrivate
    Qt::Qml
    Qt::Quick
    Qt::QuickControls2
    XCB::XCB
)

qt_internal_add_test(tst_quickpopuptypes
    SOURCES
        tst_quickpopuptypes.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
        QXLIBRE_HELPER="$<TARGET_FILE:quickpopuptypes_helper>"
    LIBRARIES
        Qt::Gui
)
add_dependencies(tst_quickpopuptypes quickpopuptypes_helper QXLibreIntegrationPlugin)
