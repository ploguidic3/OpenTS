# The shape loader is compiled straight into the harness, along with the file classes and
# the INI reader it stands on. The archives are stood in for by the harness's own
# MixFileClass::Retrieve and Offset, so mixfile.cpp is not linked. It lives outside code/ so
# that the recursive glob building OpenTS cannot pick this target's entry point up.

opents_add_test(ShapeLoad
    NAME shapeload
    SOURCES shapeloadcontract.cpp
    ENGINE
        shapeload.cpp
        cdfile.cpp
        bfiofile.cpp
        rawfile.cpp
        dbgprint.cpp
        ini.cpp
        utf8.cpp
        b64pipe.cpp
        b64straw.cpp
        base64.cpp
        buff.cpp
        crc.cpp
        cstraw.cpp
        int.cpp
        mpmath.cpp
        pipe.cpp
        pk.cpp
        straw.cpp
        trim.cpp
        xpipe.cpp
        xstraw.cpp
    INCLUDES ${OPENTS_GENERATED_DIR}
    DEFINITIONS WIN32 _WINDOWS _MBCS NOMINMAX
    UTF8
    STAMP
)
