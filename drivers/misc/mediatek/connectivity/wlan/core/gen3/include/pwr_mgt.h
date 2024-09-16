/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/pwr_mgt.h#1
*/

/*
 * ! \file   "pwr_mgt.h"
 *  \brief  In this file we define the STATE and EVENT for Power Management FSM.
 *
 *   The SCAN FSM is responsible for performing SCAN behavior when the Arbiter enter
 *   ARB_STATE_SCAN. The STATE and EVENT for SCAN FSM are defined here with detail
 *   description.
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
		/* Increase Block to Enter Low Power Semaphore count */ \
		GLUE_INC_REF_CNT(_prAdapter->u4PwrCtrlBlockCnt); \
	}

#define RECLAIM_POWER_CONTROL_TO_PM(_prAdapter, _fgEnableGINT_in_IST) \
	{ \
		ASSERT(_prAdapter->u4PwrCtrlBlockCnt != 0); \
		/* Decrease Block to Enter Low Power Semaphore count */ \
		GLUE_DEC_REF_CNT(_prAdapter->u4PwrCtrlBlockCnt); \
		if (_prAdapter->fgWiFiInSleepyState && (_prAdapter->u4PwrCtrlBlockCnt == 0)) { \
			nicpmSetFWOwn(_prAdapter, _fgEnableGINT_in_IST); \
		} \
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
