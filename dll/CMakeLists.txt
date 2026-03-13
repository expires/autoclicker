cmake_minimum_required(VERSION 3.30.2)
project(DLL)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set configuration to Release only
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Limited configuration types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)

# Set the path to JDK installation (can be overridden via -DJAVA_HOME=... or env var)
if(NOT DEFINED JAVA_HOME)
    if(DEFINED ENV{JAVA_HOME})
        set(JAVA_HOME "$ENV{JAVA_HOME}")
    else()
        set(JAVA_HOME "C:/Program Files/Java/jdk-21")
    endif()
endif()
# Add include directories for JNI
include_directories("${JAVA_HOME}/include")
include_directories("${JAVA_HOME}/include/win32")

# Link against the JVM library
set(JVM_LIB "${JAVA_HOME}/lib/jvm.lib")

# Mapping version — set via -DMAPPING_VERSION=fabric_1.21.11 or mojang_1.21.4
set(MAPPING_VERSION "fabric_1.21.11" CACHE STRING "Mapping version (filename without .json in mappings/)")

set(MAPPING_FILE "${CMAKE_SOURCE_DIR}/mappings/${MAPPING_VERSION}.json")
if(NOT EXISTS "${MAPPING_FILE}")
    message(FATAL_ERROR "Mapping file not found: ${MAPPING_FILE}")
endif()
file(READ "${MAPPING_FILE}" MAPPING_JSON)

# Extract every key from the JSON into a CMake variable
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
    mtd_Entity_getTypeName         desc_Entity_getTypeName
    mtd_Component_getString
    mtd_EntityHitResult_getEntity  desc_EntityHitResult_getEntity
    mtd_ItemStack_getItem          desc_ItemStack_getItem
    mtd_Item_getName               desc_Item_getName
)
    string(JSON ${KEY} GET "${MAPPING_JSON}" "${KEY}")
endforeach()

configure_file(SDK/Mappings.h.in ${CMAKE_CURRENT_BINARY_DIR}/Mappings.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Automatically find all .cpp and .h files in the SDK & Clicker directories
file(GLOB SDK_HEADERS "SDK/*.h")
file(GLOB SDK_SOURCES "SDK/*.cpp")
file(GLOB AUTOCLICKER_HEADERS "modules/autoclicker/*.h")
file(GLOB AUTOCLICKER_SOURCES "modules/autoclicker/*.cpp")

# Add the shared library target (DLL)
add_library(${PROJECT_NAME} SHARED 
    main.cpp 
    ${AUTOCLICKER_HEADERS}
    ${AUTOCLICKER_SOURCES}
    ${SDK_SOURCES} 
    ${SDK_HEADERS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${JVM_LIB})

source_group("Header Files\\SDK" FILES ${SDK_HEADERS})
source_group("Source Files\\SDK" FILES ${SDK_SOURCES})
source_group("Header Files\\Autoclicker" FILES ${AUTOCLICKER_HEADERS})
source_group("Source Files\\Autoclicker" FILES ${AUTOCLICKER_SOURCES})