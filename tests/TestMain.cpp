#include <aether/core/Diagnostics.hpp>
#include <aether/core/Error.hpp>
#include <aether/core/JobSystem.hpp>
#include <aether/core/Log.hpp>
#include <aether/core/Profiler.hpp>
#include <aether/core/ResourceLocator.hpp>
#include <aether/mesh/GltfLoader.hpp>
#include <aether/rendergraph/RenderGraph.hpp>
#include <aether/scene/Camera.hpp>
#include <aether/scene/CameraController.hpp>
#include <aether/scene/Scene.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testErrors() {
    const aether::Error error{aether::ErrorCode::io, "read failed", "scene.aether"};
    expect(error.describe() == "read failed [scene.aether]", "Error context is preserved");
}

void testResourceLocator() {
    const auto root = std::filesystem::temp_directory_path() / "aether-resource-test";
    std::filesystem::create_directories(root);
    const auto file = root / "shader.metallib";
    {
        std::ofstream stream(file);
        stream << "fixture";
    }

    aether::ResourceLocator locator;
    locator.addRoot(root);
    const auto found = locator.find("shader.metallib");
    expect(found.has_value(), "Resource locator finds a file below an allowed root");
    expect(!locator.find("../secret").has_value(), "Resource locator rejects traversal");
    std::filesystem::remove_all(root);
}

void testDiagnostics() {
    const auto path = std::filesystem::temp_directory_path() / "aether-diagnostics-test.json";
    aether::Log::instance().write(aether::LogLevel::warning, "fixture log");
    const aether::DiagnosticsContext context{"0.test", {{"gpu", "fixture"}}};
    const auto result = aether::Diagnostics::writeReport(path, context);
    expect(result.has_value(), "Diagnostics report is written atomically");
    std::ifstream stream(path);
    const std::string contents((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    expect(contents.find("\"applicationVersion\": \"0.test\"") != std::string::npos,
           "Diagnostics report includes application version");
    expect(contents.find("fixture log") != std::string::npos,
           "Diagnostics report includes retained logs");
    expect(contents.find("Serial") == std::string::npos,
           "Diagnostics report excludes machine serial identifiers");
    std::filesystem::remove(path);
}

void testProfiler() {
    aether::Profiler::instance().record("test", 1.25);
    const auto events = aether::Profiler::instance().snapshotAndReset();
    expect(events.size() == 1, "Profiler returns recorded events");
    expect(aether::Profiler::instance().snapshotAndReset().empty(),
           "Profiler snapshot resets data");
}

void testJobSystem() {
    aether::JobSystem jobs(2);
    auto successful =
        jobs.submit("successful", [](aether::JobContext& context) -> aether::Result<void> {
            context.setProgress(0.5);
            return {};
        });
    successful.wait();
    expect(successful.status() == aether::JobStatus::succeeded, "Job completes successfully");
    expect(successful.progress() == 1.0, "Successful job reports complete progress");

    auto failed = jobs.submit("failed", [](aether::JobContext&) -> aether::Result<void> {
        return aether::fail(aether::ErrorCode::io, "fixture failure");
    });
    failed.wait();
    expect(failed.status() == aether::JobStatus::failed, "Job failure is preserved");
    expect(failed.error().has_value(), "Failed job exposes its structured error");

    auto cancellable =
        jobs.submit("cancellable", [](aether::JobContext& context) -> aether::Result<void> {
            while (!context.isCancellationRequested()) {
                std::this_thread::yield();
            }
            return aether::fail(aether::ErrorCode::cancelled, "cancelled by test");
        });
    cancellable.cancel();
    cancellable.wait();
    expect(cancellable.status() == aether::JobStatus::cancelled, "Cooperative cancellation works");
}

void testRenderGraph() {
    using aether::rendergraph::RenderGraph;
    RenderGraph graph;
    const auto source = graph.createResource("Source", false);
    const auto projected = graph.createResource("Projected");
    const auto output = graph.createResource("Output", false);
    const auto unused = graph.createResource("Unused");
    std::vector<std::string> execution;

    expect(
        graph.addPass("Project", {source}, {projected}, [&] { execution.emplace_back("Project"); })
            .has_value(),
        "Project pass is accepted");
    expect(graph
               .addPass("Composite", {projected}, {output},
                        [&] { execution.emplace_back("Composite"); })
               .has_value(),
           "Composite pass is accepted");
    expect(graph.addPass("Unused", {}, {unused}, [&] { execution.emplace_back("Unused"); })
               .has_value(),
           "Unused pass is accepted");
    expect(graph.markOutput(output).has_value(), "Output resource is exported");

    auto compiled = graph.compile();
    expect(compiled.has_value(), "Render graph compiles");
    if (compiled) {
        expect(compiled->passes().size() == 2, "Dead render pass is culled");
        compiled->execute();
        expect(execution == std::vector<std::string>({"Project", "Composite"}),
               "Dependencies execute in order");
        expect(compiled->lifetimes().size() == 3, "Resource lifetimes include live resources");
        expect(compiled->dot().find("Unused") == std::string::npos,
               "Graph visualization excludes culled work");
    }
}

void testSceneTransforms() {
    aether::scene::Scene scene;
    const auto parent = scene.createEntity("Parent");
    const auto child = scene.createEntity("Child");
    aether::scene::Transform parentTransform;
    parentTransform.translation = {2.0F, 0.0F, 0.0F};
    aether::scene::Transform childTransform;
    childTransform.translation = {0.0F, 3.0F, 0.0F};
    expect(scene.setLocalTransform(parent, parentTransform).has_value(),
           "Parent transform is accepted");
    expect(scene.setLocalTransform(child, childTransform).has_value(),
           "Child transform is accepted");
    expect(scene.setParent(child, parent).has_value(), "Scene entity can be parented");
    auto world = scene.worldMatrix(child);
    expect(world.has_value(), "Child world matrix is evaluated");
    if (world) {
        const simd_float4 origin = simd_mul(*world, simd_float4{0.0F, 0.0F, 0.0F, 1.0F});
        expect(std::abs(origin.x - 2.0F) < 1.0e-5F && std::abs(origin.y - 3.0F) < 1.0e-5F,
               "Parent and child transforms compose in column-vector order");
    }
    expect(!scene.setParent(parent, child).has_value(), "Scene hierarchy cycles are rejected");
    expect(scene.destroyEntity(parent).has_value(), "Entity destruction succeeds");
    expect(!scene.valid(parent), "Stale entity generation is invalidated");
    expect(scene.worldMatrix(child).has_value(), "Destroying a parent safely orphans its child");
}

void testCameraProjection() {
    aether::scene::Camera camera;
    camera.infiniteFarPlane = false;
    camera.nearPlane = 0.1F;
    camera.farPlane = 100.0F;
    auto projection = camera.projectionMatrix(16.0F / 9.0F);
    expect(projection.has_value(), "Valid reverse-Z perspective matrix is created");
    if (projection) {
        const simd_float4 nearClip =
            simd_mul(*projection, simd_float4{0.0F, 0.0F, -camera.nearPlane, 1.0F});
        const simd_float4 farClip =
            simd_mul(*projection, simd_float4{0.0F, 0.0F, -camera.farPlane, 1.0F});
        expect(std::abs((nearClip.z / nearClip.w) - 1.0F) < 1.0e-5F,
               "Reverse-Z maps the near plane to one");
        expect(std::abs(farClip.z / farClip.w) < 1.0e-5F, "Reverse-Z maps the far plane to zero");
    }
    expect(!camera.projectionMatrix(0.0F).has_value(), "Invalid camera aspect is rejected");
}

void testCameraController() {
    aether::scene::CameraController controller;
    const float initialZ = controller.position().z;
    controller.setMoving(aether::scene::CameraMove::forward, true);
    controller.update(0.1);
    expect(controller.position().z < initialZ, "Fly camera moves forward in right-handed view");
    controller.clearMovement();
    const simd_float3 stopped = controller.position();
    controller.update(0.1);
    expect(simd_length(controller.position() - stopped) < 1.0e-6F,
           "Cleared camera input does not continue moving");
    controller.addLookDelta(0.0F, 100000.0F);
    expect(std::abs(controller.pitch()) < 1.56F, "Fly camera pitch is clamped below singularity");
}

void testGltfLoader() {
    const auto path = std::filesystem::path(AETHER_TEST_FIXTURES) / "triangle.gltf";
    auto loaded = aether::mesh::GltfLoader::load(path);
    expect(loaded.has_value(), "Valid glTF fixture loads");
    if (loaded) {
        expect(loaded->primitives.size() == 1, "glTF primitive is retained");
        expect(loaded->vertexCount() == 3 && loaded->indexCount() == 3,
               "glTF vertex and index counts match");
        expect(loaded->materials.size() == 2, "Default and authored materials are present");
        expect(std::abs(loaded->materials[1].metallic - 0.25F) < 1.0e-6F,
               "Metallic-roughness material factors load");
        expect(simd_length(loaded->primitives[0].vertices[0].normal) > 0.99F,
               "Missing normals are generated");
    }
    aether::mesh::GltfLimits tinyLimits;
    tinyLimits.maximumFileBytes = 1;
    expect(!aether::mesh::GltfLoader::load(path, tinyLimits).has_value(),
           "glTF file-size limit is enforced");
}

} // namespace

int main() {
    testErrors();
    testResourceLocator();
    testDiagnostics();
    testProfiler();
    testJobSystem();
    testRenderGraph();
    testSceneTransforms();
    testCameraProjection();
    testCameraController();
    testGltfLoader();
    if (failures == 0) {
        std::cout << "All AETHER foundation tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
