# The helper runs as a separate client, with and without the feature, on a private X server.
add_executable(shmuploads_helper helper.cpp)
target_link_libraries(shmuploads_helper PRIVATE
    Qt::GuiPrivate
    Qt::Widgets
    XCB::XCB
)

qt_internal_add_test(tst_shmuploads
    SOURCES
        tst_shmuploads.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
        QXLIBRE_HELPER="$<TARGET_FILE:shmuploads_helper>"
    LIBRARIES
        Qt::Core
)
add_dependencies(tst_shmuploads shmuploads_helper QXLibreIntegrationPlugin)
