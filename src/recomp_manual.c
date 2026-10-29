/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t */
#if defined(__SWITCH__)
/* No execinfo on Horizon: the host address alone (symbolise it against the ELF). */
static void backtrace_symbols_fd(void *const *buf, int n, int fd)
{
    int i;
    (void)fd;
    for (i = 0; i < n; i++)
        fprintf(stderr, "%p\n", buf[i]);
}
#elif !defined(_WIN32)
#include <execinfo.h> /* backtrace_symbols_fd: who made a null indirect call */
#endif

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern uint32_t g_eax;
extern ptrdiff_t g_xbox_mem_offset;

/* Debug/control registers the lifter emits as variables in data taken for code
 * (see recomp_gen_extra.h). Never reached by the title. */
uint32_t dr0, dr1, dr2, dr3, dr6, dr7;
uint32_t cr0, cr1, cr2, cr3, cr4;

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
#ifndef _WIN32
    /* Which lifted function made the call: the first time each host caller is
     * seen, name it (sub_XXXXXXXX+off; wsl_sym2line.sh maps that to the
     * generated line, whose comment holds the guest call site). The target
     * alone says only "null". */
    {
        static void *callers[32];
        static int ncallers;
        void *ret = __builtin_return_address(0);
        int k;
        for (k = 0; k < ncallers && callers[k] != ret; k++)
            ;
        if (k == ncallers && ncallers < 32) {
            callers[ncallers++] = ret;
            fprintf(stderr, "[ICALL] target 0x%08X is not code, called from: ", va);
            fflush(stderr);
            backtrace_symbols_fd(&ret, 1, 2);
        }
    }
#endif
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Stack-balance probes for functions entered by tail jumps ─────────────
 *
 * RECOMP_ABI_CHECK sees every call but not a `jmp` into another function, and after frame
 * ~90 a chain of callers came back with esp 4 short (logs/run14, run15): 0x180C10 tail-jumps
 * to 0x180A70, and the CRT math wrappers (0x1E58FE: mov edx, table; jmp _ctrandisp) reach
 * 0x1E8D94 the same way. Each wrapper runs the lifted body and reports, once, a return that
 * did not pop exactly the return address. */
#ifdef _WIN32
extern __declspec(thread) uint32_t g_esp;   /* thread-local in the runtime (RECOMP_TLS) */
#else
extern __thread uint32_t g_esp;
#endif
/* `extern` and this exact shape: tools/recomp/manual_scan.py only treats a function as a
 * wrapper (body emitted as sub_X_gen) when it sees `extern void sub_X_gen(void);`. */
extern void sub_00180A70_gen(void);
extern void sub_001E8D94_gen(void);

static void probe_report(const char *name, uint32_t before, uint32_t after)
{
    static int reported;
    if (after == before + 4 || reported++ >= 4)
        return;
    fprintf(stderr, "[PROBE] %s returned with esp 0x%08X -> 0x%08X (want 0x%08X)\n",
            name, before, after, before + 4);
    fflush(stderr);
}

void sub_00180A70(void)
{
    uint32_t e = g_esp;
    sub_00180A70_gen();
    probe_report("sub_00180A70", e, g_esp);
}

void sub_001E8D94(void)
{
    uint32_t e = g_esp;
    sub_001E8D94_gen();
    probe_report("sub_001E8D94 (_ctrandisp)", e, g_esp);
}

/* ── D3D interrupt handlers at DISPATCH_LEVEL ────────────────
 *
 * Ported from burnout3-sw src/recomp_manual.c (MIT, sp00nz / antoxa2584x), same runtime,
 * same XDK D3D design; Ice Age 2's handlers are thiscall with ecx = the miniport block
 * (0x0024CBC8, +0 = 0xFD000000):
 *   0x002434F0  vblank handler (bumps the vblank count +0x1C0, retires flips via 0x243410)
 *   0x00243A60  PGRAPH handler (software-method traps: NOP flip/fence -> 0x2437F0)
 * Without them the PGRAPH trap was never reported taken: D3D's write-1-to-clear ack of
 * PGRAPH_INTR is a no-op on RAM, so every pass of the DPC loop took the same flip trap
 * again -- 14105 flips queued after 2 frames, targets far ahead of the vblank count, and
 * D3DDevice_Reset waiting for them (logs/run37_peek_6). Running both at DISPATCH also
 * keeps the vblank count from moving between a flip's target and its check. */
extern uint8_t xbox_KfRaiseIrql(uint8_t irql);
extern void xbox_KfLowerIrql(uint8_t irql);
extern uint8_t xbox_CurrentIrql(void);
extern int xbox_gil_suspend(void);
extern void xbox_gil_resume(int depth);
extern void xbox_Nv2aVblankTaken(void);
extern void nv2a_pb_flip_retired(unsigned n);
extern void nv2a_pb_trap_lock(void);
extern void nv2a_pb_trap_unlock(void);
extern void nv2a_pb_trap_taken(void);
extern void sub_002434F0_gen(void);
extern void sub_00243A60_gen(void);
#ifdef _WIN32
extern __declspec(thread) uint32_t g_ecx;
#else
extern __thread uint32_t g_ecx;
#endif

static uint8_t d3d_isr_enter(void)
{
    uint8_t old = xbox_CurrentIrql();

    if (old < 2) {
        int d = xbox_gil_suspend();
        old = xbox_KfRaiseIrql(2);
        xbox_gil_resume(d);
    }
    return old;
}

static void d3d_isr_leave(uint8_t old)
{
    if (old < 2)
        xbox_KfLowerIrql(old);
}

static uint32_t guest32(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + g_xbox_mem_offset);
}

void sub_002434F0(void)
{
    uint32_t blk = g_ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t head = guest32(blk + 0x1BC);

    xbox_Nv2aVblankTaken();
    sub_002434F0_gen();
    nv2a_pb_flip_retired(guest32(blk + 0x1BC) - head);
    d3d_isr_leave(old);
}

void sub_00243A60(void)
{
    uint32_t blk = g_ecx;
    uint8_t old = d3d_isr_enter();
    uint32_t head = guest32(blk + 0x1BC);
    uint32_t nsource;

    nv2a_pb_trap_lock();
    nsource = guest32(0xFD400108u);
    sub_00243A60_gen();
    nv2a_pb_flip_retired(guest32(blk + 0x1BC) - head);
    if (nsource)
        nv2a_pb_trap_taken();
    nv2a_pb_trap_unlock();
    d3d_isr_leave(old);
}

/* D3D's flip retire (0x00243410, thiscall, ecx = miniport 0x0024CBC8): for the current flip
 * slot (+0x1BC & 1; slots at +0x174 + 12*i: pending, target vblank, surface) it retires the
 * flip only when the vblank count (+0x1C0) EQUALS the target. On hardware the vblank handler
 * runs every vblank, so equality is always met. Here a vblank can be declined while D3D's
 * ISR has the interrupt masked, the count passes a target, and that flip is never retired:
 * the next D3DDevice_Reset then waits for it forever (logs/run34_peek_1, run35_peek_1:
 * pending target 0x121 with the count at 0x5B2). A pending target already in the past is
 * moved to the current count first -- a late vblank, which is what hardware would show. */
extern void sub_00243410_gen(void);

void sub_00243410(void)
{
    uint8_t *mem = (uint8_t *)g_xbox_mem_offset;
    uint32_t mp = g_ecx;   /* callers spill their registers before a lifted call */

    if (mp >= 0x10000u && mp < 0x04000000u) {
        uint32_t count = *(volatile uint32_t *)(mem + mp + 0x1C0);
        int i;
        for (i = 0; i < 2; i++) {
            volatile uint32_t *slot = (volatile uint32_t *)(mem + mp + 0x174 + 12 * i);
            if (slot[0] && (int32_t)(slot[1] - count) < 0)
                slot[1] = count;
        }
    }
    sub_00243410_gen();
}

/* D3D's "wait until the GPU has caught up" (0x0023FEF0, stdcall (device, wait)).
 * With wait != 0 it spins until bits 6:2 of PGRAPH 0x400B10 equal the low five bits of
 * the semaphore word *(device+0x30) shifted left by 2 -- equal on hardware once the GPU
 * is idle. The executor runs every pushbuffer to the end before D3D can get here, so
 * the GPU *is* idle; only the latch lags. Sync it on entry, then run the original
 * (logs/run22: spun forever right after EUROCOM.xmv opened). A runtime tick that
 * copied it continuously trampled D3D's own use of the register during flips. */
extern void sub_0023FEF0_gen(void);

void sub_0023FEF0(void)
{
    uint8_t *mem = (uint8_t *)g_xbox_mem_offset;
    uint32_t dev = *(uint32_t *)(mem + g_esp + 4);
    uint32_t wait = *(uint32_t *)(mem + g_esp + 8);

    if (wait && dev >= 0x10000u && dev < 0x04000000u) {
        uint32_t sem_va = *(uint32_t *)(mem + dev + 0x30);
        uint32_t regs = *(uint32_t *)(mem + dev + 0x934);
        if (sem_va >= 0x80000000u && sem_va < 0x84000000u && regs == 0xFD000000u) {
            volatile uint32_t *latch = (volatile uint32_t *)(mem + 0xFD400B10u);
            uint32_t sem = *(volatile uint32_t *)(mem + sem_va);
            *latch = (*latch & ~0x7Cu) | ((sem & 0x1Fu) << 2);
        }
    }
    sub_0023FEF0_gen();
}

/* RECOMP_ABI_CHECK, our template: every lifted call reports how far esp moved (4 + the
 * callee's `ret N`). The first delta seen per callee is kept; a later call with another
 * delta came back through a mis-lifted path -- the register check alone misses a stdcall
 * callee that pops 4 bytes too few (logs/run16_probe: a whole chain 4 short, no culprit
 * flagged). Reports the first few mismatches with the host caller. */
void recomp_abi_delta_note(uint32_t va, uint32_t delta)
{
    enum { BITS = 16, SIZE = 1 << BITS };
    static uint32_t keys[SIZE], deltas[SIZE];
    static int reports;
    uint32_t h = (va * 2654435761u) >> (32 - BITS);

    static uint8_t reported[SIZE];

    /* __SEH_prolog / __SEH_epilog move esp by the caller's frame size: varies by design. */
    if (va == 0x001E5D90u || va == 0x001E5DCBu)
        return;
    while (keys[h] && keys[h] != va)
        h = (h + 1) & (SIZE - 1);
    if (!keys[h]) {
        deltas[h] = delta;
        keys[h] = va;
        return;
    }
    if (deltas[h] == delta || reported[h] || reports >= 40)
        return;
    reported[h] = 1;
    reports++;
    fprintf(stderr, "[DELTA] sub_%08X moved esp by %d, first call moved it by %d; caller: ",
            va, (int)delta, (int)deltas[h]);
    fflush(stderr);
#ifndef _WIN32
    {
        void *ret = __builtin_return_address(0);
        backtrace_symbols_fd(&ret, 1, 2);
    }
#else
    fprintf(stderr, "\n");
#endif
}

/* From RECOMP_ICALL_SAFE_AT (our xboxrecomp-b3 template): the call site and ecx too.
 * For a virtual call, ecx is the object and MEM32(ecx) its vtable, so the first
 * report per site says which vtable has the null slot. */
void recomp_icall_not_code_at(uint32_t va, uint32_t site, uint32_t ecx_)
{
    enum { SITES = 32 };
    static uint32_t sites[SITES];
    static int nsites;
    int k;

    for (k = 0; k < nsites && sites[k] != site; k++)
        ;
    if (k == nsites && nsites < SITES) {
        uint32_t vt = 0, slot0 = 0;
        sites[nsites++] = site;
        if (ecx_ >= 0x10000u && ecx_ < 0x84000000u) {
            vt = *(volatile uint32_t *)((uintptr_t)ecx_ + g_xbox_mem_offset);
            if (vt >= 0x10000u && vt < 0x84000000u)
                slot0 = *(volatile uint32_t *)((uintptr_t)vt + g_xbox_mem_offset);
        }
        fprintf(stderr, "[ICALL] null/wild target 0x%08X at site 0x%08X: ecx=0x%08X "
                        "[ecx]=0x%08X [[ecx]]=0x%08X\n", va, site, ecx_, vt, slot0);
        fflush(stderr);
    }
    recomp_icall_not_code_log(va);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}
