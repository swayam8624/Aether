find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

add_library(AetherMetal STATIC
    src/FrameContext.cpp
    src/MetalSymbols.cpp
    src/Renderer.cpp
    src/ShaderABI.cpp
)
add_library(Aether::Metal ALIAS AetherMetal)

target_include_directories(AetherMetal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/shaders
)
target_include_directories(AetherMetal SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/external/metal-cpp)
target_link_libraries(AetherMetal
    PUBLIC Aether::Core Aether::RenderGraph Aether::Scene Aether::Mesh
    PRIVATE
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
)
target_compile_options(AetherMetal PRIVATE -fblocks)
aether_configure_target(AetherMetal)
