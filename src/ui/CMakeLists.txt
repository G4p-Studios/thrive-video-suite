# ---------------------------------------------------------------------------
# thrive-ui  – Qt widgets, timeline view, panels, preferences
# ---------------------------------------------------------------------------
qt_add_library(thrive-ui STATIC
    timelinewidget.h        timelinewidget.cpp
    timelinecanvas.h        timelinecanvas.cpp
    thumbnailprovider.h     thumbnailprovider.cpp
    accessibletimelineview.h accessibletimelineview.cpp
    transportbar.h          transportbar.cpp
    mediabrowser.h          mediabrowser.cpp
    propertiespanel.h       propertiespanel.cpp
    effectsbrowser.h        effectsbrowser.cpp
    stackmanagerdialog.h    stackmanagerdialog.cpp
    preferencesdialog.h     preferencesdialog.cpp
    shortcuteditor.h        shortcuteditor.cpp
    pluginmanager.h         pluginmanager.cpp
    exportdialog.h          exportdialog.cpp
    exportprogressdialog.h  exportprogressdialog.cpp
    videopreviewwidget.h    videopreviewwidget.cpp
)

# Pin to C++20 for Qt header compatibility
set_target_properties(thrive-ui PROPERTIES CXX_STANDARD 20)

target_include_directories(thrive-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thrive-ui
    PUBLIC
        thrive-core
        thrive-engine
        thrive-accessibility
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
    PRIVATE
        MLT::mlt
        MLT::mltpp
)
