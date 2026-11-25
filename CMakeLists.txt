cmake_minimum_required(VERSION 3.15)
project(JogoPiF C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Baixa e compila a Raylib automaticamente
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# The desktop GLFW backend exposes USB encoders before an SDL mapping exists.
if(PLATFORM STREQUAL "Desktop")
    set_property(SOURCE src/game_input.c APPEND PROPERTY COMPILE_DEFINITIONS PIF_GLFW_ARCADE)
    set_property(SOURCE src/game_input.c APPEND PROPERTY INCLUDE_DIRECTORIES
                 "${raylib_SOURCE_DIR}/src/external/glfw/include")
endif()

# Fontes do projeto
set(AUDIO_SOURCES src/game_audio.c src/boss_music.c)

set(SOURCES
    src/game.c
    src/game_input.c
    src/game_text.c
    src/logica.c
    src/dev_tools.c
    src/clearing.c
    src/atmosphere.c
    src/hud.c
    src/speedrun.c
    src/menu.c
    src/menu_scene.c
    src/menu_video.c
    src/cutscene.c
    src/guardian_cutscene.c
    ${AUDIO_SOURCES}
    src/weapon_select.c
    src/bible.c
    src/gun.c
    src/enemy.c
    src/necromancer.c
    src/cemetery_enemy.c
    src/village_enemy.c
    src/encounter.c
    src/presentation.c
    src/video_settings.c
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE mfplat mfreadwrite mfuuid ole32 uuid)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(game_audio_test tests/game_audio_test.c src/boss_music.c)
    target_link_libraries(game_audio_test PRIVATE raylib)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/audit/game-audio/assets/audio/sfx")
    add_test(NAME game_audio COMMAND game_audio_test)
    set_tests_properties(game_audio PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/audit/game-audio"
                         SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_executable(atmosphere_audit tests/atmosphere_audit.c src/clearing.c)
    target_include_directories(atmosphere_audit PRIVATE src)
    target_link_libraries(atmosphere_audit PRIVATE raylib)
    add_test(NAME atmosphere_audit COMMAND atmosphere_audit)
    set_tests_properties(atmosphere_audit PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" TIMEOUT 120)
    add_executable(logica_test tests/logica_test.c src/logica.c)
    target_include_directories(logica_test PRIVATE src)
    add_test(NAME logica COMMAND logica_test)
    add_executable(speedrun_test tests/speedrun_test.c src/speedrun.c)
    target_include_directories(speedrun_test PRIVATE src)
    target_link_libraries(speedrun_test PRIVATE raylib)
    add_test(NAME speedrun COMMAND speedrun_test)
    add_executable(enemy_audit_test tests/enemy_audit_test.c src/enemy.c src/necromancer.c src/cemetery_enemy.c src/village_enemy.c src/clearing.c src/game_text.c)
    target_sources(enemy_audit_test PRIVATE src/game_input.c)
    target_include_directories(enemy_audit_test PRIVATE src)
    target_sources(enemy_audit_test PRIVATE ${AUDIO_SOURCES})
    target_link_libraries(enemy_audit_test PRIVATE raylib)
    add_test(NAME enemy_audit COMMAND enemy_audit_test)
    add_executable(map_connectivity_test tests/map_connectivity_test.c src/clearing.c)
    target_include_directories(map_connectivity_test PRIVATE src)
    target_link_libraries(map_connectivity_test PRIVATE raylib)
    add_test(NAME map_connectivity COMMAND map_connectivity_test)
    add_executable(player_collision_test tests/player_collision_test.c src/clearing.c)
    target_include_directories(player_collision_test PRIVATE src)
    target_link_libraries(player_collision_test PRIVATE raylib)
    add_test(NAME player_collision COMMAND player_collision_test)
    add_executable(encounter_test tests/encounter_test.c src/encounter.c src/logica.c src/presentation.c src/enemy.c src/necromancer.c src/cemetery_enemy.c src/village_enemy.c src/clearing.c src/menu.c src/menu_video.c ${AUDIO_SOURCES} src/game_text.c src/dev_tools.c src/bible.c src/gun.c src/video_settings.c)
    target_sources(encounter_test PRIVATE src/hud.c)
    target_sources(encounter_test PRIVATE src/speedrun.c)
    target_sources(encounter_test PRIVATE src/menu_scene.c)
    target_sources(encounter_test PRIVATE src/game_input.c)
    target_include_directories(encounter_test PRIVATE src)
    target_link_libraries(encounter_test PRIVATE raylib)
    if(WIN32)
        target_link_libraries(encounter_test PRIVATE mfplat mfreadwrite mfuuid ole32 uuid)
    endif()
    set_target_properties(encounter_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/audit")
    add_test(NAME clearing_encounter COMMAND encounter_test)
    set_tests_properties(clearing_encounter PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    add_executable(guardian_cutscene_test tests/guardian_cutscene_test.c src/guardian_cutscene.c src/game_text.c src/video_settings.c)
    target_sources(guardian_cutscene_test PRIVATE src/game_input.c)
    target_include_directories(guardian_cutscene_test PRIVATE src)
    target_link_libraries(guardian_cutscene_test PRIVATE raylib)
    set_target_properties(guardian_cutscene_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/audit")
    add_test(NAME guardian_cutscene COMMAND guardian_cutscene_test)
    set_tests_properties(guardian_cutscene PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(STORY_TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM STORY_TEST_SOURCES src/game.c)
    add_executable(guardian_story_flow_test tests/guardian_story_flow_test.c ${STORY_TEST_SOURCES})
    target_include_directories(guardian_story_flow_test PRIVATE src)
    target_link_libraries(guardian_story_flow_test PRIVATE raylib)
    if(WIN32)
        target_link_libraries(guardian_story_flow_test PRIVATE mfplat mfreadwrite mfuuid ole32 uuid)
    endif()
    set_target_properties(guardian_story_flow_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/audit/story_flow")
    add_test(NAME guardian_story_flow COMMAND guardian_story_flow_test)
    set_tests_properties(guardian_story_flow PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    # Some legacy tests initialize their scenarios inside assert().
    foreach(test game_audio_test atmosphere_audit logica_test speedrun_test enemy_audit_test map_connectivity_test
                 player_collision_test encounter_test guardian_cutscene_test guardian_story_flow_test)
        if(MSVC)
            target_compile_options(${test} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test} PRIVATE -UNDEBUG)
        endif()
    endforeach()
endif()

# Mantem os assets atualizados no diretorio do binario a cada build.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        DEPENDS ${PROJECT_NAME}
    )
endif()
