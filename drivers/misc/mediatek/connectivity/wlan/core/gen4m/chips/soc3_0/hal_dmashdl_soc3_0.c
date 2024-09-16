/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*! \file   hal_dmashdl_mt6885.c
*    \brief  DMASHDL HAL API for MT6885
*
*    This file contains all routines which are exported
     from MediaTek 802.11 Wireless LAN driver stack to GLUE Layer.
*/

#ifdef SOC3_0
#if defined(_HIF_PCIE) || defined(_HIF_AXI)

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "precomp.h"
#include "soc3_0.h"
#include "coda/soc3_0/wf_hif_dmashdl_top.h"
#include "hal_dmashdl_soc3_0.h"
#include "dma_sch.h"

/*******************************************************************************
*                              C O N S T A N T S
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

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

struct MT6885_DMASHDL_CFG rMT6885DmashdlCfg = {
	.fgSlotArbiterEn = MT6885_DMASHDL_SLOT_ARBITER_EN,

	.u2PktPleMaxPage = MT6885_DMASHDL_PKT_PLE_MAX_PAGE,

	.u2PktPseMaxPage = MT6885_DMASHDL_PKT_PSE_MAX_PAGE,

	.afgRefillEn = {
		MT6885_DMASHDL_GROUP_0_REFILL_EN,
		MT6885_DMASHDL_GROUP_1_REFILL_EN,
		MT6885_DMASHDL_GROUP_2_REFILL_EN,
		MT6885_DMASHDL_GROUP_3_REFILL_EN,
		MT6885_DMASHDL_GROUP_4_REFILL_EN,
		MT6885_DMASHDL_GROUP_5_REFILL_EN,
		MT6885_DMASHDL_GROUP_6_REFILL_EN,
		MT6885_DMASHDL_GROUP_7_REFILL_EN,
		MT6885_DMASHDL_GROUP_8_REFILL_EN,
		MT6885_DMASHDL_GROUP_9_REFILL_EN,
		MT6885_DMASHDL_GROUP_10_REFILL_EN,
		MT6885_DMASHDL_GROUP_11_REFILL_EN,
		MT6885_DMASHDL_GROUP_12_REFILL_EN,
		MT6885_DMASHDL_GROUP_13_REFILL_EN,
		MT6885_DMASHDL_GROUP_14_REFILL_EN,
		MT6885_DMASHDL_GROUP_15_REFILL_EN,
	},

	.au2MaxQuota = {
		MT6885_DMASHDL_GROUP_0_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_1_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_2_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_3_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_4_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_5_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_6_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_7_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_8_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_9_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_10_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_11_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_12_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_13_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_14_MAX_QUOTA,
		MT6885_DMASHDL_GROUP_15_MAX_QUOTA,
	},

	.au2MinQuota = {
		MT6885_DMASHDL_GROUP_0_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_1_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_2_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_3_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_4_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_5_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_6_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_7_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_8_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_9_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_10_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_11_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_12_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_13_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_14_MIN_QUOTA,
		MT6885_DMASHDL_GROUP_15_MIN_QUOTA,
	},

	.aucQueue2Group = {
		MT6885_DMASHDL_QUEUE_0_TO_GROUP,
		MT6885_DMASHDL_QUEUE_1_TO_GROUP,
		MT6885_DMASHDL_QUEUE_2_TO_GROUP,
		MT6885_DMASHDL_QUEUE_3_TO_GROUP,
		MT6885_DMASHDL_QUEUE_4_TO_GROUP,
		MT6885_DMASHDL_QUEUE_5_TO_GROUP,
		MT6885_DMASHDL_QUEUE_6_TO_GROUP,
		MT6885_DMASHDL_QUEUE_7_TO_GROUP,
		MT6885_DMASHDL_QUEUE_8_TO_GROUP,
		MT6885_DMASHDL_QUEUE_9_TO_GROUP,
		MT6885_DMASHDL_QUEUE_10_TO_GROUP,
		MT6885_DMASHDL_QUEUE_11_TO_GROUP,
		MT6885_DMASHDL_QUEUE_12_TO_GROUP,
		MT6885_DMASHDL_QUEUE_13_TO_GROUP,
		MT6885_DMASHDL_QUEUE_14_TO_GROUP,
		MT6885_DMASHDL_QUEUE_15_TO_GROUP,
		MT6885_DMASHDL_QUEUE_16_TO_GROUP,
		MT6885_DMASHDL_QUEUE_17_TO_GROUP,
		MT6885_DMASHDL_QUEUE_18_TO_GROUP,
		MT6885_DMASHDL_QUEUE_19_TO_GROUP,
		MT6885_DMASHDL_QUEUE_20_TO_GROUP,
		MT6885_DMASHDL_QUEUE_21_TO_GROUP,
		MT6885_DMASHDL_QUEUE_22_TO_GROUP,
		MT6885_DMASHDL_QUEUE_23_TO_GROUP,
		MT6885_DMASHDL_QUEUE_24_TO_GROUP,
		MT6885_DMASHDL_QUEUE_25_TO_GROUP,
		MT6885_DMASHDL_QUEUE_26_TO_GROUP,
		MT6885_DMASHDL_QUEUE_27_TO_GROUP,
		MT6885_DMASHDL_QUEUE_28_TO_GROUP,
		MT6885_DMASHDL_QUEUE_29_TO_GROUP,
		MT6885_DMASHDL_QUEUE_30_TO_GROUP,
		MT6885_DMASHDL_QUEUE_31_TO_GROUP,
	},

	.aucPriority2Group = {
		MT6885_DMASHDL_PRIORITY0_GROUP,
		MT6885_DMASHDL_PRIORITY1_GROUP,
		MT6885_DMASHDL_PRIORITY2_GROUP,
		MT6885_DMASHDL_PRIORITY3_GROUP,
		MT6885_DMASHDL_PRIORITY4_GROUP,
		MT6885_DMASHDL_PRIORITY5_GROUP,
		MT6885_DMASHDL_PRIORITY6_GROUP,
		MT6885_DMASHDL_PRIORITY7_GROUP,
		MT6885_DMASHDL_PRIORITY8_GROUP,
		MT6885_DMASHDL_PRIORITY9_GROUP,
		MT6885_DMASHDL_PRIORITY10_GROUP,
		MT6885_DMASHDL_PRIORITY11_GROUP,
		MT6885_DMASHDL_PRIORITY12_GROUP,
		MT6885_DMASHDL_PRIORITY13_GROUP,
		MT6885_DMASHDL_PRIORITY14_GROUP,
		MT6885_DMASHDL_PRIORITY15_GROUP,
	},
};

void mt6885HalDmashdlSetPlePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	uint32_t u4Val = 0;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_MASK;
	u4Val |= (u2MaxPage <<
		  WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_SHFT) &
		 WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, u4Val);
}

void mt6885HalDmashdlSetPsePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	uint32_t u4Val = 0;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_MASK;
	u4Val |= (u2MaxPage <<
		  WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_SHFT) &
		 WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, u4Val);
}

void mt6885HalDmashdlGetPktMaxPage(struct ADAPTER *prAdapter)
{
	uint32_t u4Val = 0;
	uint32_t ple_pkt_max_sz;
	uint32_t pse_pkt_max_sz;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, &u4Val);

	ple_pkt_max_sz = (u4Val &
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_MASK)>>
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_SHFT;
	pse_pkt_max_sz = (u4Val &
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_MASK)>>
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_SHFT;

	DBGLOG(HAL, INFO, "DMASHDL PLE_PACKET_MAX_SIZE (0x%08x): 0x%08x\n",
		WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, u4Val);
	DBGLOG(HAL, INFO, "PLE/PSE packet max size=0x%03x/0x%03x\n",
		ple_pkt_max_sz, pse_pkt_max_sz);

}
void mt6885HalDmashdlSetRefill(struct ADAPTER *prAdapter, uint8_t ucGroup,
			       u_int8_t fgEnable)
{
	uint32_t u4Mask;
	uint32_t u4Val = 0;

	if (ucGroup >= ENUM_MT6885_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Mask = WF_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP0_REFILL_DISABLE_MASK
		<< ucGroup;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR, &u4Val);

	if (fgEnable)
		u4Val &= ~u4Mask;
	else
		u4Val |= u4Mask;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR, u4Val);
}

void mt6885HalDmashdlGetRefill(struct ADAPTER *prAdapter)
{
	uint32_t u4Val = 0;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR, &u4Val);
	DBGLOG(HAL, INFO, "DMASHDL ReFill Control (0x%08x): 0x%08x\n",
		WF_HIF_DMASHDL_TOP_REFILL_CONTROL_ADDR, u4Val);
}

void mt6885HalDmashdlSetMaxQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MaxQuota)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;

	if (ucGroup >= ENUM_MT6885_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Addr = WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_MASK;
	u4Val |= (u2MaxQuota <<
		  WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_SHFT) &
		 WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_MASK;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void mt6885HalDmashdlSetMinQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MinQuota)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;

	if (ucGroup >= ENUM_MT6885_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Addr = WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_MASK;
	u4Val |= (u2MinQuota <<
		  WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_SHFT) &
		 WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_MASK;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void mt6885HalDmashdlGetGroupControl(struct ADAPTER *prAdapter, uint8_t ucGroup)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t max_quota;
	uint32_t min_quota;

	u4Addr = WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	max_quota = GET_DMASHDL_MAX_QUOTA_NUM(u4Val);
	min_quota = GET_DMASHDL_MIN_QUOTA_NUM(u4Val);
	DBGLOG(HAL, INFO, "\tDMASHDL Group%d control(0x%08x): 0x%08x\n",
		ucGroup, u4Addr, u4Val);
	DBGLOG(HAL, INFO, "\tmax/min quota = 0x%03x/ 0x%03x\n",
		max_quota, min_quota);

}
void mt6885HalDmashdlSetQueueMapping(struct ADAPTER *prAdapter, uint8_t ucQueue,
				     uint8_t ucGroup)
{
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;

	if (ucQueue >= 32)
		ASSERT(0);

	if (ucGroup >= ENUM_MT6885_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Addr = WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_ADDR +
		 ((ucQueue >> 3) << 2);
	u4Mask = WF_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE0_MAPPING_MASK <<
		 ((ucQueue % 8) << 2);
	u4Shft = (ucQueue % 8) << 2;

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~u4Mask;
	u4Val |= (ucGroup << u4Shft) & u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void mt6885HalDmashdlSetSlotArbiter(struct ADAPTER *prAdapter,
				    u_int8_t fgEnable)
{
	uint32_t u4Val = 0;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR, &u4Val);

	if (fgEnable)
		u4Val |=
		 WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;
	else
		u4Val &=
		~WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR, u4Val);
}

void mt6885HalDmashdlSetUserDefinedPriority(struct ADAPTER *prAdapter,
					    uint8_t ucPriority, uint8_t ucGroup)
{
	uint32_t u4Addr, u4Mask, u4Shft;
	uint32_t u4Val = 0;

	ASSERT(ucPriority < 16);
	ASSERT(ucGroup < ENUM_MT6885_DMASHDL_GROUP_NUM);

	u4Addr = WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING0_ADDR +
		((ucPriority >> 3) << 2);
	u4Mask = WF_HIF_DMASHDL_TOP_HIF_SCHEDULER_SETTING0_PRIORITY0_GROUP_MASK
		 << ((ucPriority % 8) << 2);
	u4Shft = (ucPriority % 8) << 2;

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~u4Mask;
	u4Val |= (ucGroup << u4Shft) & u4Mask;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

uint32_t mt6885HalDmashdlGetRsvCount(struct ADAPTER *prAdapter, uint8_t ucGroup)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t rsv_cnt = 0;

	u4Addr = WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	rsv_cnt = (u4Val & WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_RSV_CNT_SHFT;

	DBGLOG(HAL, INFO, "\tDMASHDL Status_RD_GP%d(0x%08x): 0x%08x\n",
		ucGroup, u4Addr, u4Val);
	DBGLOG(HAL, TRACE, "\trsv_cnt = 0x%03x\n", rsv_cnt);
	return rsv_cnt;
}

uint32_t mt6885HalDmashdlGetSrcCount(struct ADAPTER *prAdapter, uint8_t ucGroup)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t src_cnt = 0;

	u4Addr = WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	src_cnt = (u4Val & WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_MASK) >>
			WF_HIF_DMASHDL_TOP_STATUS_RD_GP0_G0_SRC_CNT_SHFT;

	DBGLOG(HAL, TRACE, "\tsrc_cnt = 0x%03x\n", src_cnt);
	return src_cnt;
}

void mt6885HalDmashdlGetPKTCount(struct ADAPTER *prAdapter, uint8_t ucGroup)
{
	uint32_t u4Addr;
	uint32_t u4Val = 0;
	uint32_t pktin_cnt = 0;
	uint32_t ask_cnt = 0;

	if ((ucGroup & 0x1) == 0)
		u4Addr = WF_HIF_DMASHDL_TOP_RD_GROUP_PKT_CNT0_ADDR
				+ (ucGroup << 1);
	else
		u4Addr = WF_HIF_DMASHDL_TOP_RD_GROUP_PKT_CNT0_ADDR
				+ ((ucGroup-1) << 1);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);
	DBGLOG(HAL, INFO, "\tDMASHDL RD_group_pkt_cnt_%d(0x%08x): 0x%08x\n",
		ucGroup / 2, u4Addr, u4Val);
	if ((ucGroup & 0x1) == 0) {
		pktin_cnt = GET_EVEN_GROUP_PKT_IN_CNT(u4Val);
		ask_cnt = GET_EVEN_GROUP_ASK_CNT(u4Val);
	} else {
		pktin_cnt = GET_ODD_GROUP_PKT_IN_CNT(u4Val);
		ask_cnt = GET_ODD_GROUP_ASK_CNT(u4Val);
	}
	DBGLOG(HAL, INFO, "\tpktin_cnt = 0x%02x, ask_cnt = 0x%02x",
		pktin_cnt, ask_cnt);
}

void mt6885DmashdlInit(struct ADAPTER *prAdapter)
{
	uint32_t idx;

	mt6885HalDmashdlSetPlePktMaxPage(prAdapter,
					 rMT6885DmashdlCfg.u2PktPleMaxPage);

	for (idx = 0; idx < ENUM_MT6885_DMASHDL_GROUP_NUM; idx++) {
		mt6885HalDmashdlSetRefill(prAdapter, idx,
					  rMT6885DmashdlCfg.afgRefillEn[idx]);

		mt6885HalDmashdlSetMaxQuota(prAdapter, idx,
					    rMT6885DmashdlCfg.au2MaxQuota[idx]);

		mt6885HalDmashdlSetMinQuota(prAdapter, idx,
					    rMT6885DmashdlCfg.au2MinQuota[idx]);
	}

	for (idx = 0; idx < 32; idx++)
		mt6885HalDmashdlSetQueueMapping(prAdapter, idx,
					 rMT6885DmashdlCfg.aucQueue2Group[idx]);

	for (idx = 0; idx < 16; idx++)
		mt6885HalDmashdlSetUserDefinedPriority(prAdapter, idx,
				      rMT6885DmashdlCfg.aucPriority2Group[idx]);

	mt6885HalDmashdlSetSlotArbiter(prAdapter,
				       rMT6885DmashdlCfg.fgSlotArbiterEn);
}

#endif /* defined(_HIF_PCIE) || defined(_HIF_AXI) */
#endif /* SOC3_0*/
