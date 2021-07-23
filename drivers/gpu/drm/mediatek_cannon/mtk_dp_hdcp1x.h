/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_DP_HDCP1X_H__
#define __MTK_DP_HDCP1X_H__

#include "mtk_dp_common.h"

#ifdef DPTX_HDCP_ENABLE

#define DP_HDCP1_BINFO_SIZE			2
#define DP_HDCP1_BCAPS_SIZE			1
#define DP_HDCP1_BSTATUS_SIZE			1
#define DP_HDCP1_AN_SIZE			8
#define DP_HDCP1_AKSV_SIZE			5
#define DP_HDCP1_BKSV_SIZE			5
#define DP_HDCP1_AINFO_SIZE			1

#define HDCP1X_BSTATUS_TIMEOUT_CNT              600
#define HDCP1X_R0_WDT                           100
#define HDCP1X_REP_RDY_WDT                      5000

#define HDCP1X_REP_MAXDEVS            128
#define HDCP1X_REAUNTH_COUNT          1

enum DPTX_DRV_HDCP1X_MainStates {
	HDCP1X_MainState_H2 = 0,
	HDCP1X_MainState_A0 = 1,
	HDCP1X_MainState_A1 = 2,
	HDCP1X_MainState_A2 = 3,
	HDCP1X_MainState_A3 = 4,
	HDCP1X_MainState_A4 = 5,
	HDCP1X_MainState_A5 = 6,
	HDCP1X_MainState_A6 = 7,
	HDCP1X_MainState_A7 = 8,
};

enum DPTX_DRV_HDCP1X_SubStates {
	HDCP1X_SubFSM_IDLE              = 0,
	HDCP1X_SubFSM_CHECKHDCPCAPABLE	= 1,
	HDCP1X_SubFSM_ExchangeKSV       = 2,
	HDCP1X_SubFSM_VerifyBksv        = 3,
	HDCP1X_SubFSM_Computation       = 4,
	HDCP1X_SubFSM_CheckR0           = 5,
	HDCP1X_SubFSM_AuthDone          = 6,
	HDCP1X_SubFSM_PollingRdyBit     = 7,
	HDCP1X_SubFSM_AuthWithRepeater  = 8,
	HDCP1X_SubFSM_AuthFail          = 9,
};

bool mdrv_DPTx_HDCP1X_irq(struct mtk_dp *mtk_dp);
bool mdrv_DPTx_HDCP1x_Support(struct mtk_dp *mtk_dp);
void mdrv_DPTx_HDCP1X_FSM(struct mtk_dp *mtk_dp);
void mdrv_DPTx_HDCP1X_SetStartAuth(struct mtk_dp *mtk_dp, bool bEnable);

#endif
#endif

