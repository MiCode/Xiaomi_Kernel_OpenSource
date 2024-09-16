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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/rlm_obss.h#1
*/

/*! \file   "rlm_obss.h"
*    \brief
*/


#ifndef _RLM_OBSS_H
#define _RLM_OBSS_H

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
#define CHNL_LIST_SZ_2G         14
#define CHNL_LIST_SZ_5G         14

#define CHNL_LEVEL0             0
#define CHNL_LEVEL1             1
#define CHNL_LEVEL2             2

#define AFFECTED_CHNL_OFFSET    5

#define OBSS_SCAN_MIN_INTERVAL  10	/* In unit of sec */

#define PUBLIC_ACTION_MAX_LEN   200	/* In unit of byte */

/* P2P GO only */
/* Define default OBSS Scan parameters (from MIB in spec.) */
#define dot11OBSSScanPassiveDwell                   20
#define dot11OBSSScanActiveDwell                    10
#define dot11OBSSScanPassiveTotalPerChannel         200
#define dot11OBSSScanActiveTotalPerChannel          20
#define dot11BSSWidthTriggerScanInterval            300	/* Unit: sec */
#define dot11BSSWidthChannelTransitionDelayFactor   5
#define dot11OBSSScanActivityThreshold              25

#define OBSS_20_40M_TIMEOUT     (dot11BSSWidthTriggerScanInterval + 10)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* Control MAC PCO function */
typedef enum _ENUM_SYS_PCO_PHASE_T {
	SYS_PCO_PHASE_DISABLED = 0,
	SYS_PCO_PHASE_20M,
	SYS_PCO_PHASE_40M
} ENUM_SYS_PCO_PHASE_T, *P_ENUM_SYS_PCO_PHASE_T;

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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID rlmObssInit(P_ADAPTER_T prAdapter);

VOID rlmObssScanDone(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr);

VOID rlmObssTriggerScan(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo);
VOID rlmObssAbortScan(P_ADAPTER_T prAdapter);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _RLM_OBSS_H */
