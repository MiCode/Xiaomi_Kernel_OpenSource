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

#ifndef _ECB_ITERATORS_H_
#define _ECB_ITERATORS_H_

#if defined(__cplusplus)
extern "C" {
#endif

//
// Loop macros to walk through the event control block
// Use for access only in the kernel mode
// To Do - Control access from kernel mode by a macro
//
#define FOR_EACH_CCCR_REG(pecb,idx) {                                                  \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_cccr_start(pecb);                                             \
             (idx) < ECB_cccr_start(pecb)+ECB_cccr_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_reg_id((pecb),(idx)) == 0) {                               \
                continue;                                                              \
            }

#define END_FOR_EACH_CCCR_REG  }}}

#define FOR_EACH_CCCR_GP_REG(pecb,idx) {                                               \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_cccr_start(pecb);                                             \
             (idx) < ECB_cccr_start(pecb)+ECB_cccr_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_is_gp_reg_get((pecb),(idx)) == 0) {                        \
                continue;                                                              \
            }

#define END_FOR_EACH_CCCR_GP_REG  }}}

#define FOR_EACH_ESCR_REG(pecb,idx) {                                                  \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_escr_start(pecb);                                             \
             (idx) < ECB_escr_start(pecb)+ECB_escr_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_reg_id((pecb),(idx)) == 0) {                               \
                continue;                                                              \
            }

#define END_FOR_EACH_ESCR_REG  }}}

#define FOR_EACH_DATA_REG(pecb,idx) {                                                  \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_data_start(pecb);                                             \
             (idx) < ECB_data_start(pecb)+ECB_data_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_reg_id((pecb),(idx)) == 0) {                               \
                continue;                                                              \
            }

#define END_FOR_EACH_DATA_REG  }}}

#define FOR_EACH_DATA_REG_UNC(pecb,device_idx,idx) {                                      \
    U32        (idx);                                                                     \
    U32        (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                \
    ECB        (pecb) = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[cur_grp];  \
    if ((pecb)) {                                                                         \
      for ((idx) = ECB_data_start(pecb);                                                  \
           (idx) < ECB_data_start(pecb)+ECB_data_pop(pecb);                               \
           (idx)++) {                                                                     \
          if (ECB_entries_reg_id((pecb),(idx)) == 0) {                                    \
              continue;                                                                   \
    }

#define END_FOR_EACH_DATA_REG_UNC  }}}

#define FOR_EACH_DATA_GP_REG(pecb,idx) {                                               \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_data_start(pecb);                                             \
             (idx) < ECB_data_start(pecb)+ECB_data_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_is_gp_reg_get((pecb),(idx)) == 0) {                        \
                continue;                                                              \
            }

#define END_FOR_EACH_DATA_GP_REG  }}}

#define FOR_EACH_DATA_GENERIC_REG(pecb,idx) {                                          \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
        for ((idx) = ECB_data_start(pecb);                                             \
             (idx) < ECB_data_start(pecb)+ECB_data_pop(pecb);                          \
             (idx)++) {                                                                \
            if (ECB_entries_is_generic_reg_get((pecb),(idx)) == 0) {                   \
                continue;                                                              \
            }

#define END_FOR_EACH_DATA_GENERIC_REG  }}}

#define FOR_EACH_REG_ENTRY(pecb,idx) {                                                 \
    U32        (idx);                                                                  \
    U32        this_cpu__ = CONTROL_THIS_CPU();                                        \
    CPU_STATE  pcpu__  = &pcb[this_cpu__];                                             \
    ECB        (pecb) = PMU_register_data[CPU_STATE_current_group(pcpu__)];            \
    if ((pecb)) {                                                                      \
    for ((idx) = 0; (idx) < ECB_num_entries(pecb); (idx)++) {                          \
        if (ECB_entries_reg_id((pecb),(idx)) == 0) {                                   \
            continue;                                                                  \
        }

#define END_FOR_EACH_REG_ENTRY  }}}

#define FOR_EACH_REG_ENTRY_UNC(pecb,device_idx,idx) {                                          \
    U32        (idx);                                                                          \
    U32        (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                     \
    ECB        (pecb)    = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[(cur_grp)];  \
    if ((pecb)) {                                                                              \
        for ((idx) = 0; (idx) < ECB_num_entries(pecb); (idx)++) {                              \
            if (ECB_entries_reg_id((pecb),(idx)) == 0) {                                       \
                continue;                                                                      \
            }

#define END_FOR_EACH_REG_ENTRY_UNC  }}}

#define FOR_EACH_PCI_DATA_REG(pecb,i, device_idx, offset_delta) {                                       \
    U32                 (i)    = 0;                                                                     \
    U32              (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                        \
    ECB                 (pecb) = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[(cur_grp)];     \
    if ((pecb)) {                                                                                       \
        for ((i) = ECB_data_start(pecb);                                                                \
             (i) < ECB_data_start(pecb)+ECB_data_pop(pecb);                                             \
             (i)++) {                                                                                   \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                           \
                continue;                                                                               \
            }                                                                                           \
            (offset_delta) =  ECB_entries_pci_id_offset(pecb,i) -                                       \
                              DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(&ECB_pcidev_entry_node(pecb));

#define END_FOR_EACH_PCI_DATA_REG    } } }

#define FOR_EACH_PCI_DATA_REG_RAW(pecb,i, device_idx ) {                                                \
    U32                 (i)       = 0;                                                                  \
    U32                 (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                        \
    ECB                 (pecb)    = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[(cur_grp)];  \
    if ((pecb)) {                                                                                       \
        for ((i) = ECB_data_start(pecb);                                                                \
             (i) < ECB_data_start(pecb)+ECB_data_pop(pecb);                                             \
             (i)++) {                                                                                   \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                           \
                continue;                                                                               \
            }

#define END_FOR_EACH_PCI_DATA_REG_RAW    } } }

#define FOR_EACH_PCI_CCCR_REG_RAW(pecb,i, device_idx ) {                                            \
    U32              (i)       = 0;                                                                 \
    U32              (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                       \
    ECB              (pecb)    = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[(cur_grp)]; \
    if ((pecb)) {                                                                                   \
        for ((i) = ECB_cccr_start(pecb);                                                            \
             (i) < ECB_cccr_start(pecb)+ECB_cccr_pop(pecb);                                         \
             (i)++) {                                                                               \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                       \
                continue;                                                                           \
            }

#define END_FOR_EACH_PCI_CCCR_REG_RAW   } } }

#define FOR_EACH_PCI_REG_RAW(pecb, i, device_idx ) {                                                   \
    U32                 (i)       = 0;                                                                 \
    U32                 (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                       \
    ECB                 (pecb)    = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[(cur_grp)]; \
    if ((pecb)) {                                                                                      \
        for ((i) = 0;                                                                                  \
             (i) < ECB_num_entries(pecb);                                                              \
             (i)++) {                                                                                  \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                          \
                continue;                                                                              \
            }

#define END_FOR_EACH_PCI_REG_RAW   } } }

#define FOR_EACH_DATA_REG_UNC_VER2(pecb,i, device_idx ) {                                                     \
    U32            (i)    = 0;                                                                                \
    U32            (cur_grp) = LWPMU_DEVICE_cur_group(&devices[(device_idx)]);                                \
    ECB            (pecb) = LWPMU_DEVICE_PMU_register_data(&devices[(device_idx)])[cur_grp];                  \
    if ((pecb)) {                                                                                             \
        for ((i) = ECB_data_start(pecb);                                                                      \
             (i) < ECB_data_start(pecb)+ECB_data_pop(pecb);                                                   \
             (i)++) {                                                                                         \
             if ((ECB_flags(pecb) & ECB_pci_id_offset_bit)  && (ECB_entries_pci_id_offset(pecb,i) == 0) ){    \
                  continue;                                                                                   \
             }                                                                                                \
             else if (ECB_entries_reg_id(pecb,i) == 0) {                                                      \
                 continue;                                                                                    \
             }                                                                                                \
             if (ECB_entries_emon_event_id_index_local(pecb_unc, k)) {                                        \
                 continue;                                                                                    \
             }

#define END_FOR_EACH_DATA_REG_UNC_VER2    } } }

#define CHECK_SAVE_RESTORE_EVENT_INDEX(prev_ei, cur_ei, evt_index)  {                                   \
        if (prev_ei == -1) {                                                                            \
            prev_ei = cur_ei;                                                                           \
        }                                                                                               \
        if (prev_ei < cur_ei) {                                                                         \
            prev_ei = cur_ei;                                                                           \
            evt_index++;                                                                                \
        }                                                                                               \
        else {                                                                                          \
             evt_index = 0;                                                                             \
             prev_ei = cur_ei;                                                                          \
        }}

#if defined(__cplusplus)
}
#endif

#endif
