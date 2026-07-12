add_executable(aether-reconstruct main.cpp)
target_link_libraries(aether-reconstruct PRIVATE Aether::Core Aether::Package)
aether_configure_target(aether-reconstruct)
