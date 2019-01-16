#include <asm/memory.h>
#include <mach/mt_reg_base.h>

        @ Aliases for mode encodings - do not change
        .equ MODE_USR, 0x10
        .equ MODE_FIQ, 0x11
        .equ MODE_IRQ, 0x12
        .equ MODE_SVC, 0x13
        .equ MODE_MON, 0x16  @ A-profile (Security Extensions) only
        .equ MODE_ABT, 0x17
        .equ MODE_UND, 0x1B
        .equ MODE_SYS, 0x1F
        .equ MODE_HYP, 0x1A
        
        .equ TTBCR_EAE, (1<<31)    @ Are we using LPAE?
        
        .equ PFR0_THUMB_EE_SUPPORT, (1<<12)

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


        @ This function takes three arguments
        @ r0: Destination start address (must be word aligned)
        @ r1: Source start address (must be word aligned)
        @ r2: Number of words to copy
        @ Return value is updated destination pointer (first unwritten word)

        .global copy_words
copy_words:
        .func
        push    {r3}
        cmp     r2, #0
        beq     1f
2:
        ldr     r3, [r1], #4
        str     r3, [r0], #4
        subs    r2, r2, #1
        bne     2b
1:
        pop     {r3}
        bx      lr

        .endfunc


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global save_performance_monitors
        
save_performance_monitors:
        .func        
        push    {r4, r8, r9, r10}
        
        mrc    p15,0,r8,c9,c12,0    @ PMon: Control Register
        bic    r1,r8,#1
        mcr    p15,0,r1,c9,c12,0    @ disable counter updates from here
        isb                         @ 0b0 => PMCR<0>
        mrc    p15,0,r9,c9,c12,5    @ PMon: Event Counter Selection Register
        mrc    p15,0,r10,c9,c12,1   @ PMon: Count Enable Set Reg
        stm    r0!, {r8-r10}
        mrc    p15,0,r8,c9,c12,2    @ PMon: Count Enable Clear Register
        mrc    p15,0,r9,c9,c13,0    @ PMon: Cycle Counter Register
        mrc    p15,0,r10,c9,c12,3   @ PMon: Overflow flag Status Register
        stm    r0!, {r8-r10}
        mrc    p15,0,r8,c9,c14,1    @ PMon: Interrupt Enable Set Registern
        mrc    p15,0,r9,c9,c14,2    @ PMon: Interrupt Enable Clear Register
        stm    r0!, {r8-r9}
        mrc    p15,0,r8,c9,c12,0    @ Read PMon Control Register
        ubfx   r9,r8,#11,#5         @ extract # of event counters, N
        tst    r9, r9
        beq    1f
        
        mov    r8,#0
0:        
        mcr    p15,0,r8,c9,c12,5    @ PMon: select CounterN
        isb
        mrc    p15,0,r3,c9,c13,1    @ PMon: save Event Type Register
        mrc    p15,0,r4,c9,c13,2    @ PMon: save Event Counter Register
        stm    r0!, {r3,r4}
        add    r8,r8,#1             @ increment index
        @ cmps   r8,r9
        cmp    r8,r9
        bne    0b
        
1:
        pop    {r4, r8, r9, r10}
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_performance_monitors
restore_performance_monitors:
        .func
        push    {r4-r6, r8-r10, r12, lr}
                                        @ NOTE: all counters disabled by PMCR<0> == 0 on reset
        
        ldr    r8,[r0]                  @ r8 = PMCR
        add    r1,r0,#20                @ r1 now points to saved PMOVSR
        ldr    r9,[r1]                  @ r9 = PMOVSR
        
        mvn    r2,#0                    @ generate Register of all 1's
        mcr    p15,0,r2,c9,c14,2        @ disable all counter related interrupts
        mcr    p15,0,r2,c9,c12,3        @ clear all overflow flags
        isb
        
        ubfx   r12,r8,#11,#5            @ extract # of event counters, N (0-31)
        tst    r12, r12
        beq    20f
        
        add    r1,r0,#32                @ r1 now points to the 1st saved event counter

        @@ Restore counters
        mov    r6,#0
10:    
        mcr    p15,0,r6,c9,c12,5        @ PMon: select CounterN
        isb
        ldm    r1!, {r3,r4}             @ Read saved data
        mcr    p15,0,r3,c9,c13,1        @ PMon: restore Event Type Register
        mcr    p15,0,r4,c9,c13,2        @ PMon: restore Event Counter Register
        add    r6,r6,#1                 @ increment index
        @ cmps   r6,r12
        cmp    r6,r12
        bne    10b
        
20:    
        tst    r9, #0x80000000          @ check for cycle count overflow flag
        beq    40f
        mcr    p15,0,r2,c9,c13,0        @ set Cycle Counter to all 1's
        isb
        mov    r3, #1
        mcr    p15,0,r3,c9,c12,0        @ set the PMCR global enable bit
        mov    r3, #0x80000000
        mcr    p15,0,r3,c9,c12,1        @ enable the Cycle Counter
        isb
        
30:    
        mrc    p15,0,r4,c9,c12,3        @ check cycle count overflow now set
        movs   r4,r4                    @ test bit<31>
        bpl    30b
        mcr    p15,0,r3,c9,c12,2        @ disable the Cycle Counter
        
40:    
        mov    r1, #0
        mcr    p15,0,r1,c9,c12,0        @ clear the PMCR global enable bit
        isb
        
        @@ Restore left regs but PMCR
        add    r1,r0,#4                 @ r1 now points to the PMSELR
        ldm    r1!,{r3,r4}
        mcr    p15,0,r3,c9,c12,5        @ PMon: Event Counter Selection Reg
        mcr    p15,0,r4,c9,c12,1        @ PMon: Count Enable Set Reg
        ldm    r1!, {r3,r4}
        mcr    p15,0,r4,c9,c13,0        @ PMon: Cycle Counter Register
        ldm    r1!,{r3,r4}
        mcr    p15,0,r3,c9,c14,2        @ PMon: Interrupt Enable Clear Reg
        mcr    p15,0,r4,c9,c14,1        @ PMon: Interrupt Enable Set Reg
        ldr    r3,[r1]
        isb
        ldr    r0,[r0]
        mcr    p15,0,r0,c9,c12,0        @ restore the PM Control Register
        isb
        
        pop    {r4-r6, r8-r10, r12, pc}
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global save_banked_registers
save_banked_registers:
        .func

        PUSH    {r3, lr} 
        
        mrs    r2, CPSR             @ save current mode
        and    r3, r2, #0x1f        @ If we are in HYP mode then use the virt.
        cmp    r3, #MODE_HYP        @ instructions to save the banked registers
        beq    save_in_hyp          @ without changing the mode
        
        cps    #MODE_SYS            @ switch to System mode
        str    sp,[r0], #4          @ save the User SP
        str    lr,[r0], #4          @ save the User LR
        cps    #MODE_ABT            @ switch to Abort mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_UND            @ switch to Undefined mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_IRQ            @ switch to IRQ mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_FIQ            @ switch to FIQ mode
        str    SP,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,r8-r12,lr}   @ save the current SPSR,r8-r12,LR
        msr    CPSR_cxsf, r2        @ switch back to original mode

        POP     {r3, lr}
        STR     SP, [r0], #4                    @ save the current SP
        MRS     r3, SPSR
        STM     r0!, {r2, r3, r4-r12, LR}       @ save the current CPSR,SPSR,r4-r12,LR
        dsb
        
        bx     lr

save_in_hyp :
        @ mrs  r1, SP_usr           @ rewrite
        cps    #MODE_SYS            @ switch to System mode
        str    sp,[r1], #4          @ save the User SP
        stm    r0!, {r1}
        
        @ mrs  r1, SP_und           @ rewrite
        cps    #MODE_UND            @ switch to Undefined mode
        str    sp,[r1], #4          @ save the current SP                
        @ mrs  r2, SPSR_und         @ rewrite      
        cps    #MODE_UND            @ switch to Undefined mode
        mrs    r2,SPSR              @ save the current SPSR        
        @ mrs  r3, LR_und           @ rewrite
        cps    #MODE_UND            @ switch to Undefined mode
        str    lr,[r3], #4          @ save the current LR        
        stm    r0!, {r1-r3}
        
        @ mrs  r1, SP_abt           @ rewrite
        cps    #MODE_ABT            @ switch to Abort mode
        str    sp,[r1], #4          @ save the current SP
        @ mrs  r2, SPSR_abt         @ rewrite
        cps    #MODE_ABT            @ switch to Abort mode
        mrs    r2,SPSR              @ save the current SPSR
        @ mrs  r3, LR_abt           @ rewrite
        str    lr,[r3], #4          @ save the current LR
        stm    r0!, {r1-r3}
        
        @ mrs  r1, SP_svc           @ rewrite
        cps    #MODE_SVC            @ switch to SVC mode
        str    sp,[r1], #4          @ save the current SP
        @ mrs  r2, SPSR_svc         @ rewrite
        cps    #MODE_SVC            @ switch to SVC mode
        mrs    r2,SPSR              @ save the current SPSR
        @ mrs  r3, LR_svc           @ rewrite
        cps    #MODE_SVC            @ switch to SVC mode
        str    lr,[r3], #4          @ save the current LR
        stm    r0!, {r1-r3}
        
        @ mrs  r1, SP_irq           @ rewrite
        cps    #MODE_IRQ            @ switch to IRQ mode
        str    sp,[r1], #4          @ save the current SP
        @ mrs  r2, SPSR_irq         @ rewrite
        cps    #MODE_IRQ            @ switch to IRQ mode
        mrs    r2,SPSR              @ save the current SPSR
        @ mrs  r3, LR_irq           @ rewrite
        cps    #MODE_IRQ            @ switch to IRQ mode
        str    lr,[r3], #4          @ save the current LR
        stm    r0!, {r1-r3}
        
        @ mrs  r1, SP_fiq           @ rewrite
        cps    #MODE_FIQ            @ switch to FIQ mode 
        str    sp,[r1], #4          @ save the current SP 
        @ mrs  r2, SPSR_fiq         @ rewrite
        cps    #MODE_FIQ            @ switch to FIQ mode
        mrs    r2,SPSR              @ save the current SPSR
        @ mrs  r3, LR_fiq           @ rewrite
        cps    #MODE_FIQ            @ switch to FIQ mode
        str    lr,[r3], #4          @ save the current LR
        stm    r0!, {r1-r3}        
        
        @ mrs  r1, r8_fiq           @ rewrite
        @ mrs  r2, r9_fiq           @ rewrite        
        @ mrs  r3, r10_fiq          @ rewrite
        @ stm  r0!, {r1-r3}         @ rewrite
        @ mrs  r1, r11_fiq          @ rewrite        
        @ mrs  r2, r12_fiq          @ rewrite        
        @ stm  r0!, {r1-r2}         @ rewrite
        cps    #MODE_FIQ            @ switch to FIQ mode
        stm    r0!,{r8-r12}         @ save the current r8-r12
        
        bx    lr        
        .endfunc

        .global mt_save_banked_registers
mt_save_banked_registers:
        .func

        mrs    r2, CPSR             @ save current mode

        cps    #MODE_SYS            @ switch to System mode
        str    sp,[r0], #4          @ save the User SP
        str    lr,[r0], #4          @ save the User LR
        cps    #MODE_ABT            @ switch to Abort mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_UND            @ switch to Undefined mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_IRQ            @ switch to IRQ mode
        str    sp,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,lr}          @ save the current SPSR, LR
        cps    #MODE_FIQ            @ switch to FIQ mode
        str    SP,[r0], #4          @ save the current SP
        mrs    r3,SPSR
        stm    r0!,{r3,r8-r12,lr}   @ save the current SPSR,r8-r12,LR
        msr    CPSR_cxsf, r2        @ switch back to original mode

        MRS     r3, SPSR
        STM     r0!, {r2, r3}       @ CPSR, SPSR
        dsb
        
        bx     lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global mt_restore_banked_registers
mt_restore_banked_registers:
        .func
        mrs    r2, CPSR         @ save current mode

        cps    #MODE_SYS        @ switch to System mode
        ldr    sp,[r0],#4       @ restore the User SP
        ldr    lr,[r0],#4       @ restore the User LR
        cps    #MODE_ABT        @ switch to Abort mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_UND        @ switch to Undefined mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_IRQ        @ switch to IRQ mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_FIQ        @ switch to FIQ mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,r8-r12,lr}  @ restore the current r8-r12,LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        msr    CPSR_cxsf, r2    @ switch back to original mode

        LDM     r0!, {r2, r3}
        MSR     SPSR_fsxc, r3                   @ restore the current SPSR
        MSR     CPSR_fsxc, r2                   @ restore the current CPSR
        dsb
0:    
        bx     lr
        .endfunc

        
        .global restore_banked_registers
restore_banked_registers:
        .func
        mrs    r2, CPSR         @ save current mode
        and    r3, r2, #0x1f    @ If we are in HYP mode then use the virt.
        cmp    r3, #MODE_HYP    @ instructions to restore the banked registers
        beq    rest_in_hyp      @ without changing the mode
        
        cps    #MODE_SYS        @ switch to System mode
        ldr    sp,[r0],#4       @ restore the User SP
        ldr    lr,[r0],#4       @ restore the User LR
        cps    #MODE_ABT        @ switch to Abort mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_UND        @ switch to Undefined mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_IRQ        @ switch to IRQ mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,lr}      @ restore the current LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        cps    #MODE_FIQ        @ switch to FIQ mode
        ldr    sp,[r0],#4       @ restore the current SP
        ldm    r0!,{r3,r8-r12,lr}  @ restore the current r8-r12,LR
        msr    SPSR_fsxc,r3     @ restore the current SPSR
        msr    CPSR_cxsf, r2    @ switch back to original mode

        LDR     SP, [r0], #4                    @ restore the current SP
        LDM     r0!, {r2, r3, r4-r12, LR}       @ restore the current r4-r12,LR
        MSR     SPSR_fsxc, r3                   @ restore the current SPSR
        MSR     CPSR_fsxc, r2                   @ restore the current CPSR
        dsb
0:    
        bx     lr

rest_in_hyp:
        ldm    r0!, {r1}
        @ msr  SP_usr, r1       @ rewrite
        cps    #MODE_SYS        @ switch to System mode
        ldr    sp,[r1],#4       @ restore the User SP
        
        ldm    r0!, {r1-r3}
        @ msr  SP_und, r1       @ rewrite
        cps    #MODE_UND        @ switch to Undefined mode
        ldr    sp,[r1],#4       @ restore the User SP
        @ msr  SPSR_und, r2     @ rewrite
        cps    #MODE_UND        @ switch to Undefined mode
        msr    SPSR_fsxc,r2     @ restore the current SPSR
        @ msr  LR_und, r3       @ rewrite        
        cps    #MODE_UND        @ switch to Undefined mode
        ldr    lr,[r3],#4       @ restore the User LR
        
        ldm    r0!, {r1-r3}
        @ msr  SP_abt, r1       @ rewrite
        cps    #MODE_ABT        @ switch to Abort mode
        ldr    sp,[r1],#4       @ restore the User SP
        @ msr  SPSR_abt, r2     @ rewrite
        cps    #MODE_ABT        @ switch to Abort mode
        msr    SPSR_fsxc,r2     @ restore the current SPSR
        @ msr  LR_abt, r3       @ rewrite
        cps    #MODE_ABT        @ switch to Abort mode
        ldr    lr,[r3],#4       @ restore the User LR
        
        ldm    r0!, {r1-r3}
        @ msr  SP_svc, r1       @ rewrite
        cps    #MODE_SVC        @ switch to SVC mode
        ldr    sp,[r1],#4       @ restore the User SP
        @ msr  SPSR_svc, r2     @ rewrite
        cps    #MODE_SVC        @ switch to SVC mode
        msr    SPSR_fsxc,r2     @ restore the current SPSR
        @ msr  LR_svc, r3       @ rewrite
        cps    #MODE_SVC        @ switch to SVC mode
        ldr    lr,[r3],#4       @ restore the User LR
        
        ldm    r0!, {r1-r3}
        @ msr  SP_irq, r1       @ rewrite
        cps    #MODE_IRQ        @ switch to IRQ mode
        ldr    sp,[r1],#4       @ restore the User SP
        @ msr  SPSR_irq, r2     @ rewrite
        cps    #MODE_IRQ        @ switch to IRQ mode
        msr    SPSR_fsxc,r2     @ restore the current SPSR
        @ msr  LR_irq, r3       @ rewrite
        cps    #MODE_IRQ        @ switch to IRQ mode
        ldr    lr,[r3],#4       @ restore the User LR
        
        ldm    r0!, {r1-r3}
        @ msr  SP_fiq, r1       @ rewrite
        cps    #MODE_FIQ        @ switch to FIQ mode
        ldr    sp,[r1],#4       @ restore the User SP
        @ msr  SPSR_fiq, r2     @ rewrite
        cps    #MODE_FIQ        @ switch to FIQ mode
        msr    SPSR_fsxc,r2     @ restore the current SPSR
        @ msr  LR_fiq, r3       @ rewrite
        cps    #MODE_FIQ        @ switch to FIQ mode
        ldr    lr,[r3],#4       @ restore the User LR
        
        @ ldm  r0!, {r1-r3}
        @ msr  r8_fiq, r1       @ rewrite                
        @ msr  r9_fiq, r2       @ rewrite        
        @ msr  r10_fiq, r3      @ rewrite
        @ ldm  r0!, {r1-r2}
        @ msr  r11_fiq, r1      @ rewrite        
        @ msr  r12_fiq, r2      @ rewrite
        cps    #MODE_FIQ        @ switch to FIQ mode
        ldm    r0!,{r8-r12}     @ restore the current r8-r12
        
        bx     lr        
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global save_cp15
save_cp15:
        .func
        @ CSSELR Cache Size Selection Register
        mrc    p15,2,r3,c0,c0,0
        str    r3,[r0], #4
        
        @ IMPLEMENTATION DEFINED - proprietary features:
        @ (CP15 register 15, TCM support, lockdown support, etc.)
        
        @ NOTE: IMP DEF registers might have save and restore order that relate
        @ to other CP15 registers or logical grouping requirements and can
        @ therefore occur at any point in this sequence.
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_cp15
restore_cp15:
        .func
        @ CSSELR ?Cache Size Selection Register
        ldr    r3,[r0], #4
        mcr    p15,2,r3,c0,c0,0
        
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Function called with two arguments:
@    r0 contains address to store control registers
@    r1 is non-zero if we are Secure
    
        .global save_control_registers
save_control_registers:
        .func
        cmp    r1, #0              @ Are we Secure?
        mrc    p15,0,r2,c1,c0,1    @ ACTLR - Auxiliary Control Register
        mrc    p15,0,r3,c1,c0,0    @ SCTLR - System Control Register
        mrc    p15,0,r12,c1,c0,2   @ CPACR - Coprocessor Access Control Register
        stm    r0!, {r2-r3, r12}
        
        mrcne    p15,0,r1,c12,c0,1   @ MVBAR - Monitor Vector Base Address Register
        mrcne    p15,0,r2,c1,c1,0    @ Secure Configuration Register
        mrcne    p15,0,r3,c1,c1,1    @ Secure Debug Enable Register
        mrcne    p15,0,r12,c1,c1,2   @ Non-Secure Access Control Register
        stmne    r0!, {r1-r3,r12}
        
        mrc    p15,0,r1,c13,c0,1    @ CONTEXTIDR
        mrc    p15,0,r2,c13,c0,2    @ TPIDRURW
        mrc    p15,0,r3,c13,c0,3    @ TPIDRURO
        mrc    p15,0,r12,c13,c0,4   @ TPIDRPRW
        stm    r0!, {r1-r3,r12}
        
        @ The next two registers are only present if ThumbEE is implemented
        mrc    p15, 0, r1, c0, c1, 0    @ Read ID_PFR0
        tst    r1, #PFR0_THUMB_EE_SUPPORT
        mrcne    p14,6,r1,c0,c0,0    @ TEECR
        mrcne    p14,6,r2,c1,c0,0    @ TEEHBR
        stmne    r0!, {r1, r2}
        
        mrc    p14,7,r1,c1,c0,0    @ JOSCR
        mrc    p14,7,r2,c2,c0,0    @ JMCR
        stm    r0!, {r1, r2}
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Function called with two arguments:
@    r0 contains address to read control registers
@    r1 is non-zero if we are Secure
    
        .global restore_control_registers
restore_control_registers:
        .func
        cmp    r1, #0              @ Are we Secure?
        ldm    r0!, {r2-r3, r12}
        mcr    p15,0,r2,c1,c0,1    @ ACTLR - Auxiliary Control Register
        mcr    p15,0,r3,c1,c0,0    @ SCTLR - System Control Register
        mcr    p15,0,r12,c1,c0,2   @ CPACR - Coprocessor Access Control Register
        
        ldmne    r0!, {r1-r3,r12}
        mcrne    p15,0,r1,c12,c0,1   @ MVBAR - Monitor Vector Base Address Register
        mcrne    p15,0,r2,c1,c1,0    @ Secure Configuration Register
        mcrne    p15,0,r3,c1,c1,1    @ Secure Debug Enable Register
        mcrne    p15,0,r12,c1,c1,2   @ Non-Secure Access Control Register
        
        ldm    r0!, {r1-r3,r12}
        mcr    p15,0,r1,c13,c0,1    @ CONTEXTIDR
        mcr    p15,0,r2,c13,c0,2    @ TPIDRURW
        mcr    p15,0,r3,c13,c0,3    @ TPIDRURO
        mcr    p15,0,r12,c13,c0,4   @ TPIDRPRW
        
        @ The next two registers are only present if ThumbEE is implemented
        mrc    p15, 0, r1, c0, c1, 0    @ Read ID_PFR0
        tst    r1, #PFR0_THUMB_EE_SUPPORT
        ldmne    r0!, {r1,r2}
        mcrne    p14,6,r1,c0,c0,0    @ TEECR
        mcrne    p14,6,r2,c1,c0,0    @ TEEHBR
        
        ldm    r0!, {r1, r2}
        mcr    p14,7,r1,c1,c0,0    @ JOSCR
        mcr    p14,7,r2,c2,c0,0    @ JMCR
        isb
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global save_mmu
save_mmu:
        .func
        push    {r4, r5, r6, r7}
        @ ASSUMPTION: no useful fault address / fault status information
        
        mrc    p15,0,r4,c12,c0,0   @ VBAR
        mrc    p15,0,r5,c2,c0,2    @ TTBCR
        
        tst    r5, #TTBCR_EAE        @ Are we using LPAE?
        
        @ save 32 or 64 bit TTBRs
        mrceq    p15,0,r6,c2,c0,0    @ 32 bit TTBR0
        mrceq    p15,0,r7,c2,c0,1    @ 32 bit TTBR1
        mrrcne    p15,0,r6,r7,c2     @ 64 bit TTBR0
        stm    r0!, {r4-r7}
        mrrcne    p15,1,r6,r7,c2     @ 64 bit TTBR1
        stmne    r0!, {r6-r7}
        
        mrc    p15,0,r4,c3,c0,0     @ DACR
        mrc    p15,0,r5,c7,c4,0     @ PAR
        mrc    p15,0,r6,c10,c2,0    @ PRRR
        mrc    p15,0,r7,c10,c2,1    @ NMRR
        stm    r0!, {r4-r7}
        
        pop    {r4, r5, r6, r7}
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_mmu
restore_mmu:
        .func
        push    {r4, r5, r6, r7}
        ldm    r0!, {r4-r7}
        mcr    p15,0,r4,c12,c0,0    @ VBAR
        mcr    p15,0,r5,c2,c0,2     @ TTBCR
        
        tst    r5, #TTBCR_EAE        @ Are we using LPAE?
        
        @ restore 32 or 64 bit TTBRs
        mcreq    p15,0,r6,c2,c0,0    @ 32 bit TTBR0
        mcreq    p15,0,r7,c2,c0,1    @ 32 bit TTBR1
        mcrrne    p15,0,r6,r7,c2     @ 64-bit TTBR0
        ldmne    r0!, {r6-r7}
        mcrrne    p15,1,r6,r7,c2     @ 64-bit TTBR1
        
        ldm    r0!, {r4-r7}
        mcr    p15,0,r4,c3,c0,0     @ DACR
        mcr    p15,0,r5,c7,c4,0     @ PAR
        mcr    p15,0,r6,c10,c2,0    @ PRRR
        mcr    p15,0,r7,c10,c2,1    @ NMRR
        
        pop    {r4, r5, r6, r7}
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global save_mpu
save_mpu:
        .func
        mrc    p15, 0, r1, c0, c0, 4     @ Read MPUIR
        and    r1, r1, #0xff00
        lsr    r1, r1, #8                @ Extract number of MPU regions
        
        @ Loop over the number of regions
10:    
        cmp    r1, #0
        beq    20f
        sub    r1, r1, #1
        mcr    p15, 0, r1, c6, c2, 0    @ Write RGNR
        mrc    p15, 0, r2, c6, c1, 0    @ Read MPU Region Base Address Register
        mrc    p15, 0, r3, c6, c1, 2    @ Read Data MPU Region Size and Enable Register
        mrc    p15, 0, r12, c6, c1, 4   @ Read Region access control Register
        stm    r0!, {r2, r3, r12}
        b      10b
        
20:    
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_mpu
restore_mpu:
        .func
        mrc    p15, 0, r1, c0, c0, 4     @ Read MPUIR
        and    r1, r1, #0xff00
        lsr    r1, r1, #8                @ Extract number of MPU regions
        
        @ Loop over the number of regions
10:    
        cmp    r1, #0
        beq    20f
        ldm    r0!, {r2, r3, r12}
        sub    r1, r1, #1
        mcr    p15, 0, r1, c6, c2, 0    @ Write RGNR
        mcr    p15, 0, r2, c6, c1, 0    @ Write MPU Region Base Address Register
        mcr    p15, 0, r3, c6, c1, 2    @ Write Data MPU Region Size and Enable Register
        mcr    p15, 0, r12, c6, c1, 4   @ Write Region access control Register
        b      10b
        
20:    
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#if !defined (MT_CPUIDLE)
        .global save_vfp
save_vfp:
        .func
                                   @ FPU state save/restore.
                                   @ FPSID,MVFR0 and MVFR1 don't get serialized/saved (Read Only).
        mrc    p15,0,r3,c1,c0,2    @ CPACR allows CP10 and CP11 access
        ORR    r2,r3,#0xF00000
        mcr    p15,0,r2,c1,c0,2
        isb
        mrc    p15,0,r2,c1,c0,2
        and    r2,r2,#0xF00000
        cmp    r2,#0xF00000
        beq    0f
        movs   r2, #0
        b      2f
        
        @    Save configuration registers and enable.
0:      
        FMRX   r12,FPEXC          @ vmrs   r12,FPEXC
        str    r12,[r0],#4        @ Save the FPEXC
                                  @ Enable FPU access to save/restore the other registers.
        ldr    r2,=0x40000000
        FMXR   FPEXC,r2           @ vmsr   FPEXC,r2
        FMRX   r2,FPSCR           @ vmrs   r2,FPSCR
        str    r2,[r0],#4         @ Save the FPSCR
                                  @ Store the VFP-D16 registers.
        vstm   r0!, {D0-D15}
                                  @ Check for Advanced SIMD/VFP-D32 support
        FMRX   r2,MVFR0           @ vmrs   r2,MVFR0
        and    r2,r2,#0xF         @ extract the A_SIMD bitfield
        cmp    r2, #0x2
        blt    1f
                                  @ Store the Advanced SIMD/VFP-D32 additional registers.
        @FIX-ME early porting
	@vstm   r0!, {D16-D31}
        
        @ IMPLEMENTATION DEFINED: save any subarchitecture defined state
        @ NOTE: Don't change the order of the FPEXC and CPACR restores
        
        @ Restore the original En bit of FPU.
1:
        FMXR   FPEXC,r12          @ vmsr   FPEXC,r12          
        
        @ Restore the original CPACR value.
2:
        mcr    p15,0,r3,c1,c0,2   
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_vfp
restore_vfp:
        .func
        @ FPU state save/restore. Obviously FPSID,MVFR0 and MVFR1 don't get
        @ serialized (RO).
        @ Modify CPACR to allow CP10 and CP11 access
        mrc    p15,0,r1,c1,c0,2
        ORR    r2,r1,#0x00F00000
        mcr    p15,0,r2,c1,c0,2
                                @ Enable FPU access to save/restore the rest of registers.
        ldr    r2,=0x40000000
        FMXR   FPEXC, r2        @ vmsr   FPEXC, r2
                                @ Recover FPEXC and FPSCR. These will be restored later.
        ldm    r0!,{r3,r12}
                                @ Restore the VFP-D16 registers.
        vldm   r0!, {D0-D15}
                                @ Check for Advanced SIMD/VFP-D32 support
        FMRX   r2, MVFR0        @ vmrs   r2, MVFR0
        and    r2,r2,#0xF       @ extract the A_SIMD bitfield
        cmp    r2, #0x2
        blt    0f
        
        @ Store the Advanced SIMD/VFP-D32 additional registers.
	@FIX-ME early porting
        @vldm    r0!, {D16-D31}
        
        @ IMPLEMENTATION DEFINED: restore any subarchitecture defined state        
0:        
        @ Restore configuration registers and enable.
        @ Restore FPSCR _before_ FPEXC since FPEXC could disable FPU
        @ and make setting FPSCR unpredictable.
        FMXR    FPSCR,r12       @ vmsr    FPSCR,r12
                                @ Restore FPEXC after FPSCR
        FMXR    FPEXC,r3        @ vmsr    FPEXC,r3                                        
                                @ Restore CPACR
        @ will restore in mt_restore_control_registers 
        @ mcr     p15,0,r1,c1,c0,2
        
        bx      lr
        .endfunc
#endif //#if !defined (MT_CPUIDLE)
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ If r1 is 0, we assume that the OS is not using the Virtualization extensions,
@ and that the warm boot code will set up CNTHCTL correctly. If r1 is non-zero
@ then CNTHCTL is saved and restored
@ CNTP_CVAL will be preserved as it is in the always-on domain.

        .global save_generic_timer
save_generic_timer:
        .func
        mrc    p15,0,r2,c14,c2,1        @ read CNTP_CTL
        mrc    p15,0,r3,c14,c2,0        @ read CNTP_TVAL
        mrc    p15,0,r12,c14,c1,0       @ read CNTKCTL
        stm    r0!, {r2, r3, r12}
        cmp    r1, #0
        mrcne  p15,4,r1,c14,c1,0        @ read CNTHCTL
        strne  r1, [r0], #4
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_generic_timer
restore_generic_timer:
        .func
        ldm    r0!, {r2, r3, r12}
        mcr    p15,0,r3,c14,c2,0        @ write CNTP_TVAL
        mcr    p15,0,r12,c14,c1,0       @ write CNTKCTL
        mcr    p15,0,r2,c14,c2,1        @ write CNTP_CTL
        cmp    r1, #0
        ldrne  r1, [r0], #4
        mcrne  p15,4,r1,c14,c1,0        @ write CNTHCTL
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global save_fault_status
save_fault_status:
        .func
        mrc    p15,0,r1,c6,c0,0    @ read DFAR
        mrc    p15,0,r2,c6,c0,2    @ read IFAR
        mrc    p15,0,r3,c5,c0,0    @ read DFSR
        mrc    p15,0,r12,c5,c0,1   @ read IFSR
        stm    r0!, {r1,r2,r3,r12}
        mrc    p15,0,r1,c5,c1,0    @ read ADFSR
        mrc    p15,0,r2,c5,c1,1    @ read AIFSR
        stm    r0!, {r1,r2}
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global restore_fault_status
restore_fault_status:
        .func
        ldm    r0!, {r1,r2,r3,r12}
        mcr    p15,0,r1,c6,c0,0    @ write DFAR
        mcr    p15,0,r2,c6,c0,2    @ write IFAR
        mcr    p15,0,r3,c5,c0,0    @ write DFSR
        mcr    p15,0,r12,c5,c0,1   @ write IFSR
        ldm    r0!, {r1,r2}
        mcr    p15,0,r1,c5,c1,0    @ write ADFSR
        mcr    p15,0,r2,c5,c1,1    @ write AIFSR
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global write_cntp_ctl
write_cntp_ctl:
        .func
        mcr	p15, 0, r0, c14, c2, 1
        dsb
        isb
        bx      lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global read_cpuid
read_cpuid:
        .func
        mrc   p15, 0, r0, c0, c0, 5
        ands  r0, r0, #0xf
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global read_clusterid
read_clusterid:
        .func
        mrc   p15, 0, r0, c0, c0, 5
        lsr   r0, r0, #0x8
        ands  r0, r0, #0xf
        bx    lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global read_nsacr
read_nsacr:
        .func
        mrc     p15, 0, r0, c1, c1, 2
        bx      lr
        .endfunc
        
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_osdlr
read_dbg_osdlr:
        .func
        mrc    p14, 0, r0, c1, c3, 4    @ Read OS Double Lock Register
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_devid
read_dbg_devid:    
        .func
        mrc     p14, 0, r0, c7, c2, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_didr
read_dbg_didr:    
        .func
        mrc     p14, 0, r0, c0, c0, 0
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_drar
read_dbg_drar:
        .func
        mrc    p14, 0, r0, c1, c0, 0    @ Read Debug ROM Address Register
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_dsar
read_dbg_dsar:
        .func
        mrc    p14, 0, r0, c2, c0, 0    @ Read Debug Self Address Offset Register
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_dtrrxext
read_dbg_dtrrxext:
        .func
        mrc     p14, 0, r0, c0, c0, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_dtrtxext
read_dbg_dtrtxext:
        .func
        mrc     p14, 0, r0, c0, c3, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_dscrext
read_dbg_dscrext:
        .func
        mrc     p14, 0, r0, c0, c2, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wfar
read_dbg_wfar:
        .func
        mrc     p14, 0, r0, c0, c6, 0
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_vcr
read_dbg_vcr:
        .func
        mrc     p14, 0, r0, c0, c7, 0
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_claimclr
read_dbg_claimclr:
        .func
        mrc     p14, 0, r0, c7, c9, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr0
read_dbg_bvr0:
        .func
        mrc     p14, 0, r0, c0, c0, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr1
read_dbg_bvr1:
        .func
        mrc     p14, 0, r0, c0, c1, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr2
read_dbg_bvr2:
        .func
        mrc     p14, 0, r0, c0, c2, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr3
read_dbg_bvr3:
        .func
        mrc     p14, 0, r0, c0, c3, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr4
read_dbg_bvr4:
        .func
        mrc     p14, 0, r0, c0, c4, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr5
read_dbg_bvr5:
        .func
        mrc     p14, 0, r0, c0, c5, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr6
read_dbg_bvr6:
        .func
        mrc     p14, 0, r0, c0, c6, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr7
read_dbg_bvr7:
        .func
        mrc     p14, 0, r0, c0, c7, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr8
read_dbg_bvr8:
        .func
        mrc     p14, 0, r0, c0, c8, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr9
read_dbg_bvr9:
        .func
        mrc     p14, 0, r0, c0, c9, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr10
read_dbg_bvr10:
        .func
        mrc     p14, 0, r0, c0, c10, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr11
read_dbg_bvr11:
        .func
        mrc     p14, 0, r0, c0, c11, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr12
read_dbg_bvr12:
        .func
        mrc     p14, 0, r0, c0, c12, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr13
read_dbg_bvr13:
        .func
        mrc     p14, 0, r0, c0, c13, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr14
read_dbg_bvr14:
        .func
        mrc     p14, 0, r0, c0, c14, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bvr15
read_dbg_bvr15:
        .func
        mrc     p14, 0, r0, c0, c15, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr0
read_dbg_bcr0:
        .func
        mrc     p14, 0, r0, c0, c0, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr1
read_dbg_bcr1:
        .func
        mrc     p14, 0, r0, c0, c1, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr2
read_dbg_bcr2:
        .func
        mrc     p14, 0, r0, c0, c2, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr3
read_dbg_bcr3:
        .func
        mrc     p14, 0, r0, c0, c3, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr4
read_dbg_bcr4:
        .func
        mrc     p14, 0, r0, c0, c4, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr5
read_dbg_bcr5:
        .func
        mrc     p14, 0, r0, c0, c5, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr6
read_dbg_bcr6:
        .func
        mrc     p14, 0, r0, c0, c6, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr7
read_dbg_bcr7:
        .func
        mrc     p14, 0, r0, c0, c7, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr8
read_dbg_bcr8:
        .func
        mrc     p14, 0, r0, c0, c8, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr9
read_dbg_bcr9:
        .func
        mrc     p14, 0, r0, c0, c9, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr10
read_dbg_bcr10:
        .func
        mrc     p14, 0, r0, c0, c10, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr11
read_dbg_bcr11:
        .func
        mrc     p14, 0, r0, c0, c11, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr12
read_dbg_bcr12:
        .func
        mrc     p14, 0, r0, c0, c12, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr13
read_dbg_bcr13:
        .func
        mrc     p14, 0, r0, c0, c13, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr14
read_dbg_bcr14:
        .func
        mrc     p14, 0, r0, c0, c14, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bcr15
read_dbg_bcr15:
        .func
        mrc     p14, 0, r0, c0, c15, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr0
read_dbg_wvr0:
        .func
        mrc     p14, 0, r0, c0, c0, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr1
read_dbg_wvr1:
        .func
        mrc     p14, 0, r0, c0, c1, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr2
read_dbg_wvr2:
        .func
        mrc     p14, 0, r0, c0, c2, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr3
read_dbg_wvr3:
        .func
        mrc     p14, 0, r0, c0, c3, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr4
read_dbg_wvr4:
        .func
        mrc     p14, 0, r0, c0, c4, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr5
read_dbg_wvr5:
        .func
        mrc     p14, 0, r0, c0, c5, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr6
read_dbg_wvr6:
        .func
        mrc     p14, 0, r0, c0, c6, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr7
read_dbg_wvr7:
        .func
        mrc     p14, 0, r0, c0, c7, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr8
read_dbg_wvr8:
        .func
        mrc     p14, 0, r0, c0, c8, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr9
read_dbg_wvr9:
        .func
        mrc     p14, 0, r0, c0, c9, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr10
read_dbg_wvr10:
        .func
        mrc     p14, 0, r0, c0, c10, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr11
read_dbg_wvr11:
        .func
        mrc     p14, 0, r0, c0, c11, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr12
read_dbg_wvr12:
        .func
        mrc     p14, 0, r0, c0, c12, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr13
read_dbg_wvr13:
        .func
        mrc     p14, 0, r0, c0, c13, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr14
read_dbg_wvr14:
        .func
        mrc     p14, 0, r0, c0, c14, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wvr15
read_dbg_wvr15:
        .func
        mrc     p14, 0, r0, c0, c15, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr0
read_dbg_wcr0:
        .func
        mrc     p14, 0, r0, c0, c0, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr1
read_dbg_wcr1:
        .func
        mrc     p14, 0, r0, c0, c1, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr2
read_dbg_wcr2:
        .func
        mrc     p14, 0, r0, c0, c2, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr3
read_dbg_wcr3:
        .func
        mrc     p14, 0, r0, c0, c3, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr4
read_dbg_wcr4:
        .func
        mrc     p14, 0, r0, c0, c4, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr5
read_dbg_wcr5:
        .func
        mrc     p14, 0, r0, c0, c5, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr6
read_dbg_wcr6:
        .func
        mrc     p14, 0, r0, c0, c6, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr7
read_dbg_wcr7:
        .func
        mrc     p14, 0, r0, c0, c7, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr8
read_dbg_wcr8:
        .func
        mrc     p14, 0, r0, c0, c8, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr9
read_dbg_wcr9:
        .func
        mrc     p14, 0, r0, c0, c9, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr10
read_dbg_wcr10:
        .func
        mrc     p14, 0, r0, c0, c10, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr11
read_dbg_wcr11:
        .func
        mrc     p14, 0, r0, c0, c11, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr12
read_dbg_wcr12:
        .func
        mrc     p14, 0, r0, c0, c12, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr13
read_dbg_wcr13:
        .func
        mrc     p14, 0, r0, c0, c13, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr14
read_dbg_wcr14:
        .func
        mrc     p14, 0, r0, c0, c14, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_wcr15
read_dbg_wcr15:
        .func
        mrc     p14, 0, r0, c0, c15, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr0
read_dbg_bxvr0:
        .func
        mrc     p14, 0, r0, c1, c0, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr1
read_dbg_bxvr1:
        .func
        mrc     p14, 0, r0, c1, c1, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr2
read_dbg_bxvr2:
        .func
        mrc     p14, 0, r0, c1, c2, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr3
read_dbg_bxvr3:
        .func
        mrc     p14, 0, r0, c1, c3, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr4
read_dbg_bxvr4:
        .func
        mrc     p14, 0, r0, c1, c4, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr5
read_dbg_bxvr5:
        .func
        mrc     p14, 0, r0, c1, c5, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr6
read_dbg_bxvr6:
        .func
        mrc     p14, 0, r0, c1, c6, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr7
read_dbg_bxvr7:
        .func
        mrc     p14, 0, r0, c1, c7, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr8
read_dbg_bxvr8:
        .func
        mrc     p14, 0, r0, c1, c8, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr9
read_dbg_bxvr9:
        .func
        mrc     p14, 0, r0, c1, c9, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr10
read_dbg_bxvr10:
        .func
        mrc     p14, 0, r0, c1, c10, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr11
read_dbg_bxvr11:
        .func
        mrc     p14, 0, r0, c1, c11, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr12
read_dbg_bxvr12:
        .func
        mrc     p14, 0, r0, c1, c12, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr13
read_dbg_bxvr13:
        .func
        mrc     p14, 0, r0, c1, c13, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr14
read_dbg_bxvr14:
        .func
        mrc     p14, 0, r0, c1, c14, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_dbg_bxvr15
read_dbg_bxvr15:
        .func
        mrc     p14, 0, r0, c1, c15, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_osdlr
write_dbg_osdlr:    
        .func
        mcr    p14, 0, r0, c1, c3, 4    @ Write OS Double Lock Register
        isb
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_oslar
write_dbg_oslar:    
        .func
        mcr     p14, 0, r0, c1, c0, 4    @ Write OS Lock address register
        isb
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_dtrrxext
write_dbg_dtrrxext: 
        .func
        mcr     p14, 0, r0, c0, c0, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_dtrtxext
write_dbg_dtrtxext:
        .func
        mcr     p14, 0, r0, c0, c3, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_dscrext
write_dbg_dscrext:
        .func
        mcr     p14, 0, r0, c0, c2, 2
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wfar
write_dbg_wfar:
        .func
        mcr     p14, 0, r0, c0, c6, 0
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_vcr
write_dbg_vcr:
        .func
        mcr     p14, 0, r0, c0, c7, 0
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_claimset
write_dbg_claimset:
        .func
        mcr     p14, 0, r0, c7, c8, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr0
write_dbg_bvr0:
        .func
        mcr     p14, 0, r0, c0, c0, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr1
write_dbg_bvr1:
        .func
        mcr     p14, 0, r0, c0, c1, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr2
write_dbg_bvr2:
        .func
        mcr     p14, 0, r0, c0, c2, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr3
write_dbg_bvr3:
        .func
        mcr     p14, 0, r0, c0, c3, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr4
write_dbg_bvr4:
        .func
        mcr     p14, 0, r0, c0, c4, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr5
write_dbg_bvr5:
        .func
        mcr     p14, 0, r0, c0, c5, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr6
write_dbg_bvr6:
        .func
        mcr     p14, 0, r0, c0, c6, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr7
write_dbg_bvr7:
        .func
        mcr     p14, 0, r0, c0, c7, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr8
write_dbg_bvr8:
        .func
        mcr     p14, 0, r0, c0, c8, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr9
write_dbg_bvr9:
        .func
        mcr     p14, 0, r0, c0, c9, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr10
write_dbg_bvr10:
        .func
        mcr     p14, 0, r0, c0, c10, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr11
write_dbg_bvr11:
        .func
        mcr     p14, 0, r0, c0, c11, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr12
write_dbg_bvr12:
        .func
        mcr     p14, 0, r0, c0, c12, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr13
write_dbg_bvr13:
        .func
        mcr     p14, 0, r0, c0, c13, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr14
write_dbg_bvr14:
        .func
        mcr     p14, 0, r0, c0, c14, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bvr15
write_dbg_bvr15:
        .func
        mcr     p14, 0, r0, c0, c15, 4
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr0
write_dbg_bcr0:
        .func
        mcr     p14, 0, r0, c0, c0, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr1
write_dbg_bcr1:
        .func
        mcr     p14, 0, r0, c0, c1, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr2
write_dbg_bcr2:
        .func
        mcr     p14, 0, r0, c0, c2, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr3
write_dbg_bcr3:
        .func
        mcr     p14, 0, r0, c0, c3, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr4
write_dbg_bcr4:
        .func
        mcr     p14, 0, r0, c0, c4, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr5
write_dbg_bcr5:
        .func
        mcr     p14, 0, r0, c0, c5, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr6
write_dbg_bcr6:
        .func
        mcr     p14, 0, r0, c0, c6, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr7
write_dbg_bcr7:
        .func
        mcr     p14, 0, r0, c0, c7, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr8
write_dbg_bcr8:
        .func
        mcr     p14, 0, r0, c0, c8, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr9
write_dbg_bcr9:
        .func
        mcr     p14, 0, r0, c0, c9, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr10
write_dbg_bcr10:
        .func
        mcr     p14, 0, r0, c0, c10, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr11
write_dbg_bcr11:
        .func
        mcr     p14, 0, r0, c0, c11, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr12
write_dbg_bcr12:
        .func
        mcr     p14, 0, r0, c0, c12, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr13
write_dbg_bcr13:
        .func
        mcr     p14, 0, r0, c0, c13, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr14
write_dbg_bcr14:
        .func
        mcr     p14, 0, r0, c0, c14, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bcr15
write_dbg_bcr15:
        .func
        mcr     p14, 0, r0, c0, c15, 5
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr0
write_dbg_wvr0:
        .func
        mcr     p14, 0, r0, c0, c0, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr1
write_dbg_wvr1:
        .func
        mcr     p14, 0, r0, c0, c1, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr2
write_dbg_wvr2:
        .func
        mcr     p14, 0, r0, c0, c2, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr3
write_dbg_wvr3:
        .func
        mcr     p14, 0, r0, c0, c3, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr4
write_dbg_wvr4:
        .func
        mcr     p14, 0, r0, c0, c4, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr5
write_dbg_wvr5:
        .func
        mcr     p14, 0, r0, c0, c5, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr6
write_dbg_wvr6:
        .func
        mcr     p14, 0, r0, c0, c6, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr7
write_dbg_wvr7:
        .func
        mcr     p14, 0, r0, c0, c7, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr8
write_dbg_wvr8:
        .func
        mcr     p14, 0, r0, c0, c8, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr9
write_dbg_wvr9:
        .func
        mcr     p14, 0, r0, c0, c9, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr10
write_dbg_wvr10:
        .func
        mcr     p14, 0, r0, c0, c10, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr11
write_dbg_wvr11:
        .func
        mcr     p14, 0, r0, c0, c11, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr12
write_dbg_wvr12:
        .func
        mcr     p14, 0, r0, c0, c12, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr13
write_dbg_wvr13:
        .func
        mcr     p14, 0, r0, c0, c13, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr14
write_dbg_wvr14:
        .func
        mcr     p14, 0, r0, c0, c14, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wvr15
write_dbg_wvr15:
        .func
        mcr     p14, 0, r0, c0, c15, 6
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr0
write_dbg_wcr0:
        .func
        mcr     p14, 0, r0, c0, c0, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr1
write_dbg_wcr1:
        .func
        mcr     p14, 0, r0, c0, c1, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr2
write_dbg_wcr2:
        .func
        mcr     p14, 0, r0, c0, c2, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr3
write_dbg_wcr3:
        .func
        mcr     p14, 0, r0, c0, c3, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr4
write_dbg_wcr4:
        .func
        mcr     p14, 0, r0, c0, c4, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr5
write_dbg_wcr5:
        .func
        mcr     p14, 0, r0, c0, c5, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr6
write_dbg_wcr6:
        .func
        mcr     p14, 0, r0, c0, c6, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr7
write_dbg_wcr7:
        .func
        mcr     p14, 0, r0, c0, c7, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr8
write_dbg_wcr8:
        .func
        mcr     p14, 0, r0, c0, c8, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr9
write_dbg_wcr9:
        .func
        mcr     p14, 0, r0, c0, c9, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr10
write_dbg_wcr10:
        .func
        mcr     p14, 0, r0, c0, c10, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr11
write_dbg_wcr11:
        .func
        mcr     p14, 0, r0, c0, c11, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr12
write_dbg_wcr12:
        .func
        mcr     p14, 0, r0, c0, c12, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr13
write_dbg_wcr13:
        .func
        mcr     p14, 0, r0, c0, c13, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr14
write_dbg_wcr14:
        .func
        mcr     p14, 0, r0, c0, c14, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_wcr15
write_dbg_wcr15:
        .func
        mcr     p14, 0, r0, c0, c15, 7
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr0
write_dbg_bxvr0:
        .func
        mcr     p14, 0, r0, c1, c0, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr1
write_dbg_bxvr1:
        .func
        mcr     p14, 0, r0, c1, c1, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr2
write_dbg_bxvr2:
        .func
        mcr     p14, 0, r0, c1, c2, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr3
write_dbg_bxvr3:
        .func
        mcr     p14, 0, r0, c1, c3, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr4
write_dbg_bxvr4:
        .func
        mcr     p14, 0, r0, c1, c4, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr5
write_dbg_bxvr5:
        .func
        mcr     p14, 0, r0, c1, c5, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr6
write_dbg_bxvr6:
        .func
        mcr     p14, 0, r0, c1, c6, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr7
write_dbg_bxvr7:
        .func
        mcr     p14, 0, r0, c1, c7, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr8
write_dbg_bxvr8:
        .func
        mcr     p14, 0, r0, c1, c8, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr9
write_dbg_bxvr9:
        .func
        mcr     p14, 0, r0, c1, c9, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr10
write_dbg_bxvr10:
        .func
        mcr     p14, 0, r0, c1, c10, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr11
write_dbg_bxvr11:
        .func
        mcr     p14, 0, r0, c1, c11, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr12
write_dbg_bxvr12:
        .func
        mcr     p14, 0, r0, c1, c12, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr13
write_dbg_bxvr13:
        .func
        mcr     p14, 0, r0, c1, c13, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr14
write_dbg_bxvr14:
        .func
        mcr     p14, 0, r0, c1, c14, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global write_dbg_bxvr15
write_dbg_bxvr15:
        .func
        mcr     p14, 0, r0, c1, c15, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global read_id_pfr1
read_id_pfr1: 
        .func
        mrc     p15, 0, r0, c0, c1, 1
        bx    lr
        .endfunc
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        .global invalidate_unified_TLB_inner_shareable
invalidate_unified_TLB_inner_shareable:
        .func        
        mov r0, #0
        mcr p15, 0, r0, c8, c3, 0        
        bx     lr
        .endfunc

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ Function called with two arguments:
@    r0 contains address to read control registers
@    r1 is non-zero if we are Secure
    
        .global mt_restore_control_registers
mt_restore_control_registers:
        .func
        cmp    r1, #0              @ Are we Secure?
        ldm    r0!, {r2-r3, r12}
        mcr    p15,0,r2,c1,c0,1    @ ACTLR - Auxiliary Control Register
        @ mcr    p15,0,r3,c1,c0,0  @ SCTLR - System Control Register - move to tail        
        mcr    p15,0,r12,c1,c0,2   @ CPACR - Coprocessor Access Control Register
        
        @-----------------cannot use r3
        
        @ldmne    r0!, {r1-r3,r12}
        @mcrne    p15,0,r1,c12,c0,1   @ MVBAR - Monitor Vector Base Address Register
        @mcrne    p15,0,r2,c1,c1,0    @ Secure Configuration Register
        @mcrne    p15,0,r3,c1,c1,1    @ Secure Debug Enable Register
        @mcrne    p15,0,r12,c1,c1,2   @ Non-Secure Access Control Register
        @
        ldmne    r0!, {r1,r2}
        mcrne    p15,0,r1,c12,c0,1   @ MVBAR - Monitor Vector Base Address Register
        mcrne    p15,0,r2,c1,c1,0    @ Secure Configuration Register
        ldmne    r0!, {r1,r2}
        mcrne    p15,0,r1,c1,c1,1    @ Secure Debug Enable Register
        mcrne    p15,0,r2,c1,c1,2   @ Non-Secure Access Control Register
        
        @ldm    r0!, {r1-r3,r12}
        @mcr    p15,0,r1,c13,c0,1    @ CONTEXTIDR
        @mcr    p15,0,r2,c13,c0,2    @ TPIDRURW
        @mcr    p15,0,r3,c13,c0,3    @ TPIDRURO
        @mcr    p15,0,r12,c13,c0,4   @ TPIDRPRW
        @
        ldm    r0!, {r1,r2}
        mcr    p15,0,r1,c13,c0,1    @ CONTEXTIDR
        mcr    p15,0,r2,c13,c0,2    @ TPIDRURW
        ldm    r0!, {r1,r2}
        mcr    p15,0,r1,c13,c0,3    @ TPIDRURO
        mcr    p15,0,r2,c13,c0,4   @ TPIDRPRW
        
        @ The next two registers are only present if ThumbEE is implemented
        mrc    p15, 0, r1, c0, c1, 0    @ Read ID_PFR0
        tst    r1, #PFR0_THUMB_EE_SUPPORT
        ldmne    r0!, {r1,r2}
        mcrne    p14,6,r1,c0,c0,0    @ TEECR
        mcrne    p14,6,r2,c1,c0,0    @ TEEHBR
        
        ldm    r0!, {r1, r2}
        mcr    p14,7,r1,c1,c0,0    @ JOSCR
        mcr    p14,7,r2,c2,c0,0    @ JMCR
        isb

        mov     r2, r3
        @-----------------can use r3

        @ mt        
        @ adr     r3, new_stack
        @ ldr     r3, [r3, #0]

        mrc   p15, 0, r3, c0, c0, 5
        ands  r3, r3, #0xf

        cmp r3, #0
        beq L_CPU_0       
        cmp r3, #1
        beq L_CPU_1
        cmp r3, #2
        beq L_CPU_2
        cmp r3, #3
        beq L_CPU_3

L_CPU_0:         
        adrl     r3, new_stack_0
        beq L_CPU_FINAL	
L_CPU_1:         
        adrl     r3, new_stack_1
        beq L_CPU_FINAL
L_CPU_2:         
        adrl     r3, new_stack_2
        beq L_CPU_FINAL
L_CPU_3:         
        adrl     r3, new_stack_3
        beq L_CPU_FINAL
        
L_CPU_FINAL:        	
        ldr     r3, [r3, #0]
        	
        mov     sp, r3

        add     lr, lr, #PAGE_OFFSET
        @ sub     lr, lr, #PHYS_OFFSET
        ldr     r1,=PHYS_OFFSET
        sub     lr, lr, r1

        add     r11, r11, #PAGE_OFFSET
        @ sub     r11, r11, #PHYS_OFFSET
        ldr     r1,=PHYS_OFFSET
        sub     r11, r11, r1
        
        mcr     p15,0,r2,c1,c0,0    @ SCTLR - System Control Register 

        mov     pc, lr
        isb
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        
        bx    lr
        .endfunc   
        
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        .global cpu_wake_up
cpu_wake_up:
        .func

        .if 0
1:
        nop
        b       1b
        .endif

        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop

        mrc   p15, 0, r0, c0, c0, 5
        ands  r0, r0, #0xf

        cmp r0, #0
        beq L_CPU_0_WP       
        cmp r0, #1
        beq L_CPU_1_WP
        cmp r0, #2
        beq L_CPU_2_WP
        cmp r0, #3
        beq L_CPU_3_WP

L_CPU_0_WP:         
        adrl     r0, new_stack_0
        beq L_CPU_FINAL_WP	
L_CPU_1_WP:         
        adrl     r0, new_stack_1
        beq L_CPU_FINAL_WP
L_CPU_2_WP:         
        adrl     r0, new_stack_2
        beq L_CPU_FINAL_WP
L_CPU_3_WP:         
        adrl     r0, new_stack_3
        beq L_CPU_FINAL_WP
        
L_CPU_FINAL_WP:        	
        ldr     r0, [r0, #0]
        
        @ sub   r0, r0, #PAGE_OFFSET
        ldr     r1,=PAGE_OFFSET
        sub     r0, r0, r1
        
        @ add   r0, r0, #PHYS_OFFSET
        ldr     r1,=PHYS_OFFSET
        add     r0, r0, r1
        
        mov     sp, r0

        .if 0

        ldr     r4, .L2
        mov     r5, #82
        str     r5, [r4, #0]
        mov     r5, #69
        str     r5, [r4, #0]
        mov     r5, #83
        str     r5, [r4, #0]
        mov     r5, #69
        str     r5, [r4, #0]
        mov     r5, #84
        str     r5, [r4, #0]
        mov     r5, #10
        str     r5, [r4, #0]
        mov     r5, #13
        str     r5, [r4, #0]
        .endif

#FIX-ME for early porting
        @b       cpu_start_restore
        .endfunc

        .if 0
.L2:
                @ uart tx register, need update for diff project/platform
                .word -1056915456 
        .endif
        
#ifdef CONFIG_DEBUG_RODATA
new_stack_0:      .word nstack_0+1020
new_stack_1:      .word nstack_1+1020
new_stack_2:      .word nstack_2+1020
new_stack_3:      .word nstack_3+1020
.section".bss","aw",%nobits
nstack_0:         .space 1024
nstack_1:         .space 1024
nstack_2:         .space 1024
nstack_3:         .space 1024
#else
new_stack_0:      .word nstack_0+1020
nstack_0:         .space 1024
new_stack_1:      .word nstack_1+1020
nstack_1:         .space 1024
new_stack_2:      .word nstack_2+1020
nstack_2:         .space 1024
new_stack_3:      .word nstack_3+1020
nstack_3:         .space 1024
#endif // end of !CONFIG_DEBUG_RODATA

