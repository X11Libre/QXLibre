# Measures software-rendered window updates. Not a ctest: see README.md for how to run it.
add_executable(qxlibre_perf qxlibre_perf.cpp)
target_link_libraries(qxlibre_perf PRIVATE
    Qt::GuiPrivate
    Qt::Widgets
    XCB::XCB
)
