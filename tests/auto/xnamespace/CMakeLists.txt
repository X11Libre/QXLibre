# The helper runs as a separate client inside a namespace on a private X server.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_XTEST REQUIRED IMPORTED_TARGET xcb-xtest)

add_executable(xnamespace_helper helper.cpp)
target_link_libraries(xnamespace_helper PRIVATE
    Qt::GuiPrivate
    Qt::Qml
    Qt::Quick
    Qt::QuickControls2
    Qt::Widgets
    PkgConfig::XCB_XTEST
    XCB::XCB
)

qt_internal_add_test(tst_xnamespace
    SOURCES
        tst_xnamespace.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
        QXLIBRE_HELPER="$<TARGET_FILE:xnamespace_helper>"
    LIBRARIES
        Qt::Core
)
add_dependencies(tst_xnamespace xnamespace_helper QXLibreIntegrationPlugin)
