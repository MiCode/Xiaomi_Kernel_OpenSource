/*COPYRIGHT**
    Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

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
#include "snb_unc_gt.h"
#include "ecb_iterators.h"
#include "inc/pci.h"

static PVOID   snbunc_gt_virtual_address = NULL;
static U32     snbunc_gt_rc6_reg1;
static U32     snbunc_gt_rc6_reg2;
static U32     snbunc_gt_clk_gt_reg1;
static U32     snbunc_gt_clk_gt_reg2;
static U32     snbunc_gt_clk_gt_reg3;
static U32     snbunc_gt_clk_gt_reg4;

/*!
 * @fn          static VOID snbunc_gt_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @param       device id
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_gt_Write_PMU (
    VOID  *param
)
{
    U32                        dev_idx  = *((U32*)param);
    ECB                        pecb     = LWPMU_DEVICE_PMU_register_data(&devices[dev_idx])[0];
    DRV_PCI_DEVICE_ENTRY_NODE  dpden;
    U32                        pci_address;
    U64                        device_id;
    U32                        vendor_id;
    U64                        bar_lo;
    U32                        offset_delta;
    U32                        tmp_value;
    U32                        this_cpu = CONTROL_THIS_CPU();
    U32                        value;
    CPU_STATE                  pcpu     = &pcb[this_cpu];

    if (!CPU_STATE_system_master(pcpu)) {
        return;
    }

    dpden = ECB_pcidev_entry_node(pecb);
    pci_address = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_dev_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_func_no(&dpden),
                                0);
    value     = PCI_Read_Ulong(pci_address);
    vendor_id = DRV_GET_PCI_VENDOR_ID(value);
    device_id = DRV_GET_PCI_DEVICE_ID(value);

    if (DRV_IS_INTEL_VENDOR_ID(vendor_id) && DRV_IS_SNB_GT_DEVICE_ID(device_id)){
        SEP_PRINT_DEBUG("Found SNB Desktop  GT\n");
    }
    pci_address = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_dev_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_func_no(&dpden),
                                DRV_PCI_DEVICE_ENTRY_bar_offset(&dpden));
    bar_lo                     = PCI_Read_Ulong(pci_address);
    bar_lo                     &= SNBUNC_GT_BAR_MASK;
    snbunc_gt_virtual_address  = ioremap_nocache( bar_lo,SNB_GT_MMIO_SIZE);

   FOR_EACH_PCI_DATA_REG_RAW(pecb,i, dev_idx ) {
        offset_delta           = ECB_entries_pci_id_offset(pecb, i);
        // this is needed for overflow detection of the accumulators.
        if (LWPMU_DEVICE_counter_mask(&devices[dev_idx]) == 0) {
            LWPMU_DEVICE_counter_mask(&devices[dev_idx]) = (U64)ECB_entries_max_bits(pecb,i);
        }
    } END_FOR_EACH_PCI_CCCR_REG_RAW;

    //enable the global control to clear the counter first
    SYS_Write_MSR(PERF_GLOBAL_CTRL, ECB_entries_reg_value(pecb,0));
    FOR_EACH_PCI_CCCR_REG_RAW(pecb,i, dev_idx ) {
        offset_delta           = ECB_entries_pci_id_offset(pecb, i);
        if  (offset_delta == PERF_GLOBAL_CTRL) {
           continue;
        }
        DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                                offset_delta,
                                SNB_GT_CLEAR_COUNTERS);
#if defined(MYDEBUG)
        SEP_PRINT("CCCR offset delta is 0x%x W is clear ctrs\n",offset_delta);
#endif
    } END_FOR_EACH_PCI_CCCR_REG_RAW;

    //disable the counters
    SYS_Write_MSR(PERF_GLOBAL_CTRL, 0LL);

    FOR_EACH_PCI_CCCR_REG_RAW(pecb,i, dev_idx ) {
        offset_delta           = ECB_entries_pci_id_offset(pecb, i);
        if  (offset_delta == PERF_GLOBAL_CTRL) {
           continue;
        }
        DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                                offset_delta,
                                ((U32)ECB_entries_reg_value(pecb,i)));
        tmp_value = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, offset_delta);

        // remove compiler warning on unused variables
        if (tmp_value) {
        }

#if defined(MYDEBUG)
        SEP_PRINT("CCCR offset delta is 0x%x R is 0x%x W is 0x%llx\n",offset_delta, tmp_value, ECB_entries_reg_value(pecb,i));
#endif

    } END_FOR_EACH_PCI_CCCR_REG_RAW;

    return;
}

/*!
 * @fn          static VOID snbunc_gt_Disable_RC6_Clock_Gating(VOID)
 *
 * @brief       This snippet of code allows GT events to count by
 *              disabling settings related to clock gating/power
 * @param       none
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_gt_Disable_RC6_Clock_Gating (
    VOID
)
{
    U32          tmp;

#if defined(MYDEBUG)
    SEP_PRINT("Disabling rc6 and clock gating\n");
#endif
    // Disable RC6
    snbunc_gt_rc6_reg1  =  DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_RC6_REG1);
    tmp                 =  snbunc_gt_rc6_reg1 | SNBUNC_GT_RC6_REG1_OR_VALUE;
    snbunc_gt_rc6_reg2  =  DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_RC6_REG2);

    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_RC6_REG2,
                            SNBUNC_GT_RC6_REG2_VALUE);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_RC6_REG1,
                            tmp);

#if defined(MYDEBUG)
    SEP_PRINT("Original value of RC6 rc6_1 = 0x%x, rc6_2 = 0x%x\n",snbunc_gt_rc6_reg1, snbunc_gt_rc6_reg2);
#endif
    // Disable clock gating
    // Save
    snbunc_gt_clk_gt_reg1 = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_GCPUNIT_REG1);
    snbunc_gt_clk_gt_reg2 = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_GCPUNIT_REG2);
    snbunc_gt_clk_gt_reg3 = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_GCPUNIT_REG3);
    snbunc_gt_clk_gt_reg4 = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, SNBUNC_GT_GCPUNIT_REG4);

#if defined(MYDEBUG)
    SEP_PRINT("Original value of RC6 ck_1 = 0x%x, ck_2 = 0x%x\n",snbunc_gt_clk_gt_reg1, snbunc_gt_clk_gt_reg2);
    SEP_PRINT("Original value of RC6 ck_3 = 0x%x, ck_4 = 0x%x\n",snbunc_gt_clk_gt_reg3, snbunc_gt_clk_gt_reg4);
#endif
    // Disable
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG1,
                            SNBUNC_GT_GCPUNIT_REG1_VALUE);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG2,
                            SNBUNC_GT_GCPUNIT_REG2_VALUE);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG3,
                            SNBUNC_GT_GCPUNIT_REG3_VALUE);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG4,
                            SNBUNC_GT_GCPUNIT_REG4_VALUE);

    return;
}


/*!
 * @fn          static VOID snbunc_gt_Restore_RC6_Clock_Gating(VOID)
 *
 * @brief       This snippet of code restores the system settings
 *              for clock gating/power
 * @param       none
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_gt_Restore_RC6_Clock_Gating (
    VOID
)
{
#if defined(MYDEBUG)
    SEP_PRINT("Restore rc6 and clock gating\n");
#endif
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_RC6_REG2,
                            snbunc_gt_rc6_reg2);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_RC6_REG1,
                            snbunc_gt_rc6_reg1);

    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG1,
                            snbunc_gt_clk_gt_reg1);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG2,
                            snbunc_gt_clk_gt_reg2);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG3,
                            snbunc_gt_clk_gt_reg3);
    DRV_WRITE_PCI_REG_ULONG(snbunc_gt_virtual_address,
                            SNBUNC_GT_GCPUNIT_REG4,
                            snbunc_gt_clk_gt_reg4);

    return;
}

/*!
 * @fn         static VOID snbunc_gt_Enable_PMU(PVOID)
 *
 * @brief      Disable the clock gating and Set the global enable
 *
 * @param      device_id
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_gt_Enable_PMU (
    PVOID   param
)
{
    U32          dev_idx     = *((U32*)param);
    ECB          pecb        = LWPMU_DEVICE_PMU_register_data(&devices[dev_idx])[0];
    U32          this_cpu    = CONTROL_THIS_CPU();
    CPU_STATE    pcpu        = &pcb[this_cpu];

    if (!CPU_STATE_system_master(pcpu)) {
        return;
    }

    snbunc_gt_Disable_RC6_Clock_Gating();

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        SYS_Write_MSR(PERF_GLOBAL_CTRL, ECB_entries_reg_value(pecb,0));
#if defined(MYDEBUG)
        SEP_PRINT("Enabling GT  Global control = 0x%llx\n",ECB_entries_reg_value(pecb,0));
#endif
    }

    return;
}
/*!
 * @fn         static VOID snbunc_gt_Disable_PMU(PVOID)
 *
 * @brief      Unmap the virtual address when sampling/driver stops
 *             and restore system values for clock gating settings
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
snbunc_gt_Disable_PMU (
    PVOID  param
)
{
    U32          this_cpu    = CONTROL_THIS_CPU();
    CPU_STATE    pcpu        = &pcb[this_cpu];

    if (!CPU_STATE_system_master(pcpu)) {
        return;
    }
    snbunc_gt_Restore_RC6_Clock_Gating();
    SEP_PRINT_DEBUG("snbunc_gt_Disable_PMU \n");
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
        SYS_Write_MSR(PERF_GLOBAL_CTRL, 0LL);
        iounmap((void*)(UIOP)(snbunc_gt_virtual_address));
    }

    return;
}


/*!
 * @fn snbunc_gt_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 * @param    id       The id refers to the device index
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *
 */
static VOID
snbunc_gt_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64                  *data       = (U64*) param;
    U32                   cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB                   pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];
    U32                   offset_delta;
    U32                   tmp_value_lo        = 0;
    U32                   tmp_value_hi        = 0;
    SNBGT_CTR_NODE        snbgt_ctr_value;

    // Write GroupID
    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;
    SNBGT_CTR_NODE_value_reset(snbgt_ctr_value);

    //Read in the counts into temporary buffe
    FOR_EACH_PCI_DATA_REG_RAW(pecb, i, id) {
        offset_delta                           = ECB_entries_pci_id_offset(pecb, i);
        tmp_value_lo                           = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, offset_delta);
        offset_delta                           = offset_delta + NEXT_ADDR_OFFSET;
        tmp_value_hi                           = DRV_READ_PCI_REG_ULONG(snbunc_gt_virtual_address, offset_delta);
        data                                   = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        SNBGT_CTR_NODE_low(snbgt_ctr_value)    = tmp_value_lo;
        SNBGT_CTR_NODE_high(snbgt_ctr_value)   = tmp_value_hi;
        *data                                  = SNBGT_CTR_NODE_value(snbgt_ctr_value);
#if defined(MYDEBUG)
        SEP_PRINT("DATA offset delta is 0x%x R is 0x%x\n", offset_delta, SNBGT_CTR_NODE_value(snbgt_ctr_value));
#endif
    } END_FOR_EACH_PCI_DATA_REG_RAW;

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  snbunc_gt_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    snbunc_gt_Write_PMU,         // write
    snbunc_gt_Disable_PMU,       // freeze
    snbunc_gt_Enable_PMU,        // restart
    NULL,                        // read
    NULL,                        // check for overflow
    NULL,                        // swap_group
    NULL,                        // read_lbrs
    NULL,                        // cleanup
    NULL,                        // hw_errata
    NULL,                        // read_power
    NULL,                        // check_overflow_errata
    snbunc_gt_Read_Counts,       // read_counts
    NULL,                        // check_overflow_gp_errata
    NULL,                        // read_ro
    NULL,                        // platform_info
    NULL,
    NULL                         // scan for uncore
};
