add_subdirectory(platformname)

find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS Qml Quick QuickControls2)
if(TARGET Qt::Quick AND TARGET Qt::QuickControls2)
    add_subdirectory(quickpopuptypes)
endif()
