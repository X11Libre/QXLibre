# Measures software-rendered window updates. Not a ctest: see README.md for how to run it.
add_executable(qxlibre_perf qxlibre_perf.cpp)
target_link_libraries(qxlibre_perf PRIVATE
    Qt::GuiPrivate
    Qt::Widgets
    XCB::XCB
)

# Checks that measure.py still works with the tool, on a private Xvfb. The figures mean nothing.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME qxlibre_perf_script
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/measure.py" --xvfb
            --tool "$<TARGET_FILE:qxlibre_perf>" --plugins "${QT_BUILD_DIR}/${INSTALL_PLUGINSDIR}"
            --out "${CMAKE_CURRENT_BINARY_DIR}/smoke" --runs 1 --frames 5 --seconds 1)
    set_tests_properties(qxlibre_perf_script PROPERTIES SKIP_RETURN_CODE 77)
endif()
