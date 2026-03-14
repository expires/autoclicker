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
    class_Level class_ClientLevel
    fld_Minecraft_instance desc_Minecraft_instance
    fld_Minecraft_player   desc_Minecraft_player
    fld_Minecraft_gui      desc_Minecraft_gui
    fld_Minecraft_gameMode desc_Minecraft_gameMode
    fld_Minecraft_screen   desc_Minecraft_screen
    fld_Minecraft_hitResult desc_Minecraft_hitResult
    fld_Minecraft_level     desc_Minecraft_level
    fld_InteractionHand_MAIN desc_InteractionHand_MAIN
    mtd_Minecraft_isPaused
    mtd_Gui_getChat                desc_Gui_getChat
    mtd_Screen_isPauseScreen       mtd_Screen_shouldCloseOnEsc
    mtd_MPGM_getPlayerMode         desc_MPGM_getPlayerMode
    mtd_MPGM_isDestroying          mtd_MPGM_getDestroyStage
    mtd_HitResult_getType          desc_HitResult_getType
    mtd_LivingEntity_isUsingItem
    mtd_LivingEntity_getItemInHand desc_LivingEntity_getItemInHand
    mtd_LivingEntity_getHealth     desc_LivingEntity_getHealth
    mtd_LivingEntity_getMaxHealth  desc_LivingEntity_getMaxHealth
    mtd_Entity_getName             desc_Entity_getName
    mtd_Entity_getTypeName         desc_Entity_getTypeName
    mtd_Entity_getX mtd_Entity_getY mtd_Entity_getZ
    mtd_Component_getString
    mtd_EntityHitResult_getEntity  desc_EntityHitResult_getEntity
    mtd_ItemStack_getItem          desc_ItemStack_getItem
    mtd_Item_getName               desc_Item_getName
    mtd_Level_players              desc_Level_players
)
    string(JSON ${KEY} GET "${MAPPING_JSON}" "${KEY}")
endforeach()

configure_file(SDK/Mappings.h.in ${CMAKE_CURRENT_BINARY_DIR}/Mappings.h @ONLY)
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
file(GLOB LLM_HEADERS          "modules/llm/*.h")
file(GLOB LLM_SOURCES          "modules/llm/*.cpp")
file(GLOB OVERLAY_HEADERS      "overlay/*.h")
file(GLOB OVERLAY_SOURCES      "overlay/*.cpp")

add_library(${PROJECT_NAME} SHARED
    main.cpp
    ${AUTOCLICKER_HEADERS}
    ${AUTOCLICKER_SOURCES}
    ${LLM_HEADERS}
    ${LLM_SOURCES}
    ${SDK_SOURCES}
    ${SDK_HEADERS}
    ${OVERLAY_HEADERS}
    ${OVERLAY_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${JVM_LIB} imgui minhook winhttp)

source_group("Header Files\\SDK"         FILES ${SDK_HEADERS})
source_group("Source Files\\SDK"         FILES ${SDK_SOURCES})
source_group("Header Files\\Autoclicker" FILES ${AUTOCLICKER_HEADERS})
source_group("Source Files\\Autoclicker" FILES ${AUTOCLICKER_SOURCES})
source_group("Header Files\\LLM"         FILES ${LLM_HEADERS})
source_group("Source Files\\LLM"         FILES ${LLM_SOURCES})
source_group("Header Files\\Overlay"     FILES ${OVERLAY_HEADERS})
source_group("Source Files\\Overlay"     FILES ${OVERLAY_SOURCES})
