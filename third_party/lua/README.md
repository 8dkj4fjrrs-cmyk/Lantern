# Lua 5.4.9

The scripting language of Lantern's gameplay scripts (ADR 0013).

- **Source:** https://github.com/lua/lua, tag `v5.4.9`
  (commit `312b9efaa1061c2c4cad08554dbc1351c3270eef`, the official
  repository mirror of lua.org).
- **License:** MIT, see [LICENSE](LICENSE) (the notice at the end of
  `lua.h`).
- **Unmodified.** Only the files Lantern builds are copied into `src/`: the
  language core, and the base, coroutine, string, table, math and utf8
  libraries.
- **Left out:**
  - the io, os, debug and package libraries, so game scripts cannot reach
    files, programs or native code on a player's computer;
  - the `lua` and `luac` programs;
  - Lua's test suite (`ltests.*`, `testes/`) and manual.

Built by `CMakeLists.txt` as the static library `lantern_lua`, compiled as
C++.

To update: copy the same files from a newer 5.4 release, and update this
file, `licenses/lua.txt` (`tools/release/fetch_licenses.py`) and
`THIRD_PARTY_NOTICES.md`.
