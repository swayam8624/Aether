add_library(AetherReconstruction STATIC
    src/SparseModelValidator.cpp
)
add_library(Aether::Reconstruction ALIAS AetherReconstruction)
target_include_directories(AetherReconstruction PUBLIC include)
target_link_libraries(AetherReconstruction PUBLIC Aether::Core)
aether_configure_target(AetherReconstruction)
