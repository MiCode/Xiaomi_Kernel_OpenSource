#     Copyright (C) 2002-2014 Intel Corporation.  All Rights Reserved.
# 
#     This file is part of SEP Development Kit
# 
#     SEP Development Kit is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     version 2 as published by the Free Software Foundation.
# 
#     SEP Development Kit is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with SEP Development Kit; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# 
#     As a special exception, you may use this file as part of a free software
#     library without restriction.  Specifically, if other files instantiate
#     templates or use macros or inline functions from this file, or you compile
#     this file and link it with other files to produce an executable, this
#     file does not by itself cause the resulting executable to be covered by
#     the GNU General Public License.  This exception does not however
#     invalidate any other reasons why the executable file might be covered by
#     the GNU General Public License.


#include <asm/dwarf2.h>
#include <asm/calling.h>
#include <asm/msr.h>

.text

#***********************************************************************
#
#    SYS_Get_IDT_Base
#            Get the IDT Desc address
#
#    Entry:  pointer to location to store idt Desc
#
#    Exit:  none 
#
# void SYS_Get_IDT_Base(U64 *pIdtDesc);
#
#***********************************************************************
        .global SYS_Get_IDT_Base
SYS_Get_IDT_Base:
        SIDT (%rdi)
        ret

#***********************************************************************
#
#    SYS_Get_GDT_Base
#            Get the GDT Desc address
#
#    Entry:  pointer to location to store gdt Desc
#
#    Exit:  none 
#
# void SYS_Get_GDT_Base(U64 *pGdtDesc);
#
#***********************************************************************
        .global SYS_Get_GDT_Base
SYS_Get_GDT_Base:
        SGDT (%rdi)
        ret

#***********************************************************************
#
#    SYS_Get_TSC
#            Get the current TSC
#
#    Entry:  pointer to location to store gdt Desc
#
#    Exit:  none 
#
# void SYS_Get_TSC(U64 *tsc);
#
#***********************************************************************
#        .global SYS_Get_TSC
#SYS_Get_TSC:
#        rdtsc
#        ret

#***********************************************************************
#
#    SYS_IO_Delay
#            Add a short delay to the instruction stream
#
#    Entry:  none
#
#    Exit:  none 
#
# void SYS_IO_Delay(void);
#
#***********************************************************************
        .global SYS_IO_Delay
SYS_IO_Delay:
         ret

# ----------------------------------------------------------------------------
# name:         SYS_PerfVec_Handler
#
# description:  ISR entry for local APIC PERF interrupt vector
#
# Input:        n/a
#
# Output:       n/a 
# ----------------------------------------------------------------------------

        .global SYS_Perfvec_Handler
SYS_Perfvec_Handler:
	CFI_STARTPROC
        pushq %rax          // fake an error code...
        cld                 // cause the kernel likes it this way...

        SAVE_ALL            // Save the world!

        movl  $MSR_GS_BASE,%ecx     // for the moment, do the safe swapgs check
        rdmsr
        xorl  %ebx,%ebx             // assume no swapgs (ebx == 0)
        testl %edx,%edx
        js    1f
        swapgs
        movl  $1,%ebx               // ebx == 1 means we did a swapgs
1:      movq %rsp,%rdi              // pt_regs is the first argument

        //
        // ebx is zero if no swap, one if swap
        // ebx is preserved in C calling convention...
        //
        // NOTE: the C code is responsible for ACK'ing the APIC!!!
        //
        call PMI_Interrupt_Handler

        //
        // Don't want an interrupt while we are doing the swapgs stuff
        //
        cli
        testl %ebx,%ebx
        jz 2f
        swapgs
2:      RESTORE_ALL
        popq    %rax
        iretq
        CFI_ENDPROC
