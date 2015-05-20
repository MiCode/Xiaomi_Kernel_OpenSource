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
#include "avoton_power.h"
#include "ecb_iterators.h"

extern DRV_CONFIG     pcfg;
extern U64           *read_unc_ctr_info;

/******************************************************************************************
 * @fn          static VOID avoton_power_Write_PMU(VOID*)
 *
 * @brief       No registers to write and setup the accumalators with initial values
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
avoton_power_Write_PMU (
    VOID  *param
)
{

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn avoton_power_Read_Counts(param, id)
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
avoton_power_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64  *data       = (U64*) param;
    U32   cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB   pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];

    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;

    FOR_EACH_DATA_REG_UNC(pecb, id, i) {
        data  = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        *data = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn avoton_power_Read_PMU_Data(param)
 *
 * @param    param    The read thread node to process
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *           Uncore PMU does not support sampling, i.e. ignore the id parameter.
 */
static VOID
avoton_power_Read_PMU_Data (
    PVOID  param
)
{
    S32                   j;
    U64                  *buffer              = read_unc_ctr_info;
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
        //SEP_PRINT_DEBUG("cpu=%d j=%d mec=%d mid=%d tec=%d i=%d gi=%d ei=%d count=%llu\n", this_cpu, j, module_event_count, CPU_STATE_cpu_module_num(pcpu), thread_event_count, i, ECB_entries_group_index(pecb,i), ECB_entries_emon_event_id_index_local(pecb,i), buffer[j]);
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  avoton_power_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    avoton_power_Write_PMU,      // write
    NULL,                        // freeze
    NULL,                        // restart
    avoton_power_Read_PMU_Data,  // read
    NULL,                        // check for overflow
    NULL,                        // swap group
    NULL,                        // read lbrs
    NULL,                        // cleanup
    NULL,                        // hw errata
    NULL,                        // read power
    NULL,                        // check overflow errata
    avoton_power_Read_Counts,    // read counts
    NULL,                        // check overflow gp errata
    NULL,                        // platform info
    NULL,
    NULL                         // scan for uncore
};
