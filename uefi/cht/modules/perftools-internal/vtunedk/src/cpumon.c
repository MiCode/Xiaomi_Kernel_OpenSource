/*COPYRIGHT**
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
**COPYRIGHT*/

/*
 *  CVS_Id="$Id$"
 */

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/interrupt.h>
#if defined(DRV_EM64T)
#include <asm/desc.h>
#endif

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "apic.h"
#endif
#include "lwpmudrv.h"
#include "control.h"
#include "utility.h"
#include "cpumon.h"
#include "pmi.h"

#if defined DRV_USE_NMI
#include <linux/ptrace.h>
#include <asm/nmi.h>
#include <linux/notifier.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
static int
cpumon_NMI_Handler(unsigned int cpu, struct pt_regs *regs)
{
    PMI_Interrupt_Handler(regs);
    return NMI_HANDLED;
}

#define EBS_NMI_CALLBACK                        cpumon_NMI_Handler
#define SET_NMI_CALLBACK(type,func,flags,name)  register_nmi_handler((type),(func),(flags),(name))
#define UNSET_NMI_CALLBACK(type,name)           unregister_nmi_handler((type),(name))
#endif

#endif // DRV_USE_NMI

/*
 * CPU Monitoring Functionality
 */


/*
 * General per-processor initialization
 */
#if defined(DRV_IA32) && !defined(DRV_USE_NMI)

typedef union {
    unsigned long long    u64[1];
    unsigned short int    u16[4];
} local_handler_t;

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Save_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Save the old handler for restoration when done
 *
 */
static void 
cpumon_Save_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    U64                 *idt_base;
    CPU_STATE            pcpu;

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();

    SYS_Local_Irq_Save(eflags);
    CPU_STATE_idt_base(pcpu) = idt_base = SYS_Get_IDT_Base();
    // save original perf. vector
    CPU_STATE_saved_ih(pcpu) = idt_base[CPU_PERF_VECTOR];
    SEP_PRINT_DEBUG("saved_ih is 0x%llx\n", CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Init_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 *
 */
static VOID 
cpumon_Init_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    U64                 *idt_base;
    CPU_STATE            pcpu;
    local_handler_t      lhandler;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long        cr0_value;
#endif

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();
    SYS_Local_Irq_Save(eflags);
    
    idt_base = CPU_STATE_idt_base(pcpu);
    // install perf. handler
    // These are the necessary steps to have an ISR entry
    // Note the changes in the data written
    lhandler.u64[0] = (unsigned long)SYS_Perfvec_Handler;
    lhandler.u16[3] = lhandler.u16[1];
    lhandler.u16[1] = SYS_Get_cs();
    lhandler.u16[2] = 0xee00;

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    idt_base[CPU_PERF_VECTOR] = lhandler.u64[0];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif

    SYS_Local_Irq_Restore(eflags);
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Destroy_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Restore the old handler
 * @brief  Finish clean up of the apic
 *
 */
static VOID 
cpumon_Destroy_Cpu (
    PVOID ctx
)
{
    unsigned long        eflags;
    unsigned long long  *idt_base;
    CPU_STATE            pcpu;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long        cr0_value;
#endif

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();

    SYS_Local_Irq_Save(eflags);
    // restore perf. vector (to a safe stub pointer)
    idt_base = SYS_Get_IDT_Base();
    APIC_Disable_PMI();

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    idt_base[CPU_PERF_VECTOR] = CPU_STATE_saved_ih(pcpu);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif

    SYS_Local_Irq_Restore(eflags);

    return;
}
#endif

#if defined(DRV_EM64T) && !defined(DRV_USE_NMI)

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Set_IDT_Func(idt, func)
 *
 * @param  GATE_STRUCT*  - address of the idt vector
 * @param  PVOID         - function to set in IDT 
 *
 * @return None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 * @brief  Save the old handler for restoration when done
 *
 */
static VOID
cpumon_Set_IDT_Func (
    GATE_STRUCT   *idt,
    PVOID          func
)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    _set_gate(&idt[CPU_PERF_VECTOR], GATE_INTERRUPT, (unsigned long) func, 3, 0);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long cr0_value;
#endif
    GATE_STRUCT  local;
    // _set_gate() cannot be used because the IDT table is not exported.

    pack_gate(&local, GATE_INTERRUPT, (unsigned long)func, 3, 0, __KERNEL_CS);

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    write_idt_entry((idt), CPU_PERF_VECTOR, &local);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif
#endif
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Save_Cpu(param)
 *
 * @param  param - Unused, set up to enable parallel calls
 *
 * @return None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 * @brief  Save the old handler for restoration when done
 *
 */
static VOID 
cpumon_Save_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    IDTGDT_DESC          idt_base;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT          old_gate;
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    SYS_Get_IDT_Base((PVOID*)&idt_base);
    idt  = idt_base.idtgdt_base;

    CPU_STATE_idt_base(pcpu) = idt;
    memcpy (&old_gate, &idt[CPU_PERF_VECTOR], 16);

    CPU_STATE_saved_ih(pcpu)  = (PVOID) ((((U64) old_gate.offset_high) << 32)   | 
                                         (((U64) old_gate.offset_middle) << 16) | 
                                          ((U64) old_gate.offset_low));
 
    SEP_PRINT_DEBUG("saved_ih is 0x%llx\n", CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Init_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 *
 */
static VOID 
cpumon_Init_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    idt = CPU_STATE_idt_base(pcpu);
    cpumon_Set_IDT_Func(idt, SYS_Perfvec_Handler);
    SYS_Local_Irq_Restore(eflags);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Destroy_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Restore the old handler
 * @brief  Finish clean up of the apic
 *
 */
static VOID 
cpumon_Destroy_Cpu (
    PVOID ctx
)
{
    unsigned long        eflags;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    APIC_Disable_PMI();
    idt = CPU_STATE_idt_base(pcpu);
    cpumon_Set_IDT_Func(idt, CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);

    return;
}
#endif

#if defined(DRV_IA32) || defined(DRV_EM64T)
/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void CPUMON_Install_Cpuhools(void)
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief  set up the interrupt handler (on a per-processor basis)
 * @brief  Initialize the APIC in two phases (current CPU, then others)
 *
 */
extern VOID 
CPUMON_Install_Cpuhooks (
    void
)
{
    S32   me        = 0;
    PVOID linear    = NULL;

#ifndef DRV_USE_NMI
    CONTROL_Invoke_Parallel(cpumon_Save_Cpu, (PVOID)(size_t)me);
    CONTROL_Invoke_Parallel(cpumon_Init_Cpu, (PVOID)(size_t)me);
#endif
    APIC_Init(&linear);
    CONTROL_Invoke_Parallel(APIC_Init, &linear);
    CONTROL_Invoke_Parallel(APIC_Install_Interrupt_Handler, (PVOID)(size_t)me);

#ifdef DRV_USE_NMI
    SET_NMI_CALLBACK(NMI_LOCAL, EBS_NMI_CALLBACK, NMI_FLAG_FIRST, "sep_pmi");
#endif
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void CPUMON_Remove_Cpuhools(void)
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief  De-Initialize the APIC in phases
 * @brief  clean up the interrupt handler (on a per-processor basis)
 *
 */
extern VOID 
CPUMON_Remove_Cpuhooks (
    void
)
{
    int            i;
#ifndef DRV_USE_NMI
    unsigned long  eflags;

    SYS_Local_Irq_Save(eflags);
    cpumon_Destroy_Cpu((PVOID)(size_t)0);
    SYS_Local_Irq_Restore(eflags);
    CONTROL_Invoke_Parallel_XS(cpumon_Destroy_Cpu, 
                               (PVOID)(size_t)0);
#else
    UNSET_NMI_CALLBACK(NMI_LOCAL, "sep_pmi");
#endif

    // de-initialize APIC
    APIC_Unmap(CPU_STATE_apic_linear_addr(&pcb[0]));
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        APIC_Deinit_Phase1(i);
    }

    return;
}
#endif /* defined(DRV_IA32) || defined(DRV_EM64T) */
