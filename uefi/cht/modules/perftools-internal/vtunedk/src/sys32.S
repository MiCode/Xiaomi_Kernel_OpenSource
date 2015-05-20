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


#include <linux/version.h>
#include <asm/segment.h>

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,20)
#define USE_KERNEL_PERCPU_SEGMENT_GS
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,29)
#define USE_KERNEL_PERCPU_SEGMENT_FS
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#define USE_KERNEL_PERCPU_SEGMENT_FS
#define USE_KERNEL_PERCPU_SEGMENT_GS
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#if !defined(__KERNEL_PERCPU)
#define __KERNEL_PERCPU __KERNEL_PDA
#endif
#endif

#***********************************************************************
#
#    SYS_Get_IDT_Base_HWR
#            Get the IDT Desc address
#
#    Entry: none
#
#    Exit:  base address in eax
#
# void SYS_Get_IDT_Base_HWR(U64 *pIdtDesc);
#
#***********************************************************************
        .text
        .align  4
	.global SYS_IO_Delay
SYS_IO_Delay:
	ret

        .global SYS_Get_IDT_Base_HWR
SYS_Get_IDT_Base_HWR:
        subl    $8,%esp
        sidt    2(%esp)
	movl    4(%esp),%eax
	addl    $8,%esp
        ret
	.global SYS_Get_cs
SYS_Get_cs:
	mov	%cs, %ax
	andl	$0x0000ffff, %eax
	ret
	
	.global SYS_Get_TSC
SYS_Get_TSC:
	rdtsc
	ret
        .text
        .align  4
        .global SYS_Perfvec_Handler
SYS_Perfvec_Handler:
                                        # This is the same as KERNEL's
        pushl   %eax                    # Filler for Error Code

        cld
        pushl   %es                     # SAVE_ALL macro to access pt_regs
        pushl   %ds                     # inside our ISR.
#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        pushl   %gs
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        pushl   %fs
#endif
        pushl   %eax
        pushl   %ebp
        pushl   %edi
        pushl   %esi
        pushl   %edx
        pushl   %ecx
        pushl   %ebx

        movl    $(__KERNEL_DS), %edx    # Use KERNEL DS selector
        movl    %edx,%ds                # Make sure we set Kernel
        movl    %edx,%es                # DS into local DS and ES

#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        movl    $(__KERNEL_PERCPU), %edx    # Use kernel percpu segment
        movl    %edx,%gs                    # ... and load it into %gs
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        movl    $(__KERNEL_PERCPU), %edx    # Use kernel percpu segment
        movl    %edx,%fs                    # ... and load it into %fs
#endif

	movl	%esp, %ebx		# get ready to put *pt_regs on stack
        
        pushl   %ebx			# put *pt_regs on the stack
        call PMI_Interrupt_Handler
        addl $0x4, %esp			# pop to nowhere...
 
       pop     %ebx                    # restore register set
        pop     %ecx
        pop     %edx
        pop     %esi
        pop     %edi
        pop     %ebp
        pop     %eax
#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        pop     %fs
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        pop     %gs
#endif
        pop     %ds
        pop     %es
        pop     %eax
        
        iret
# ----------------------------------------------------------------------------
# name:         get_CSD
#
# description:  get the CS descriptor
#
# input:        code segment selector
#
# output:       code segment descriptor
# ----------------------------------------------------------------------------
        .text
        .align  4
        .globl  SYS_Get_CSD

SYS_Get_CSD:
        pushl   %ebp
        movl    %esp, %ebp
        pushal                                  # save regs

        subl    $8,%esp
        xorl    %eax, %eax
        movw    8(%ebp), %ax                    # eax.lo = cs
        sgdt    (%esp)                          # store gdt reg
        leal    (%esp), %ebx                    # ebx = gdt reg ptr
        movl    2(%ebx), %ecx                   # ecx = gdt base
        xorl    %edx, %edx
        movw    %ax, %dx
        andl    $4, %edx
        cmpl    $0, %edx                        # test ti. GDT?
        jz      .bsr_10                         # ..yes
        xorl    %edx, %edx
        sldt    %dx                             # ..no dx=ldtsel
        andb    $0xf8, %dl                      # clear ti,rpl
        addl    2(%ebx), %edx                   # add gdt base
        movb    7(%edx), %cl                    # ecx = ldt base
        shll    $8, %ecx                        # ..
        movb    4(%edx), %cl                    # ..
        shll    $16, %ecx                       # ..
        movw    2(%edx), %cx                    # ..
.bsr_10:
        andb    $0xf8, %al                      # clear ti & rpl
        addl    %eax, %ecx                      # add to gdt/ldt
        movl    (%ecx), %eax                    # copy code seg
        movl    12(%ebp), %edx                  # ..descriptor (csdlo)
        movl    %eax, (%edx)                    # ..descriptor (csdlo)
        movl    4(%ecx), %eax                   # ..from gdt or
        movl    16(%ebp), %edx                  # ..ldt to sample (csdhi)
        movl    %eax, (%edx)                    # ..ldt to sample (csdhi)
        addl    $8,%esp
        popal                                   # restore regs
        leave
        ret
