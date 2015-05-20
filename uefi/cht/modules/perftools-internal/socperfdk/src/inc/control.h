/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  BSD LICENSE 

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ***********************************************************************************************
*/

#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <linux/smp.h>
#include <linux/timer.h>
#if defined(DRV_IA32)
#include <asm/apic.h>
#endif
#include <asm/io.h>
#if defined(DRV_IA32)
#include <asm/msr.h>
#endif
#include <asm/atomic.h>

#include "lwpmudrv_defines.h"
#include "socperfdrv.h"
#include "lwpmudrv_types.h"

// large memory allocation will be used if the requested size (in bytes) is
// above this threshold
#define  MAX_KMALLOC_SIZE ((1<<17)-1)

// check whether Linux driver should use unlocked ioctls (not protected by BKL)
#if defined(HAVE_UNLOCKED_IOCTL)
#define DRV_USE_UNLOCKED_IOCTL
#endif
#if defined(DRV_USE_UNLOCKED_IOCTL)
#define IOCTL_OP .unlocked_ioctl
#define IOCTL_OP_TYPE long
#define IOCTL_USE_INODE
#else
#define IOCTL_OP .ioctl
#define IOCTL_OP_TYPE S32
#define IOCTL_USE_INODE struct   inode  *inode,
#endif

// Information about the state of the driver
typedef struct GLOBAL_STATE_NODE_S  GLOBAL_STATE_NODE;
typedef        GLOBAL_STATE_NODE   *GLOBAL_STATE;
struct GLOBAL_STATE_NODE_S {
    volatile S32    cpu_count;
    volatile S32    dpc_count;

    S32             num_cpus;       // Number of CPUs in the system
    S32             active_cpus;    // Number of active CPUs - some cores can be
                                    // deactivated by the user / admin
    S32             num_em_groups;
    S32             num_descriptors;
    volatile S32    current_phase;
};

// Access Macros
#define  GLOBAL_STATE_num_cpus(x)          ((x).num_cpus)
#define  GLOBAL_STATE_active_cpus(x)       ((x).active_cpus)
#define  GLOBAL_STATE_cpu_count(x)         ((x).cpu_count)
#define  GLOBAL_STATE_dpc_count(x)         ((x).dpc_count)
#define  GLOBAL_STATE_num_em_groups(x)     ((x).num_em_groups)
#define  GLOBAL_STATE_num_descriptors(x)   ((x).num_descriptors)
#define  GLOBAL_STATE_current_phase(x)     ((x).current_phase)
#define  GLOBAL_STATE_sampler_id(x)        ((x).sampler_id)

/*
 *
 *
 * CPU State data structure and access macros
 *
 */
typedef struct CPU_STATE_NODE_S  CPU_STATE_NODE;
typedef        CPU_STATE_NODE   *CPU_STATE;
struct CPU_STATE_NODE_S {
    S32         apic_id;             // Processor ID on the system bus
    PVOID       apic_linear_addr;    // linear address of local apic
    PVOID       apic_physical_addr;  // physical address of local apic

    PVOID       idt_base;            // local IDT base address
    atomic_t    in_interrupt;

#if defined(DRV_IA32)
    U64         saved_ih;            // saved perfvector to restore
#endif
#if defined(DRV_EM64T)
    PVOID       saved_ih;            // saved perfvector to restore
#endif

    S64        *em_tables;           // holds the data that is saved/restored
                                     // during event multiplexing

    struct timer_list *em_timer;
    U32         current_group;
    S32         trigger_count;
    S32         trigger_event_num;

    DISPATCH    dispatch;
    PVOID       lbr_area;
    PVOID       old_dts_buffer;
    PVOID       dts_buffer;
    U32         initial_mask;
    U32         accept_interrupt;

#if defined(BUILD_CHIPSET)
    // Chipset counter stuff
    U32         chipset_count_init;  // flag to initialize the last MCH and ICH arrays below.
    U64         last_mch_count[8];
    U64         last_ich_count[8];
    U64         last_gmch_count[MAX_CHIPSET_COUNTERS];
    U64         last_mmio_count[32]; // it's only 9 now but the next generation may have 29.
#endif

    U64        *pmu_state;           // holds PMU state (e.g., MSRs) that will be
                                     // saved before and restored after collection
    S32         socket_master;
    S32         core_master;
    S32         thr_master;
    U64         num_samples;
    U64         reset_mask;
    U64         group_swap;
    U64         last_uncore_count[16];
};

#define CPU_STATE_apic_id(cpu)              (cpu)->apic_id
#define CPU_STATE_apic_linear_addr(cpu)     (cpu)->apic_linear_addr
#define CPU_STATE_apic_physical_addr(cpu)   (cpu)->apic_physical_addr
#define CPU_STATE_idt_base(cpu)             (cpu)->idt_base
#define CPU_STATE_in_interrupt(cpu)         (cpu)->in_interrupt
#define CPU_STATE_saved_ih(cpu)             (cpu)->saved_ih
#define CPU_STATE_saved_ih_hi(cpu)          (cpu)->saved_ih_hi
#define CPU_STATE_dpc(cpu)                  (cpu)->dpc
#define CPU_STATE_em_tables(cpu)            (cpu)->em_tables
#define CPU_STATE_pmu_state(cpu)            (cpu)->pmu_state
#define CPU_STATE_em_dpc(cpu)               (cpu)->em_dpc
#define CPU_STATE_em_timer(cpu)             (cpu)->em_timer
#define CPU_STATE_current_group(cpu)        (cpu)->current_group
#define CPU_STATE_trigger_count(cpu)        (cpu)->trigger_count
#define CPU_STATE_trigger_event_num(cpu)    (cpu)->trigger_event_num
#define CPU_STATE_dispatch(cpu)             (cpu)->dispatch
#define CPU_STATE_lbr(cpu)                  (cpu)->lbr
#define CPU_STATE_old_dts_buffer(cpu)       (cpu)->old_dts_buffer
#define CPU_STATE_dts_buffer(cpu)           (cpu)->dts_buffer
#define CPU_STATE_initial_mask(cpu)         (cpu)->initial_mask
#define CPU_STATE_accept_interrupt(cpu)     (cpu)->accept_interrupt
#define CPU_STATE_msr_value(cpu)            (cpu)->msr_value
#define CPU_STATE_msr_addr(cpu)             (cpu)->msr_addr
#define CPU_STATE_socket_master(cpu)        (cpu)->socket_master
#define CPU_STATE_core_master(cpu)          (cpu)->core_master
#define CPU_STATE_thr_master(cpu)           (cpu)->thr_master
#define CPU_STATE_num_samples(cpu)          (cpu)->num_samples
#define CPU_STATE_reset_mask(cpu)           (cpu)->reset_mask
#define CPU_STATE_group_swap(cpu)           (cpu)->group_swap

/*
 * For storing data for --read/--write-msr command line options
 */
typedef struct MSR_DATA_NODE_S MSR_DATA_NODE;
typedef        MSR_DATA_NODE  *MSR_DATA;
struct MSR_DATA_NODE_S {
    U64         value;             // Used for emon, for read/write-msr value
    U64         addr;
};

#define MSR_DATA_value(md)   (md)->value
#define MSR_DATA_addr(md)    (md)->addr

/*
 * Memory Allocation tracker
 *
 * Currently used to track large memory allocations
 */

typedef struct MEM_EL_NODE_S  MEM_EL_NODE;
typedef        MEM_EL_NODE   *MEM_EL;
struct MEM_EL_NODE_S {
    char     *address;         // pointer to piece of memory we're tracking
    S32       size;            // size (bytes) of the piece of memory
    DRV_BOOL  is_addr_vmalloc; // flag to check if the memory is allocated using vmalloc
};

// accessors for MEM_EL defined in terms of MEM_TRACKER below

#define MEM_EL_MAX_ARRAY_SIZE  32   // minimum is 1, nominal is 64

typedef struct MEM_TRACKER_NODE_S  MEM_TRACKER_NODE;
typedef        MEM_TRACKER_NODE   *MEM_TRACKER;
struct MEM_TRACKER_NODE_S {
    S32         max_size;     // number of elements in the array (default: MEM_EL_MAX_ARRAY_SIZE)
    MEM_EL      mem;          // array of large memory items we're tracking
    MEM_TRACKER prev,next;    // enables bi-directional scanning of linked list
};
#define MEM_TRACKER_max_size(mt)         (mt)->max_size
#define MEM_TRACKER_mem(mt)              (mt)->mem
#define MEM_TRACKER_prev(mt)             (mt)->prev
#define MEM_TRACKER_next(mt)             (mt)->next
#define MEM_TRACKER_mem_address(mt, i)   (MEM_TRACKER_mem(mt)[(i)].address)
#define MEM_TRACKER_mem_size(mt, i)      (MEM_TRACKER_mem(mt)[(i)].size)
#define MEM_TRACKER_mem_vmalloc(mt, i)   (MEM_TRACKER_mem(mt)[(i)].is_addr_vmalloc)

/****************************************************************************
 ** Global State variables exported
 ***************************************************************************/
extern   CPU_STATE            pcb;
extern   U64                 *tsc_info;
extern   GLOBAL_STATE_NODE    driver_state;
extern   MSR_DATA             msr_data;
extern   U32                 *core_to_package_map;
extern   U32                  num_packages;
extern   U64                 *restore_bl_bypass;
extern   U32                 **restore_ha_direct2core;
extern   U32                 **restore_qpi_direct2core;
/****************************************************************************
 **  Handy Short cuts
 ***************************************************************************/

/*
 * CONTROL_THIS_CPU()
 *     Parameters
 *         None
 *     Returns
 *         CPU number of the processor being executed on
 *
 */
#define CONTROL_THIS_CPU()     smp_processor_id()

/****************************************************************************
 **  Interface definitions
 ***************************************************************************/

/*
 *  Execution Control Functions
 */

extern VOID
CONTROL_Invoke_Cpu (
    S32   cpuid,
    VOID  (*func)(PVOID),
    PVOID ctx
);

/*
 * @fn VOID CONTROL_Invoke_Parallel_Service(func, ctx, blocking, exclude)
 *
 * @param    func     - function to be invoked by each core in the system
 * @param    ctx      - pointer to the parameter block for each function invocation
 * @param    blocking - Wait for invoked function to complete
 * @param    exclude  - exclude the current core from executing the code
 *
 * @returns  none
 *
 * @brief    Service routine to handle all kinds of parallel invoke on all CPU calls
 *
 * <I>Special Notes:</I>
 *         Invoke the function provided in parallel in either a blocking/non-blocking mode.
 *         The current core may be excluded if desired.
 *         NOTE - Do not call this function directly from source code.  Use the aliases
 *         CONTROL_Invoke_Parallel(), CONTROL_Invoke_Parallel_NB(), CONTROL_Invoke_Parallel_XS().
 *
 */
extern VOID
CONTROL_Invoke_Parallel_Service (
        VOID   (*func)(PVOID),
        PVOID  ctx,
        S32    blocking,
        S32    exclude
);

/*
 * @fn VOID CONTROL_Invoke_Parallel(func, ctx)
 *
 * @param    func     - function to be invoked by each core in the system
 * @param    ctx      - pointer to the parameter block for each function invocation
 *
 * @returns  none
 *
 * @brief    Invoke the named function in parallel. Wait for all the functions to complete.
 *
 * <I>Special Notes:</I>
 *        Invoke the function named in parallel, including the CPU that the control is
 *        being invoked on
 *        Macro built on the service routine
 *
 */
#define CONTROL_Invoke_Parallel(a,b)      CONTROL_Invoke_Parallel_Service((a),(b),TRUE,FALSE)

/*
 * @fn VOID CONTROL_Invoke_Parallel_NB(func, ctx)
 *
 * @param    func     - function to be invoked by each core in the system
 * @param    ctx      - pointer to the parameter block for each function invocation
 *
 * @returns  none
 *
 * @brief    Invoke the named function in parallel. DO NOT Wait for all the functions to complete.
 *
 * <I>Special Notes:</I>
 *        Invoke the function named in parallel, including the CPU that the control is
 *        being invoked on
 *        Macro built on the service routine
 *
 */
#define CONTROL_Invoke_Parallel_NB(a,b)   CONTROL_Invoke_Parallel_Service((a),(b),FALSE,FALSE)

/*
 * @fn VOID CONTROL_Invoke_Parallel_XS(func, ctx)
 *
 * @param    func     - function to be invoked by each core in the system
 * @param    ctx      - pointer to the parameter block for each function invocation
 *
 * @returns  none
 *
 * @brief    Invoke the named function in parallel. Wait for all the functions to complete.
 *
 * <I>Special Notes:</I>
 *        Invoke the function named in parallel, excluding the CPU that the control is
 *        being invoked on
 *        Macro built on the service routine
 *
 */
#define CONTROL_Invoke_Parallel_XS(a,b)   CONTROL_Invoke_Parallel_Service((a),(b),TRUE,TRUE)


/*
 * @fn VOID CONTROL_Memory_Tracker_Init(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Initializes Memory Tracker
 *
 * <I>Special Notes:</I>
 *           This should only be called when the
 *           the driver is being loaded.
 */
extern VOID
CONTROL_Memory_Tracker_Init (
    VOID
);

/*
 * @fn VOID CONTROL_Memory_Tracker_Free(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Frees memory used by Memory Tracker
 *
 * <I>Special Notes:</I>
 *           This should only be called when the
 *           driver is being unloaded.
 */
extern VOID
CONTROL_Memory_Tracker_Free (
    VOID
);

/*
 * @fn VOID CONTROL_Memory_Tracker_Compaction(void)
 *
 * @param    None
 *
 * @returns  None
 *
 * @brief    Compacts the memory allocator if holes are detected
 *
 * <I>Special Notes:</I>
 *           At end of collection (or at other safe sync point), 
 *           reclaim/compact space used by mem tracker
 */
extern VOID
CONTROL_Memory_Tracker_Compaction (
    void
);

/*
 * @fn PVOID CONTROL_Allocate_Memory(size)
 *
 * @param    IN size     - size of the memory to allocate
 *
 * @returns  char*       - pointer to the allocated memory block
 *
 * @brief    Allocate and zero memory
 *
 * <I>Special Notes:</I>
 *           Allocate memory in the GFP_KERNEL pool.
 *
 *           Use this if memory is to be allocated within a context where
 *           the allocator can block the allocation (e.g., by putting
 *           the caller to sleep) while it tries to free up memory to
 *           satisfy the request.  Otherwise, if the allocation must
 *           occur atomically (e.g., caller cannot sleep), then use
 *           CONTROL_Allocate_KMemory instead.
 */
extern PVOID
CONTROL_Allocate_Memory (
    size_t    size
);

/*
 * @fn PVOID CONTROL_Allocate_KMemory(size)
 *
 * @param    IN size     - size of the memory to allocate
 *
 * @returns  char*       - pointer to the allocated memory block
 *
 * @brief    Allocate and zero memory
 *
 * <I>Special Notes:</I>
 *           Allocate memory in the GFP_ATOMIC pool.
 *
 *           Use this if memory is to be allocated within a context where
 *           the allocator cannot block the allocation (e.g., by putting
 *           the caller to sleep) as it tries to free up memory to
 *           satisfy the request.  Examples include interrupt handlers,
 *           process context code holding locks, etc.
 */
extern PVOID
CONTROL_Allocate_KMemory (
    size_t  size
);

/*
 * @fn PVOID CONTROL_Free_Memory(location)
 *
 * @param    IN location  - size of the memory to allocate
 *
 * @returns  pointer to the allocated memory block
 *
 * @brief    Frees the memory block
 *
 * <I>Special Notes:</I>
 *           Does not try to free memory if fed with a NULL pointer
 *           Expected usage:
 *               ptr = CONTROL_Free_Memory(ptr);
 */
extern PVOID
CONTROL_Free_Memory (
    PVOID    location
);

#endif  
