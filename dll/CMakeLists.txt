cmake_minimum_required(VERSION 3.30.2)
project(DLL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Limited configuration types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)

# Java
if(NOT DEFINED JAVA_HOME)
    if(DEFINED ENV{JAVA_HOME})
        set(JAVA_HOME "$ENV{JAVA_HOME}")
    else()
        set(JAVA_HOME "C:/Program Files/Java/jdk-21")
    endif()
endif()
include_directories("${JAVA_HOME}/include")
include_directories("${JAVA_HOME}/include/win32")
set(JVM_LIB "${JAVA_HOME}/lib/jvm.lib")

# Mapping version
set(MAPPING_VERSION "fabric_1.21.11" CACHE STRING "Mapping version (filename without .json in mappings/)")
set(MAPPING_FILE "${CMAKE_SOURCE_DIR}/mappings/${MAPPING_VERSION}.json")
if(NOT EXISTS "${MAPPING_FILE}")
    message(FATAL_ERROR "Mapping file not found: ${MAPPING_FILE}")
endif()
file(READ "${MAPPING_FILE}" MAPPING_JSON)

foreach(KEY
    class_Minecraft class_LocalPlayer class_Screen class_HitResult class_HitResultType
    class_MultiPlayerGameMode class_Gui class_ChatComponent class_LivingEntity class_Entity
    class_Component class_InteractionHand class_ItemStack class_GameType class_EntityHitResult class_Item
    fld_Minecraft_instance desc_Minecraft_instance
    fld_Minecraft_player   desc_Minecraft_player
    fld_Minecraft_gui      desc_Minecraft_gui
    fld_Minecraft_gameMode desc_Minecraft_gameMode
    fld_Minecraft_screen   desc_Minecraft_screen
    fld_Minecraft_hitResult desc_Minecraft_hitResult
    fld_InteractionHand_MAIN desc_InteractionHand_MAIN
    mtd_Minecraft_isPaused
    mtd_Gui_getChat                desc_Gui_getChat
    mtd_Screen_isPauseScreen       mtd_Screen_shouldCloseOnEsc
    mtd_MPGM_getPlayerMode         desc_MPGM_getPlayerMode
    mtd_MPGM_isDestroying          mtd_MPGM_getDestroyStage
    mtd_HitResult_getType          desc_HitResult_getType
    mtd_LivingEntity_isUsingItem
    mtd_LivingEntity_getItemInHand desc_LivingEntity_getItemInHand
    mtd_Entity_getName             desc_Entity_getName
    mtd_Entity_getUUID             desc_Entity_getUUID
    mtd_Entity_getTypeName         desc_Entity_getTypeName
    mtd_Component_getString
    mtd_EntityHitResult_getEntity  desc_EntityHitResult_getEntity
    mtd_ItemStack_getItem          desc_ItemStack_getItem
    mtd_Item_getName               desc_Item_getName
    class_ClientLevel class_AbstractClientPlayer class_Camera class_GameRenderer class_AABB class_Vec3
    fld_Minecraft_level         desc_Minecraft_level
    fld_Minecraft_gameRenderer  desc_Minecraft_gameRenderer
    fld_Entity_position desc_Entity_position fld_Entity_yRot fld_Entity_xRot
    mtd_Entity_getBoundingBox   desc_Entity_getBoundingBox
    mtd_Entity_setGlowingTag fld_Entity_hasGlowingTag
    fld_Entity_xo fld_Entity_yo fld_Entity_zo
    class_DeltaTracker
    mtd_Minecraft_getDeltaTracker   desc_Minecraft_getDeltaTracker
    mtd_DeltaTracker_getPartialTick desc_DeltaTracker_getPartialTick
    class_PlayerTeam
    mtd_Entity_getTeam               desc_Entity_getTeam
    mtd_PlayerTeam_formatNameForTeam desc_PlayerTeam_formatNameForTeam
    class_Style class_TextColor
    mtd_Component_getStyle    desc_Component_getStyle
    mtd_Component_getSiblings desc_Component_getSiblings
    mtd_Style_getColor        desc_Style_getColor
    mtd_TextColor_getValue
    fld_AABB_minX fld_AABB_minY fld_AABB_minZ fld_AABB_maxX fld_AABB_maxY fld_AABB_maxZ
    fld_Vec3_x fld_Vec3_y fld_Vec3_z
    fld_Camera_position desc_Camera_position fld_Camera_xRot fld_Camera_yRot
    fld_GameRenderer_mainCamera desc_GameRenderer_mainCamera
    mtd_GameRenderer_getFov     desc_GameRenderer_getFov
    mtd_ClientLevel_players     desc_ClientLevel_players
    class_PauseScreen
    mtd_Minecraft_setScreen     desc_Minecraft_setScreen
    mtd_Minecraft_pauseGame     desc_Minecraft_pauseGame
)
    string(JSON ${KEY} GET "${MAPPING_JSON}" "${KEY}")
endforeach()

configure_file(SDK/Mappings.h.in ${CMAKE_CURRENT_BINARY_DIR}/Mappings.h @ONLY)

if(DEFINED ENV{AC_WEBHOOK_PATH})
    set(DISCORD_WEBHOOK_PATH "$ENV{AC_WEBHOOK_PATH}" CACHE STRING "" FORCE)
else()
    set(DISCORD_WEBHOOK_PATH "" CACHE STRING "Discord webhook path")
endif()
if(DEFINED ENV{AC_BANNED_PATH})
    set(BANNED_PATH "$ENV{AC_BANNED_PATH}" CACHE STRING "" FORCE)
else()
    set(BANNED_PATH "" CACHE STRING "GitHub banned list path")
endif()
string(LENGTH "${DISCORD_WEBHOOK_PATH}" _webhook_len)
string(LENGTH "${BANNED_PATH}" _banned_len)
message(STATUS "DISCORD_WEBHOOK_PATH length: ${_webhook_len}")
message(STATUS "BANNED_PATH length: ${_banned_len}")
configure_file(Config.h.in ${CMAKE_CURRENT_BINARY_DIR}/Config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ── ImGui ────────────────────────────────────────────────────────────────────
include(FetchContent)
cmake_policy(SET CMP0169 OLD)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE opengl32)

# ── MinHook ──────────────────────────────────────────────────────────────────
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

# ── Sources ──────────────────────────────────────────────────────────────────
file(GLOB SDK_HEADERS          "SDK/*.h")
file(GLOB SDK_SOURCES          "SDK/*.cpp")
file(GLOB AUTOCLICKER_HEADERS  "modules/autoclicker/*.h")
file(GLOB AUTOCLICKER_SOURCES  "modules/autoclicker/*.cpp")
file(GLOB ESP_HEADERS          "modules/esp/*.h")
file(GLOB ESP_SOURCES          "modules/esp/*.cpp")
file(GLOB OVERLAY_HEADERS      "overlay/*.h")
file(GLOB OVERLAY_SOURCES      "overlay/*.cpp")
file(GLOB NETWORK_HEADERS      "network/*.h")
file(GLOB NETWORK_SOURCES      "network/*.cpp")

add_library(${PROJECT_NAME} SHARED
    main.cpp
    ${AUTOCLICKER_HEADERS}
    ${AUTOCLICKER_SOURCES}
    ${ESP_HEADERS}
    ${ESP_SOURCES}
    ${SDK_SOURCES}
    ${SDK_HEADERS}
    ${OVERLAY_HEADERS}
    ${OVERLAY_SOURCES}
    ${NETWORK_HEADERS}
    ${NETWORK_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${JVM_LIB} imgui minhook winhttp)

source_group("Header Files\\SDK"         FILES ${SDK_HEADERS})
source_group("Source Files\\SDK"         FILES ${SDK_SOURCES})
source_group("Header Files\\Autoclicker" FILES ${AUTOCLICKER_HEADERS})
source_group("Source Files\\Autoclicker" FILES ${AUTOCLICKER_SOURCES})
source_group("Header Files\\ESP"         FILES ${ESP_HEADERS})
source_group("Source Files\\ESP"         FILES ${ESP_SOURCES})
source_group("Header Files\\Overlay"     FILES ${OVERLAY_HEADERS})
source_group("Source Files\\Overlay"     FILES ${OVERLAY_SOURCES})
source_group("Header Files\\Network"     FILES ${NETWORK_HEADERS})
source_group("Source Files\\Network"     FILES ${NETWORK_SOURCES})
