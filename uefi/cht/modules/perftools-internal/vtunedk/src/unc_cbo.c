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
#if !defined (DRV_ANDROID)
#include "ivt_unc_ubox.h"
#include "ivt_unc_cbo.h"
#include "hsx_unc_ubox.h"
#include "hsx_unc_cbo.h"
#endif
#include "snb_unc_cbo.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "unc_common.h"


/***********************************************************************
 *
 * dispatch function for SNB CBO
 *
 ***********************************************************************/


/*!
 * @fn          static VOID unc_cbo_snb_Write_PMU(VOID*)
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
unc_cbo_snb_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             CBO_PERF_GLOBAL_CTRL,
                             0,
                             0,
                             NULL);
    return;
}

/*!
 * @fn         static VOID unc_cbo_snb_Disable_PMU(PVOID)
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
unc_cbo_snb_Disable_PMU (
    PVOID  param
)
{
    SYS_Write_MSR(CBO_PERF_GLOBAL_CTRL, 0LL);

    return;
}


/*!
 * @fn         static VOID unc_cbo_snb_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the CCCR registers
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_cbo_snb_Enable_PMU (
    PVOID   param
)
{
    /*
     * Get the value from the event block
     *   0 == location of the global control reg for this block.
     *   Generalize this location awareness when possible
     */
    ECB pecb_unc;
    U32 dev_idx = *((U32*)param);

    pecb_unc = LWPMU_DEVICE_PMU_register_data(&devices[dev_idx])[0];
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        SYS_Write_MSR(CBO_PERF_GLOBAL_CTRL, ECB_entries_reg_value(pecb_unc,0));
    }

    return;
}


/***********************************************************************
 *
 * dispatch function for IVT CBO
 *
 ***********************************************************************/
 
#if !defined (DRV_ANDROID)
static DRV_BOOL
unc_cbo_ivt_is_Unit_Ctl(
    U32 msr_addr
)
{
    return (IS_THIS_BOX_CTL_MSR(msr_addr));
}

static DRV_BOOL
unc_cbo_ivt_is_PMON_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_EVSEL_PMON_CTL_MSR(msr_addr));
}

DEVICE_CALLBACK_NODE  unc_cbo_ivt_callback = {
    NULL,
    NULL,
    unc_cbo_ivt_is_Unit_Ctl,
    unc_cbo_ivt_is_PMON_Ctl
};


/******************************************************************************************
 * @fn          static VOID unc_cbo_ivt_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_ivt_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                             0,
                             0,
                             NULL);

    return;
}

/******************************************************************************************
 * @fn         static VOID unc_cbo_ivt_Disable_PMU(PVOID)
 *
 * @brief      Disable the per unit global control to stop the PMU counters.
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_ivt_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_MSR_Disable_PMU(param, 
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                              DISABLE_CBO_COUNTERS,
                              0,
                              &unc_cbo_ivt_callback);

    return;
}

/******************************************************************************************
 * @fn         static VOID unc_cbo_ivt_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the EVSEL registers
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_ivt_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_MSR_Enable_PMU(param, 
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR, 
                              ENABLE_ALL_PMU,
                              DISABLE_CBO_COUNTERS,
                              ENABLE_CBO_COUNTERS,
                              &unc_cbo_ivt_callback);
    return;
}



/***********************************************************************
 *
 * dispatch function for HSX CBO
 *
 ***********************************************************************/
 


static DRV_BOOL
unc_cbo_hsx_is_Unit_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_BOX_CTL_MSR(msr_addr));
}

static DRV_BOOL
unc_cbo_hsx_is_PMON_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_EVSEL_PMON_CTL_MSR(msr_addr));
}

DEVICE_CALLBACK_NODE  unc_cbo_hsx_callback = {
    NULL,
    NULL,
    unc_cbo_hsx_is_Unit_Ctl,
    unc_cbo_hsx_is_PMON_Ctl
};


/******************************************************************************************
 * @fn          static VOID unc_cbo_hsx_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_hsx_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                             0,
                             0,
                             NULL);

    return;
}

/******************************************************************************************
 * @fn         static VOID unc_cbo_hsx_Disable_PMU(PVOID)
 *
 * @brief      Disable the per unit global control to stop the PMU counters.
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_hsx_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_MSR_Disable_PMU(param, 
                               HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                               DISABLE_HASWELL_SERVER_CBO_COUNTERS,
                               0,
                               &unc_cbo_hsx_callback);

    return;
}

/******************************************************************************************
 * @fn         static VOID unc_cbo_hsx_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the EVSEL registers
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_cbo_hsx_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_MSR_Enable_PMU(param, 
                          HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                          ENABLE_ALL_HASWELL_SERVER_PMU,
                          DISABLE_HASWELL_SERVER_CBO_COUNTERS,
                          ENABLE_HASWELL_SERVER_CBO_COUNTERS,
                          &unc_cbo_hsx_callback);

    return;
}
#endif

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  snbunc_cbo_dispatch =
{
    NULL,                            // initialize
    NULL,                            // destroy
    unc_cbo_snb_Write_PMU,           // write
    unc_cbo_snb_Disable_PMU,         // freeze
    unc_cbo_snb_Enable_PMU,          // restart
    UNC_COMMON_MSR_Read_PMU_Data,    // read
    NULL,                            // check for overflow
    NULL,
    NULL,
    UNC_COMMON_MSR_Clean_Up,
    NULL,
    NULL,
    NULL,
    UNC_COMMON_MSR_Read_Counts,
    NULL,                           // check_overflow_gp_errata
    NULL,                           // read_ro
    NULL,                           // platform_info
    NULL,
    NULL                            // scan for uncore
};

#if !defined (DRV_ANDROID)
/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  ivtunc_cbo_dispatch =
{
    NULL,                          // initialize
    NULL,                          // destroy
    unc_cbo_ivt_Write_PMU,         // write
    unc_cbo_ivt_Disable_PMU,       // freeze
    unc_cbo_ivt_Enable_PMU,        // restart
    UNC_COMMON_MSR_Read_PMU_Data,  // read
    NULL,                          // check for overflow
    NULL,                          //swap group
    NULL,                          //read lbrs
    NULL,                          //cleanup
    NULL,                          //hw errata
    NULL,                          //read power
    NULL,                          //check overflow errata
    UNC_COMMON_MSR_Read_Counts,    //read counts
    NULL,                          //check overflow gp errata
    NULL,                          // read_ro
    NULL,                          //platform info
    NULL,
    NULL                           // scan for uncore
};

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_server_cbo_dispatch =
{
    NULL,                                // initialize
    NULL,                                // destroy
    unc_cbo_hsx_Write_PMU,               // write
    unc_cbo_hsx_Disable_PMU,             // freeze
    unc_cbo_hsx_Enable_PMU,              // restart
    UNC_COMMON_MSR_Read_PMU_Data,        // read
    NULL,                                // check for overflow
    NULL,                                //swap group
    NULL,                                //read lbrs
    NULL,                                //cleanup
    NULL,                                //hw errata
    NULL,                                //read power
    NULL,                                //check overflow errata
    UNC_COMMON_MSR_Read_Counts,          //read counts
    NULL,                                //check overflow gp errata
    NULL,                                // read_ro
    NULL,                                //platform info
    NULL,
    NULL                                 // scan for uncore
};
#endif
