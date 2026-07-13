add_library(AetherCapture STATIC src/CaptureValidator.cpp)
add_library(Aether::Capture ALIAS AetherCapture)

target_include_directories(AetherCapture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(AetherCapture PUBLIC Aether::Core
    PRIVATE "-framework CoreFoundation" "-framework CoreGraphics" "-framework ImageIO")
target_compile_features(AetherCapture PUBLIC cxx_std_23)
aether_configure_target(AetherCapture)
