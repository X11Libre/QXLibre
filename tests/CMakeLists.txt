add_subdirectory(auto)

find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS Qml Quick Widgets)
if(TARGET Qt::Widgets)
    add_subdirectory(manual/perf)
endif()
if(QT_FEATURE_xcb_sm)
    add_subdirectory(manual/session)
endif()
