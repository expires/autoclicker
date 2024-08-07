cmake_minimum_required(VERSION 3.30.2)
project(GUI)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Fetch GLFW
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(glfw)

# Fetch ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(imgui)

# Fetch imgui_toggle
FetchContent_Declare(
    imgui_toggle
    GIT_REPOSITORY https://github.com/cmdwtf/imgui_toggle.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(imgui_toggle)

# Collect ImGui sources
file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/*.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Collect ImGui headers for IDEs that support source groups
file(GLOB IMGUI_HEADERS
    ${imgui_SOURCE_DIR}/*.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
)

# Collect imgui_toggle sources
file(GLOB TOGGLE_SOURCES
    ${imgui_toggle_SOURCE_DIR}/*.cpp
)

# Collect imgui_toggle headers
file(GLOB TOGGLE_HEADERS
    ${imgui_toggle_SOURCE_DIR}/*.h
)

# Create source groups for IDE organization (optional)
source_group(TREE ${imgui_SOURCE_DIR} PREFIX "ImGui" FILES ${IMGUI_SOURCES} ${IMGUI_HEADERS})
source_group(TREE ${imgui_toggle_SOURCE_DIR} PREFIX "Toggle" FILES ${TOGGLE_SOURCES} ${TOGGLE_HEADERS})

# Create an executable with WIN32 option to prevent console window from popping up
add_executable(${PROJECT_NAME} WIN32 main.cpp ${IMGUI_SOURCES} ${TOGGLE_SOURCES})

# Explicitly set the entry point to `main`
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL glfw)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${GLFW_INCLUDE_DIRS}
    ${imgui_toggle_SOURCE_DIR}
)
