/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
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

        .text
        .align  4
        .global vtss_io_delay
vtss_io_delay:
        ret

/*
 * User gs save/restore
 *
 * %gs is used for userland TLS and kernel only uses it for stack
 * canary which is required to be at %gs:20 by gcc.  Read the comment
 * at the top of stackprotector.h for more info.
 */
#ifdef CONFIG_X86_32_LAZY_GS

/* unfortunately push/pop can't be no-op */
.macro PUSH_GS
        pushl $0
.endm
.macro POP_GS
        addl $4, %esp
.endm
.macro SET_KERNEL_GS reg
.endm

#else  /* CONFIG_X86_32_LAZY_GS */

.macro PUSH_GS
        pushl %gs
.endm
.macro POP_GS
        popl %gs
.endm
.macro SET_KERNEL_GS reg
        movl $(__KERNEL_STACK_CANARY), \reg
        movl \reg, %gs
.endm

#endif /* CONFIG_X86_32_LAZY_GS */

        .align  16
        .global vtss_perfvec_handler
        .extern vtss_pmi_handler
vtss_perfvec_handler:
        pushl   %eax
        cld
#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        PUSH_GS
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        pushl   %fs
#endif
        pushl   %es                     # SAVE_ALL macro to access pt_regs
        pushl   %ds                     # inside our ISR.
        pushl   %eax
        pushl   %ebp
        pushl   %edi
        pushl   %esi
        pushl   %edx
        pushl   %ecx
        pushl   %ebx

        xorl    %eax, %eax
        xorl    %edx, %edx
        movl    $0x01d9, %ecx           # DEBUGCTL_MSR
        wrmsr                           # disable branch tracing

        movl    $(__KERNEL_DS), %edx    # Use KERNEL DS selector
        movl    %edx, %ds               # Make sure we set Kernel
        movl    %edx, %es               # DS into local DS and ES

#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        movl    $(__KERNEL_PERCPU), %edx # Use kernel percpu segment
        movl    %edx, %fs                # ... and load it into %fs
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        SET_KERNEL_GS %edx
#endif

        movl    %esp, %ebx              # get ready to put *pt_regs on stack
        pushl   %ebx                    # put *pt_regs on the stack
        call    vtss_pmi_handler
        addl    $0x4, %esp              # pop to nowhere...

        pop     %ebx                    # restore register set
        pop     %ecx
        pop     %edx
        pop     %esi
        pop     %edi
        pop     %ebp
        pop     %eax
        pop     %ds
        pop     %es
#if defined(USE_KERNEL_PERCPU_SEGMENT_FS)
        pop     %fs
#endif
#if defined(USE_KERNEL_PERCPU_SEGMENT_GS)
        POP_GS
#endif
        pop     %eax
        iret
