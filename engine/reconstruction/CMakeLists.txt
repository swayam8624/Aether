add_library(AetherReconstruction STATIC
    src/SparseModelValidator.cpp
    src/LiveReconstructionSession.cpp
    src/FeatureTracker.cpp
    src/VoxelFusion.cpp
)
add_library(Aether::Reconstruction ALIAS AetherReconstruction)
target_include_directories(AetherReconstruction PUBLIC include)
target_link_libraries(AetherReconstruction PUBLIC Aether::Core Aether::Capture Aether::Mesh)
aether_configure_target(AetherReconstruction)
