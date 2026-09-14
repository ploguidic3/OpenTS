# The expansion and the frame decode have no engine dependencies, so the harness compiles
# them alone.

opents_add_test(ScaleBlit
    NAME scaleblit
    SOURCES shapeexpansion.cpp
    ENGINE
        scaleblit.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS
)
