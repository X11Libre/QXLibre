# Fixture theme plugin that records platformName() during theme creation. It isn't installed.
qt_add_plugin(qxlibretesttheme SHARED theme/qxlibretesttheme.cpp)
target_link_libraries(qxlibretesttheme PRIVATE Qt::GuiPrivate)
set_target_properties(qxlibretesttheme PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/platformthemes"
)

qt_internal_add_test(tst_platformname
    SOURCES
        tst_platformname.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
        QXLIBRE_FIXTURE_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/plugins"
    LIBRARIES
        Qt::Gui
        Qt::GuiPrivate
)
add_dependencies(tst_platformname qxlibretesttheme QXLibreIntegrationPlugin)
