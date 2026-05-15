cmake_minimum_required(VERSION 3.24)
project(MetalEngine)

set(CMAKE_CXX_STANDARD 20)

# 1. Find the macOS System Frameworks (Foundation, Metal, etc.)
# This is the equivalent of 'find_package(Vulkan)'
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZ_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
find_library(METALK_FRAMEWORK MetalKit REQUIRED)

# 2. Define your sources
add_executable(${PROJECT_NAME}
        MetalPractice/main.cpp
        MetalPractice/control/app_delegate.cpp
        MetalPractice/control/app_delegate.h
        MetalPractice/config.h
        MetalPractice/control/view_delegate.cpp
        MetalPractice/control/view_delegate.h
        MetalPractice/view/renderer.cpp
        MetalPractice/view/renderer.h
        MetalPractice/view/mesh_factory.cpp
        MetalPractice/view/mesh_factory.h
)

# 3. Include the metal-cpp headers
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/metal-cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/external/metal-cpp-extensions
)

# 4. Link everything together
target_link_libraries(${PROJECT_NAME} PRIVATE
        ${METAL_FRAMEWORK}
        ${QUARTZ_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${METALK_FRAMEWORK}
)

# 5. Tell CMake this is a macOS App Bundle (important for windowing)
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourname.metalengine"
    )
endif()