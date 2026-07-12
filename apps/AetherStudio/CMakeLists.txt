include(CompileMetal)

set(AETHER_STUDIO_SOURCES
    Sources/AetherProjectDocument.swift
    Sources/AetherStudioApp.swift
    Sources/ContentView.swift
    Sources/AetherViewport.swift
    Bridge/AetherViewportBridge.mm
)

add_executable(AetherStudio MACOSX_BUNDLE ${AETHER_STUDIO_SOURCES})
target_link_libraries(AetherStudio PRIVATE Aether::Metal
    "-framework AppKit" "-framework Metal" "-framework MetalKit" "-framework SwiftUI")
target_include_directories(AetherStudio PRIVATE Bridge)

set(bridging_header "${CMAKE_CURRENT_SOURCE_DIR}/Bridge/AetherStudio-Bridging-Header.h")
target_compile_options(AetherStudio PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-import-objc-header;${bridging_header}>"
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>"
)

set_target_properties(AetherStudio PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "AETHER"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.swayamsingal.aether"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.swayamsingal.aether"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${bridging_header}"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "15.0"
)

aether_add_metal_library(AetherShaders "AetherShaders.metallib"
    "${PROJECT_SOURCE_DIR}/shaders/viewport.metal"
    "${PROJECT_SOURCE_DIR}/shaders/pbr.metal"
    "${PROJECT_SOURCE_DIR}/shaders/gaussian.metal")
if(TARGET AetherShaders)
    add_dependencies(AetherStudio AetherShaders)
    target_sources(AetherStudio PRIVATE "${AetherShaders_OUTPUT}")
    set_source_files_properties("${AetherShaders_OUTPUT}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
        GENERATED TRUE)
endif()
