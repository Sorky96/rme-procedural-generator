add_library(rme_procedural_core STATIC core/generation.cpp core/surface.cpp core/decoration.cpp core/reference_header.cpp core/semantic_map.cpp core/topology.cpp core/features.cpp core/feature_report.cpp core/png_writer.cpp core/semantic_image.cpp core/similarity.cpp core/reference_split.cpp core/style_profile.cpp core/style_fit.cpp core/doodad_class.cpp core/decor_context.cpp core/style_profile_set.cpp)
target_compile_features(rme_procedural_core PUBLIC cxx_std_23)
target_include_directories(rme_procedural_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rme PRIVATE rme_procedural_core)
target_sources(rme PRIVATE adapter/generation_input.cpp adapter/generation_input.h)
target_sources(rme PRIVATE adapter/ground_resolver.cpp adapter/ground_resolver.h)
target_sources(rme PRIVATE adapter/surface_resolver.cpp adapter/surface_resolver.h)
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

target_sources(rme PRIVATE adapter/reference_import.cpp adapter/reference_import.h)

target_sources(rme PRIVATE adapter/terrain_commit.h
    ${PROJECT_SOURCE_DIR}/source/editor/operations/procedural_commit_operations.cpp)

target_sources(rme PRIVATE
    ${PROJECT_SOURCE_DIR}/source/ui/dialogs/terrain_generator_dialog.cpp
    ${PROJECT_SOURCE_DIR}/source/ui/dialogs/terrain_generator_dialog.h)

add_executable(test_procedural_core ${PROJECT_SOURCE_DIR}/tests/test_procedural_core.cpp)
target_link_libraries(test_procedural_core PRIVATE rme_procedural_core)
add_test(NAME test_procedural_core COMMAND test_procedural_core)

add_executable(test_procedural_surface ${PROJECT_SOURCE_DIR}/tests/test_procedural_surface.cpp)
target_link_libraries(test_procedural_surface PRIVATE rme_procedural_core)
add_test(NAME test_procedural_surface COMMAND test_procedural_surface)

add_executable(test_procedural_reference_header ${PROJECT_SOURCE_DIR}/tests/test_procedural_reference_header.cpp)
target_link_libraries(test_procedural_reference_header PRIVATE rme_procedural_core)
add_test(NAME test_procedural_reference_header COMMAND test_procedural_reference_header)

add_executable(test_procedural_semantic_map ${PROJECT_SOURCE_DIR}/tests/test_procedural_semantic_map.cpp)
target_link_libraries(test_procedural_semantic_map PRIVATE rme_procedural_core)
add_test(NAME test_procedural_semantic_map COMMAND test_procedural_semantic_map)

add_executable(test_procedural_topology ${PROJECT_SOURCE_DIR}/tests/test_procedural_topology.cpp)
target_link_libraries(test_procedural_topology PRIVATE rme_procedural_core)
add_test(NAME test_procedural_topology COMMAND test_procedural_topology)

add_executable(test_procedural_features ${PROJECT_SOURCE_DIR}/tests/test_procedural_features.cpp)
target_link_libraries(test_procedural_features PRIVATE rme_procedural_core)
add_test(NAME test_procedural_features COMMAND test_procedural_features)

add_executable(test_procedural_semantic_image ${PROJECT_SOURCE_DIR}/tests/test_procedural_semantic_image.cpp)
target_link_libraries(test_procedural_semantic_image PRIVATE rme_procedural_core)
add_test(NAME test_procedural_semantic_image COMMAND test_procedural_semantic_image)

add_executable(test_procedural_similarity ${PROJECT_SOURCE_DIR}/tests/test_procedural_similarity.cpp)
target_link_libraries(test_procedural_similarity PRIVATE rme_procedural_core)
add_test(NAME test_procedural_similarity COMMAND test_procedural_similarity)

add_executable(test_procedural_style_profile ${PROJECT_SOURCE_DIR}/tests/test_procedural_style_profile.cpp)
target_link_libraries(test_procedural_style_profile PRIVATE rme_procedural_core)
add_test(NAME test_procedural_style_profile COMMAND test_procedural_style_profile)

add_executable(test_procedural_style_fit ${PROJECT_SOURCE_DIR}/tests/test_procedural_style_fit.cpp)
target_link_libraries(test_procedural_style_fit PRIVATE rme_procedural_core)
add_test(NAME test_procedural_style_fit COMMAND test_procedural_style_fit)

add_executable(reference_split ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference_split_main.cpp)
target_link_libraries(reference_split PRIVATE rme_procedural_core)

add_executable(style_keymap ${CMAKE_CURRENT_SOURCE_DIR}/tools/style_keymap_main.cpp)
target_link_libraries(style_keymap PRIVATE rme_procedural_core)

add_executable(style_fit ${CMAKE_CURRENT_SOURCE_DIR}/tools/style_fit_main.cpp)
target_link_libraries(style_fit PRIVATE rme_procedural_core)

add_executable(test_procedural_decor ${PROJECT_SOURCE_DIR}/tests/test_procedural_decor.cpp)
target_link_libraries(test_procedural_decor PRIVATE rme_procedural_core)
add_test(NAME test_procedural_decor COMMAND test_procedural_decor)

add_executable(style_eval ${CMAKE_CURRENT_SOURCE_DIR}/tools/style_eval_main.cpp)
target_link_libraries(style_eval PRIVATE rme_procedural_core)

add_executable(style_styles ${CMAKE_CURRENT_SOURCE_DIR}/tools/style_styles_main.cpp)
target_link_libraries(style_styles PRIVATE rme_procedural_core)

add_executable(test_procedural_style_profile_set ${PROJECT_SOURCE_DIR}/tests/test_procedural_style_profile_set.cpp)
target_link_libraries(test_procedural_style_profile_set PRIVATE rme_procedural_core)
add_test(NAME test_procedural_style_profile_set COMMAND test_procedural_style_profile_set)

target_sources(rme PRIVATE adapter/style_profile_store.cpp adapter/style_profile_store.h)
