/*
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#ifndef _APIC_H_
#define _APIC_H_

#include <stddef.h>
#include <linux/irq.h>

typedef U64 *PHYSICAL_ADDRESS;
/**
// Data Types and Macros
*/
/*
// APIC registers and constants
*/
// APIC base MSR
#define DRV_APIC_BASE_MSR   0x001b

// APIC registers
#define DRV_APIC_LCL_ID     0x0020
#define DRV_APIC_LCL_TSKPRI 0x0080
#define DRV_APIC_LCL_PPR    0x00a0
#define DRV_APIC_LCL_EOI    0x00b0
#define DRV_APIC_LCL_LDEST  0x00d0
#define DRV_APIC_LCL_DSTFMT 0x00e0
#define DRV_APIC_LCL_SVR    0x00f0
#define DRV_APIC_LCL_ICR    0x0300
#define DRV_APIC_LVT_TIMER  0x0320
#define DRV_APIC_LVT_PMI    0x0340
#define DRV_APIC_LVT_LINT0  0x0350
#define DRV_APIC_LVT_LINT1  0x0360
#define DRV_APIC_LVT_ERROR  0x0370

#define DRV_APIC_LCL_ID_MSR     0x802
#define DRV_APIC_LCL_TSKPRI_MSR 0x808
#define DRV_APIC_LCL_PPR_MSR    0x80a
#define DRV_APIC_LCL_EOI_MSR    0x80b
#define DRV_APIC_LCL_LDEST_MSR  0x80d
#define DRV_APIC_LCL_DSTFMT_MSR 0x80e
#define DRV_APIC_LCL_SVR_MSR    0x80f
#define DRV_APIC_LCL_ICR_MSR    0x830
#define DRV_APIC_LVT_TIMER_MSR  0x832
#define DRV_APIC_LVT_PMI_MSR    0x834
#define DRV_APIC_LVT_LINT0_MSR  0x835
#define DRV_APIC_LVT_LINT1_MSR  0x836
#define DRV_APIC_LVT_ERROR_MSR  0x837

// masks for LVT
#define DRV_LVT_MASK        0x10000
#define DRV_LVT_EDGE        0x00000
#define DRV_LVT_LEVEL       0x08000
#define DRV_LVT_EXTINT      0x00700
#define DRV_LVT_NMI         0x00400

// task priorities
#define DRV_APIC_TSKPRI_LO  0x0000
#define DRV_APIC_TSKPRI_HI  0x00f0

#define DRV_X2APIC_ENABLED  0xc00LL

//// Interrupt vector for PMU overflow event
//
//     Choose the highest unused IDT vector possible so that our
//     callback routine runs at the highest priority allowed;
//     must avoid using pre-defined vectors in,
//              include/asm/irq.h
//              include/asm/hw_irq.h
//              include/asm/irq_vectors.h
//
// FIRST_DEVICE_VECTOR should be valid for kernels 2.6.33 and earlier
#if defined(DRV_USE_NMI)
#define CPU_PERF_VECTOR     DRV_LVT_NMI
#else
#if defined(FIRST_DEVICE_VECTOR)
#define CPU_PERF_VECTOR     (FIRST_DEVICE_VECTOR - 1)
// FIRST_EXTERNAL_VECTOR should be valid for kernels 2.6.34 and later
#else
#define CPU_PERF_VECTOR     (FIRST_EXTERNAL_VECTOR + 1)
#endif
#endif
// Has the APIC Been enabled
#define DRV_APIC_BASE_GLOBAL_ENABLED(a)    ((a) & 1 << 11)
#define DRV_APIC_VIRTUAL_WIRE_ENABLED(a)   ((a) & 0x100)

extern U32 drv_x2apic_enabled;
/**
// Function Declarations
*/

/*
// APIC control functions
*/
extern VOID APIC_Disable_PMI(VOID);
extern VOID APIC_Deinit_Phase1(int cpu_idx);
extern VOID APIC_Ack_Eoi(VOID);
extern VOID APIC_Enable_Pmi(VOID);
extern VOID APIC_Init(PVOID param);
extern VOID APIC_Install_Interrupt_Handler(PVOID param);
extern VOID APIC_Unmap(PVOID addr);

#endif 
