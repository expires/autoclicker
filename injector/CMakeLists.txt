cmake_minimum_required(VERSION 3.30.2)
project(INJECTOR)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set configuration to Release only
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Limited configuration types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)

# Add the executable target (exe)
add_executable(${PROJECT_NAME} 
    main.cpp 
)

# Link necessary libraries (e.g., JVM_LIB, if required)
target_link_libraries(${PROJECT_NAME} PRIVATE ${JVM_LIB})
