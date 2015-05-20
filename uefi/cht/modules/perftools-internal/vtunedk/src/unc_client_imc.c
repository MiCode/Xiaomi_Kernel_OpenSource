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
#include <linux/wait.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "lwpmudrv.h"
#include "utility.h"
#include "control.h"
#include "output.h"
#include "unc_common.h"
#include "snb_unc_imc.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "inc/pci.h"

extern EVENT_CONFIG   global_ec;
extern U64           *read_counter_info;
extern U64           *prev_counter_data;
extern LBR            lbr;
extern DRV_CONFIG     pcfg;
extern PWR            pwr;
PVOID                 virtual_address;

/*!
 * @fn          static VOID snbunc_imc_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_imc_Write_PMU (
    VOID  *param
)
{
    U32                        dev_idx   = *((U32*)param);
    U32                        offset_delta;
    DRV_CONFIG                 pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    U32                        j;
    U32                        event_id   = 0;
    U32                        tmp_value;
    U32                        this_cpu    = CONTROL_THIS_CPU();
    CPU_STATE                  pcpu        = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    //Read in the counts into temporary buffer
    FOR_EACH_PCI_DATA_REG(pecb,i,dev_idx,offset_delta) {
        if (DRV_CONFIG_event_based_counts(pcfg_unc)) {
            // this is needed for overflow detection of the accumulators.
            event_id                            = ECB_entries_event_id_index_local(pecb,i);
            tmp_value                           = readl((U32*)((char*)(virtual_address) + offset_delta));
            for ( j = 0; j < (U32)GLOBAL_STATE_num_cpus(driver_state) ; j++) {
                   LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[j][event_id + 1] = tmp_value; // need to account for group id
                   SEP_PRINT_DEBUG("initial value for i =%d is 0x%x\n",i,LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[j][i]);
            }
        }

        if (LWPMU_DEVICE_counter_mask(&devices[dev_idx]) == 0) {
            LWPMU_DEVICE_counter_mask(&devices[dev_idx]) = (U64)ECB_entries_max_bits(pecb,i);
        }
    } END_FOR_EACH_PCI_DATA_REG;
    SEP_PRINT_DEBUG("BAR address is 0x%llx and virt is 0x%llx phys is 0x%llx\n",DRV_PCI_DEVICE_ENTRY_bar_address(&ECB_pcidev_entry_node(pecb)), virtual_address, physical_address);
    return;
}


/*!
 * @fn         static VOID snbunc_imc_Enable_PMU(PVOID)
 *
 * @brief      Capture the previous values to calculate delta later.
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static void
snbunc_imc_Enable_PMU (
    PVOID  param
)
{
    S32            j;
    U64           *buffer       = prev_counter_data;
    U32            this_cpu     = CONTROL_THIS_CPU();
    U32            dev_idx      = *((U32*)param);
    U32            start_index;
    DRV_CONFIG     pcfg_unc;
    CPU_STATE      pcpu         = &pcb[this_cpu];
    U32            offset_delta;
    U32            cur_grp      = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    U32            package_event_count = 0;

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);

    // NOTE THAT the enable function currently captures previous values
    // for EMON collection to avoid unnecessary memory copy.
    if (!DRV_CONFIG_emon_mode(pcfg_unc)) {
        return;
    }

    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);

    FOR_EACH_PCI_DATA_REG(pecb,i,dev_idx,offset_delta) {
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = start_index + package_event_count + ECB_entries_group_index(pecb, i) + ECB_entries_emon_event_id_index_local(pecb,i);

            buffer[j] = readl((U32*)((char*)(virtual_address) + offset_delta));
            SEP_PRINT_DEBUG("snbunc_imc_Enable_PMU cpu=%d, ei=%d, eil=%d, MSR=0x%x, j=0x%d, si=0x%d, value=0x%x\n",
                this_cpu, ECB_entries_event_id_index(pecb, i), ECB_entries_emon_event_id_index_local(pecb,i),
                ECB_entries_reg_id(pecb,i),j, start_index,value);
            package_event_count++;
        }
    } END_FOR_EACH_PCI_DATA_REG;

    return;
}


/*!
 * @fn         static VOID snbunc_imc_Disable_PMU(PVOID)
 *
 * @brief      Unmap the virtual address when you stop sampling.
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static void
snbunc_imc_Disable_PMU (
    PVOID  param
)
{
    int                        this_cpu    = CONTROL_THIS_CPU();
    CPU_STATE                  pcpu        = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn snbunc_imc_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 * @param    id       The event id for the which the sample is generated
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *           Uncore PMU does not support sampling, i.e. ignore the id parameter.
 */
static VOID
snbunc_imc_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64            *data       = (U64*) param;
    U32             cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB             pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];
    U32             offset_delta;

    // Write GroupID
    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;

    //Read in the counts into temporary buffer
    FOR_EACH_PCI_DATA_REG(pecb, i, id, offset_delta) {
        data  = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        *data = readl((U32*)((char*)(virtual_address) + offset_delta));
    } END_FOR_EACH_PCI_DATA_REG;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn snbunc_imc_Read_PMU_Data(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Read all the data MSR's into a buffer.  Called by the interrupt handler.
 *
 */
static VOID
snbunc_imc_Read_PMU_Data (
     PVOID   param
)
{
    S32            j;
    U64           *buffer       = read_counter_info;
    U64           *prev_buffer  = prev_counter_data;
    U32            this_cpu     = CONTROL_THIS_CPU();
    U32            dev_idx      = *((U32*)param);
    U32            start_index;
    DRV_CONFIG     pcfg_unc;
    CPU_STATE      pcpu         = &pcb[this_cpu];
    U32            offset_delta;
    U32            cur_grp      = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    U32            package_event_count = 0;
    U64            tmp_value;

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);

    FOR_EACH_PCI_DATA_REG(pecb,i,dev_idx,offset_delta) {
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = start_index + package_event_count + ECB_entries_group_index(pecb, i) + ECB_entries_emon_event_id_index_local(pecb,i);

            tmp_value = readl((U32*)((char*)(virtual_address) + offset_delta));
            if (ECB_entries_counter_type(pecb,i) == STATIC_COUNTER) {
                buffer[j] = tmp_value;
            }
            else {
                if (tmp_value >= prev_buffer[j]) {
                    buffer[j] = tmp_value - prev_buffer[j];
                }
                else {
                    buffer[j] = tmp_value + (ECB_entries_max_bits(pecb,i) - prev_buffer[j]);
                }
            }

            SEP_PRINT_DEBUG("snbunc_imc_Read_PMU_Data cpu=%d, ei=%d, eil=%d, MSR=0x%x, j=0x%d, si=0x%d, value=0x%x\n",
                this_cpu, ECB_entries_event_id_index(pecb, i), ECB_entries_emon_event_id_index_local(pecb,i),
                ECB_entries_reg_id(pecb,i),j, start_index, value);
            package_event_count++;
        }
    } END_FOR_EACH_PCI_DATA_REG;
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn snbunc_imc_Initialize(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Do the mapping of the physical address (to do the invalidates in the TLB)
 *           NOTE: this should never be done with SMP call
 *
 */
static VOID
snbunc_imc_Initialize (
     PVOID   param
)
{
    DRV_PCI_DEVICE_ENTRY_NODE  dpden;
    U32                        pci_address;
    U32                        bar_lo;
    U64                        next_bar_offset;
    U64                        bar_hi;
    U64                        physical_address;
    U64                        final_bar;

    U32                        dev_idx   = *((U32*)param);
    U32                        cur_grp  = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    ECB                        pecb     = LWPMU_DEVICE_PMU_register_data(&devices[dev_idx])[cur_grp];

    dpden = ECB_pcidev_entry_node(pecb);
    pci_address = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_dev_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_func_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_bar_offset(&dpden));
    bar_lo      = PCI_Read_Ulong(pci_address);

    next_bar_offset     = DRV_PCI_DEVICE_ENTRY_bar_offset(&dpden) + NEXT_ADDR_OFFSET;
    pci_address         = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_dev_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_func_no(&dpden),
                                next_bar_offset);
    bar_hi              = PCI_Read_Ulong(pci_address);
    final_bar = (bar_hi << SNBUNC_IMC_BAR_ADDR_SHIFT) | bar_lo;
    final_bar &= SNBUNC_IMC_BAR_ADDR_MASK;

    DRV_PCI_DEVICE_ENTRY_bar_address(&ECB_pcidev_entry_node(pecb)) = final_bar;
    physical_address     = DRV_PCI_DEVICE_ENTRY_bar_address(&ECB_pcidev_entry_node(pecb))
                                 + DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(&ECB_pcidev_entry_node(pecb));
    virtual_address      = ioremap_nocache(physical_address,4096);
    
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn snbunc_imc_Destroy(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Invalidate the entry in TLB of the physical address
 *           NOTE: this should never be done with SMP call
 *
 */
static VOID
snbunc_imc_Destroy (
     PVOID   param
)
{
    SEP_PRINT_DEBUG("snbunc_imc_Disable_PMU : Unmapping the address\n");
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
      iounmap((PVOID)(UIOP)virtual_address);
    }
    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  snbunc_imc_dispatch =
{
    snbunc_imc_Initialize,       // initialize
    snbunc_imc_Destroy,          // destroy
    snbunc_imc_Write_PMU,        // write
    snbunc_imc_Disable_PMU,      // freeze
    snbunc_imc_Enable_PMU,       // restart
    snbunc_imc_Read_PMU_Data,    // read
    NULL,                        // check for overflow
    NULL,
    NULL,
    UNC_COMMON_Dummy_Func,
    NULL,
    NULL,
    NULL,
    snbunc_imc_Read_Counts,      //read_counts
    NULL,
    NULL,
    NULL,
    NULL,
    NULL                         // scan for uncore
};
