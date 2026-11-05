add_subdirectory(auto)

find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS Widgets)
if(TARGET Qt::Widgets)
    add_subdirectory(manual/perf)
endif()
