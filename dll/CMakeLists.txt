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
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MAPPING_FILE}")
file(READ "${MAPPING_FILE}" MAPPING_JSON)

string(JSON MNC_GENERATION ERROR_VARIABLE _genErr GET "${MAPPING_JSON}" "generation")
if(_genErr OR NOT MNC_GENERATION)
    set(MNC_GENERATION "modern")
endif()
if(MNC_GENERATION STREQUAL "legacy")
    set(MNC_LEGACY 1)
else()
    set(MNC_LEGACY 0)
endif()
message(STATUS "Mapping ${MAPPING_VERSION}: generation=${MNC_GENERATION} (MNC_LEGACY=${MNC_LEGACY})")

foreach(KEY
    class_Minecraft class_LocalPlayer class_Screen class_HitResult class_HitResultType
    class_MultiPlayerGameMode class_Gui class_ChatComponent class_LivingEntity class_Entity
    class_Component class_InteractionHand class_ItemStack class_GameType class_EntityHitResult class_Item class_BlockItem
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
    mtd_LivingEntity_getHealth     mtd_LivingEntity_getMaxHealth
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
    mtd_Camera_getFov desc_Camera_getFov
    fld_GameRenderer_mainCamera desc_GameRenderer_mainCamera
    mtd_GameRenderer_getFov     desc_GameRenderer_getFov
    mtd_ClientLevel_players     desc_ClientLevel_players
    class_PauseScreen class_AbstractContainerScreen class_ChatScreen
    mtd_Minecraft_setScreen     desc_Minecraft_setScreen
    mtd_Minecraft_pauseGame     desc_Minecraft_pauseGame
    class_Inventory
    mtd_Player_getInventory     desc_Player_getInventory
    mtd_Player_dropSelectedItem desc_Player_dropSelectedItem
    fld_Inventory_selected
    mtd_Inventory_getItem       desc_Inventory_getItem
    mtd_ItemStack_getHoverName  desc_ItemStack_getHoverName
    mtd_ItemStack_isEmpty
    class_BlockPos class_BlockState
    mtd_Level_getBlockState     desc_Level_getBlockState
    mtd_BlockState_isAir
    mtd_BlockState_blocksMotion

    fld_Entity_posX fld_Entity_posY fld_Entity_posZ
    mtd_Entity_getPositionVector desc_Entity_getPositionVector
    fld_HitResult_typeOfHit desc_HitResult_typeOfHit
    fld_HitResult_entityHit desc_HitResult_entityHit
    fld_World_playerEntities desc_World_playerEntities
    mtd_BlockState_getBlock desc_BlockState_getBlock
    mtd_Block_getMaterial   desc_Block_getMaterial
    mtd_Material_blocksMovement
    fld_Player_inventory desc_Player_inventory
    class_GameSettings
    fld_Minecraft_gameSettings desc_Minecraft_gameSettings
    fld_GameSettings_fovSetting
    class_Timer
    fld_Minecraft_timer desc_Minecraft_timer
    fld_Timer_renderPartialTicks
    fld_Entity_prevYRot fld_Entity_prevXRot
    class_ActiveRenderInfo
    fld_ActiveRenderInfo_modelview  desc_ActiveRenderInfo_modelview
    fld_ActiveRenderInfo_projection desc_ActiveRenderInfo_projection
    fld_ActiveRenderInfo_viewport   desc_ActiveRenderInfo_viewport
    mtd_Entity_getDisplayName       desc_Entity_getDisplayName
    mtd_GameType_getId
)
    string(JSON ${KEY} ERROR_VARIABLE _keyErr GET "${MAPPING_JSON}" "${KEY}")
    if(_keyErr)
        set(${KEY} "")
    endif()
endforeach()

configure_file(SDK/Mappings.h.in ${CMAKE_CURRENT_BINARY_DIR}/Mappings.h @ONLY)

if(DEFINED ENV{MNC_WEBHOOK_PATH})
    set(DISCORD_WEBHOOK_PATH "$ENV{MNC_WEBHOOK_PATH}" CACHE STRING "" FORCE)
else()
    set(DISCORD_WEBHOOK_PATH "" CACHE STRING "Discord webhook path")
endif()
if(DEFINED ENV{MNC_BANNED_PATH})
    set(BANNED_PATH "$ENV{MNC_BANNED_PATH}" CACHE STRING "" FORCE)
else()
    set(BANNED_PATH "" CACHE STRING "GitHub banned list path")
endif()
string(LENGTH "${DISCORD_WEBHOOK_PATH}" _webhook_len)
string(LENGTH "${BANNED_PATH}" _banned_len)
message(STATUS "DISCORD_WEBHOOK_PATH length: ${_webhook_len}")
message(STATUS "BANNED_PATH length: ${_banned_len}")
configure_file(config/Config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/Config.h @ONLY)

# ── Embedded logo ────────────────────────────────────────────────────────────
# The DLL is injected, so it can't rely on a disk path; bake data/logo.png into
# the binary as a byte array. Regenerated at configure time.
set(LOGO_SRC    "${CMAKE_SOURCE_DIR}/data/logo.png")
set(LOGO_HEADER "${CMAKE_CURRENT_BINARY_DIR}/LogoData.h")
if(EXISTS "${LOGO_SRC}")
    file(READ "${LOGO_SRC}" LOGO_HEX HEX)
    string(REGEX REPLACE "(..)" "0x\\1," LOGO_BYTES "${LOGO_HEX}")
    string(LENGTH "${LOGO_HEX}" _logo_hexlen)
    math(EXPR LOGO_LEN "${_logo_hexlen} / 2")
    file(WRITE "${LOGO_HEADER}"
        "#pragma once\n// Auto-generated from data/logo.png — do not edit.\n"
        "inline const unsigned char g_logoPng[] = { ${LOGO_BYTES} };\n"
        "inline const unsigned int  g_logoPngSize = ${LOGO_LEN};\n")
else()
    message(WARNING "data/logo.png not found; logo will be disabled")
    file(WRITE "${LOGO_HEADER}"
        "#pragma once\ninline const unsigned char g_logoPng[] = { 0 };\n"
        "inline const unsigned int  g_logoPngSize = 0;\n")
endif()

# Build-time Revision.h — re-runs every build via a phony custom target so
# the embedded BUILD_REVISION matches the current HEAD SHA (+ -dirty if the
# working tree has uncommitted changes). The helper script writes the file
# only when content changes, so a no-op build doesn't ripple recompiles
# into network/Network.cpp unless the SHA actually moved.
set(REVISION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/Revision.h)
add_custom_target(generate_revision_h ALL
    BYPRODUCTS ${REVISION_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT=${REVISION_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RevisionHeader.cmake
    COMMENT "Generating Revision.h"
    VERBATIM
)
# Seed the file at configure time so the first build has something to
# include even before the custom target has fired.
if(NOT EXISTS ${REVISION_HEADER})
    file(WRITE ${REVISION_HEADER}
        "#pragma once\n#define BUILD_REVISION \"unknown\"\n")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/SDK)

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

# ── stb (header-only, for stb_image PNG decode) ──────────────────────────────
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# ── Sources ──────────────────────────────────────────────────────────────────
file(GLOB SDK_HEADERS          "SDK/*.h")
file(GLOB SDK_SOURCES          "SDK/*.cpp")

string(JSON MNC_SDK ERROR_VARIABLE _sdkErr GET "${MAPPING_JSON}" "sdk")
if(_sdkErr OR NOT MNC_SDK)
    if(MNC_LEGACY)
        set(MNC_SDK "1.8.9")
    else()
        set(MNC_SDK "1.21.11")
    endif()
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/${MNC_SDK}")
    message(FATAL_ERROR "SDK implementation dir not found: SDK/${MNC_SDK}")
endif()

string(JSON MNC_SDK_BASE ERROR_VARIABLE _baseErr GET "${MAPPING_JSON}" "sdkBase")
if(_baseErr OR NOT MNC_SDK_BASE)
    set(MNC_SDK_BASE "")
endif()

file(GLOB SDK_VERSION_SOURCES "SDK/${MNC_SDK}/*.cpp")

if(MNC_SDK_BASE)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/${MNC_SDK_BASE}")
        message(FATAL_ERROR "SDK base dir not found: SDK/${MNC_SDK_BASE}")
    endif()
    set(_override_names "")
    foreach(_f ${SDK_VERSION_SOURCES})
        get_filename_component(_n "${_f}" NAME)
        list(APPEND _override_names "${_n}")
    endforeach()
    file(GLOB SDK_BASE_SOURCES "SDK/${MNC_SDK_BASE}/*.cpp")
    foreach(_f ${SDK_BASE_SOURCES})
        get_filename_component(_n "${_f}" NAME)
        if(NOT _n IN_LIST _override_names)
            list(APPEND SDK_VERSION_SOURCES "${_f}")
        endif()
    endforeach()
    message(STATUS "Mapping ${MAPPING_VERSION}: SDK impl dir = SDK/${MNC_SDK} (base SDK/${MNC_SDK_BASE})")
else()
    message(STATUS "Mapping ${MAPPING_VERSION}: SDK impl dir = SDK/${MNC_SDK}")
endif()

list(APPEND SDK_SOURCES ${SDK_VERSION_SOURCES})
file(GLOB AUTOCLICKER_HEADERS  "modules/autoclicker/*.h")
file(GLOB AUTOCLICKER_SOURCES  "modules/autoclicker/*.cpp")
file(GLOB ESP_HEADERS          "modules/esp/*.h")
file(GLOB ESP_SOURCES          "modules/esp/*.cpp")
file(GLOB MACROS_HEADERS       "modules/macros/*.h")
file(GLOB MACROS_SOURCES       "modules/macros/*.cpp")
file(GLOB AIM_HEADERS          "modules/aim/*.h")
file(GLOB AIM_SOURCES          "modules/aim/*.cpp")
file(GLOB AUTOBLOCK_HEADERS    "modules/autoblock/*.h")
file(GLOB AUTOBLOCK_SOURCES    "modules/autoblock/*.cpp")
file(GLOB FRIENDS_HEADERS      "modules/friends/*.h")
file(GLOB FRIENDS_SOURCES      "modules/friends/*.cpp")
file(GLOB SCAFFOLD_HEADERS     "modules/scaffold/*.h")
file(GLOB SCAFFOLD_SOURCES     "modules/scaffold/*.cpp")
file(GLOB SPRINTRESET_HEADERS  "modules/sprintreset/*.h")
file(GLOB SPRINTRESET_SOURCES  "modules/sprintreset/*.cpp")
file(GLOB OVERLAY_HEADERS      "overlay/*.h" "overlay/tabs/*.h")
file(GLOB OVERLAY_SOURCES      "overlay/*.cpp" "overlay/tabs/*.cpp")
file(GLOB NETWORK_HEADERS      "network/*.h")
file(GLOB NETWORK_SOURCES      "network/*.cpp")

add_library(${PROJECT_NAME} SHARED
    main.cpp
    logger/Logger.h
    logger/Logger.cpp
    config/Settings.h
    config/Settings.cpp
    teardown/Teardown.h
    teardown/Teardown.cpp
    ${AUTOCLICKER_HEADERS}
    ${AUTOCLICKER_SOURCES}
    ${ESP_HEADERS}
    ${ESP_SOURCES}
    ${MACROS_HEADERS}
    ${MACROS_SOURCES}
    ${AIM_HEADERS}
    ${AIM_SOURCES}
    ${AUTOBLOCK_HEADERS}
    ${AUTOBLOCK_SOURCES}
    ${FRIENDS_HEADERS}
    ${FRIENDS_SOURCES}
    ${SCAFFOLD_HEADERS}
    ${SCAFFOLD_SOURCES}
    ${SPRINTRESET_HEADERS}
    ${SPRINTRESET_SOURCES}
    ${SDK_SOURCES}
    ${SDK_HEADERS}
    ${OVERLAY_HEADERS}
    ${OVERLAY_SOURCES}
    ${NETWORK_HEADERS}
    ${NETWORK_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${JVM_LIB} imgui minhook winhttp)
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR})
target_compile_definitions(${PROJECT_NAME} PRIVATE MNC_LEGACY=${MNC_LEGACY} MNC_GAME_VERSION="${MNC_SDK}")
add_dependencies(${PROJECT_NAME} generate_revision_h)

source_group("Header Files\\SDK"         FILES ${SDK_HEADERS})
source_group("Source Files\\SDK"         FILES ${SDK_SOURCES})
source_group("Header Files\\Autoclicker" FILES ${AUTOCLICKER_HEADERS})
source_group("Source Files\\Autoclicker" FILES ${AUTOCLICKER_SOURCES})
source_group("Header Files\\ESP"         FILES ${ESP_HEADERS})
source_group("Source Files\\ESP"         FILES ${ESP_SOURCES})
source_group("Header Files\\Macros"      FILES ${MACROS_HEADERS})
source_group("Source Files\\Macros"      FILES ${MACROS_SOURCES})
source_group("Header Files\\Aim"         FILES ${AIM_HEADERS})
source_group("Source Files\\Aim"         FILES ${AIM_SOURCES})
source_group("Header Files\\Autoblock" FILES ${AUTOBLOCK_HEADERS})
source_group("Source Files\\Autoblock" FILES ${AUTOBLOCK_SOURCES})
source_group("Header Files\\Friends"     FILES ${FRIENDS_HEADERS})
source_group("Source Files\\Friends"     FILES ${FRIENDS_SOURCES})
source_group("Header Files\\Scaffold"    FILES ${SCAFFOLD_HEADERS})
source_group("Source Files\\Scaffold"    FILES ${SCAFFOLD_SOURCES})
source_group("Header Files\\SprintReset" FILES ${SPRINTRESET_HEADERS})
source_group("Source Files\\SprintReset" FILES ${SPRINTRESET_SOURCES})
source_group("Header Files\\Overlay"     FILES ${OVERLAY_HEADERS})
source_group("Source Files\\Overlay"     FILES ${OVERLAY_SOURCES})
source_group("Header Files\\Network"     FILES ${NETWORK_HEADERS})
source_group("Source Files\\Network"     FILES ${NETWORK_SOURCES})
