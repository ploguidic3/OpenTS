# The tile geometry has no engine dependencies, so the harness compiles it alone.

opents_add_test(IsoTileTable
    NAME isotiletable
    SOURCES isotilegeometry.cpp
    ENGINE
        isotiletable.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS
)
