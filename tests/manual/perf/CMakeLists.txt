# Measures software-rendered window updates. Not a ctest: see README.md for how to run it.
add_executable(qxlibre_perf qxlibre_perf.cpp)
target_link_libraries(qxlibre_perf PRIVATE
    Qt::GuiPrivate
    Qt::Widgets
    XCB::XCB
)

# The Qt Quick scenario is optional, so that the tool still builds on a Qt without it.
if(TARGET Qt::Quick)
    target_compile_definitions(qxlibre_perf PRIVATE QXLIBRE_PERF_HAS_QUICK)
    target_link_libraries(qxlibre_perf PRIVATE Qt::Qml Qt::Quick)
endif()

# Times a window that requests updates from a timer, as Qt's animation timer does.
add_executable(qxlibre_timer_window timer_window.cpp)
target_link_libraries(qxlibre_timer_window PRIVATE Qt::Gui)

# Logs each animation tick and frame of a Qt Quick scene, and can change the animation timer.
find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS CorePrivate)
if(TARGET Qt::Quick AND TARGET Qt::CorePrivate)
    add_executable(qxlibre_animation_probe animation_probe.cpp)
    target_link_libraries(qxlibre_animation_probe PRIVATE Qt::CorePrivate Qt::Gui Qt::Qml Qt::Quick)
endif()

# Checks that measure.py still works with the tool, on a private Xvfb. The figures mean nothing.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME qxlibre_perf_script
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/measure.py" --xvfb
            --tool "$<TARGET_FILE:qxlibre_perf>" --plugins "${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
            --out "${CMAKE_CURRENT_BINARY_DIR}/smoke" --runs 1 --frames 5 --seconds 1)
    set_tests_properties(qxlibre_perf_script PROPERTIES SKIP_RETURN_CODE 77)
endif()
