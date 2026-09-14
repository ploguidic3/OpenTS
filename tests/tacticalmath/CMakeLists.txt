# The projection primitives have no engine dependencies, so the harness compiles them alone.

opents_add_test(TacticalMath
    NAME tacticalmath
    SOURCES tacticalprojection.cpp
    ENGINE
        assetscale.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS
)
