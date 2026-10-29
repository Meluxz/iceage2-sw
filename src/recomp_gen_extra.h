/*
 * Force-included into every generated chunk (CMakeLists.txt).
 *
 * The lifter turns moves to and from x86 debug/control registers into plain C variables it never
 * declares. Ice Age 2 has two, both in data the disassembler took for code (no title code touches
 * these registers): `eax = dr0` in sub_0020FFE8 (D3DX) and `cr1 = edi` in a 112 KB "function" at
 * 0x0021EE6C. Declared here, defined in recomp_manual.c, so those chunks compile; if execution ever
 * reaches them the bug is the jump that got there, not these.
 */
#ifndef ICEAGE2_RECOMP_GEN_EXTRA_H
#define ICEAGE2_RECOMP_GEN_EXTRA_H
#include <stdint.h>
extern uint32_t dr0, dr1, dr2, dr3, dr6, dr7;
extern uint32_t cr0, cr1, cr2, cr3, cr4;
#endif
