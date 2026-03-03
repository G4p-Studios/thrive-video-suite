# ---------------------------------------------------------------------------
# Tests – each test file is a separate executable via QTEST_MAIN
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing()

# Helper macro to reduce boilerplate
macro(thrive_add_test _name)
    qt_add_executable(${_name} ${_name}.cpp)
    set_target_properties(${_name} PROPERTIES
        QT_EXCLUDE_FROM_TRANSLATION ON
        CXX_STANDARD 23
    )
    target_link_libraries(${_name} PRIVATE
        thrive-core
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        ${ARGN}
    )
    add_test(NAME ${_name} COMMAND ${_name})
endmacro()

# ── core model tests ─────────────────────────────────────────────────
thrive_add_test(test_timecode)
thrive_add_test(test_timeline)
thrive_add_test(test_clip)
thrive_add_test(test_track)
thrive_add_test(test_effect)
thrive_add_test(test_marker)
thrive_add_test(test_transition)
thrive_add_test(test_project)

# ── command / undo tests ─────────────────────────────────────────────
thrive_add_test(test_commands)

# ── accessibility tests ──────────────────────────────────────────────
thrive_add_test(test_announcer thrive-accessibility)
