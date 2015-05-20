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
#include "vtss_config.h"
#include "apic.h"
#include "globals.h"

#include <asm/io.h>

/* APIC base MSR */
#define VTSS_APIC_BASE_MSR       0x01b

/* x2APIC MSRs */
#define VTSS_APIC_LCL_ID_MSR     0x802
#define VTSS_APIC_LCL_TSKPRI_MSR 0x808
#define VTSS_APIC_LCL_PPR_MSR    0x80a
#define VTSS_APIC_LCL_EOI_MSR    0x80b
#define VTSS_APIC_LCL_LDEST_MSR  0x80d
#define VTSS_APIC_LCL_DSTFMT_MSR 0x80e
#define VTSS_APIC_LCL_SVR_MSR    0x80f
#define VTSS_APIC_LCL_ICR_MSR    0x830
#define VTSS_APIC_LVT_TIMER_MSR  0x832
#define VTSS_APIC_LVT_PMI_MSR    0x834
#define VTSS_APIC_LVT_LINT0_MSR  0x835
#define VTSS_APIC_LVT_LINT1_MSR  0x836
#define VTSS_APIC_LVT_ERROR_MSR  0x837

/* APIC registers */
#define VTSS_APIC_LCL_ID     0x0020
#define VTSS_APIC_LCL_TSKPRI 0x0080
#define VTSS_APIC_LCL_PPR    0x00a0
#define VTSS_APIC_LCL_EOI    0x00b0
#define VTSS_APIC_LCL_LDEST  0x00d0
#define VTSS_APIC_LCL_DSTFMT 0x00e0
#define VTSS_APIC_LCL_SVR    0x00f0
#define VTSS_APIC_LCL_ICR    0x0300
#define VTSS_APIC_LVT_TIMER  0x0320
#define VTSS_APIC_LVT_PMI    0x0340
#define VTSS_APIC_LVT_LINT0  0x0350
#define VTSS_APIC_LVT_LINT1  0x0360
#define VTSS_APIC_LVT_ERROR  0x0370

/* masks for LVT */
#define LVT_MASK   0x10000
#define LVT_EDGE   0x00000
#define LVT_LEVEL  0x08000
#define LVT_EXTINT 0x00700
#define LVT_NMI    0x00400

/* task priorities */
#define VTSS_APIC_TSKPRI_LO 0x0000
#define VTSS_APIC_TSKPRI_HI 0x00f0

#define VTSS_X2APIC_ENABLED 0x0c00ULL

#ifndef preempt_enable_no_resched
#define preempt_enable_no_resched() preempt_enable()
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
#ifdef CONFIG_RESOURCES_64BIT
typedef u64 resource_size_t;
#else
typedef u32 resource_size_t;
#endif
#endif /* 2.6.27 */

static int vtss_x2apic_mode = 0;

extern void vtss_io_delay(void);

/* unmask PMI */
void vtss_pmi_enable(void)
{
    if (vtss_x2apic_mode) {
        long long msr_val;

        rdmsrl(VTSS_APIC_LVT_PMI_MSR, msr_val);
        msr_val &= ~LVT_MASK;
        wrmsrl(VTSS_APIC_LVT_PMI_MSR, msr_val);
    } else {
        char *apic = (char*)pcb_cpu.apic_linear_addr;

        if (apic != NULL) {
            *((int*)&apic[VTSS_APIC_LVT_PMI]) &= ~LVT_MASK;
        }
    }
}

/* mask PMI */
void vtss_pmi_disable(void)
{
    if (vtss_x2apic_mode) {
        wrmsrl(VTSS_APIC_LVT_PMI_MSR, (CPU_PERF_VECTOR | LVT_MASK));
    } else {
        char *apic = (char*)pcb_cpu.apic_linear_addr;

        if (apic != NULL) {
            *((int*)&apic[VTSS_APIC_LVT_PMI]) = (CPU_PERF_VECTOR | LVT_MASK);
        }
    }
}

static void vtss_apic_pmi_disable_on_cpu(void *ctx)
{
    vtss_pmi_disable();
}

/* acknowledge end of interrupt */
void vtss_apic_ack_eoi(void)
{
    if (vtss_x2apic_mode) {
        wrmsrl(VTSS_APIC_LCL_EOI_MSR, 0LL);
    } else {
        char *apic = (char*)pcb_cpu.apic_linear_addr;

        if (apic != NULL) {
            *((int*)&apic[VTSS_APIC_LCL_EOI]) = 0;
        }
    }
}

/* read the contents of Processor Priority Register */
int vtss_apic_read_priority(void)
{
    if (vtss_x2apic_mode) {
        long long msr_val;

        rdmsrl(VTSS_APIC_LCL_PPR_MSR, msr_val);
        return (int)msr_val;
    } else {
        char *apic = (char*)pcb_cpu.apic_linear_addr;

        return (apic != NULL) ? *((int*)&apic[VTSS_APIC_LCL_PPR]) : 0;
    }
}

/* collect APIC physical addresses */
static void vtss_apic_init_phase1(void *ctx)
{
    unsigned long long addr;

    rdmsrl(VTSS_APIC_BASE_MSR, addr);
    if (cpu_has_x2apic && (addr & VTSS_X2APIC_ENABLED) == VTSS_X2APIC_ENABLED) {
        vtss_x2apic_mode = 1;
        rdmsrl(VTSS_APIC_LCL_ID_MSR, addr);
        /* the most significant byte contains the ID */
        pcb_cpu.apic_id = (addr >> 24);
        TRACE("x2apic_enabled=1, apic_id=%d", pcb_cpu.apic_id);
    } else {
        pcb_cpu.apic_physical_addr = (void*)(size_t)addr;
        TRACE("apic_physical_addr=0x%p", pcb_cpu.apic_physical_addr);
    }
}

/* map APIC physical address to logical address space */
static void vtss_apic_init_phase2(void *ctx)
{
    char *apic;
    int cpu = (int)(size_t)ctx;

    if (smp_processor_id() == cpu) {
        if (pcb_cpu.apic_linear_addr == NULL) {
            TRACE("Map apic now!");
            pcb_cpu.apic_linear_addr =
                ioremap_nocache((resource_size_t)((size_t)pcb_cpu.apic_physical_addr & 0x00000000fffff000ULL),
                                (resource_size_t)0x1000);
        }
    } else {
        pcb_cpu.apic_linear_addr = pcb(cpu).apic_linear_addr;
    }
    apic = (char*)pcb_cpu.apic_linear_addr;
    if (apic != NULL) {
        /* the most significant byte contains the ID */
        pcb_cpu.apic_id = *((int*)&apic[VTSS_APIC_LCL_ID]) >> 24;
        TRACE("apic_linear_addr=0x%p, apic_id=%d", apic, pcb_cpu.apic_id);
    } else {
        ERROR("ioremap_nocache() failed");
    }
}

/* enable local APIC for SP systems */
static void vtss_apic_init_phase3(void* ctx)
{
    unsigned long flags;
    char *apic = (char*)pcb_cpu.apic_linear_addr;
    unsigned long long addr = (unsigned long long)(size_t)pcb_cpu.apic_physical_addr;

    if (!(VTSS_APIC_BASE_GLOBAL_ENABLED(addr)) || !(VTSS_APIC_VIRTUAL_WIRE_ENABLED(*((int *)&apic[VTSS_APIC_LCL_SVR])))) {
        char pic0, pic1;

        /* setup virtual wire */
        local_irq_save(flags);

        /* mask PICs */
        pic0 = inb(0x21);
        vtss_io_delay();
        pic1 = inb(0xA1);
        vtss_io_delay();
        outb(0xFF, 0xA1);
        vtss_io_delay();
        outb(0xFF, 0x21);

        local_irq_enable();
        vtss_io_delay();
        vtss_io_delay();
        local_irq_disable();

        /* enable via VTSS_APIC_BASE_MSR */
        addr |= (1 << 11);
        TRACE("set APIC_BASE_MSR to 0x%llx", addr);
        wrmsrl(VTSS_APIC_BASE_MSR, addr);

        /* enable in SVR and set VW */
        *((int*)&apic[VTSS_APIC_LCL_SVR])    = 0x01FF;
        *((int*)&apic[VTSS_APIC_LCL_TSKPRI]) = VTSS_APIC_TSKPRI_HI;
        *((int*)&apic[VTSS_APIC_LCL_ID])     = 0;
        *((int*)&apic[VTSS_APIC_LCL_LDEST])  = 0;        /* set local dest. id */
        *((int*)&apic[VTSS_APIC_LCL_DSTFMT]) = -1;       /* set dest. format   */
        *((int*)&apic[VTSS_APIC_LVT_TIMER])  = LVT_MASK; /* mask local timer   */
        *((int*)&apic[VTSS_APIC_LVT_ERROR])  = LVT_MASK; /* mask error         */
        /* INTs are redirected to PICs */
        *((int*)&apic[VTSS_APIC_LVT_LINT0])  = LVT_EXTINT + LVT_EDGE;
        *((int*)&apic[VTSS_APIC_LVT_LINT1])  = LVT_NMI + LVT_LEVEL;
        *((int*)&apic[VTSS_APIC_LCL_TSKPRI]) = VTSS_APIC_TSKPRI_LO;

        /* unmask PICs */
        outb(pic0, 0x21);
        outb(pic1, 0xA1);

        local_irq_restore(flags);
    }
}

int vtss_apic_map(void)
{
    if (!vtss_x2apic_mode) {
        int cpu;
        preempt_disable();
        cpu = smp_processor_id();
        /* 2.a map APIC physical address to logical address space */
        /* 2.b initialize PMI entry in LVT for MP systems */
        vtss_apic_init_phase2((void*)(size_t)cpu);
        smp_call_function(vtss_apic_init_phase2, (void*)(size_t)cpu, SMP_CALL_FUNCTION_ARGS);
        preempt_enable_no_resched();
        /* 3. enable local APIC for SP systems */
//        on_each_cpu(vtss_apic_init_phase3, NULL, SMP_CALL_FUNCTION_ARGS);
    }
    TRACE("APIC map");
    return 0;
}
void vtss_apic_init(void)
{
    TRACE("IDT vector 0x%u is used for PMU interrupts.", CPU_PERF_VECTOR);
    /* 1. collect APIC physical addresses or check for x2apic */
    on_each_cpu(vtss_apic_init_phase1, NULL, SMP_CALL_FUNCTION_ARGS);

    /* 2.a map APIC physical address to logical address space */
    /* 2.b initialize PMI entry in LVT for MP systems */
    /* 3. enable local APIC for SP systems */
    vtss_apic_map();
    /* 4. disable PMI on each CPU */
    on_each_cpu(vtss_apic_pmi_disable_on_cpu, NULL, SMP_CALL_FUNCTION_ARGS);
    TRACE("APIC init");
}

static void vtss_apic_unmap_cpu(void *ctx)
{
    int cpu = (int)(size_t)ctx;
    char *apic = (char*)pcb_cpu.apic_linear_addr;

    if (smp_processor_id() == cpu && apic != NULL) {
        TRACE("Unmap apic now!\n");
        iounmap(apic);
    }
    pcb_cpu.apic_linear_addr = NULL;
}


/* unmap APIC logical address range */
static void vtss_apic_fini_phase1(void *ctx)
{
    vtss_apic_unmap_cpu(ctx);
}

void vtss_apic_unmap(void)
{
    if (!vtss_x2apic_mode) {
        preempt_disable();
        on_each_cpu(vtss_apic_unmap_cpu, (void*)(size_t)smp_processor_id(), SMP_CALL_FUNCTION_ARGS);
        preempt_enable();
        TRACE("APIC unmap");
    }
}
void vtss_apic_fini(void)
{
    on_each_cpu(vtss_apic_pmi_disable_on_cpu, NULL, SMP_CALL_FUNCTION_ARGS);
    if (!vtss_x2apic_mode) {
        TRACE("!vtss_x2apic_mode");
        /* unmap APIC logical address range */
        preempt_disable();
        on_each_cpu(vtss_apic_fini_phase1, (void*)(size_t)smp_processor_id(), SMP_CALL_FUNCTION_ARGS);
        preempt_enable_no_resched();
    }
    TRACE("APIC fini");
}
