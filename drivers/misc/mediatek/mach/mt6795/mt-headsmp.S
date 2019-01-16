#include <linux/linkage.h>
#include <linux/init.h>

    __CPUINIT

CCI400_SI3_SNOOP_CONTROL:
.long 0x10394000
CCI400_STATUS:
.long 0x1039000C
MISCDBG:
.long 0x1020020C

ENTRY(mt_secondary_startup)
    bl      v7_invalidate_l1

    /* CONFIG_ARM_ERRATA_836870, prevent software livelock */
    mrc     p15, 0, r9, c0, c0, 0           @ MIDR
    movw    r10, #0xfff0
    movt    r10, #0xff0f
    and     r9, r10
    movw    r10, #0xD030
    movt    r10, #0x410F
    teq     r9, r10
    bne     1f

    mrrc    p15, 0, r0, r1, c15             @ Read CPU Auxiliary Control Register
    orr     r0, r0, #0x01000000             @ set CPUACTLR[24] = 1
    mcrr    p15, 0, r0, r1, c15             @ Write CPU Auxiliary Control Register
    dsb
    isb
1:

    mrc     p15, 0, r0, c0, c0, 5           @ MPIDR
    ubfx    r0, r0, #8, #4                  @ Cluster Id
    cmp     r0, #0                          @ Cluster 1
    beq     cluster0

    mrc     p15, 1, r1, c9, c0, 2           @ L2CTLR of CA17
    ldr     r2, =0xFFFC0000                 @ Latency
    and     r1, r1, r2
    ldr     r2, =0x00009251
    orr     r1, r1, r2
    mcr     p15, 1, r1, c9, c0, 2

#ifdef CONFIG_ARM_ERRATA_828420
    mrc     p15, 0, r1, c0, c0, 0
    ubfx    r1, r1, #4, #12
    ldr     r2, =0x00000c0e
    teq     r1, r2
    bne     1f
    mrc     p15, 0, r1, c15, c0, 1
    orr     r1, r1, #1 << 30
    mcr     p15, 0, r1, c15, c0, 1
1:
#endif

    ldr     r2, MISCDBG
    ldr     r1, [r2]
    bic     r1, r1, #0x11
    str     r1, [r2]

    ldr     r2, CCI400_SI3_SNOOP_CONTROL
    ldr     r1, [r2]
    orr     r1, r1, #0x3
    str     r1, [r2]
    ldr     r2, CCI400_STATUS
    b       1f
0:
    dsb
1:
    ldr     r1, [r2]
    tst     r1, #1
    bne     0b
    
cluster0:
    b secondary_startup
END(mt_secondary_startup)

