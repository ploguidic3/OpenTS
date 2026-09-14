# The reduction has no engine dependencies, so the harness compiles it on its own.

opents_add_test(VoxelDownsample
    NAME voxeldownsample
    SOURCES voxelreduction.cpp
    ENGINE
        voxeldownsample.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS
)
