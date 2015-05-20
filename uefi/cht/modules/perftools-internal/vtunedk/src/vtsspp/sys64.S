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
#include <asm/dwarf2.h>
#include <asm/calling.h>
#include <asm/msr.h>

        .text
        .align  16
        .global vtss_io_delay
vtss_io_delay:
        ret

        .align  16
        .global vtss_perfvec_handler
        .extern vtss_pmi_handler
vtss_perfvec_handler:
        CFI_STARTPROC
        pushq   %rax                    # fake an error code...
        cld                             # cause the kernel likes it this way...
        SAVE_ALL                        # Save the world!

        xorl    %eax, %eax
        xorl    %edx, %edx
        movl    $0x01d9, %ecx           # DEBUGCTL_MSR
        wrmsr                           # disable branch tracing

        movl    $MSR_GS_BASE, %ecx      # for the moment, do the safe swapgs check
        rdmsr
        xorl    %ebx, %ebx              # assume no swapgs (ebx == 0)
        testl   %edx, %edx
        js      1f                      # negative -> in kernel
        SWAPGS
        movl    $1, %ebx                # ebx == 1 means we did a swapgs
1:      movq    %rsp, %rdi              # pt_regs is the first argument

        # ebx is zero if no swap, one if swap
        # ebx is preserved in C calling convention...
        #
        # NOTE: the C code is responsible for ACK'ing the APIC!!!
        call    vtss_pmi_handler
        # Don't want an interrupt while we are doing the swapgs stuff
        cli
        testl   %ebx, %ebx
        jz      2f
        SWAPGS
2:      RESTORE_ALL
        popq    %rax
        iretq
        CFI_ENDPROC
