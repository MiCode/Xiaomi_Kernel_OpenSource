       .text
       .global __switch_to_smp
       .global __switch_to_amp
.equ ACTLR_SMP_BIT ,0x00000040

__switch_to_smp:
    MRC p15,0,r0,c1,c0,1
    ORR r0,r0,#ACTLR_SMP_BIT
    MCR p15,0,r0,c1,c0,1
    BX lr

__switch_to_amp:
    MRC p15,0,r0,c1,c0,1
    BIC r0,r0,#ACTLR_SMP_BIT
    MCR p15,0,r0,c1,c0,1
    BX lr
