/**
 * Ice Age 2: The Meltdown - recompiled entry point for POSIX hosts (Linux under WSL).
 *
 * Built against xboxrecomp-b3, the toolkit vendored in burnout3-sw (MIT, sp00nz / antoxa2584x),
 * whose boot this follows (burnout3-sw src/main.c):
 *   crash reporter -> load default.xbe -> xbox_MemoryLayoutInit -> APU (MMIO model) ->
 *   USB OHCI -> NV2A renderer (OpenGL) -> xbox_kernel_init -> xbox_path_init ->
 *   xbox_kernel_bridge_init -> g_esp = XBOX_STACK_TOP -> recomp_dispatch_init -> xbe_entry_point()
 * Windows keeps main.c, which Smart App Control will not let run on this PC.
 *
 * Game data: $ICEAGE2_GAME_DIR, else ./game (the extracted disc; tools/play.sh sets it);
 * on the Switch sdmc:/switch/iceage2/game. The Switch build (devkitA64 + libnx, the Vulkan
 * renderer on NVK) uses switch_nx.c from burnout3-sw for the log, env file and exceptions.
 */

#ifndef __SWITCH__
#  include <execinfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xbox/xboxrecomp.h>

#ifdef __SWITCH__
/* libnx lives in switch_nx.c: <switch.h> and the Win32 vocabulary collide. */
void switch_boot(void);
void switch_shutdown(void);
void switch_error_dialog(const char *msg, const char *details);
#endif

#define ICEAGE2_ENTRY_POINT     0x0010603Cu
/* XDK 5849 D3D (not LTCG): global device pointer, read at 0x002401F1 / 0x00240500.
 *   +0x2C  write sequence (PUT side), bumped as fences are inserted (0x00240130)
 *   +0x30  pointer to the semaphore word the GPU releases (GET side)
 * Same layout as Burnout 3 / NFSU2 (their device pointer is elsewhere). */
#define ICEAGE2_D3D_DEVICE_PTR  0x0024AF98u
#ifdef __SWITCH__
#  define ICEAGE2_DEFAULT_GAME  "sdmc:/switch/iceage2/game"
#  define ICEAGE2_DEFAULT_SAVE  "sdmc:/switch/iceage2/save"
#else
#  define ICEAGE2_DEFAULT_GAME  "game"
#  define ICEAGE2_DEFAULT_SAVE  NULL
#endif
/* The MCPX APU's register window, routed to the emulated APU (RECOMP_AC97_READY=plain). */
#define ICEAGE2_APU_BASE        0xFE800000u
#define ICEAGE2_APU_END         0xFE880000u

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;

typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);
extern MCPXAPUState *g_apu_state;
extern uint64_t mcpx_apu_mmio_read(MCPXAPUState *d, uint64_t addr, unsigned int size);
extern void mcpx_apu_mmio_write(MCPXAPUState *d, uint64_t addr, uint64_t val, unsigned int size);

extern void xbe_entry_point(void);
extern int recomp_dispatch_init(void);   /* generated recomp_dispatch.c; no header declares it */
extern void xbox_OhciInit(void);
extern void nv2a_gl_install(void);
void xbox_guest_pin(int interrupt);      /* win32_compat.c: the Xbox's one CPU */
void xbox_gil_enter(void);               /* kernel_bridge.c: the guest lock */
void xbox_gil_leave(void);
void xbox_gil_mark_main(void);

/* DirectSound's DSP liveness check (0x0025294A): the word its pointer at 0x00259648 names
 * (0xFE85A018, EP DSP memory) must read 0xCCCCCC, which the DSP program writes once it
 * runs; otherwise DirectSound resets the DSPs (0x0025267B) and tries again. The emulated
 * APU's DSPs are a passthrough stub that never writes it, so the XMV player reset them
 * over and over at its second movie (BLUESKY.xmv: 114 resets, logs/run29_peek). */
#define ICEAGE2_DSOUND_DSP_MARKER_PTR 0x00259648u

static uint32_t apu_mmio_rd(void *opaque, uint32_t off, unsigned size)
{
    uint32_t marker = *(volatile uint32_t *)((uintptr_t)ICEAGE2_DSOUND_DSP_MARKER_PTR + g_xbox_mem_offset);
    if (size == 4 && marker >= ICEAGE2_APU_BASE && off == marker - ICEAGE2_APU_BASE)
        return 0xCCCCCCu;
    return (uint32_t)mcpx_apu_mmio_read((MCPXAPUState *)opaque, off, size);
}

static void apu_mmio_wr(void *opaque, uint32_t off, uint32_t val, unsigned size)
{
    mcpx_apu_mmio_write((MCPXAPUState *)opaque, off, val, size);
}

/* AC'97 bus-master (NABM) registers at 0xFEC00100. Each DMA channel (PCM In 0x100, PCM Out 0x110,
 * Mic 0x120) has a control byte at +0x0B whose bit 1 (RR, reset registers) software sets and the
 * controller clears. DirectSound (0x002583BB..0x002583D6) writes RR and reads the byte once,
 * then spins on that stale copy: the runtime's ack thread clears it too late (logs/run07_b3
 * hung there). DSOUND is lifted with MMIO accessors, so the write itself drops RR here and the
 * read already sees the reset done. Everything else is plain memory, as before. */
#define ICEAGE2_AC97_NABM_LO    0xFEC00100u
#define ICEAGE2_AC97_NABM_HI    0xFEC00200u

static uint32_t ac97_nabm_rd(void *opaque, uint32_t off, unsigned size)
{
    volatile uint8_t *p = (volatile uint8_t *)opaque + off;
    return size == 1 ? *p : size == 2 ? *(volatile uint16_t *)p : *(volatile uint32_t *)p;
}

static void ac97_nabm_wr(void *opaque, uint32_t off, uint32_t val, unsigned size)
{
    volatile uint8_t *base = (volatile uint8_t *)opaque;
    unsigned i;

    for (i = 0; i < size; i++) {
        uint32_t o = off + i;
        uint8_t b = (uint8_t)(val >> (8 * i));
        if ((o & 0x0Fu) == 0x0Bu)
            b &= (uint8_t)~0x02u;          /* RR: the reset completes at once */
        base[o] = b;
    }
}

/* For gdb (tools/gdb_stackcheck.py): the calling thread's guest registers. The runtime is built
 * without debug info, and inside lifted code g_esp & co. name local copies. */
uint32_t iceage2_guest_reg(int r)
{
    switch (r) {
    case 0: return g_esp;
    case 1: return g_ebx;
    case 2: return g_esi;
    case 3: return g_edi;
    default: return g_eax;
    }
}

/* ── Crash report ─────────────────────────────────────────── */

/* The guest registers, recent indirect calls and the return addresses on the guest stack.
 * Also called by switch_nx.c's CPU exception handler. */
void print_guest_state(void)
{
    int k;

    fprintf(stderr, "  Xbox regs: eax=0x%08X ecx=0x%08X edx=0x%08X esp=0x%08X\n",
            g_eax, g_ecx, g_edx, g_esp);
    fprintf(stderr, "  Xbox regs: ebx=0x%08X esi=0x%08X edi=0x%08X\n", g_ebx, g_esi, g_edi);
    fprintf(stderr, "  recent ICALL targets:");
    for (k = 0; k < 16; k++)
        fprintf(stderr, " %08X", g_icall_trace[(g_icall_trace_idx + k) & 15]);
    fprintf(stderr, "\n");

    /* Guest return addresses still on the guest stack, innermost first. */
    if (g_xbox_mem_offset && g_esp) {
        const uint32_t *sp = (const uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
        int shown = 0, i;
        fprintf(stderr, "  guest stack (return addresses):\n");
        for (i = 0; i < 256 && shown < 24; i++) {
            if (sp[i] > g_xbox_code_lo && sp[i] < g_xbox_code_hi) {
                fprintf(stderr, "    [esp+%-4d] 0x%08X\n", i * 4, sp[i]);
                shown++;
            }
        }
    }
    fflush(stderr);
}

#ifdef __SWITCH__
/* The CPU exception handler is in switch_nx.c; it calls print_guest_state. */
static void install_crash_reporter(void) {}
#else
static void crash_signal(int sig, siginfo_t *si, void *uc)
{
    uintptr_t fault = (uintptr_t)si->si_addr;
    void *frames[32];
    int n;

    (void)uc;
    fprintf(stderr, "[CRASH] signal %d at host addr %p\n", sig, si->si_addr);
    if (g_xbox_mem_offset)
        fprintf(stderr, "  Xbox VA of fault: 0x%08X\n",
                (uint32_t)(fault - (uintptr_t)g_xbox_mem_offset));
    print_guest_state();

    /* Host frames name the lifted function (sub_XXXXXXXX) that faulted. */
    n = backtrace(frames, 32);
    fprintf(stderr, "  host backtrace:\n");
    fflush(stderr);
    backtrace_symbols_fd(frames, n, 2);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_crash_reporter(void)
{
    static char altstack[64 * 1024];
    struct sigaction sa;
    stack_t ss;

    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}
#endif

/* ── Boot ─────────────────────────────────────────────────── */

static void *load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    long size;
    void *data;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)size);
    if (data && fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        data = NULL;
    }
    fclose(f);
    if (data)
        *out_size = (size_t)size;
    return data;
}

static int game_main(void)
{
    char xbe_path[512];
    const char *game_dir = getenv("ICEAGE2_GAME_DIR");
    void *xbe_data;
    size_t xbe_size = 0;

    xbox_guest_pin(0);     /* the boot thread becomes the title's first thread */

    /* Runtime defaults this title needs; an explicit setting still wins.
     *   RECOMP_VBLANK         the linked D3D waits on its vblank ISR every frame
     *   RECOMP_AC97_READY     DirectSoundCreate needs the codec; "plain" routes the APU
     *                         through the MMIO accessors DSOUND is lifted with
     *   RECOMP_USB            the pad is on the MCPX's OHCI (XPP)
     *   RECOMP_PB_EXEC        the title draws through NV2A pushbuffers
     *   RECOMP_TITLE_KEVENTS  D3D builds the KEVENT at 0x0024CD5C itself (logs/run03) */
    setenv("RECOMP_VBLANK", "1", 0);
    setenv("RECOMP_AC97_READY", "plain", 0);
    setenv("RECOMP_USB", "1", 0);
    setenv("RECOMP_PB_EXEC", "1", 0);
    setenv("RECOMP_TITLE_KEVENTS", "1", 0);
#ifdef __SWITCH__
    /* Every periodic log line flushes to the SD card; keep the log to what matters on a
     * console (burnout3-sw does the same). RECOMP_QUIET=0 in iceage2_env.txt brings it back. */
    setenv("RECOMP_QUIET", "1", 0);
    /* The executor's off-screen culling bounds every batch on the CPU: 22% of
     * the render thread on the console (logs/switch_prof1), for 22% of the
     * draws skipped -- and a skipped draw costs NVK less than the test did
     * (3.4 us a batch on the PC, 1235 tests a frame). RECOMP_CULL=1 brings it back. */
    setenv("RECOMP_CULL", "0", 0);
#else
    /* Keyboard as player 1 (arrows/Enter/Z X A S/numpad, xinput_device.c), merged over a pad. */
    setenv("RECOMP_KEYBOARD", "1", 0);
#endif
    /* PGRAPH does not hold for a retired flip: the hold cost the executor
     * ~10 ms a frame and the first level ran at 20-22 fps instead of 28-29
     * (logs/perf03, perf05). The title paces itself on its vblank waits. */
    setenv("RECOMP_FLIP_STALL", "0", 0);
    /* The music streamer is a time-critical guest thread that refills five
     * 4096-sample rings; queued FIFO behind the main thread for the guest lock
     * it ran late and the VP replayed the previous lap (crackle). Handed the
     * lock by priority: 3-10% stale ring samples in the first level -> 0-2.6%,
     * and the frame rate steadier (logs perf_base / perf_eager). */
    setenv("RECOMP_GIL_EAGER", "1", 0);

    if (!game_dir || !game_dir[0])
        game_dir = ICEAGE2_DEFAULT_GAME;
    snprintf(xbe_path, sizeof(xbe_path), "%s/default.xbe", game_dir);

    xbe_data = load_file(xbe_path, &xbe_size);
    if (!xbe_data) {
        fprintf(stderr, "[FATAL] cannot read %s (set ICEAGE2_GAME_DIR)\n", xbe_path);
#ifdef __SWITCH__
        switch_error_dialog("Cannot read default.xbe.",
                            "Copy the extracted disc to sdmc:/switch/iceage2/game/.");
#endif
        return 1;
    }
    printf("XBE %s: %zu bytes\n", xbe_path, xbe_size);

    /* Before any window or GL context exists: on POSIX the guest window is mapped at
     * computed addresses without a reservation, and SDL/OpenGL (WSLg) set up first took
     * 0x80000000+offset (logs/run01). */
    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        fprintf(stderr, "[FATAL] cannot map the Xbox memory layout\n");
        free(xbe_data);
        return 1;
    }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    printf("Xbox memory mapped at host offset 0x%llX\n", (unsigned long long)g_xbox_mem_offset);

    /* The emulated APU, reached through the MMIO accessors (no fault handling).
     * ICEAGE2_APU=0 runs without it (DirectSound then creates, but nothing plays). */
    if (getenv("RECOMP_AC97_READY")) {
        const char *apu = getenv("ICEAGE2_APU");
        int plain = strcmp(getenv("RECOMP_AC97_READY"), "plain") == 0;

        if (!plain || !apu || strcmp(apu, "0") != 0) {
            g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
            fprintf(stderr, "[BOOT] emulated APU %s\n", g_apu_state ? "up" : "FAILED to initialise");
            if (g_apu_state && plain)
                xbox_MmioRegister(ICEAGE2_APU_BASE, ICEAGE2_APU_END,
                                  apu_mmio_rd, apu_mmio_wr, g_apu_state);
        }
        if (plain)
            xbox_MmioRegister(ICEAGE2_AC97_NABM_LO, ICEAGE2_AC97_NABM_HI, ac97_nabm_rd, ac97_nabm_wr,
                              (void *)((uintptr_t)ICEAGE2_AC97_NABM_LO + g_xbox_mem_offset));
    }

    /* The USB host controllers XPP drives to find the gamepad (RECOMP_USB). */
    xbox_OhciInit();

    /* The GPU renderer; ICEAGE2_GL=0 falls back to the executor's CPU one. */
    {
        const char *gl = getenv("ICEAGE2_GL");
        if (!gl || strcmp(gl, "0") != 0) {
#if defined(ICEAGE2_VULKAN)
            extern void nv2a_vk_install(void);
            nv2a_vk_install();
#else
            nv2a_gl_install();
#endif
        }
    }

    xbox_kernel_init();
    xbox_path_init(game_dir, ICEAGE2_DEFAULT_SAVE);
    xbox_kernel_bridge_init();

    /* D3D's fence waits (0x002401F0 -> 0x0023FEF0) spin until the GPU semaphore at
     * *(device+0x30) catches up with device+0x2C, and until PGRAPH 0x400B10 shows it.
     * Nothing releases it unless the runtime knows where it is (logs/run04 hung after
     * the first clear). The runtime resolves the chain once the device exists, hands
     * the word to the pushbuffer executor (real releases, as burnout3-sw does) and
     * keeps the PGRAPH latch in step (kernel_bridge.c). xbox_Nv2aMirrorFence ("GPU
     * always caught up") broke flip accounting once it worked on the timer thread
     * (logs/run26_fencefix*). */
    {
        extern void xbox_Nv2aSemaphoreChain(uint32_t device_ptr_va, uint32_t sem_ptr_off);
        xbox_Nv2aSemaphoreChain(ICEAGE2_D3D_DEVICE_PTR, 0x30);
    }

    g_esp = XBOX_STACK_TOP;

    if (!recomp_dispatch_init())
        fprintf(stderr, "[BOOT] flat dispatch unavailable; "
                        "indirect calls will use the binary search\n");

    xbox_WatchdogStart();

    printf("Starting guest at 0x%08X (esp=0x%08X)\n", ICEAGE2_ENTRY_POINT, g_esp);
    xbox_gil_mark_main();
    xbox_gil_enter();      /* guest code from here on */
    xbe_entry_point();
    xbox_gil_leave();

    printf("Guest returned. Cleaning up.\n");
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

#ifdef __SWITCH__
#include <pthread.h>
/* Horizon's main thread stack is small for recompiled code: boot the title on a thread with
 * a desktop-sized stack instead (as burnout3-sw does). */
void xbox_nx_spread_thread(void);        /* win32_compat.c: off core 0, all cores allowed */
void xbox_nx_track_thread(void *entry);  /* win32_compat.c: Switch [perf] report */

static void *game_thread(void *arg)
{
    xbox_nx_spread_thread();
    xbox_nx_track_thread(NULL);
    *(int *)arg = game_main();
    return NULL;
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#ifdef __SWITCH__
    switch_boot();
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("=== Ice Age 2: The Meltdown - Static Recompilation (POSIX) ===\n");
    install_crash_reporter();
#ifdef __SWITCH__
    {
        pthread_attr_t attr;
        pthread_t th;
        int rc = 1;

        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 16u * 1024 * 1024);
        if (pthread_create(&th, &attr, game_thread, &rc) == 0)
            pthread_join(th, NULL);
        else
            fprintf(stderr, "[FATAL] cannot start the game thread\n");
        switch_shutdown();
        return rc;
    }
#else
    return game_main();
#endif
}
