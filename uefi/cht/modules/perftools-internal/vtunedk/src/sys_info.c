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

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv.h"
#include "control.h"
#include "utility.h"
#include "apic.h"
#include "sys_info.h"

#define VTSA_CPUID VTSA_CPUID_X86

extern U64              total_ram;
static IOCTL_SYS_INFO  *ioctl_sys_info      = NULL;
static size_t           ioctl_sys_info_size = 0;
static U32             *cpuid_entry_count   = NULL;
static U32             *cpuid_total_count   = NULL;

#define VTSA_NA64       ((U64) -1)
#define VTSA_NA32       ((U32) -1)
#define VTSA_NA         ((U32) -1)

#define SYS_INFO_NUM_SETS(rcx)             ((rcx) + 1)
#define SYS_INFO_LINE_SIZE(rbx)            (((rbx) & 0xfff) + 1)
#define SYS_INFO_LINE_PARTITIONS(rbx)      ((((rbx) >> 12) & 0x3ff) + 1)
#define SYS_INFO_NUM_WAYS(rbx)             ((((rbx) >> 22) & 0x3ff) + 1)

#define SYS_INFO_CACHE_SIZE(rcx,rbx) (SYS_INFO_NUM_SETS((rcx))        *    \
                                      SYS_INFO_LINE_SIZE((rbx))       *    \
                                      SYS_INFO_LINE_PARTITIONS((rbx)) *    \
                                      SYS_INFO_NUM_WAYS((rbx)))

#define MSR_FB_PCARD_ID_FUSE   0x17     // platform id fuses MSR
#define LOW_PART(x)     (x & 0xFFFFFFFF)

/* Find most signicant bit set to 1? */
static U64
sys_info_nbits (
    U64    number
)
{
    U64 i;

    for (i = 0; number > 0; i++) {
        number >>= 1;
    }
    //
    // adjust to 0 based number so we return 1 bit for value of 2
    //
    return (i-1);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void sys_info_Get_Num_Cpuid_Funcs(basic_funcs, basic_4_funcs, extended_funcs)
 *
 * @param basic_functions    - pointer to the number of basic functions
 * @param basic_4_funcs      - pointer to the basic 4 functions
 * @param extended_funcs     - pointer to the number of extended functions
 * @return total number of cpuid functions
 *
 * @brief  This routine gets the number of basic and extended cpuid functions.
 *
 */
static U32
sys_info_Get_Num_Cpuid_Funcs (
    OUT U32 *basic_funcs,
    OUT U32 *basic_4_funcs,
    OUT U32 *extended_funcs
)
{
    U64 num_basic_funcs      = 0x0LL;
    U64 num_basic_4_funcs    = 0x0LL;
    U64 num_extended_funcs   = 0x0LL;
    U64 rax;
    U64 rbx;
    U64 rcx;
    U64 rdx;
    U64 i;

    UTILITY_Read_Cpuid(0, &num_basic_funcs, &rbx, &rcx, &rdx);
    UTILITY_Read_Cpuid(0x80000000, &num_extended_funcs, &rbx, &rcx, &rdx);

    if (num_extended_funcs & 0x80000000) {
        num_extended_funcs -= 0x80000000;
    }

    //
    // make sure num_extended_funcs is not bogus
    //
    if (num_extended_funcs > 0x1000) {
        num_extended_funcs = 0;
    }

    //
    // if number of basic funcs is greater than 4, figure out how many
    // time we should call CPUID with eax = 0x4.
    //
    num_basic_4_funcs = 0;
    if (num_basic_funcs >= 4) {
        for (i = 0, rax = (U64)-1; (rax & 0x1f) != 0; i++) {
            rcx = i;
            UTILITY_Read_Cpuid(4, &rax, &rbx, &rcx, &rdx);
        }
        num_basic_4_funcs = i - 1;
    }
    if (num_basic_funcs >= 0xb) {
        i = 0;
        do {
            rcx = i;
            UTILITY_Read_Cpuid(0xb, &rax, &rbx, &rcx, &rdx);
            i++;
        } while (!(LOW_PART(rax) == 0 && LOW_PART(rbx) == 0));
        num_basic_4_funcs += i;
    }
    SEP_PRINT_DEBUG("sys_info_Get_Num_Cpuid_Funcs: num_basic_4_funcs = %llx\n",
                num_basic_4_funcs);

    //
    // adjust number to include 0 and 0x80000000 functions.
    //
    num_basic_funcs++;
    num_extended_funcs++;

    SEP_PRINT_DEBUG("sys_info_Get_Num_Cpuid_Funcs: num_basic_funcs = %llx\n", num_basic_funcs);
    SEP_PRINT_DEBUG("sys_info_Get_Num_Cpuid_Funcs: num_extended_funcs = %llx\n", num_extended_funcs);

    //
    // fill-in the parameter for the caller
    //
    if (basic_funcs != NULL) {
        *basic_funcs = (U32) num_basic_funcs;
    }
    if (basic_4_funcs != NULL) {
        *basic_4_funcs = (U32) num_basic_4_funcs;
    }
    if (extended_funcs != NULL) {
        *extended_funcs = (U32) num_extended_funcs;
    }

    return ((U32) (num_basic_funcs + num_basic_4_funcs + num_extended_funcs));
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void sys_info_Get_Cpuid_Entry_Cpunt(buffer)
 *
 * @param  buffer    - pointer to the buffer to hold the info
 * @return None
 *
 * @brief  Service Routine to query the CPU for the number of entries needed
 *
 */
static VOID
sys_info_Get_Cpuid_Entry_Count (
    PVOID    buffer
)
{
    U32 current_processor;
    U32 *current_cpu_buffer;

    current_processor = CONTROL_THIS_CPU();
    SEP_PRINT_DEBUG("tbs:sys_info_Get_Cpuid_Entry_Count:%x: begin\n", current_processor);

    current_cpu_buffer = (U32 *) ((U8 *) buffer + current_processor * sizeof(U32));

#if defined(ALLOW_ASSERT)
    ASSERT(((U8 *) current_cpu_buffer + sizeof(U32)) <=
           ((U8 *) current_cpu_buffer + GLOBAL_STATE_active_cpus(driver_state) * sizeof(U32)));
#endif
    *current_cpu_buffer = sys_info_Get_Num_Cpuid_Funcs(NULL, NULL, NULL);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static U32 sys_info_Get_Cpuid_Buffer_Size(cpuid_entries)
 *
 * @param    cpuid_entries   - number of cpuid entries
 * @return   size of buffer needed in bytes
 *
 * @brief  This routine returns number of bytes needed to hold the CPU_CS_INFO
 * @brief  structure.
 *
 */
static U32
sys_info_Get_Cpuid_Buffer_Size (
    U32 cpuid_entries
)
{
    U32  cpuid_size;
    U32  buffer_size;

    cpuid_size = sizeof(VTSA_CPUID);

    buffer_size = sizeof(IOCTL_SYS_INFO) +
                  sizeof(VTSA_GEN_ARRAY_HDR) +
                  sizeof(VTSA_NODE_INFO) +
                  sizeof(VTSA_GEN_ARRAY_HDR) +
                  GLOBAL_STATE_active_cpus(driver_state) * sizeof(VTSA_GEN_PER_CPU) +
                  GLOBAL_STATE_active_cpus(driver_state) * sizeof(VTSA_GEN_ARRAY_HDR) +
                  cpuid_entries * cpuid_size;

    return buffer_size;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void sys_info_Fill_CPUID(...)
 *
 * @param        num_cpuids,
 * @param        basic_funcs,
 * @param        extended_funcs,
 * @param        cpu,
 * @param       *current_cpuid
 * @param       *gen_per_cpu,
 * @param       *local_gpc
 *
 * @return   None
 *
 * @brief  This routine is called to build per cpu information.
 * @brief  Fills in the cpuid for the processor in the right location in the buffer
 *
 */
static void
sys_info_Fill_CPUID (
    U32                  num_cpuids,
    U32                  basic_funcs,
    U32                  extended_funcs,
    U32                  cpu,
    VTSA_CPUID          *current_cpuid,
    VTSA_GEN_PER_CPU    *gen_per_cpu,
    VTSA_GEN_PER_CPU    *local_gpc
)
{
    U32                  i, index, j;
    U64                  cpuid_function;
    U64                  rax, rbx, rcx, rdx;
    VTSA_CPUID          *cpuid_el;
    char                *apic;
    DRV_BOOL             ht_supported             = FALSE;
    U32                  apic_id                  = 0;
    U32                  num_logical_per_physical = 0;
    U32                  cores_per_die            = 1;
    U32                  threads_per_core         = 1;
    U32                  thread_id                = 0;
    U32                  core_id                  = 0;
    U32                  package_id               = 0;
    U32                  module_id                = 0;
    U32                  cores_sharing_cache      = 0;
    U32                  cache_mask_width         = 0;

    if (drv_x2apic_enabled) {
        apic_id    = SYS_Read_MSR(DRV_APIC_LCL_ID_MSR);
    }
    else {
        apic       = (char*) CPU_STATE_apic_linear_addr(&pcb[cpu]);
        apic_id    = (*(U32*)&apic[DRV_APIC_LCL_ID]) >> 24;
    }
    SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpu %x: apic_id = %d\n", cpu, apic_id);

    for (i = 0, index = 0; index < num_cpuids; i++) {
        cpuid_function = (i < basic_funcs) ? i : (0x80000000 + i - basic_funcs);

        if (cpuid_function == 0x4) {
            for (j = 0, rax = (U64)-1; (rax & 0x1f) != 0; j++) {
                rcx = j;
                UTILITY_Read_Cpuid(cpuid_function, &rax, &rbx, &rcx, &rdx);
                cpuid_el = &current_cpuid[index];
                index++;

#if defined(ALLOW_ASSERT)
                ASSERT(((U8 *)cpuid_el + sizeof(VTSA_CPUID)) <= cpuid_buffer_limit);
#endif

                VTSA_CPUID_X86_cpuid_eax_input(cpuid_el) = (U32) cpuid_function;
                VTSA_CPUID_X86_cpuid_eax(cpuid_el)       = (U32) rax;
                VTSA_CPUID_X86_cpuid_ebx(cpuid_el)       = (U32) rbx;
                VTSA_CPUID_X86_cpuid_ecx(cpuid_el)       = (U32) rcx;
                VTSA_CPUID_X86_cpuid_edx(cpuid_el)       = (U32) rdx;
                SEP_PRINT_DEBUG("CPUID: Function: %x\n", (U32)cpuid_function);
                SEP_PRINT_DEBUG("CPUID: \trax: %x, rbx: %x, rcx: %x, rdx: %x\n",
                                (U32)rax, (U32)rbx, (U32)rcx, (U32)rdx);

                if ((rax & 0x1f) != 0) {
                    local_gpc = &gen_per_cpu[cpu];
                    if (((rax >> 5) & 0x3) == 2) {
                        VTSA_GEN_PER_CPU_cpu_cache_L2(local_gpc) =
                                   (U32)(SYS_INFO_CACHE_SIZE(rcx,rbx) >> 10);
                        SEP_PRINT_DEBUG("L2 Cache: %x\n", VTSA_GEN_PER_CPU_cpu_cache_L2(local_gpc));
                        cores_sharing_cache = ((U16)(rax >> 14) & 0xfff) + 1;
                        SEP_PRINT_DEBUG("CORES_SHARING_CACHE=%d j=%d cpu=%d\n", cores_sharing_cache, j, cpu);
                    }

                    if (((rax >> 5) & 0x3) == 3) {
                        VTSA_GEN_PER_CPU_cpu_cache_L3(local_gpc) =
                                    (U32)(SYS_INFO_CACHE_SIZE(rcx,rbx) >> 10);
                        SEP_PRINT_DEBUG("L3 Cache: %x\n", VTSA_GEN_PER_CPU_cpu_cache_L3(local_gpc));
                    }
                }
                if (j == 0) {
                    cores_per_die = ((U16)(rax >> 26) & 0x3f) + 1;
                }
            }
            if (cores_sharing_cache != 0) {
                cache_mask_width = (U32)sys_info_nbits(cores_sharing_cache);
                SEP_PRINT_DEBUG("CACHE MASK WIDTH=%x\n", cache_mask_width);
            }
        }
        else if (cpuid_function == 0xb) {
            j = 0;
            do {
                rcx = j;
                UTILITY_Read_Cpuid(cpuid_function, &rax, &rbx, &rcx, &rdx);
                cpuid_el = &current_cpuid[index];
                index++;

#if defined(ALLOW_ASSERT)
                ASSERT(((U8 *)cpuid_el + sizeof(VTSA_CPUID_X86)) <= cpuid_buffer_limit);
#endif

                VTSA_CPUID_X86_cpuid_eax_input(cpuid_el) = (U32) cpuid_function;
                VTSA_CPUID_X86_cpuid_eax(cpuid_el)       = (U32) rax;
                VTSA_CPUID_X86_cpuid_ebx(cpuid_el)       = (U32) rbx;
                VTSA_CPUID_X86_cpuid_ecx(cpuid_el)       = (U32) rcx;
                VTSA_CPUID_X86_cpuid_edx(cpuid_el)       = (U32) rdx;
                SEP_PRINT_DEBUG("CPUID: Function: %x\n", (U32)cpuid_function);
                SEP_PRINT_DEBUG("CPUID: \trax: %x, rbx: %x, rcx: %x, rdx: %x\n",
                                (U32)rax, (U32)rbx, (U32)rcx, (U32)rdx);
                j++;
            } while (!(LOW_PART(rax) == 0 && LOW_PART(rbx) == 0));
        }
        else {
            UTILITY_Read_Cpuid(cpuid_function, &rax, &rbx, &rcx, &rdx);
            cpuid_el = &current_cpuid[index];
            index++;

            SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpu %x: num_cpuids = %x i = %x index = %x\n",
                            cpu, num_cpuids, i, index);

#if defined(ALLOW_ASSERT)
            ASSERT(((U8 *)cpuid_el + sizeof(VTSA_CPUID_X86)) <= cpuid_buffer_limit);

            ASSERT(((U8 *)cpuid_el + sizeof(VTSA_CPUID_X86)) <=
                   ((U8 *)current_cpuid + (num_cpuids * sizeof(VTSA_CPUID_X86))));
#endif

            VTSA_CPUID_X86_cpuid_eax_input(cpuid_el) = (U32) cpuid_function;
            VTSA_CPUID_X86_cpuid_eax(cpuid_el)       = (U32) rax;
            VTSA_CPUID_X86_cpuid_ebx(cpuid_el)       = (U32) rbx;
            VTSA_CPUID_X86_cpuid_ecx(cpuid_el)       = (U32) rcx;
            VTSA_CPUID_X86_cpuid_edx(cpuid_el)       = (U32) rdx;
            SEP_PRINT_DEBUG("CPUID: Function: %x\n", (U32)cpuid_function);
            SEP_PRINT_DEBUG("CPUID: \trax: %x, rbx: %x, rcx: %x, rdx: %x\n",
                            (U32)rax, (U32)rbx, (U32)rcx, (U32)rdx);

            if (cpuid_function == 0) {
                if ((U32)rbx == 0x756e6547  &&
                    (U32)rcx == 0x6c65746e  &&
                    (U32)rdx == 0x49656e69) {
                    VTSA_GEN_PER_CPU_platform_id(local_gpc) = SYS_Read_MSR(MSR_FB_PCARD_ID_FUSE);
                }
            }
            else if (cpuid_function == 1) {
                ht_supported             = (rdx >> 28) & 1 ? TRUE : FALSE;
                num_logical_per_physical = (U32)((rbx & 0xff0000) >> 16);
                if (num_logical_per_physical == 0) {
                    num_logical_per_physical = 1;
                }
            }
        }
    }

    // set cpu_cache_L2 if not already set using 0x80000006 function
    if (gen_per_cpu[cpu].cpu_cache_L2 == VTSA_NA && extended_funcs >= 6) {

        UTILITY_Read_Cpuid(0x80000006, &rax, &rbx, &rcx, &rdx);
        VTSA_GEN_PER_CPU_cpu_cache_L2(local_gpc) = (U32)(rcx >> 16);
    }

    if (!ht_supported || num_logical_per_physical == cores_per_die) {
        threads_per_core = 1;
        thread_id        = 0;
    }
    else {
        // assume each core only has 2 threads when ht is enabled
        threads_per_core = 2;
        thread_id        = (U16)(apic_id & 1);
    }

    core_id    = (apic_id >> sys_info_nbits(threads_per_core)) & (cores_per_die-1);
    package_id = (apic_id >> (sys_info_nbits(cores_per_die) + sys_info_nbits(threads_per_core)));
     if (cache_mask_width) {
        module_id = (U32)(cpu/2);
    }
    SEP_PRINT_DEBUG("MODULE ID=%d CORE ID=%d for cpu=%d PACKAGE ID=%d\n", module_id, core_id, cpu, package_id);
    SEP_PRINT_DEBUG("num_logical_per_physical=%d cores_per_die=%d\n", num_logical_per_physical, cores_per_die);
    SEP_PRINT_DEBUG("package_id %d, apic_id %x, sys_info_nbits(cores_per_die) %lld, sys_info_nbits(threads_per_core %lld\n", package_id, apic_id, sys_info_nbits(cores_per_die), sys_info_nbits(threads_per_core));

    VTSA_GEN_PER_CPU_cpu_intel_processor_number(local_gpc) = VTSA_NA32;
    VTSA_GEN_PER_CPU_cpu_package_num(local_gpc)            = (U16)package_id;
    VTSA_GEN_PER_CPU_cpu_core_num(local_gpc)               = (U16)core_id;
    VTSA_GEN_PER_CPU_cpu_hw_thread_num(local_gpc)          = (U16)thread_id;
    VTSA_GEN_PER_CPU_cpu_threads_per_core(local_gpc)       = (U16)threads_per_core;
    VTSA_GEN_PER_CPU_cpu_module_num(local_gpc)             = (U16)module_id;
    VTSA_GEN_PER_CPU_cpu_num_modules(local_gpc)            = (U16)(GLOBAL_STATE_num_cpus(driver_state)/2);
    GLOBAL_STATE_num_modules(driver_state)                 = VTSA_GEN_PER_CPU_cpu_num_modules(local_gpc);
    SEP_PRINT_DEBUG("MODULE COUNT=%d\n", GLOBAL_STATE_num_modules(driver_state));

    core_to_package_map[cpu] = package_id;

    if (num_packages < package_id + 1) {
        num_packages = package_id + 1;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn static void sys_info_Build_Percpu(buffer)
 *
 * @param    buffer  -  points to the base of GEN_PER_CPU structure
 * @return   None
 *
 * @brief  This routine is called to build per cpu information.
 *
 */
static VOID
sys_info_Build_Percpu (
    VOID    *buffer
)
{
    U32                  basic_funcs, basic_4_funcs, extended_funcs;
    U32                  num_cpuids;
    U32                  cpu;
    VTSA_CPUID          *current_cpuid;
    VTSA_GEN_ARRAY_HDR  *cpuid_gen_array_hdr;
    VTSA_GEN_PER_CPU    *gen_per_cpu, *local_gpc;
    VTSA_FIXED_SIZE_PTR *fsp;
    U8                  *cpuid_gen_array_hdr_base;
#if defined(ALLOW_ASSERT)
    U8                  *cpuid_buffer_limit;
#endif

    cpu        = CONTROL_THIS_CPU();
    num_cpuids = (U32) sys_info_Get_Num_Cpuid_Funcs(&basic_funcs,
                                                    &basic_4_funcs,
                                                    &extended_funcs);

    // get the GEN_PER_CPU entry for the current processor.
    gen_per_cpu = (VTSA_GEN_PER_CPU*) buffer;
    SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpu %x: gen_per_cpu = %p\n", cpu, gen_per_cpu);

    // get GEN_ARRAY_HDR and cpuid array base
    cpuid_gen_array_hdr_base = (U8 *) gen_per_cpu +
                               GLOBAL_STATE_active_cpus(driver_state) * sizeof(VTSA_GEN_PER_CPU);

    SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpuid_gen_array_hdr_base = %p\n", cpuid_gen_array_hdr_base);
    SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpu = %x\n", cpu);
    SEP_PRINT_DEBUG("sys_info_Build_Percpu: cpuid_total_count[cpu] = %x\n",   cpuid_total_count[cpu]);
    SEP_PRINT_DEBUG("sys_info_Build_Percpu: sizeof(VTSA_CPUID) = %lx\n",   sizeof(VTSA_CPUID));

    cpuid_gen_array_hdr = (VTSA_GEN_ARRAY_HDR *) ((U8 *) cpuid_gen_array_hdr_base  +
                                                  sizeof(VTSA_GEN_ARRAY_HDR) * cpu +
                                                  cpuid_total_count[cpu] * sizeof(VTSA_CPUID));

    // get current cpuid array base.
    current_cpuid = (VTSA_CPUID *) ((U8 *) cpuid_gen_array_hdr + sizeof(VTSA_GEN_ARRAY_HDR));
#if defined(ALLOW_ASSERT)
    // get the absolute buffer limit
    cpuid_buffer_limit = (U8 *)ioctl_sys_info +
                              GENERIC_IOCTL_size(&IOCTL_SYS_INFO_gen(ioctl_sys_info));
#endif

    //
    // Fill in GEN_PER_CPU
    //
    local_gpc                                   = &(gen_per_cpu[cpu]);
    VTSA_GEN_PER_CPU_cpu_number(local_gpc)      = cpu;
    VTSA_GEN_PER_CPU_cpu_speed_mhz(local_gpc)   = VTSA_NA32;
    VTSA_GEN_PER_CPU_cpu_fsb_mhz(local_gpc)     = VTSA_NA32;

    fsp                                        = &VTSA_GEN_PER_CPU_cpu_cpuid_array(local_gpc);
    VTSA_FIXED_SIZE_PTR_is_ptr(fsp)            = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(fsp)         = (U64) ((U8 *)cpuid_gen_array_hdr -
                                                 (U8 *)&IOCTL_SYS_INFO_sys_info(ioctl_sys_info));

    /*
     * Get the time stamp difference between this cpu and cpu 0.
     * This value will be used by user mode code to generate standardize
     * time needed for sampling over time (SOT) functionality.
     */
    VTSA_GEN_PER_CPU_cpu_tsc_offset(local_gpc)  =  TSC_SKEW(cpu);


    //
    // fill GEN_ARRAY_HDR
    //
    fsp  = &VTSA_GEN_ARRAY_HDR_hdr_next_gen_hdr(cpuid_gen_array_hdr);
    VTSA_GEN_ARRAY_HDR_hdr_size(cpuid_gen_array_hdr)          = sizeof(VTSA_GEN_ARRAY_HDR);
    VTSA_FIXED_SIZE_PTR_is_ptr(fsp)                           = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(fsp)                        = 0;
    VTSA_GEN_ARRAY_HDR_array_num_entries(cpuid_gen_array_hdr) = num_cpuids;
    VTSA_GEN_ARRAY_HDR_array_entry_size(cpuid_gen_array_hdr)  = sizeof(VTSA_CPUID);
    VTSA_GEN_ARRAY_HDR_array_type(cpuid_gen_array_hdr)        = GT_CPUID;
#if defined(DRV_IA32)
    VTSA_GEN_ARRAY_HDR_array_subtype(cpuid_gen_array_hdr)     = GST_X86;
#elif defined(DRV_EM64T)
    VTSA_GEN_ARRAY_HDR_array_subtype(cpuid_gen_array_hdr)     = GST_EM64T;
#endif

    //
    // fill out cpu id information
    //
    sys_info_Fill_CPUID (num_cpuids,
                         basic_funcs,
                         extended_funcs,
                         cpu,
                         current_cpuid,
                         gen_per_cpu,
                         local_gpc);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void SYS_Info_Build(void)
 *
 * @param    None
 * @return   None
 *
 * @brief  This is the driver routine that constructs the VTSA_SYS_INFO
 * @brief  structure used to report system information into the tb5 file
 *
 */
extern U32
SYS_INFO_Build (
    VOID
)
{
    VTSA_GEN_ARRAY_HDR  *gen_array_hdr;
    VTSA_NODE_INFO      *node_info;
    VTSA_SYS_INFO       *sys_info;
    VTSA_FIXED_SIZE_PTR *fsp;
    U8                  *gen_per_cpu;
    U32                  buffer_size;
    U32                  total_cpuid_entries;
    S32                  i;
    struct sysinfo       k_sysinfo;
    int                  me;
    PVOID                linear;

    SEP_PRINT_DEBUG("SYS_INFO_Build(): Entered\n");

    if (ioctl_sys_info) {
        /* The sys info has already been computed.  Do not redo */
        buffer_size = GENERIC_IOCTL_size(&IOCTL_SYS_INFO_gen(ioctl_sys_info));
        return buffer_size - sizeof(GENERIC_IOCTL);
    }

    si_meminfo(&k_sysinfo);

    buffer_size = GLOBAL_STATE_active_cpus(driver_state) * sizeof(U32);
    cpuid_entry_count = CONTROL_Allocate_Memory(buffer_size);
    if (cpuid_entry_count == NULL) {
        SEP_PRINT_ERROR("SYS_INFO_Build: memory alloc failed\n");
        return 0;
    }

    cpuid_total_count = CONTROL_Allocate_Memory(buffer_size);
    if (cpuid_total_count == NULL) {
        SEP_PRINT_ERROR("SYS_INFO_Build: memory alloc failed\n");
        cpuid_entry_count = CONTROL_Free_Memory(cpuid_entry_count);
        return 0;
    }

    CONTROL_Invoke_Parallel(sys_info_Get_Cpuid_Entry_Count, (VOID *)cpuid_entry_count);

    total_cpuid_entries = 0;
    for(i = 0; i < GLOBAL_STATE_active_cpus(driver_state); i++) {
         cpuid_total_count[i]  = total_cpuid_entries;
         total_cpuid_entries  += cpuid_entry_count[i];
    }

    ioctl_sys_info_size = sys_info_Get_Cpuid_Buffer_Size(total_cpuid_entries);
    ioctl_sys_info      = CONTROL_Allocate_Memory(ioctl_sys_info_size);
    if (ioctl_sys_info == NULL) {
        SEP_PRINT_ERROR("SYS_INFO_Build: memory alloc failed\n");
        cpuid_entry_count = CONTROL_Free_Memory(cpuid_entry_count);
        cpuid_total_count = CONTROL_Free_Memory(cpuid_total_count);

//        return STATUS_INSUFFICIENT_RESOURCES;
        return 0;
    }

    //
    // fill in ioctl and cpu_cs_info fields.
    //
    GENERIC_IOCTL_size(&IOCTL_SYS_INFO_gen(ioctl_sys_info)) = ioctl_sys_info_size;
    GENERIC_IOCTL_ret(&IOCTL_SYS_INFO_gen(ioctl_sys_info))  = VT_SUCCESS;

    sys_info = &IOCTL_SYS_INFO_sys_info(ioctl_sys_info);
    VTSA_SYS_INFO_min_app_address(sys_info)        = VTSA_NA64;
    VTSA_SYS_INFO_max_app_address(sys_info)        = VTSA_NA64;
    VTSA_SYS_INFO_page_size(sys_info)              = k_sysinfo.mem_unit;
    VTSA_SYS_INFO_allocation_granularity(sys_info) = k_sysinfo.mem_unit;

    //
    // offset from ioctl_sys_info
    //
    VTSA_FIXED_SIZE_PTR_is_ptr(&VTSA_SYS_INFO_node_array(sys_info))    = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(&VTSA_SYS_INFO_node_array(sys_info)) = sizeof(VTSA_SYS_INFO);

    //
    // fill in node_info array header
    //
    gen_array_hdr = (VTSA_GEN_ARRAY_HDR *) ((U8 *) sys_info +
                     VTSA_FIXED_SIZE_PTR_fs_offset(&VTSA_SYS_INFO_node_array(sys_info)));

    SEP_PRINT_DEBUG("SYS_INFO_Build: gen_array_hdr = %p\n", gen_array_hdr);
    fsp = &VTSA_GEN_ARRAY_HDR_hdr_next_gen_hdr(gen_array_hdr);
    VTSA_FIXED_SIZE_PTR_is_ptr(fsp)                     = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(fsp)                  = 0;

    VTSA_GEN_ARRAY_HDR_hdr_size(gen_array_hdr)          = sizeof(VTSA_GEN_ARRAY_HDR);
    VTSA_GEN_ARRAY_HDR_array_num_entries(gen_array_hdr) = 1;
    VTSA_GEN_ARRAY_HDR_array_entry_size(gen_array_hdr)  = sizeof(VTSA_NODE_INFO);
    VTSA_GEN_ARRAY_HDR_array_type(gen_array_hdr)        = GT_NODE;
    VTSA_GEN_ARRAY_HDR_array_subtype(gen_array_hdr)     = GST_UNK;

    //
    // fill in node_info
    //
    node_info = (VTSA_NODE_INFO *) ((U8 *) gen_array_hdr + sizeof(VTSA_GEN_ARRAY_HDR));
    SEP_PRINT_DEBUG("SYS_INFO_Build: node_info = %p\n", node_info);

    VTSA_NODE_INFO_node_type_from_shell(node_info) = VTSA_NA32;

    VTSA_NODE_INFO_node_id(node_info)              = VTSA_NA32;
    VTSA_NODE_INFO_node_num_available(node_info)   = GLOBAL_STATE_active_cpus(driver_state);
    VTSA_NODE_INFO_node_num_used(node_info)        = VTSA_NA32;
    total_ram                                      = k_sysinfo.totalram << PAGE_SHIFT;
    VTSA_NODE_INFO_node_physical_memory(node_info) = total_ram;

    fsp = &VTSA_NODE_INFO_node_percpu_array(node_info);
    VTSA_FIXED_SIZE_PTR_is_ptr(fsp)      = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(fsp)   = sizeof(VTSA_SYS_INFO)      +
                                           sizeof(VTSA_GEN_ARRAY_HDR) +
                                           sizeof(VTSA_NODE_INFO);
    //
    // fill in gen_per_cpu array header
    //
    gen_array_hdr = (VTSA_GEN_ARRAY_HDR *) ((U8 *) sys_info + VTSA_FIXED_SIZE_PTR_fs_offset(fsp));
    SEP_PRINT_DEBUG("SYS_INFO_Build: gen_array_hdr = %p\n", gen_array_hdr);

    fsp = &VTSA_GEN_ARRAY_HDR_hdr_next_gen_hdr(gen_array_hdr);
    VTSA_FIXED_SIZE_PTR_is_ptr(fsp)                     = 0;
    VTSA_FIXED_SIZE_PTR_fs_offset(fsp)                  = 0;

    VTSA_GEN_ARRAY_HDR_hdr_size(gen_array_hdr)          = sizeof(VTSA_GEN_ARRAY_HDR);
    VTSA_GEN_ARRAY_HDR_array_num_entries(gen_array_hdr) = GLOBAL_STATE_active_cpus(driver_state);
    VTSA_GEN_ARRAY_HDR_array_entry_size(gen_array_hdr)  = sizeof(VTSA_GEN_PER_CPU);
    VTSA_GEN_ARRAY_HDR_array_type(gen_array_hdr)        = GT_PER_CPU;

#if defined(DRV_IA32)
    VTSA_GEN_ARRAY_HDR_array_subtype(gen_array_hdr)     = GST_X86;
#elif defined(DRV_EM64T)
    VTSA_GEN_ARRAY_HDR_array_subtype(gen_array_hdr)     = GST_EM64T;
#endif

    gen_per_cpu = (U8 *) gen_array_hdr + sizeof(VTSA_GEN_ARRAY_HDR);

    me     = 0;
    linear = NULL;
    APIC_Init(&linear);
    CONTROL_Invoke_Parallel(APIC_Init, &linear);
    CONTROL_Invoke_Parallel(sys_info_Build_Percpu, (VOID *)gen_per_cpu);
    APIC_Unmap(CPU_STATE_apic_linear_addr(&pcb[me]));
    // de-initialize APIC
    for(i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        APIC_Deinit_Phase1(i);
    }

    /*
     * Cleanup - deallocate memory that is no longer needed
     */
    cpuid_total_count = CONTROL_Free_Memory(cpuid_total_count);
    cpuid_entry_count = CONTROL_Free_Memory(cpuid_entry_count);

    return ioctl_sys_info_size - sizeof(GENERIC_IOCTL);
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void SYS_Info_Transfer(out_buf, out_buf_len)
 *
 * @param  out_buf      - pointer to the buffer to write the data into
 * @param  out_buf_len  - length of the buffer passed in
 *
 * @brief  Transfer the data collected via the SYS_INFO_Build routine
 * @brief  back to the caller.
 *
 */
extern VOID
SYS_INFO_Transfer (
    PVOID           out_buf,
    unsigned long   out_buf_len
)
{
    unsigned long exp_size;
    ssize_t       unused;

    if (ioctl_sys_info == NULL || out_buf_len == 0) {
        return;
    }
    exp_size = GENERIC_IOCTL_size(&IOCTL_SYS_INFO_gen(ioctl_sys_info)) - sizeof(GENERIC_IOCTL);
    if (out_buf_len < exp_size) {
        SEP_PRINT_ERROR("SYS_INFO_Transfer:  Insufficient Space\n");
        return;
    }
    unused = copy_to_user(out_buf, &(IOCTL_SYS_INFO_sys_info(ioctl_sys_info)), out_buf_len);
    if (unused) {
    // no-op ... eliminates "variable not used" compiler warning
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void SYS_Info_Destroy(void)
 *
 * @param    None
 * @return   None
 *
 * @brief  Free any memory associated with the sys info before unloading the driver
 *
 */
extern VOID
SYS_INFO_Destroy (
    void
)
{
    ioctl_sys_info      = CONTROL_Free_Memory(ioctl_sys_info);
    ioctl_sys_info_size = 0;

    return;
}
