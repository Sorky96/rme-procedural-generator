add_library(rme_procedural_core STATIC core/generation.cpp)
target_compile_features(rme_procedural_core PUBLIC cxx_std_23)
target_include_directories(rme_procedural_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rme PRIVATE rme_procedural_core)
target_sources(rme PRIVATE adapter/generation_input.cpp adapter/generation_input.h)
target_sources(rme PRIVATE adapter/ground_resolver.cpp adapter/ground_resolver.h)
target_sources(rme PRIVATE adapter/snapshot_memory.cpp adapter/snapshot_memory.h
    adapter/snapshot_equal.cpp adapter/snapshot_equal.h
    adapter/generation_snapshot.cpp adapter/generation_snapshot.h)

target_sources(rme PRIVATE adapter/terrain_preview.h
    ${PROJECT_SOURCE_DIR}/source/editor/operations/procedural_draw_operations.cpp)

target_sources(rme PRIVATE adapter/terrain_preparation.cpp adapter/terrain_preparation.h)
target_sources(rme PRIVATE adapter/generation_selection.cpp adapter/generation_selection.h)
target_sources(rme PRIVATE adapter/terrain_validation.cpp adapter/terrain_validation.h)
target_sources(rme PRIVATE adapter/generation_assets.cpp adapter/generation_assets.h)
target_sources(rme PRIVATE adapter/terrain_target.cpp adapter/terrain_target.h)

target_sources(rme PRIVATE adapter/terrain_commit.h
    ${PROJECT_SOURCE_DIR}/source/editor/operations/procedural_commit_operations.cpp)

target_sources(rme PRIVATE
    ${PROJECT_SOURCE_DIR}/source/ui/dialogs/terrain_generator_dialog.cpp
    ${PROJECT_SOURCE_DIR}/source/ui/dialogs/terrain_generator_dialog.h)

add_executable(test_procedural_core ${PROJECT_SOURCE_DIR}/tests/test_procedural_core.cpp)
target_link_libraries(test_procedural_core PRIVATE rme_procedural_core)
add_test(NAME test_procedural_core COMMAND test_procedural_core)
