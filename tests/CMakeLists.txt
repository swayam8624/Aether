add_executable(AetherTests
    TestMain.cpp
)
target_link_libraries(AetherTests PRIVATE Aether::Core Aether::RenderGraph Aether::Scene Aether::Mesh Aether::Gaussian Aether::Package)
target_compile_definitions(AetherTests PRIVATE
    AETHER_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
aether_configure_target(AetherTests)
add_test(NAME AetherTests COMMAND AetherTests)

if(TARGET AetherMetal)
    add_executable(AetherMetalTests MetalTests.cpp)
    target_link_libraries(AetherMetalTests PRIVATE Aether::Metal)
    target_compile_options(AetherMetalTests PRIVATE -fblocks)
    target_compile_definitions(AetherMetalTests PRIVATE
        AETHER_TEST_SHADER_LIBRARY="${PROJECT_BINARY_DIR}/apps/AetherStudio/AetherShaders.metallib"
        AETHER_TEST_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/triangle.gltf")
    if(TARGET AetherShaders)
        add_dependencies(AetherMetalTests AetherShaders)
    endif()
    aether_configure_target(AetherMetalTests)
    add_test(NAME AetherMetalTests COMMAND AetherMetalTests)

    add_executable(AetherSwiftTests
        SwiftDocumentTests.swift
        ${PROJECT_SOURCE_DIR}/apps/AetherStudio/Sources/AetherProjectDocument.swift
    )
    target_link_libraries(AetherSwiftTests PRIVATE "-framework SwiftUI" "-framework UniformTypeIdentifiers")
    add_test(NAME AetherSwiftTests COMMAND AetherSwiftTests)
endif()
