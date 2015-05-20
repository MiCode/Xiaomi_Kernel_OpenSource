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


#ifndef _UTILITY_H_
#define _UTILITY_H_

/**
// Data Types and Macros
*/
#pragma pack(push, 1)

#pragma pack(pop)

/*
 *  These routines have macros defined in asm/system.h
 */
#define SYS_Local_Irq_Enable()       local_irq_enable()
#define SYS_Local_Irq_Disable()      local_irq_disable()
#define SYS_Local_Irq_Save(flags)    local_irq_save(flags)
#define SYS_Local_Irq_Restore(flags) local_irq_restore(flags)

#if defined(DRV_IA32) || defined(DRV_EM64T)
#include <asm/msr.h>
#else
#include <asm/intrinsics.h>
#endif

#if defined(DRV_IA32)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
#define SYS_Write_MSR(msr,val)       wrmsr((U32)msr, (unsigned long)(val&0xFFFFFFFFLL), (unsigned long)(val>>32))  // void..(unsigned int msr, unsigned long long val)
#else
#define SYS_Write_MSR(msr,val)       wrmsr_safe(msr,(unsigned long)(val&0xFFFFFFFFLL), (unsigned long)(val>>32))  // void..(unsigned int msr, unsigned long long val)
#endif

#endif

#if defined(DRV_EM64T)
#define SYS_Write_MSR(msr,val)       wrmsrl((U32)msr,val)  // void..(unsigned int msr, unsigned long long val)
#endif

#if defined(DRV_IA32) || defined(DRV_EM64T)
extern U64
SYS_Read_MSR (U32 msr);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0) && defined(CONFIG_UIDGID_STRICT_TYPE_CHECKS))
#define DRV_GET_UID(p)      p->cred->uid.val
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#define DRV_GET_UID(p)      p->cred->uid
#else
#define DRV_GET_UID(p)      p->uid
#endif

extern void SYS_Perfvec_Handler (void);

extern void *SYS_get_stack_ptr0 (void);
extern void *SYS_get_stack_ptr3 (void);
extern void *SYS_get_user_fp (void);
extern short SYS_Get_cs (void);

#if defined(DRV_IA32)
extern void *SYS_Get_IDT_Base_HWR(void);   /// IDT base from hardware IDTR
extern void *SYS_Get_GDT_Base_HWR(void);   /// GDT base from hardware GDTR
extern U64   SYS_Get_TSC(void);

#define SYS_Get_IDT_Base SYS_Get_IDT_Base_HWR
#define SYS_Get_GDT_Base SYS_Get_GDT_Base_HWR
#endif

#if defined(DRV_EM64T)
extern unsigned short SYS_Get_Code_Selector0 (void);
extern void SYS_Get_IDT_Base (void **);
extern void SYS_Get_GDT_Base (void **);
#endif

extern void SYS_IO_Delay (void);
#define SYS_Inb(port)       inb(port)
#define SYS_Outb(byte,port) outb(byte,port)

/* typedef int                 OSSTATUS; */

/*
 * Lock implementations
 */
#define SYS_Locked_Inc(var)              atomic_inc((var))
extern void SYS_Locked_Dec (volatile int* var);

extern void  UTILITY_Read_TSC (U64* pTsc);


extern DRV_BOOL
UTILITY_down_read_mm (
    struct task_struct *p
);

extern void
UTILITY_up_read_mm(struct task_struct *p);

#if 0 && defined(DRV_EM64T)
extern void SYS_Get_GDT (U64 *pGdtDesc);
extern void SYS_Get_IDT (U64 *pIdtDesc);
#endif

#if defined(DRV_IA32) || defined(DRV_EM64T)
extern void
UTILITY_Read_Cpuid(
    U64  cpuid_function,
    U64 *rax_value,
    U64 *rbx_value,
    U64 *rcx_value,
    U64 *rdx_value
);
#endif

extern  DISPATCH
UTILITY_Configure_CPU (U32);

#if defined(DRV_IA32)
asmlinkage void SYS_Get_CSD (U32, U32 *, U32 *);
#endif

extern  CS_DISPATCH
UTILITY_Configure_Chipset (void);

#endif 
