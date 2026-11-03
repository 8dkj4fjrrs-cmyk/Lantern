"""Checks that every program in an unpacked Windows package can find its DLLs.

    python tools/release/check_windows_dlls.py dist/Lantern-0.9.0-alpha-Windows-AMD64

A Windows program lists the DLLs it needs. Those that come with every Windows
(KERNEL32.dll, USER32.dll, api-ms-win-crt-...) are always found; any other
must sit beside the program, or Windows refuses to start it ("zlib1.dll was
not found"). A developer's PC hides such mistakes, because Visual Studio and
build folders put many DLLs on the PATH, so the release checks the package
itself. Exits with 1, listing what is missing, if anything is.

Needs the pefile package (pip install pefile).
"""
import sys
from pathlib import Path

import pefile

# DLLs that are part of Windows itself (lower case). The Visual C++ runtime
# (msvcp140.dll, vcruntime140*.dll) is deliberately not here: not every PC
# has it, so Lantern ships it.
WINDOWS_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "advapi32.dll", "ole32.dll", "oleaut32.dll",
    "comdlg32.dll", "winspool.drv", "uuid.dll", "winmm.dll", "version.dll", "ws2_32.dll", "userenv.dll",
    "psapi.dll", "iphlpapi.dll", "opengl32.dll", "glu32.dll", "imm32.dll", "shlwapi.dll", "dwmapi.dll",
    "setupapi.dll", "dbghelp.dll", "bcrypt.dll", "crypt32.dll", "secur32.dll", "ntdll.dll", "comctl32.dll",
    "dxgi.dll", "d3d11.dll", "d3d9.dll", "dinput8.dll", "xinput1_4.dll", "hid.dll", "mf.dll", "mfplat.dll",
    "mfreadwrite.dll", "propsys.dll", "rpcrt4.dll", "wininet.dll", "normaliz.dll", "wldap32.dll",
}


def needed_dlls(program):
    pe = pefile.PE(str(program), fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
        pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"]])
    names = [e.dll.decode() for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]
    # Delay-loaded DLLs are only needed when used; report them separately.
    delayed = [e.dll.decode() for e in getattr(pe, "DIRECTORY_ENTRY_DELAY_IMPORT", [])]
    return names, delayed


def is_windows_dll(name):
    lower = name.lower()
    return lower in WINDOWS_DLLS or lower.startswith(("api-ms-win-", "ext-ms-win-"))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    root = Path(sys.argv[1])
    programs = sorted(root.rglob("*.exe"))
    if not programs:
        sys.exit(f"No programs (.exe) found in {root}")
    missing = []
    for program in programs:
        names, delayed = needed_dlls(program)
        beside = {p.name.lower() for p in program.parent.iterdir()}
        for name in names:
            if not is_windows_dll(name) and name.lower() not in beside:
                missing.append(f"{program.relative_to(root)} needs {name}, which is not beside it")
        print(f"{program.relative_to(root)}: {len(names)} DLLs"
              + (f" (delay-loaded: {', '.join(delayed)})" if delayed else ""))
    if missing:
        print("Missing DLLs:\n  " + "\n  ".join(missing))
        sys.exit(1)
    print("Every program finds its DLLs.")


if __name__ == "__main__":
    main()
