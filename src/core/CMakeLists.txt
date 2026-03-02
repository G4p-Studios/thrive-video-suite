# ---------------------------------------------------------------------------
# thrive-core  – timeline data model, undo/redo, project serialization
# ---------------------------------------------------------------------------
qt_add_library(thrive-core STATIC
    timeline.h          timeline.cpp
    track.h             track.cpp
    clip.h              clip.cpp
    marker.h            marker.cpp
    effect.h            effect.cpp
    transition.h        transition.cpp
    timecode.h          timecode.cpp
    project.h           project.cpp
    projectserializer.h projectserializer.cpp
    commands.h          commands.cpp
)

target_include_directories(thrive-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(thrive-core
    PUBLIC
        Qt6::Core
        Qt6::Widgets        # for QUndoStack / QUndoCommand
    PRIVATE
        QuaZip::QuaZip
)
