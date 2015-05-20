/*COPYRIGHT**
    Copyright (C) 2011-2014 Intel Corporation.  All Rights Reserved.

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
#include "hsw_unc_ncu.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "unc_common.h"


static VOID inline
unc_ncu_Enable_PMU(
    VOID *param,
    U32   control_msr
)
{
    ECB pecb_unc;
    U32 dev_idx = *((U32*)param);
 
    pecb_unc = LWPMU_DEVICE_PMU_register_data(&devices[dev_idx])[0];
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        SYS_Write_MSR(control_msr, ECB_entries_reg_value(pecb_unc,0));
    }
}


/*!
 * @fn          static VOID unc_ncu_hsw_Write_PMU(VOID*)
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
unc_ncu_hsw_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             HSW_NCU_PERF_GLOBAL_CTRL,
                             0,
                             0,
                             NULL);

    return;
}

/*!
 * @fn         static VOID unc_ncu_hsw_Disable_PMU(PVOID)
 *
 * @brief      Zero out the global control register.  This automatically disables the
 *             PMU counters.
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_ncu_hsw_Disable_PMU (
    PVOID  param
)
{
    SYS_Write_MSR(HSW_NCU_PERF_GLOBAL_CTRL, 0LL);

    return;
}

/*!
 * @fn         static VOID unc_ncu_hsw_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the CCCR registers
 *
 * @param      param - device index
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_ncu_hsw_Enable_PMU (
    PVOID   param
)
{
    unc_ncu_Enable_PMU(param, HSW_NCU_PERF_GLOBAL_CTRL);

    return;
}


/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswellunc_ncu_dispatch =
{
    NULL,                          // initialize
    NULL,                          // destroy
    unc_ncu_hsw_Write_PMU,         // write
    unc_ncu_hsw_Disable_PMU,       // freeze
    unc_ncu_hsw_Enable_PMU,        // restart
    UNC_COMMON_MSR_Read_PMU_Data,  // read
    NULL,                          // check for overflow
    NULL,
    NULL,
    UNC_COMMON_MSR_Clean_Up,
    NULL,
    NULL,
    NULL,
    UNC_COMMON_MSR_Read_Counts,
    NULL,                          // check_overflow_gp_errata
    NULL,                          // read_ro
    NULL,                          // platform_info
    NULL,
    NULL                           // scan for uncore
};

