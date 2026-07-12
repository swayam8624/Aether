add_executable(AetherTests
    TestMain.cpp
)
target_link_libraries(AetherTests PRIVATE Aether::Core Aether::RenderGraph)
aether_configure_target(AetherTests)
add_test(NAME AetherTests COMMAND AetherTests)
