"""Make nx-hbloader a forwarder: on the first load it takes the NRO path and argv
from its own romfs (nextNroPath, nextArgv) instead of sdmc:/hbmenu.nro, and when
that NRO returns it exits to HOME rather than loading hbmenu.
    python3 fwd_hbl_patch.py /root/fwd/nx-hbloader/source/main.c
"""
import sys

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
if "FORWARDER:" in src:
    print("already patched")
    sys.exit(0)

old = """    if (g_nextNroPath[0] == '\\0')
    {
        memcpy(g_nextNroPath, DEFAULT_NRO, sizeof(DEFAULT_NRO));
        memcpy(g_nextArgv,    DEFAULT_NRO, sizeof(DEFAULT_NRO));
    }
"""
new = """    // FORWARDER: the first load comes from romfs:/nextNroPath (+ nextArgv); when that
    // NRO returns, leave instead of falling back to hbmenu.
    static int s_forwarded;
    if (g_nextNroPath[0] == '\\0')
    {
        if (s_forwarded)
            svcExitProcess();
        s_forwarded = 1;
        if (R_SUCCEEDED(romfsInit())) {
            int f = open("romfs:/nextNroPath", O_RDONLY);
            if (f >= 0) {
                ssize_t n = read(f, g_nextNroPath, sizeof(g_nextNroPath) - 1);
                g_nextNroPath[n > 0 ? n : 0] = '\\0';
                close(f);
            }
            f = open("romfs:/nextArgv", O_RDONLY);
            if (f >= 0) {
                ssize_t n = read(f, g_nextArgv, sizeof(g_nextArgv) - 1);
                g_nextArgv[n > 0 ? n : 0] = '\\0';
                close(f);
            } else {
                memcpy(g_nextArgv, g_nextNroPath, sizeof(g_nextNroPath));
            }
            romfsExit();
        }
    }
    if (g_nextNroPath[0] == '\\0')
    {
        memcpy(g_nextNroPath, DEFAULT_NRO, sizeof(DEFAULT_NRO));
        memcpy(g_nextArgv,    DEFAULT_NRO, sizeof(DEFAULT_NRO));
    }
"""
if src.count(old) != 1:
    sys.exit("hbloader source changed: patch point not found")
src = src.replace(old, new)
open(path, "w", encoding="utf-8").write(src)
print("patched")
