# The HUD scale arithmetic is compiled straight into the harness. It has no engine
# dependencies, so nothing else is linked. It lives outside code/ so that the recursive glob
# building OpenTS cannot pick this target's entry point up.

opents_add_test(UIScale
    NAME uiscale
    SOURCES uiscalecontract.cpp
    ENGINE
        uiscale.cpp
    DEFINITIONS WIN32 _WINDOWS _MBCS NOMINMAX
    UTF8
)
