# The helper runs as a separate client, with and without the feature, on a private X server.
add_executable(present_helper helper.cpp)
target_link_libraries(present_helper PRIVATE
    Qt::GuiPrivate
    XCB::PRESENT
    XCB::XCB
)
if(TARGET Qt::Quick)
    target_compile_definitions(present_helper PRIVATE QXLIBRE_TEST_HAS_QUICK)
    target_link_libraries(present_helper PRIVATE Qt::Qml Qt::Quick)
endif()

qt_internal_add_test(tst_present
    SOURCES
        tst_present.cpp
    DEFINES
        QXLIBRE_PLUGIN_DIR="${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
        QXLIBRE_HELPER="$<TARGET_FILE:present_helper>"
    LIBRARIES
        Qt::Core
)
add_dependencies(tst_present present_helper QXLibreIntegrationPlugin)
