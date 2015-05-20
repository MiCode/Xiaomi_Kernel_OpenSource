/*COPYRIGHT**
    Copyright (C) 2012-2014 Intel Corporation.  All Rights Reserved.

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
#include "hsw_unc_power.h"
#include "snb_unc_power.h"
#include "avt_unc_power.h"
#include "ecb_iterators.h"
#include "unc_common.h"

extern U64           *read_counter_info;
extern U64           *prev_counter_data;

/******************************************************************************************
 * @fn          static VOID unc_power_snb_Write_PMU(VOID*)
 *
 * @brief       No registers to write and setup the accumalators with initial values
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_power_snb_Write_PMU (
    VOID  *param
)
{
    U32    dev_idx        = *((U32*)param);
    U64    tmp_value      = 0;
    U32    j;
    U32    event_id       = 0;

    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        for ( j = 0; j < (U32)GLOBAL_STATE_num_cpus(driver_state); j++) {
             tmp_value = SYS_Read_MSR(ECB_entries_reg_id(pecb,i)) & SNB_POWER_MSR_DATA_MASK;
             LWPMU_DEVICE_prev_val_per_thread(&devices[dev_idx])[j][event_id + 1] = tmp_value; // need to account for group id
        }
        // Initialize counter_mask for accumulators
        if (LWPMU_DEVICE_counter_mask(&devices[dev_idx]) == 0) {
            LWPMU_DEVICE_counter_mask(&devices[dev_idx]) = (U64)ECB_entries_max_bits(pecb,i);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn unc_power_snb_Read_Counts(param, id)
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
unc_power_snb_Read_Counts (
    PVOID  param,
    U32    id
)
{
    UNC_COMMON_MSR_Read_Counts_With_Mask(param, id, SNB_POWER_MSR_DATA_MASK);

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn       unc_power_hsw_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *           Uncore PMU does not support sampling, i.e. ignore the id parameter.
 */
static VOID
unc_power_hsw_Read_Counts (
    PVOID  param,
    U32    id
)
{
    UNC_COMMON_MSR_Read_Counts_With_Mask(param, id, HASWELL_POWER_MSR_DATA_MASK);

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn       unc_power_hsw_Enable_PMU(param)
 *
 * @param    None
 *
 * @return   None
 *
 * @brief      Capture the previous values to calculate delta later.
 */
static VOID
unc_power_hsw_Enable_PMU (
    PVOID  param
)
{
    S32                   j;
    U64                  *buffer             = prev_counter_data;
    U32                   dev_idx            = *((U32*)param);
    U32                   start_index;
    DRV_CONFIG            pcfg_unc;
    U32                   this_cpu           = CONTROL_THIS_CPU();
    CPU_STATE             pcpu               = &pcb[this_cpu];
    U32                   num_cpus           = GLOBAL_STATE_num_cpus(driver_state);
    U32                   thread_event_count = 0;

    pcfg_unc = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);

    // NOTE THAT the enable function currently captures previous values
    // for EMON collection to avoid unnecessary memory copy.
    if (!DRV_CONFIG_emon_mode(pcfg_unc)) {
        return;
    }

    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, 0);

    FOR_EACH_DATA_REG_UNC(pecb, dev_idx, i) {
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = start_index + thread_event_count*(num_cpus-1) + ECB_entries_group_index(pecb,i) + ECB_entries_emon_event_id_index_local(pecb,i);
            if (!CPU_STATE_socket_master(pcpu)) {
                continue;
            }
        }
        else {
            j = start_index + this_cpu + thread_event_count*(num_cpus-1) + ECB_entries_group_index(pecb,i) + ECB_entries_emon_event_id_index_local(pecb,i);
            thread_event_count++;
        }
        buffer[j] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn       unc_power_hsw_Read_PMU_Data(param)
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
unc_power_hsw_Read_PMU_Data (
    PVOID  param
)
{
    S32                   j;
    U64                  *buffer             = read_counter_info;
    U64                  *prev_buffer        = prev_counter_data;
    U32                   dev_idx            = *((U32*)param);
    U32                   start_index;
    DRV_CONFIG            pcfg_unc;
    U32                   this_cpu           = CONTROL_THIS_CPU();
    CPU_STATE             pcpu               = &pcb[this_cpu];
    U32                   num_cpus           = GLOBAL_STATE_num_cpus(driver_state);
    U32                   thread_event_count = 0;
    U64                   tmp_value;

    pcfg_unc    = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, 0);

    FOR_EACH_DATA_REG_UNC(pecb, dev_idx, i) {
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = start_index + thread_event_count*(num_cpus-1) + ECB_entries_group_index(pecb,i) + ECB_entries_emon_event_id_index_local(pecb,i);
            if (!CPU_STATE_socket_master(pcpu)) {
                continue;
            }
        }
        else {
            j = start_index + this_cpu + thread_event_count*(num_cpus-1) + ECB_entries_group_index(pecb,i) + ECB_entries_emon_event_id_index_local(pecb,i);
            thread_event_count++;
        }
        tmp_value = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
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
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn unc_power_avt_Read_Counts(param, id)
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
unc_power_avt_Read_Counts (
    PVOID  param,
    U32    id
)
{
    UNC_COMMON_MSR_Read_Counts_With_Mask(param, id, 0xFFFFFFFFFFFFFFFFULL);

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn unc_power_avt_Enable_PMU(param)
 *
 * @param    None
 *
 * @return   None
 *
 * @brief    Capture the previous values to calculate delta later.
 */
static VOID
unc_power_avt_Enable_PMU (
    PVOID  param
)
{
    S32                   j;
    U64                  *buffer              = prev_counter_data;
    U32                   dev_idx             = *((U32*)param);
    U32                   start_index;
    DRV_CONFIG            pcfg_unc;
    U32                   this_cpu            = CONTROL_THIS_CPU();
    CPU_STATE             pcpu                = &pcb[this_cpu];
    U32                   num_cpus            = GLOBAL_STATE_num_cpus(driver_state);
    U32                   cur_grp             = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    U32                   package_event_count = 0;
    U32                   thread_event_count  = 0;
    U32                   module_event_count  = 0;

    pcfg_unc    = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);

    // NOTE THAT the enable function currently captures previous values
    // for EMON collection to avoid unnecessary memory copy.
    if (!DRV_CONFIG_emon_mode(pcfg_unc)) {
        return;
    }

    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);

    FOR_EACH_DATA_REG_UNC(pecb, dev_idx, i) {
        j =   start_index + ECB_entries_group_index(pecb,i)  +
               package_event_count*num_packages +
               module_event_count*(GLOBAL_STATE_num_modules(driver_state)) +
               thread_event_count*num_cpus ;
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = j + core_to_package_map[this_cpu];
            package_event_count++;
            if (!CPU_STATE_socket_master(pcpu)) {
                continue;
            }
        }
        else if (ECB_entries_event_scope(pecb,i) == MODULE_EVENT) {
            j = j + CPU_STATE_cpu_module_num(pcpu);
            module_event_count++;
            if (!CPU_STATE_cpu_module_master(pcpu)) {
                continue;
            }
        }
        else {
            j = j + this_cpu;
            thread_event_count++;
        }
        buffer[j] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn unc_power_avt_Read_PMU_Data(param)
 *
 * @param    param    The read thread node to process
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *           Uncore PMU does not support sampling, i.e. ignore the id parameter.
 */
static VOID
unc_power_avt_Read_PMU_Data (
    PVOID  param
)
{
    S32                   j;
    U64                  *buffer              = read_counter_info;
    U64                  *prev_buffer         = prev_counter_data;
    U32                   dev_idx             = *((U32*)param);
    U32                   start_index;
    DRV_CONFIG            pcfg_unc;
    U32                   this_cpu            = CONTROL_THIS_CPU();
    CPU_STATE             pcpu                = &pcb[this_cpu];
    U32                   num_cpus            = GLOBAL_STATE_num_cpus(driver_state);
    U32                   cur_grp             = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    U32                   package_event_count = 0;
    U32                   thread_event_count  = 0;
    U32                   module_event_count  = 0;
    U64                   tmp_value;

    pcfg_unc    = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    start_index = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);

    FOR_EACH_DATA_REG_UNC(pecb, dev_idx, i) {
        j =   start_index + ECB_entries_group_index(pecb,i)  +
               package_event_count*num_packages +
               module_event_count*(GLOBAL_STATE_num_modules(driver_state)) +
               thread_event_count*num_cpus ;
        if (ECB_entries_event_scope(pecb,i) == PACKAGE_EVENT) {
            j = j + core_to_package_map[this_cpu];
            package_event_count++;
            if (!CPU_STATE_socket_master(pcpu)) {
                continue;
            }
        }
        else if (ECB_entries_event_scope(pecb,i) == MODULE_EVENT) {
            j = j + CPU_STATE_cpu_module_num(pcpu);
            module_event_count++;
            if (!CPU_STATE_cpu_module_master(pcpu)) {
                continue;
            }
        }
        else {
            j = j + this_cpu;
            thread_event_count++;
        }
        tmp_value = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
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
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  snb_power_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    unc_power_snb_Write_PMU,     // write
    NULL,                        // freeze
    NULL,                        // restart
    NULL,                        // read
    NULL,                        // check for overflow
    NULL,                        // swap group
    NULL,                        // read lbrs
    NULL,                        // cleanup
    NULL,                        // hw errata
    NULL,                        // read power
    NULL,                        // check overflow errata
    unc_power_snb_Read_Counts,   // read counts
    NULL,                        // check overflow gp errata
    NULL,                        // read_ro
    NULL,                        // platform info
    NULL,
    NULL                         // scan for uncore
};

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_power_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    UNC_COMMON_Dummy_Func,       // write
    NULL,                        // freeze
    unc_power_hsw_Enable_PMU,    // restart
    unc_power_hsw_Read_PMU_Data, // read
    NULL,                        // check for overflow
    NULL,                        // swap group
    NULL,                        // read lbrs
    NULL,                        // cleanup
    NULL,                        // hw errata
    NULL,                        // read power
    NULL,                        // check overflow errata
    unc_power_hsw_Read_Counts,   // read counts
    NULL,                        // check overflow gp errata
    NULL,                        // read_ro
    NULL,                        // platform info
    NULL,
    NULL                         // scan for uncore
};

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  avoton_power_dispatch =
{
    NULL,                         // initialize
    NULL,                         // destroy
    UNC_COMMON_Dummy_Func,        // write
    NULL,                         // freeze
    unc_power_avt_Enable_PMU,     // restart
    unc_power_avt_Read_PMU_Data,  // read
    NULL,                         // check for overflow
    NULL,                         // swap group
    NULL,                         // read lbrs
    NULL,                         // cleanup
    NULL,                         // hw errata
    NULL,                         // read power
    NULL,                         // check overflow errata
    unc_power_avt_Read_Counts,    // read counts
    NULL,                         // check overflow gp errata
    NULL,                         // platform info
    NULL,
    NULL                          // scan for uncore
};
