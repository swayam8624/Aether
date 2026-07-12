add_library(AetherScene STATIC
    src/Camera.cpp
    src/CameraController.cpp
    src/Scene.cpp
    src/Transform.cpp
)
add_library(Aether::Scene ALIAS AetherScene)

target_include_directories(AetherScene PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(AetherScene PUBLIC Aether::Core)
target_compile_features(AetherScene PUBLIC cxx_std_23)
aether_configure_target(AetherScene)
