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
 * Id: @(#)
 */

/*! \file   "cnm_scan.h"
 *   \brief
 *
 */


#ifndef _CNM_SCAN_H
#define _CNM_SCAN_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define SCN_CHANNEL_DWELL_TIME_MIN_MSEC		12
#define SCN_CHANNEL_DWELL_TIME_EXT_MSEC		98

#define SCN_TOTAL_PROBEREQ_NUM_FOR_FULL		3
#define SCN_SPECIFIC_PROBEREQ_NUM_FOR_FULL	1

#define SCN_TOTAL_PROBEREQ_NUM_FOR_PARTIAL	2
#define SCN_SPECIFIC_PROBEREQ_NUM_FOR_PARTIAL	1

/* Used by partial scan */
#define SCN_INTERLACED_CHANNEL_GROUPS_NUM	3

#define SCN_PARTIAL_SCAN_NUM			3

#define SCN_PARTIAL_SCAN_IDLE_MSEC		100

#define SCN_P2P_FULL_SCAN_PARAM			0

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* The type of Scan Source */
enum ENUM_SCN_REQ_SOURCE {
	SCN_REQ_SOURCE_HEM = 0,
	SCN_REQ_SOURCE_NET_FSM,
	SCN_REQ_SOURCE_ROAMING,	/* ROAMING Module is independent of AIS FSM */
	SCN_REQ_SOURCE_OBSS,	/* 2.4G OBSS scan */
	SCN_REQ_SOURCE_NUM
};

enum ENUM_SCAN_PROFILE {
	SCAN_PROFILE_FULL = 0,
	SCAN_PROFILE_PARTIAL,
	SCAN_PROFILE_VOIP,
	SCAN_PROFILE_FULL_2G4,
	SCAN_PROFILE_NUM
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if 0
void cnmScanInit(void);

void cnmScanRunEventScanRequest(IN struct MSG_HDR *prMsgHdr);

u_int8_t cnmScanRunEventScanAbort(IN struct MSG_HDR *prMsgHdr);

void cnmScanProfileSelection(void);

void cnmScanProcessStart(void);

void cnmScanProcessStop(void);

void cnmScanRunEventReqAISAbsDone(IN struct MSG_HDR *prMsgHdr);

void cnmScanRunEventCancelAISAbsDone(IN struct MSG_HDR *prMsgHdr);

void cnmScanPartialScanTimeout(uint32_t u4Param);

void cnmScanRunEventScnFsmComplete(IN struct MSG_HDR *prMsgHdr);
#endif

#endif /* _CNM_SCAN_H */
