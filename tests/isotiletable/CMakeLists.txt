# The tile geometry has no engine dependencies beyond the scale it is built for.

opents_add_test(IsoTileTable
    NAME isotiletable
    SOURCES isotilegeometry.cpp
    ENGINE
        isotiletable.cpp
        assetscale.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS
)
