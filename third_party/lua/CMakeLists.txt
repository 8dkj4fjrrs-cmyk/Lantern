# rolo_lua — the Lua 5.4.9 language, for gameplay scripts (ADR 0013).
#
# Only Lua's core is built: the language, and the base, coroutine, string,
# table, math and utf8 libraries. Left out on purpose (their files are not in
# src/): io, os, debug, package/loadlib and the lua/luac programs. A game's
# scripts therefore cannot read or write files, run programs or load native
# code on a player's computer.
#
# Lua is compiled as C++, not C. Then a Lua error unwinds the stack with a C++
# exception instead of longjmp, so destructors of C++ objects in Rolo's
# bindings still run when a script fails.

set(ROLO_LUA_SOURCES
    lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldebug.c ldo.c ldump.c
    lfunc.c lgc.c llex.c lmathlib.c lmem.c lobject.c lopcodes.c lparser.c lstate.c
    lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c)
list(TRANSFORM ROLO_LUA_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")
set_source_files_properties(${ROLO_LUA_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(rolo_lua STATIC ${ROLO_LUA_SOURCES})
add_library(rolo::lua ALIAS rolo_lua)
# SYSTEM: Lua's headers are not ours, so our strict warnings do not apply.
target_include_directories(rolo_lua SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(rolo_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # Lua's own sources: quiet the CRT deprecation warnings (sprintf etc.).
    target_compile_definitions(rolo_lua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
