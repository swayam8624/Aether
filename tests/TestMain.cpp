#include <aether/core/Error.hpp>
#include <aether/core/Profiler.hpp>
#include <aether/core/ResourceLocator.hpp>
#include <aether/rendergraph/RenderGraph.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

void testProfiler() {
    aether::Profiler::instance().record("test", 1.25);
    const auto events = aether::Profiler::instance().snapshotAndReset();
    expect(events.size() == 1, "Profiler returns recorded events");
    expect(aether::Profiler::instance().snapshotAndReset().empty(),
           "Profiler snapshot resets data");
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

} // namespace

int main() {
    testErrors();
    testResourceLocator();
    testProfiler();
    testRenderGraph();
    if (failures == 0) {
        std::cout << "All AETHER foundation tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
