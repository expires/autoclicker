cmake_minimum_required(VERSION 3.30.2)
project(flexclicker)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set configuration to Release only
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Limited configuration types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)

# Set the path to JDK installation
set(JAVA_HOME "C:/Program Files/Java/jdk-21")
# Add include directories for JNI
include_directories("${JAVA_HOME}/include")
include_directories("${JAVA_HOME}/include/win32")

# Link against the JVM library
set(JVM_LIB "${JAVA_HOME}/lib/jvm.lib")

# Automatically find all .cpp and .h files in the SDK & Clicker directories
file(GLOB SDK_HEADERS "SDK/*.h")
file(GLOB SDK_SOURCES "SDK/*.cpp")
file(GLOB CLICKER_HEADERS "Clicker/*.h")
file(GLOB CLICKER_SOURCES "Clicker/*.cpp")

# Add the shared library target (DLL)
add_library(autoclicker SHARED 
    autoclicker.cpp 
    ${CLICKER_HEADERS}
    ${CLICKER_SOURCES}
    ${SDK_SOURCES} 
    ${SDK_HEADERS}
)

target_link_libraries(autoclicker PRIVATE ${JVM_LIB})

source_group("Header Files\\SDK" FILES ${SDK_HEADERS})
source_group("Source Files\\SDK" FILES ${SDK_SOURCES})
source_group("Header Files\\Clicker" FILES ${CLICKER_HEADERS})
source_group("Source Files\\Clicker" FILES ${CLICKER_SOURCES})