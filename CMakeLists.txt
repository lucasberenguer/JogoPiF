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

# Fontes do projeto
set(SOURCES
    src/game.c
    src/clearing.c
    src/hud.c
    src/menu.c
    src/menu_video.c
    src/weapon_select.c
    src/bible.c
    src/gun.c
    src/enemy.c
    src/cemetery_enemy.c
    src/encounter.c
    src/presentation.c
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE mfplat mfreadwrite mfuuid ole32 uuid)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(enemy_audit_test tests/enemy_audit_test.c src/enemy.c src/cemetery_enemy.c src/clearing.c)
    target_include_directories(enemy_audit_test PRIVATE src)
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
    add_executable(encounter_test tests/encounter_test.c src/encounter.c src/presentation.c src/enemy.c src/cemetery_enemy.c src/clearing.c src/menu.c src/menu_video.c)
    target_include_directories(encounter_test PRIVATE src)
    target_link_libraries(encounter_test PRIVATE raylib)
    if(WIN32)
        target_link_libraries(encounter_test PRIVATE mfplat mfreadwrite mfuuid ole32 uuid)
    endif()
    set_target_properties(encounter_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/audit")
    add_test(NAME clearing_encounter COMMAND encounter_test)
    set_tests_properties(clearing_encounter PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
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
