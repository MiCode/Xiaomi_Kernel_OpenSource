       .text 
       .global __inner_flush_dcache_all
       .global __inner_flush_dcache_L1
       .global __inner_flush_dcache_L2
       .global __inner_clean_dcache_all
       .global __inner_clean_dcache_L1
       .global __inner_clean_dcache_L2
       .global __inner_inv_dcache_all
       .global __inner_inv_dcache_L1
       .global __inner_inv_dcache_L2
       .global __enable_dcache
       .global __enable_icache
       .global __enable_cache
       .global __disable_dcache
       .global __disable_icache
       .global __disable_cache
       .global __disable_dcache__inner_flush_dcache_L1
       .global __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2
       .global __disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2
       .global d_i_dis_flush_all
.equ C1_IBIT ,  0x00001000
.equ C1_CBIT ,  0x00000004
.equ PSR_F_BIT, 0x00000040
.equ PSR_I_BIT, 0x00000080

__enable_icache:
    MRC p15,0,r0,c1,c0,0
    ORR r0,r0,#C1_IBIT
    MCR p15,0,r0,c1,c0,0
    BX lr
__disable_icache:
    MRC p15,0,r0,c1,c0,0
    BIC r0,r0,#C1_IBIT
    MCR p15,0,r0,c1,c0,0
    BX lr
__enable_dcache:
    MRC p15,0,r0,c1,c0,0
    ORR r0,r0,#C1_CBIT
    dsb
    MCR p15,0,r0,c1,c0,0
    dsb
    isb
    BX lr
__disable_dcache:
    MRC p15,0,r0,c1,c0,0
    BIC r0,r0,#C1_CBIT
    dsb
    MCR p15,0,r0,c1,c0,0
    dsb
    isb
 /*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
    MCR p15,0,r0,c8,c7,1
    dsb
    isb
    BX lr
__enable_cache:
    MRC p15,0,r0,c1,c0,0
    ORR r0,r0,#C1_IBIT
    ORR r0,r0,#C1_CBIT
    MCR p15,0,r0,c1,c0,0
    BX lr
__disable_cache:
    MRC p15,0,r0,c1,c0,0
    BIC r0,r0,#C1_IBIT
    BIC r0,r0,#C1_CBIT
    MCR p15,0,r0,c1,c0,0
/*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
    MCR p15,0,r0,c8,c7,1
    dsb
    BX lr


__inner_flush_dcache_all:
        push    {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     all_finished                    @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 0
all_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     all_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
all_loop2:
        mov     r9, r4                          @ create working copy of max way size
all_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#ifdef CONFIG_L1C_OPT
#replace DCCISW by DCISW+DCCSW
        cmp     r10, #2
        mrsne   r1, cpsr                      @disable IRQ and save flag to make clean and invalidate atomic
	orrne   r8, r1, #PSR_I_BIT | PSR_F_BIT
	msrne   cpsr_c, r8 
        mcrne   p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcrne   p15, 0, r11, c7, c6, 2         @ invalidate by set/way
	msrne   cpsr_c, r1
        mcreq   p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#else
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     all_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     all_loop2
all_skip:
        add     r10, r10, #2                    @ increment cache number
        cmp     r3, r10
        bgt     all_loop1
all_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        pop     {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
        bx      lr

__inner_flush_dcache_L1:
        push    {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L1_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
L1_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L1_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L1_loop2:
        mov     r9, r4                          @ create working copy of max way size
L1_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#ifdef CONFIG_L1C_OPT
#replace DCCISW by DCISW+DCCSW
        mrs   	r1, cpsr                      @disable IRQ and save flag to make clean and invalidate atomic
	orr     r8, r1, #PSR_I_BIT | PSR_F_BIT
	msr     cpsr_c, r8 
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcr     p15, 0, r11, c7, c6, 2         @ invalidate by set/way
	msr   	cpsr_c, r1
#else
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     L1_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L1_loop2
L1_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L1_loop1
L1_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        pop     {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
        bx      lr

__inner_flush_dcache_L2:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L2_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
L2_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L2_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L2_loop2:
        mov     r9, r4                          @ create working copy of max way size
L2_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     L2_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L2_loop2
L2_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L2_loop1
L2_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

 __inner_clean_dcache_all:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     all_cl_finished                    @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 0
all_cl_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     all_cl_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
all_cl_loop2:
        mov     r9, r4                          @ create working copy of max way size
all_cl_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way

        subs    r9, r9, #1                      @ decrement the way
        bge     all_cl_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     all_cl_loop2
all_cl_skip:
        add     r10, r10, #2                    @ increment cache number
        cmp     r3, r10
        bgt     all_cl_loop1
all_cl_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__inner_clean_dcache_L1:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L1_cl_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
L1_cl_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L1_cl_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L1_cl_loop2:
        mov     r9, r4                          @ create working copy of max way size
L1_cl_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way

        subs    r9, r9, #1                      @ decrement the way
        bge     L1_cl_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L1_cl_loop2
L1_cl_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L1_cl_loop1
L1_cl_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__inner_clean_dcache_L2:
#if 0
        mov     r0, sp
        mcr     p15, 0, r0, c7, c14, 1          @ clean and invalidate D entry
        dsb
        sub     r0, r0, #64
        mcr     p15, 0, r0, c7, c14, 1          @ clean and invalidate D entry 
        dsb
#endif
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
#if 0
        mov     r0, sp
        mcr     p15, 0, r0, c7, c14, 1          @ clean and invalidate D entry
        dsb
        sub     r0, r0, #64
        mcr     p15, 0, r0, c7, c14, 1          @ clean and invalidate D entry
        dsb
#endif
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L2_cl_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
L2_cl_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L2_cl_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L2_cl_loop2:
        mov     r9, r4                          @ create working copy of max way size
L2_cl_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     L2_cl_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L2_cl_loop2
L2_cl_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L2_cl_loop1
L2_cl_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr
 __inner_inv_dcache_all:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     all_inv_finished                    @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 0
all_inv_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     all_inv_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
all_inv_loop2:
        mov     r9, r4                          @ create working copy of max way size
all_inv_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0,  r11, c7, c6, 2         @ invalidate by set/way

        subs    r9, r9, #1                      @ decrement the way
        bge     all_inv_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     all_inv_loop2
all_inv_skip:
        add     r10, r10, #2                    @ increment cache number
        cmp     r3, r10
        bgt     all_inv_loop1
all_inv_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__inner_inv_dcache_L1:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L1_inv_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
L1_inv_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L1_inv_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L1_inv_loop2:
        mov     r9, r4                          @ create working copy of max way size
L1_inv_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0,  r11, c7, c6, 2         @ invalidate by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     L1_inv_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L1_inv_loop2
L1_inv_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L1_inv_loop1
L1_inv_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__inner_inv_dcache_L2:
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        @push    {r4,r5,r7,r9,r10,r11}
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     L2_inv_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
L2_inv_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     L2_inv_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
L2_inv_loop2:
        mov     r9, r4                          @ create working copy of max way size
L2_inv_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0,  r11, c7, c6, 2         @ invalidate by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     L2_inv_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     L2_inv_loop2
L2_inv_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     L2_inv_loop1
L2_inv_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
        @pop     {r4,r5,r7,r9,r10,r11}
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__disable_dcache__inner_flush_dcache_L1:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
        MRC p15,0,r0,c1,c0,0
        BIC r0,r0,#C1_CBIT
        dsb
        MCR p15,0,r0,c1,c0,0
        dsb
        isb
/*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
        MCR p15,0,r0,c8,c7,1
        dsb
        isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DF1_L1_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
DF1_L1_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DF1_L1_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DF1_L1_loop2:
        mov     r9, r4                          @ create working copy of max way size
DF1_L1_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#if 1
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcr     p15, 0, r11, c7, c6, 2         @ invalidate by set/way
#endif

#if 0
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     DF1_L1_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DF1_L1_loop2
DF1_L1_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DF1_L1_loop1
DF1_L1_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * pop  stack                                                                  *
 ******************************************************************************/
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr
 
__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
        MRC p15,0,r0,c1,c0,0
        BIC r0,r0,#C1_CBIT
        dsb
        MCR p15,0,r0,c1,c0,0
        dsb
        isb
/*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
        MCR p15,0,r0,c8,c7,1
        dsb
        isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DF1F2_L1_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
DF1F2_L1_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DF1F2_L1_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DF1F2_L1_loop2:
        mov     r9, r4                          @ create working copy of max way size
DF1F2_L1_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#if 1
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcr     p15, 0, r11, c7, c6, 2         @ invalidate by set/way
#endif

#if 0
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     DF1F2_L1_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DF1F2_L1_loop2
DF1F2_L1_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DF1F2_L1_loop1
DF1F2_L1_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
        clrex
/*******************************************************************************
 * __inner_flush_dcache_L2                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DF1F2_L2_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
DF1F2_L2_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DF1F2_L2_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DF1F2_L2_loop2:
        mov     r9, r4                          @ create working copy of max way size
DF1F2_L2_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     DF1F2_L2_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DF1F2_L2_loop2
DF1F2_L2_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DF1F2_L2_loop1
DF1F2_L2_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * pop stack                                                                   *
 ******************************************************************************/
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr

__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
        MRC p15,0,r0,c1,c0,0
        BIC r0,r0,#C1_CBIT
        dsb
        MCR p15,0,r0,c1,c0,0
        dsb
        isb
/*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
        MCR p15,0,r0,c8,c7,1
        dsb
        isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DF1C2_L1_finished                        @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
DF1C2_L1_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DF1C2_L1_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DF1C2_L1_loop2:
        mov     r9, r4                          @ create working copy of max way size
DF1C2_L1_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#if 1
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcr     p15, 0, r11, c7, c6, 2         @ invalidate by set/way
#endif

#if 0
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     DF1C2_L1_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DF1C2_L1_loop2
DF1C2_L1_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DF1C2_L1_loop1
DF1C2_L1_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
        clrex
/*******************************************************************************
 * __inner_clean_dcache_L2                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DF1C2_L2_cl_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
DF1C2_L2_cl_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DF1C2_L2_cl_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DF1C2_L2_cl_loop2:
        mov     r9, r4                          @ create working copy of max way size
DF1C2_L2_cl_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     DF1C2_L2_cl_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DF1C2_L2_cl_loop2
DF1C2_L2_cl_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DF1C2_L2_cl_loop1
DF1C2_L2_cl_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * pop  stack                                                                  *
 ******************************************************************************/
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr


d_i_dis_flush_all:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
        push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
        MRC p15,0,r0,c1,c0,0
        BIC r0,r0,#C1_CBIT
        BIC r0,r0,#C1_IBIT
        dsb
        MCR p15,0,r0,c1,c0,0
        dsb
        isb
/*
Erratum:794322,An instruction fetch can be allocated into the L2 cache after the cache is disabled Status
This erratum can be avoided by inserting both of the following after the SCTLR.C bit is cleared to 0, and before the caches are cleaned or invalidated:
1) A TLBIMVA operation to any address.
2) A DSB instruction.
*/
        MCR p15,0,r0,c8,c7,1
        dsb
        isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DIF1F2_L1_finished               @ if loc is 0, then no need to clean
        mov     r10, #0                         @ start clean at cache level 1
DIF1F2_L1_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DIF1F2_L1_skip                   @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DIF1F2_L1_loop2:
        mov     r9, r4                          @ create working copy of max way size
DIF1F2_L1_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
#if 1
        mcr     p15, 0, r11, c7, c10, 2         @ clean by set/way
        mcr     p15, 0, r11, c7, c6, 2         @ invalidate by set/way
#endif

#if 0
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
#endif
        subs    r9, r9, #1                      @ decrement the way
        bge     DIF1F2_L1_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DIF1F2_L1_loop2
DIF1F2_L1_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DIF1F2_L1_loop1
DIF1F2_L1_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
        clrex
/*******************************************************************************
 * __inner_flush_dcache_L2                                                     *
 ******************************************************************************/
        dmb                                     @ ensure ordering with previous memory accesses
        mrc     p15, 1, r0, c0, c0, 1           @ read clidr
        ands    r3, r0, #0x7000000              @ extract loc from clidr
        mov     r3, r3, lsr #23                 @ left align loc bit field
        beq     DIF1F2_L2_finished                        @ if loc is 0, then no need to clean
        mov     r10, #2                         @ start clean at cache level 2
DIF1F2_L2_loop1:
        add     r2, r10, r10, lsr #1            @ work out 3x current cache level
        mov     r1, r0, lsr r2                  @ extract cache type bits from clidr
        and     r1, r1, #7                      @ mask of the bits for current cache only
        cmp     r1, #2                          @ see what cache we have at this level
        blt     DIF1F2_L2_skip                            @ skip if no cache, or just i-cache
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        isb                                     @ isb to sych the new cssr&csidr
        mrc     p15, 1, r1, c0, c0, 0           @ read the new csidr
        and     r2, r1, #7                      @ extract the length of the cache lines
        add     r2, r2, #4                      @ add 4 (line length offset)
        ldr     r4, =0x3ff
        ands    r4, r4, r1, lsr #3              @ find maximum number on the way size
        clz     r5, r4                          @ find bit position of way size increment
        ldr     r7, =0x7fff
        ands    r7, r7, r1, lsr #13             @ extract max number of the index size
DIF1F2_L2_loop2:
        mov     r9, r4                          @ create working copy of max way size
DIF1F2_L2_loop3:
        orr     r11, r10, r9, lsl r5            @ factor way and cache number into r11
        orr     r11, r11, r7, lsl r2            @ factor index number into r11
        mcr     p15, 0, r11, c7, c14, 2         @ clean & invalidate by set/way
        subs    r9, r9, #1                      @ decrement the way
        bge     DIF1F2_L2_loop3
        subs    r7, r7, #1                      @ decrement the index
        bge     DIF1F2_L2_loop2
DIF1F2_L2_skip:
        @add     r10, r10, #2                    @ increment cache number
        @cmp     r3, r10
        @bgt     DIF1F2_L2_loop1
DIF1F2_L2_finished:
        mov     r10, #0                         @ swith back to cache level 0
        mcr     p15, 2, r10, c0, c0, 0          @ select current cache level in cssr
        dsb
        isb
/*******************************************************************************
 * pop stack                                                                   *
 ******************************************************************************/
        pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
        bx      lr


        .end
