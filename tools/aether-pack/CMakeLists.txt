add_executable(aether-pack main.cpp)
target_link_libraries(aether-pack PRIVATE Aether::Core Aether::Gaussian Aether::Hybrid Aether::Package)
aether_configure_target(aether-pack)
