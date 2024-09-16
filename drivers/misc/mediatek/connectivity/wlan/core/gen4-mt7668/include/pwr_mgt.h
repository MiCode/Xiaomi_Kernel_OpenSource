/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/pwr_mgt.h#1
*/

/*! \file   "pwr_mgt.h"
*    \brief  In this file we define the STATE and EVENT for Power Management FSM.
*
*    The SCAN FSM is responsible for performing SCAN behavior when the Arbiter enter
*    ARB_STATE_SCAN. The STATE and EVENT for SCAN FSM are defined here with detail
*    description.
*/

#ifndef _PWR_MGT_H
#define _PWR_MGT_H
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define PM_UAPSD_AC0                        (BIT(0))
#define PM_UAPSD_AC1                        (BIT(1))
#define PM_UAPSD_AC2                        (BIT(2))
#define PM_UAPSD_AC3                        (BIT(3))

#define PM_UAPSD_ALL                        (PM_UAPSD_AC0 | PM_UAPSD_AC1 | PM_UAPSD_AC2 | PM_UAPSD_AC3)
#define PM_UAPSD_NONE                       0

#define LP_OWN_BACK_TOTAL_DELAY_MS      2048	/* exponential of 2 */
#define LP_OWN_BACK_LOOP_DELAY_MS       1	/* exponential of 2 */
#define LP_OWN_BACK_CLR_OWN_ITERATION   256	/* exponential of 2 */
#define LP_OWN_BACK_FAILED_RETRY_CNT    5
#define LP_OWN_BACK_FAILED_LOG_SKIP_MS  2000
#define LP_OWN_BACK_FAILED_RESET_CNT    5
#define LP_OWN_BACK_FAILED_DBGCR_POLL_ROUND	5
#define LP_DBGCR_POLL_ROUND				1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _PM_PROFILE_SETUP_INFO_T {
	/* Profile setup */
	UINT_8 ucBmpDeliveryAC;	/* 0: AC_BE, 1: AC_BK, 2: AC_VI, 3: AC_VO */
	UINT_8 ucBmpTriggerAC;	/* 0: AC_BE, 1: AC_BK, 2: AC_VI, 3: AC_VO */

	UINT_8 ucUapsdSp;	/* Number of triggered packets in UAPSD */

} PM_PROFILE_SETUP_INFO_T, *P_PM_PROFILE_SETUP_INFO_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#if !CFG_ENABLE_FULL_PM
#define ACQUIRE_POWER_CONTROL_FROM_PM(_prAdapter)
#define RECLAIM_POWER_CONTROL_TO_PM(_prAdapter, _fgEnableGINT_in_IST)
#else
#define ACQUIRE_POWER_CONTROL_FROM_PM(_prAdapter) \
	{ \
			nicpmSetDriverOwn(_prAdapter); \
	}

#define RECLAIM_POWER_CONTROL_TO_PM(_prAdapter, _fgEnableGINT_in_IST) \
	{ \
			nicpmSetFWOwn(_prAdapter, _fgEnableGINT_in_IST); \
	}
#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _PWR_MGT_H */
