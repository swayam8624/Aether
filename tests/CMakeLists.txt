add_executable(AetherTests
    TestMain.cpp
)
target_link_libraries(AetherTests PRIVATE Aether::Core Aether::RenderGraph Aether::Scene Aether::Mesh Aether::Gaussian Aether::Package)
target_compile_definitions(AetherTests PRIVATE
    AETHER_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
aether_configure_target(AetherTests)
add_test(NAME AetherTests COMMAND AetherTests)

if(TARGET AetherCapture)
    add_executable(AetherCaptureTests CaptureTests.cpp)
    target_link_libraries(AetherCaptureTests PRIVATE Aether::Capture
        "-framework CoreFoundation" "-framework CoreGraphics" "-framework ImageIO")
    aether_configure_target(AetherCaptureTests)
    add_test(NAME AetherCaptureTests COMMAND AetherCaptureTests)
endif()

if(TARGET AetherMetal AND TARGET AetherShaders)
    add_executable(AetherMetalTests MetalTests.cpp)
    target_link_libraries(AetherMetalTests PRIVATE Aether::Metal)
    target_compile_options(AetherMetalTests PRIVATE -fblocks)
    target_compile_definitions(AetherMetalTests PRIVATE
        AETHER_TEST_SHADER_LIBRARY="${PROJECT_BINARY_DIR}/apps/AetherStudio/AetherShaders.metallib"
        AETHER_TEST_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/triangle.gltf"
        AETHER_TEST_TEXTURED_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/textured-triangle.gltf"
        AETHER_TEST_INSTANCED_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/instanced-triangle.gltf"
        AETHER_TEST_ANIMATED_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/animated-triangle.gltf"
        AETHER_TEST_SKINNED_GLTF="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/skinned-triangle.gltf"
        AETHER_TEST_PLY="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/gaussian-minimal.ply")
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

if(TARGET aether-benchmark AND TARGET aether-pack)
    set(AETHER_BENCHMARK_TEST_SCENE "${CMAKE_CURRENT_BINARY_DIR}/benchmark-fixture.aether")
    add_test(NAME AetherBenchmarkFixturePack
        COMMAND $<TARGET_FILE:aether-pack>
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/package-scene"
            --output "${AETHER_BENCHMARK_TEST_SCENE}" --json)
    set_tests_properties(AetherBenchmarkFixturePack PROPERTIES
        FIXTURES_SETUP AetherBenchmarkScene)
    add_test(NAME AetherBenchmarkSmoke
        COMMAND $<TARGET_FILE:aether-benchmark>
            "${AETHER_BENCHMARK_TEST_SCENE}"
            --camera-path "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/camera-path.json"
            --width 32 --height 32 --frames 2 --warmup 1 --json)
    set_tests_properties(AetherBenchmarkSmoke PROPERTIES
        FIXTURES_REQUIRED AetherBenchmarkScene)
endif()

if(TARGET aether-reconstruct)
    set(AETHER_RECONSTRUCTION_TEST_JOB "${CMAKE_CURRENT_BINARY_DIR}/reconstruction-job")
    add_test(NAME AetherReconstructionAdapter
        COMMAND $<TARGET_FILE:aether-reconstruct>
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mock-reconstruction"
            --output "${AETHER_RECONSTRUCTION_TEST_JOB}"
            --colmap "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mock-colmap.zsh"
            --brush "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mock-brush.zsh"
            --seed 42 --steps 10 --json)
endif()
