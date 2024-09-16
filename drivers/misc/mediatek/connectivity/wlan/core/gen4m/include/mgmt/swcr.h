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
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/include/mgmt/swcr.h#1
 */

/*! \file   "swcr.h"
 *    \brief
 */

/*
 *
 */

#ifndef _SWCR_H
#define _SWCR_H

#include "nic_cmd_event.h"

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
#define TEST_PS 1

#define SWCR_VAR(x) ((void *)&x)
#define SWCR_FUNC(x)  ((void *)x)

#define SWCR_T_FUNC BIT(7)

#define SWCR_L_32 3
#define SWCR_L_16 2
#define SWCR_L_8  1

#define SWCR_READ 0
#define SWCR_WRITE 1

#define SWCR_MAP_NUM(x)  (ARRAY_SIZE(x))

#define SWCR_CR_NUM 7

#define SWCR_GET_RW_INDEX(action, rw, index) \
do { \
	index = action & 0x7F; \
	rw = action >> 7; \
} while (0)

extern uint32_t g_au4SwCr[];	/*: 0: command other: data */

typedef void(*PFN_SWCR_RW_T) (struct ADAPTER *prAdapter,
	uint8_t ucRead, uint16_t u2Addr, uint32_t *pu4Data);
typedef void(*PFN_CMD_RW_T) (struct ADAPTER *prAdapter,
			     uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			     uint8_t ucOpt1);

struct SWCR_MAP_ENTRY {
	uint16_t u2Type;
	void *u4Addr;
};

struct SWCR_MOD_MAP_ENTRY {
	uint8_t ucMapNum;
	struct SWCR_MAP_ENTRY *prSwCrMap;
};

enum ENUM_SWCR_DBG_TYPE {
	SWCR_DBG_TYPE_ALL = 0,
	SWCR_DBG_TYPE_TXRX,
	SWCR_DBG_TYPE_RX_RATES,
	SWCR_DBG_TYPE_PS,
	SWCR_DBG_TYPE_NUM
};

enum ENUM_SWCR_DBG_ALL {
	SWCR_DBG_ALL_TX_CNT = 0,
	SWCR_DBG_ALL_TX_BCN_CNT,
	SWCR_DBG_ALL_TX_FAILED_CNT,
	SWCR_DBG_ALL_TX_RETRY_CNT,
	SWCR_DBG_ALL_TX_AGING_TIMEOUT_CNT,
	SWCR_DBG_ALL_TX_PS_OVERFLOW_CNT,
	SWCR_DBG_ALL_TX_MGNT_DROP_CNT,
	SWCR_DBG_ALL_TX_ERROR_CNT,

	SWCR_DBG_ALL_RX_CNT,
	SWCR_DBG_ALL_RX_DROP_CNT,
	SWCR_DBG_ALL_RX_DUP_DROP_CNT,
	SWCR_DBG_ALL_RX_TYPE_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_CLASS_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_AMPDU_ERROR_DROP_CNT,

	SWCR_DBG_ALL_RX_STATUS_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_FORMAT_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_ICV_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_KEY_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_TKIP_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_MIC_ERROR_DROP_CNT,
	SWCR_DBG_ALL_RX_BIP_ERROR_DROP_CNT,

	SWCR_DBG_ALL_RX_FCSERR_CNT,
	SWCR_DBG_ALL_RX_FIFOFULL_CNT,
	SWCR_DBG_ALL_RX_PFDROP_CNT,

	SWCR_DBG_ALL_PWR_PS_POLL_CNT,
	SWCR_DBG_ALL_PWR_TRIGGER_NULL_CNT,
	SWCR_DBG_ALL_PWR_BCN_IND_CNT,
	SWCR_DBG_ALL_PWR_BCN_TIMEOUT_CNT,
	SWCR_DBG_ALL_PWR_PM_STATE0,
	SWCR_DBG_ALL_PWR_PM_STATE1,
	SWCR_DBG_ALL_PWR_CUR_PS_PROF0,
	SWCR_DBG_ALL_PWR_CUR_PS_PROF1,

	SWCR_DBG_ALL_AR_STA0_RATE,
	SWCR_DBG_ALL_AR_STA0_BWGI,
	SWCR_DBG_ALL_AR_STA0_RX_RATE_RCPI,

	SWCR_DBG_ALL_ROAMING_ENABLE,
	SWCR_DBG_ALL_ROAMING_ROAM_CNT,
	SWCR_DBG_ALL_ROAMING_INT_CNT,

	SWCR_DBG_ALL_BB_RX_MDRDY_CNT,
	SWCR_DBG_ALL_BB_RX_FCSERR_CNT,
	SWCR_DBG_ALL_BB_CCK_PD_CNT,
	SWCR_DBG_ALL_BB_OFDM_PD_CNT,
	SWCR_DBG_ALL_BB_CCK_SFDERR_CNT,
	SWCR_DBG_ALL_BB_CCK_SIGERR_CNT,
	SWCR_DBG_ALL_BB_OFDM_TAGERR_CNT,
	SWCR_DBG_ALL_BB_OFDM_SIGERR_CNT,

	SWCR_DBG_ALL_NUM
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

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

void swCrReadWriteCmd(struct ADAPTER *prAdapter,
		      uint8_t ucRead, uint16_t u2Addr, uint32_t *pu4Data);

/* Debug Support */
void swCrFrameCheckEnable(struct ADAPTER *prAdapter,
			  uint32_t u4DumpType);
void swCrDebugInit(struct ADAPTER *prAdapter);
void swCrDebugCheckEnable(struct ADAPTER *prAdapter,
	u_int8_t fgIsEnable, uint8_t ucType, uint32_t u4Timeout);
void swCrDebugUninit(struct ADAPTER *prAdapter);

#if CFG_SUPPORT_SWCR
void swCtrlCmdCategory0(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1);
void swCtrlCmdCategory1(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1);
#if TEST_PS
void testPsCmdCategory0(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1);
void testPsCmdCategory1(struct ADAPTER *prAdapter,
			uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			uint8_t ucOpt1);
#endif
#if CFG_SUPPORT_802_11V
#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) && (WNM_UNIT_TEST == 1)
void testWNMCmdCategory0(struct ADAPTER *prAdapter,
			 uint8_t ucCate, uint8_t ucAction, uint8_t ucOpt0,
			 uint8_t ucOpt1);
#endif
#endif
void swCtrlSwCr(struct ADAPTER *prAdapter, uint8_t ucRead,
		uint16_t u2Addr, uint32_t *pu4Data);

/* Support Debug */
void swCrDebugCheck(struct ADAPTER *prAdapter,
		    struct CMD_SW_DBG_CTRL *prCmdSwCtrl);
void swCrDebugCheckTimeout(IN struct ADAPTER *prAdapter,
			   unsigned long ulParamPtr);
void swCrDebugQuery(IN struct ADAPTER *prAdapter,
		    IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void swCrDebugQueryTimeout(IN struct ADAPTER *prAdapter,
			   IN struct CMD_INFO *prCmdInfo);
#endif

#endif
