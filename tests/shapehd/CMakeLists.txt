# The shape enlarger is compiled straight into the harness. Only the debug log it writes to
# comes with it. It lives outside code/ so that the recursive glob building OpenTS cannot
# pick this target's entry point up.

opents_add_test(ShapeHD
    NAME shapehd
    SOURCES shapehdcontract.cpp
    ENGINE
        shapehd.cpp
        dbgprint.cpp
    INCLUDES ${OPENTS_GENERATED_DIR}
    DEFINITIONS WIN32 _WINDOWS _MBCS NOMINMAX
    STAMP
)
