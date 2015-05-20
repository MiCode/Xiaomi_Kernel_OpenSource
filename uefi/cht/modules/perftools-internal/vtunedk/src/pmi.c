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
#include <linux/kernel.h>
#include <linux/ptrace.h>
#if defined(DRV_EM64T)
#include <asm/desc.h>
#endif

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "apic.h"
#endif
#include "lwpmudrv.h"
#include "output.h"
#include "control.h"
#include "pmi.h"
#include "utility.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "pebs.h"
#endif
#include "lwpmudrv_chipset.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "sepdrv_p_state.h"
#endif

// Desc id #0 is used for module records
#define COMPUTE_DESC_ID(index)     ((index))

extern DRV_CONFIG     pcfg;
extern uid_t          uid;

#define EFLAGS_V86_MASK       0x00020000L

/*********************************************************************
 * Global Variables / State
 *********************************************************************/

/*********************************************************************
 * Interrupt Handler
 *********************************************************************/

/*
 *  PMI_Interrupt_Handler
 *      Arguments
 *          IntFrame - Pointer to the Interrupt Frame
 *
 *      Returns
 *          None
 *
 *      Description
 *  Grab the data that is needed to populate the sample records
 */
#if defined(DRV_IA32)

asmlinkage VOID
PMI_Interrupt_Handler (
     struct pt_regs *regs
)
{
    SampleRecordPC  *psamp;
    CPU_STATE        pcpu;
    BUFFER_DESC      bd;
    U32              csdlo;        // low  half code seg descriptor
    U32              csdhi;        // high half code seg descriptor
    U32              seg_cs;       // code seg selector
    DRV_MASKS_NODE   event_mask;
    U32              this_cpu;
    U32              i;
    U32              pid;
    U32              tid;
    U64              tsc;
    U32              desc_id;
    EVENT_DESC       evt_desc;
#if defined(SECURE_SEP)
    uid_t            l_uid;
#endif
    U32              accept_interrupt = 1;
    U32              dev_idx;
    U32              event_idx;
    DRV_CONFIG       pcfg_unc;
    DISPATCH         dispatch_unc;
    U64             *result_buffer;
    U64              diff;
    U64              lbr_tos_from_ip = 0;

    this_cpu = CONTROL_THIS_CPU();
    pcpu     = &pcb[this_cpu];
    bd       = &cpu_buf[this_cpu];
    SYS_Locked_Inc(&CPU_STATE_in_interrupt(pcpu));

    // Disable the counter control
    dispatch->freeze(NULL);

#if defined(SECURE_SEP)
    l_uid            = DRV_GET_UID(current);
    accept_interrupt = (l_uid == uid);
#endif
    dispatch->check_overflow(&event_mask);
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING &&
        CPU_STATE_accept_interrupt(&pcb[this_cpu])) {

        pid = GET_CURRENT_TGID();
        tid = current->pid;

        if (DRV_CONFIG_target_pid(pcfg) > 0 && pid != DRV_CONFIG_target_pid(pcfg)) {
            accept_interrupt = 0;
        }

        if (accept_interrupt) {
            UTILITY_Read_TSC(&tsc);

            for (i = 0; i < event_mask.masks_num; i++) {
                desc_id  = COMPUTE_DESC_ID(DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                evt_desc = desc_data[desc_id];
                psamp = (SampleRecordPC *)OUTPUT_Reserve_Buffer_Space(bd,
                                                 EVENT_DESC_sample_size(evt_desc));

                if (!psamp) {
                    continue;
                }
                CPU_STATE_num_samples(pcpu)           += 1;
                SAMPLE_RECORD_descriptor_id(psamp)     = desc_id;
                SAMPLE_RECORD_tsc(psamp)               = tsc;
                SAMPLE_RECORD_pid_rec_index_raw(psamp) = 1;
                SAMPLE_RECORD_pid_rec_index(psamp)     = pid;
                SAMPLE_RECORD_tid(psamp)               = tid;
                SAMPLE_RECORD_eip(psamp)               = REGS_eip(regs);
                SAMPLE_RECORD_eflags(psamp)            = REGS_eflags(regs);
                SAMPLE_RECORD_cpu_num(psamp)           = (U16) this_cpu;
                SAMPLE_RECORD_cs(psamp)                = (U16) REGS_xcs(regs);

                if (SAMPLE_RECORD_eflags(psamp) & EFLAGS_V86_MASK) {
                    csdlo = 0;
                    csdhi = 0;
                }
                else {
                    seg_cs = SAMPLE_RECORD_cs(psamp);
                    SYS_Get_CSD(seg_cs, &csdlo, &csdhi);
                }
                SAMPLE_RECORD_csd(psamp).u1.lowWord  = csdlo;
                SAMPLE_RECORD_csd(psamp).u2.highWord = csdhi;

                SEP_PRINT_DEBUG("SAMPLE_RECORD_pid_rec_index(psamp)  %x\n", SAMPLE_RECORD_pid_rec_index(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_tid(psamp) %x\n", SAMPLE_RECORD_tid(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_eip(psamp) %x\n", SAMPLE_RECORD_eip(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_eflags(psamp) %x\n", SAMPLE_RECORD_eflags(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_cpu_num(psamp) %x\n", SAMPLE_RECORD_cpu_num(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_cs(psamp) %x\n", SAMPLE_RECORD_cs(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_csd(psamp).lowWord %x\n", SAMPLE_RECORD_csd(psamp).u1.lowWord);
                SEP_PRINT_DEBUG("SAMPLE_RECORD_csd(psamp).highWord %x\n", SAMPLE_RECORD_csd(psamp).u2.highWord);

                SAMPLE_RECORD_event_index(psamp) = DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]);
                if (DRV_EVENT_MASK_precise(&event_mask.eventmasks[i]) == 1) {
                    if (EVENT_DESC_pebs_offset(evt_desc) ||
                        EVENT_DESC_latency_offset_in_sample(evt_desc)) {
                        PEBS_Fill_Buffer((S8 *)psamp,
                                     evt_desc,
                                     DRV_CONFIG_virt_phys_translation(pcfg));
                    }
                    PEBS_Modify_IP((S8 *)psamp, FALSE);
                    PEBS_Modify_TSC((S8 *)psamp);
                }
                if (DRV_CONFIG_collect_lbrs(pcfg) && (DRV_EVENT_MASK_lbr_capture(&event_mask.eventmasks[i]))) {
                    lbr_tos_from_ip = dispatch->read_lbrs(!DRV_CONFIG_store_lbrs(pcfg) ? NULL:((S8 *)(psamp)+EVENT_DESC_lbr_offset(evt_desc)));
                    if (DRV_EVENT_MASK_branch(&event_mask.eventmasks[i]) && DRV_CONFIG_precise_ip_lbrs(pcfg) && lbr_tos_from_ip) {
                        SAMPLE_RECORD_eip(psamp)       = (U32) lbr_tos_from_ip;
                        SEP_PRINT_DEBUG("UPDATED SAMPLE_RECORD_eip(psamp) %x\n", SAMPLE_RECORD_eip(psamp));
                    }
                }
                if (DRV_CONFIG_power_capture(pcfg)) {
                    dispatch->read_power(((S8 *)(psamp)+EVENT_DESC_power_offset_in_sample(evt_desc)));
                }
                if (DRV_CONFIG_enable_chipset(pcfg)) {
                    cs_dispatch->read_counters(((S8 *)(psamp)+DRV_CONFIG_chipset_offset(pcfg)));
                }
                if (DRV_CONFIG_event_based_counts(pcfg)) {
                    dispatch->read_counts((S8 *)psamp, DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                }
#if (defined(DRV_IA32) || defined(DRV_EM64T))
        if (DRV_CONFIG_enable_p_state(pcfg)) {
            if (DRV_CONFIG_read_pstate_msrs(pcfg)) {
                SEPDRV_P_STATE_Read((S8 *)(psamp)+EVENT_DESC_p_state_offset(evt_desc), pcpu);
            }
            if (!DRV_CONFIG_event_based_counts(pcfg) && CPU_STATE_p_state_counting(pcpu)) {
                        dispatch->read_counts((S8 *)psamp, DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                    }
                }
#endif
                // need to do this per device
                for (dev_idx = 0; dev_idx < num_devices; dev_idx++) {
                    pcfg_unc = LWPMU_DEVICE_pcfg(&devices[dev_idx]);
                    dispatch_unc = LWPMU_DEVICE_dispatch(&devices[dev_idx]);
                    if (pcfg_unc && DRV_CONFIG_event_based_counts(pcfg_unc)) {
                        dispatch_unc->read_counts((S8 *)(psamp), dev_idx);
                        SAMPLE_RECORD_uncore_valid(psamp) = 1;

                        // skip first element because it's the group number
                        result_buffer = (U64*) ((S8*)(psamp) + DRV_CONFIG_results_offset(pcfg_unc));
                        for (event_idx = 1;
                             event_idx < LWPMU_DEVICE_num_events(&devices[dev_idx]) + 1; // need to account for 1st element being group id
                             event_idx++) {

                            // check for overflow
                            if (result_buffer[event_idx] < LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx]) {
                                // overflow occurred!  need to compensate
                                diff  = LWPMU_DEVICE_counter_mask(&devices[dev_idx])
                                      - LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                                diff += result_buffer[event_idx];
                            }
                            else {
                                diff = result_buffer[event_idx] - LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                            }
                            // accumulate current results into accumulator
                            LWPMU_DEVICE_acc_per_thread(&devices[dev_idx])[this_cpu][event_idx] += diff;
                            // update previous results array
                            LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx] = result_buffer[event_idx];
                            // update results buffer for this thread
                            result_buffer[event_idx] = LWPMU_DEVICE_acc_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                        }
                    }
                }
            } // for
        }
    }
    if (DRV_CONFIG_pebs_mode(pcfg)) {
        PEBS_Reset_Index(this_cpu);
    }

    APIC_Ack_Eoi();

    // Reset the data counters
    if (CPU_STATE_trigger_count(&pcb[this_cpu]) == 0) {
        dispatch->swap_group(FALSE);
    }
    // Re-enable the counter control
    dispatch->restart(NULL);
    atomic_set(&CPU_STATE_in_interrupt(&pcb[this_cpu]), 0);

    return;
}

#endif  // DRV_IA32


#if defined(DRV_EM64T)

#define IS_LDT_BIT       0x4
#define SEGMENT_SHIFT    3
IDTGDT_DESC              gdt_desc;

static U32
pmi_Get_CSD (
    U32     seg,
    U32    *low,
    U32    *high
)
{
    PVOID               gdt_max_addr;
    struct desc_struct *gdt;
    CodeDescriptor     *csd;

    //
    // These could be pre-init'ed values
    //
    gdt_max_addr = (PVOID) (((U64) gdt_desc.idtgdt_base) + gdt_desc.idtgdt_limit);
    gdt          = gdt_desc.idtgdt_base;

    //
    // end pre-init potential...
    //

    //
    // We don't do ldt's
    //
    if (seg & IS_LDT_BIT) {
        *low  = 0;
        *high = 0;
        return (FALSE);
    }

    //
    // segment offset is based on dropping the bottom 3 bits...
    //
    csd = (CodeDescriptor *) &(gdt[seg >> SEGMENT_SHIFT]);

    if (((PVOID) csd) >= gdt_max_addr) {
        SEP_PRINT_WARNING("segment too big in get_CSD(0x%x)\n", seg);
        return FALSE;
    }

    *low  = csd->u1.lowWord;
    *high = csd->u2.highWord;

    SEP_PRINT_DEBUG("get_CSD - seg 0x%x, low %08x, high %08x, reserved_0: %d\n",
                     seg, *low, *high, csd->u2.s2.reserved_0);

    return TRUE;
}

asmlinkage VOID
PMI_Interrupt_Handler (
     struct pt_regs *regs
)
{
    SampleRecordPC  *psamp;
    CPU_STATE        pcpu;
    BUFFER_DESC      bd;
    DRV_MASKS_NODE   event_mask;
    U32              this_cpu;
    U32              i;
    U32              is_64bit_addr;
    U32              pid;
    U32              tid;
    U64              tsc;
    U32              desc_id;
    EVENT_DESC       evt_desc;
    U32              accept_interrupt = 1;
#if defined(SECURE_SEP)
    uid_t            l_uid;
#endif
    DRV_CONFIG       pcfg_unc;
    DISPATCH         dispatch_unc;
    U32              dev_idx;
    U32              event_idx;
    U64             *result_buffer;
    U64              diff;
    U64              lbr_tos_from_ip = 0;

    // Disable the counter control
    dispatch->freeze(NULL);
    this_cpu = CONTROL_THIS_CPU();
    pcpu     = &pcb[this_cpu];
    bd       = &cpu_buf[this_cpu];
    SYS_Locked_Inc(&CPU_STATE_in_interrupt(pcpu));

#if defined(SECURE_SEP)
    l_uid            = DRV_GET_UID(current);
    accept_interrupt = (l_uid == uid);
#endif
    dispatch->check_overflow(&event_mask);
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING &&
        CPU_STATE_accept_interrupt(&pcb[this_cpu])) {

        pid  = GET_CURRENT_TGID();
        tid  = current->pid;

        if (DRV_CONFIG_target_pid(pcfg) > 0 && pid != DRV_CONFIG_target_pid(pcfg)) {
            accept_interrupt = 0;
        }
        if (accept_interrupt) {
            UTILITY_Read_TSC(&tsc);

            for (i = 0; i < event_mask.masks_num; i++) {
                desc_id  = COMPUTE_DESC_ID(DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                evt_desc = desc_data[desc_id];
                psamp = (SampleRecordPC *)OUTPUT_Reserve_Buffer_Space(bd,
                                            EVENT_DESC_sample_size(evt_desc));
                if (!psamp) {
                    continue;
                }
                CPU_STATE_num_samples(pcpu)           += 1;
                SAMPLE_RECORD_descriptor_id(psamp)     = desc_id;
                SAMPLE_RECORD_tsc(psamp)               = tsc;
                SAMPLE_RECORD_pid_rec_index_raw(psamp) = 1;
                SAMPLE_RECORD_pid_rec_index(psamp)     = pid;
                SAMPLE_RECORD_tid(psamp)               = tid;
                SAMPLE_RECORD_cpu_num(psamp)           = (U16) this_cpu;
                SAMPLE_RECORD_cs(psamp)                = (U16) REGS_cs(regs);

                pmi_Get_CSD(SAMPLE_RECORD_cs(psamp),
                        &SAMPLE_RECORD_csd(psamp).u1.lowWord,
                        &SAMPLE_RECORD_csd(psamp).u2.highWord);

                SEP_PRINT_DEBUG("SAMPLE_RECORD_pid_rec_index(psamp)  %x\n", SAMPLE_RECORD_pid_rec_index(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_tid(psamp) %x\n", SAMPLE_RECORD_tid(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_cpu_num(psamp) %x\n", SAMPLE_RECORD_cpu_num(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_cs(psamp) %x\n", SAMPLE_RECORD_cs(psamp));
                SEP_PRINT_DEBUG("SAMPLE_RECORD_csd(psamp).lowWord %x\n", SAMPLE_RECORD_csd(psamp).u1.lowWord);
                SEP_PRINT_DEBUG("SAMPLE_RECORD_csd(psamp).highWord %x\n", SAMPLE_RECORD_csd(psamp).u2.highWord);

                is_64bit_addr = (SAMPLE_RECORD_csd(psamp).u2.s2.reserved_0 == 1);
                if (is_64bit_addr) {
                    SAMPLE_RECORD_iip(psamp)           = REGS_rip(regs);
                    SAMPLE_RECORD_ipsr(psamp)          = (REGS_eflags(regs) & 0xffffffff) |
                        (((U64) SAMPLE_RECORD_csd(psamp).u2.s2.dpl) << 32);
                    SAMPLE_RECORD_ia64_pc(psamp)       = TRUE;
                }
                else {
                    SAMPLE_RECORD_eip(psamp)           = REGS_rip(regs);
                    SAMPLE_RECORD_eflags(psamp)        = REGS_eflags(regs);
                    SAMPLE_RECORD_ia64_pc(psamp)       = FALSE;

                    SEP_PRINT_DEBUG("SAMPLE_RECORD_eip(psamp) 0x%x\n", SAMPLE_RECORD_eip(psamp));
                    SEP_PRINT_DEBUG("SAMPLE_RECORD_eflags(psamp) %x\n", SAMPLE_RECORD_eflags(psamp));
                }

                SAMPLE_RECORD_event_index(psamp) = DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]);
                if (DRV_EVENT_MASK_precise(&event_mask.eventmasks[i])) {
                    if ( EVENT_DESC_pebs_offset(evt_desc) ||
                         EVENT_DESC_latency_offset_in_sample(evt_desc)) {
                         PEBS_Fill_Buffer((S8 *)psamp,
                                     evt_desc,
                                     DRV_CONFIG_virt_phys_translation(pcfg));
                    }
                    PEBS_Modify_IP((S8 *)psamp, is_64bit_addr);
                    PEBS_Modify_TSC((S8 *)psamp);
                }
                if (DRV_CONFIG_collect_lbrs(pcfg) && (DRV_EVENT_MASK_lbr_capture(&event_mask.eventmasks[i]))) {
                    lbr_tos_from_ip = dispatch->read_lbrs(!DRV_CONFIG_store_lbrs(pcfg) ? NULL:((S8 *)(psamp)+EVENT_DESC_lbr_offset(evt_desc)));
                    if (DRV_EVENT_MASK_branch(&event_mask.eventmasks[i]) && DRV_CONFIG_precise_ip_lbrs(pcfg) && lbr_tos_from_ip) {
                        if (is_64bit_addr) {
                            SAMPLE_RECORD_iip(psamp)       = lbr_tos_from_ip;
                            SEP_PRINT_DEBUG("UPDATED SAMPLE_RECORD_iip(psamp) 0x%llx\n", SAMPLE_RECORD_iip(psamp));
                        }
                        else {
                            SAMPLE_RECORD_eip(psamp)       = (U32) lbr_tos_from_ip;
                            SEP_PRINT_DEBUG("UPDATED SAMPLE_RECORD_eip(psamp) 0x%x\n", SAMPLE_RECORD_eip(psamp));
                        }
                    }
                }
                if (DRV_CONFIG_power_capture(pcfg)) {
                    dispatch->read_power(((S8 *)(psamp)+EVENT_DESC_power_offset_in_sample(evt_desc)));
                }
                if (DRV_CONFIG_enable_chipset(pcfg)) {
                    cs_dispatch->read_counters(((S8 *)(psamp)+DRV_CONFIG_chipset_offset(pcfg)));
                }
                if (DRV_CONFIG_event_based_counts(pcfg)) {
                    dispatch->read_counts((S8 *)psamp, DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                }
#if (defined(DRV_IA32) || defined(DRV_EM64T))
                if (DRV_CONFIG_enable_p_state(pcfg)) {
                    SEPDRV_P_STATE_Read((S8 *)(psamp)+EVENT_DESC_p_state_offset(evt_desc), pcpu);
                    if (!DRV_CONFIG_event_based_counts(pcfg) && CPU_STATE_p_state_counting(pcpu)) {
                        dispatch->read_counts((S8 *) psamp, DRV_EVENT_MASK_event_idx(&event_mask.eventmasks[i]));
                    }
                }
#endif
                for (dev_idx = 0; dev_idx < num_devices; dev_idx++) {
                    pcfg_unc = LWPMU_DEVICE_pcfg(&devices[dev_idx]);
                    dispatch_unc = LWPMU_DEVICE_dispatch(&devices[dev_idx]);
                    if (pcfg_unc && DRV_CONFIG_event_based_counts(pcfg_unc)) {
                        dispatch_unc->read_counts((S8 *)(psamp), dev_idx);
                        SAMPLE_RECORD_uncore_valid(psamp) = 1;

                        // skip first element because it's the group number
                        result_buffer = (U64*) ((S8*)(psamp) + DRV_CONFIG_results_offset(pcfg_unc));
                        for (event_idx = 1;
                             event_idx < LWPMU_DEVICE_num_events(&devices[dev_idx]) + 1; // need to account for 1st element being group id
                             event_idx++) {

                            // check for overflow
                            if (result_buffer[event_idx] < LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx]) {
                                // overflow occurred!  need to compensate
                                diff  = LWPMU_DEVICE_counter_mask(&devices[dev_idx])
                                      - LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                                diff += result_buffer[event_idx];
                            }
                            else {
                                diff = result_buffer[event_idx] - LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                            }
                            // accumulate current results into accumulator
                            LWPMU_DEVICE_acc_per_thread(&devices[dev_idx])[this_cpu][event_idx] += diff;
                            // update previous results array
                            LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[this_cpu][event_idx] = result_buffer[event_idx];
                            // update results buffer for this thread
                            result_buffer[event_idx] = LWPMU_DEVICE_acc_per_thread(&devices[dev_idx])[this_cpu][event_idx];
                        }
                    }
                }
            }
        }
    }
    if (DRV_CONFIG_pebs_mode(pcfg)) {
        PEBS_Reset_Index(this_cpu);
    }

    APIC_Ack_Eoi();

    // Reset the data counters
    if (CPU_STATE_trigger_count(&pcb[this_cpu]) == 0) {
        dispatch->swap_group(FALSE);
    }
    // Re-enable the counter control
    dispatch->restart(NULL);
    atomic_set(&CPU_STATE_in_interrupt(&pcb[this_cpu]), 0);

    return;
}

#endif
