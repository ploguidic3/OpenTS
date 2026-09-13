# The Media Foundation player is compiled straight into the harness, with the debug log
# it writes to. The movie it opens is one it encodes itself with the sink writer, so no
# game data is read. Media Foundation is delay loaded as it is in the game, so that a
# runner without it reports the skip rather than failing to load. It lives outside code/
# so that the recursive glob building OpenTS cannot pick this target's entry point up.

opents_add_test(VideoPlayer
    NAME videoplayer
    SOURCES videoplayercontract.cpp
    ENGINE
        video/mfplayer.cpp
        dbgprint.cpp
    INCLUDES ${OPENTS_GENERATED_DIR}
    DEFINITIONS WIN32 _WINDOWS _MBCS NOMINMAX
    LIBRARIES mfplat mfreadwrite mfuuid ole32 delayimp
    UTF8
    STAMP
)

if(MSVC)
    target_link_options(VideoPlayer PRIVATE /DELAYLOAD:mfplat.dll /DELAYLOAD:mfreadwrite.dll)
endif()
