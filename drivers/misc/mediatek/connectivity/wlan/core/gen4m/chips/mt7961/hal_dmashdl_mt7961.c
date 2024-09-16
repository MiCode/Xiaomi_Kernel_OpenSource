/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2019 MediaTek Inc.
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
 * Copyright(C) 2019 MediaTek Inc. All rights reserved.
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
/*! \file   hal_dmashdl_mt7961.c
*    \brief  DMASHDL HAL API for MT7961
*
*    This file contains all routines which are exported
     from MediaTek 802.11 Wireless LAN driver stack to GLUE Layer.
*/

#ifdef MT7961
#if defined(_HIF_PCIE) || defined(_HIF_AXI) || defined(_HIF_USB)

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "precomp.h"
#include "mt7961.h"
#include "coda/mt7961/wf_hif_dmashdl_top.h"
#include "hal_dmashdl_mt7961.h"

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

struct MT7961_DMASHDL_CFG rMT7961DmashdlCfg = {
	.fgSlotArbiterEn = MT7961_DMASHDL_SLOT_ARBITER_EN,

	.u2PktPleMaxPage = MT7961_DMASHDL_PKT_PLE_MAX_PAGE,

	.u2PktPseMaxPage = MT7961_DMASHDL_PKT_PSE_MAX_PAGE,

	.afgRefillEn = {
		MT7961_DMASHDL_GROUP_0_REFILL_EN,
		MT7961_DMASHDL_GROUP_1_REFILL_EN,
		MT7961_DMASHDL_GROUP_2_REFILL_EN,
		MT7961_DMASHDL_GROUP_3_REFILL_EN,
		MT7961_DMASHDL_GROUP_4_REFILL_EN,
		MT7961_DMASHDL_GROUP_5_REFILL_EN,
		MT7961_DMASHDL_GROUP_6_REFILL_EN,
		MT7961_DMASHDL_GROUP_7_REFILL_EN,
		MT7961_DMASHDL_GROUP_8_REFILL_EN,
		MT7961_DMASHDL_GROUP_9_REFILL_EN,
		MT7961_DMASHDL_GROUP_10_REFILL_EN,
		MT7961_DMASHDL_GROUP_11_REFILL_EN,
		MT7961_DMASHDL_GROUP_12_REFILL_EN,
		MT7961_DMASHDL_GROUP_13_REFILL_EN,
		MT7961_DMASHDL_GROUP_14_REFILL_EN,
		MT7961_DMASHDL_GROUP_15_REFILL_EN,
	},

	.au2MaxQuota = {
		MT7961_DMASHDL_GROUP_0_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_1_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_2_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_3_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_4_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_5_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_6_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_7_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_8_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_9_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_10_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_11_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_12_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_13_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_14_MAX_QUOTA,
		MT7961_DMASHDL_GROUP_15_MAX_QUOTA,
	},

	.au2MinQuota = {
		MT7961_DMASHDL_GROUP_0_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_1_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_2_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_3_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_4_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_5_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_6_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_7_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_8_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_9_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_10_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_11_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_12_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_13_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_14_MIN_QUOTA,
		MT7961_DMASHDL_GROUP_15_MIN_QUOTA,
	},

	.aucQueue2Group = {
		MT7961_DMASHDL_QUEUE_0_TO_GROUP,
		MT7961_DMASHDL_QUEUE_1_TO_GROUP,
		MT7961_DMASHDL_QUEUE_2_TO_GROUP,
		MT7961_DMASHDL_QUEUE_3_TO_GROUP,
		MT7961_DMASHDL_QUEUE_4_TO_GROUP,
		MT7961_DMASHDL_QUEUE_5_TO_GROUP,
		MT7961_DMASHDL_QUEUE_6_TO_GROUP,
		MT7961_DMASHDL_QUEUE_7_TO_GROUP,
		MT7961_DMASHDL_QUEUE_8_TO_GROUP,
		MT7961_DMASHDL_QUEUE_9_TO_GROUP,
		MT7961_DMASHDL_QUEUE_10_TO_GROUP,
		MT7961_DMASHDL_QUEUE_11_TO_GROUP,
		MT7961_DMASHDL_QUEUE_12_TO_GROUP,
		MT7961_DMASHDL_QUEUE_13_TO_GROUP,
		MT7961_DMASHDL_QUEUE_14_TO_GROUP,
		MT7961_DMASHDL_QUEUE_15_TO_GROUP,
		MT7961_DMASHDL_QUEUE_16_TO_GROUP,
		MT7961_DMASHDL_QUEUE_17_TO_GROUP,
		MT7961_DMASHDL_QUEUE_18_TO_GROUP,
		MT7961_DMASHDL_QUEUE_19_TO_GROUP,
		MT7961_DMASHDL_QUEUE_20_TO_GROUP,
		MT7961_DMASHDL_QUEUE_21_TO_GROUP,
		MT7961_DMASHDL_QUEUE_22_TO_GROUP,
		MT7961_DMASHDL_QUEUE_23_TO_GROUP,
		MT7961_DMASHDL_QUEUE_24_TO_GROUP,
		MT7961_DMASHDL_QUEUE_25_TO_GROUP,
		MT7961_DMASHDL_QUEUE_26_TO_GROUP,
		MT7961_DMASHDL_QUEUE_27_TO_GROUP,
		MT7961_DMASHDL_QUEUE_28_TO_GROUP,
		MT7961_DMASHDL_QUEUE_29_TO_GROUP,
		MT7961_DMASHDL_QUEUE_30_TO_GROUP,
		MT7961_DMASHDL_QUEUE_31_TO_GROUP,
	},

	.aucPriority2Group = {
		MT7961_DMASHDL_PRIORITY0_GROUP,
		MT7961_DMASHDL_PRIORITY1_GROUP,
		MT7961_DMASHDL_PRIORITY2_GROUP,
		MT7961_DMASHDL_PRIORITY3_GROUP,
		MT7961_DMASHDL_PRIORITY4_GROUP,
		MT7961_DMASHDL_PRIORITY5_GROUP,
		MT7961_DMASHDL_PRIORITY6_GROUP,
		MT7961_DMASHDL_PRIORITY7_GROUP,
		MT7961_DMASHDL_PRIORITY8_GROUP,
		MT7961_DMASHDL_PRIORITY9_GROUP,
		MT7961_DMASHDL_PRIORITY10_GROUP,
		MT7961_DMASHDL_PRIORITY11_GROUP,
		MT7961_DMASHDL_PRIORITY12_GROUP,
		MT7961_DMASHDL_PRIORITY13_GROUP,
		MT7961_DMASHDL_PRIORITY14_GROUP,
		MT7961_DMASHDL_PRIORITY15_GROUP,
	},
};

void mt7961HalDmashdlSetPlePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	uint32_t u4Val;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_MASK;
	u4Val |= (u2MaxPage <<
		  WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_SHFT) &
		 WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PLE_PACKET_MAX_SIZE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, u4Val);
}

void mt7961HalDmashdlSetPsePktMaxPage(struct ADAPTER *prAdapter,
				      uint16_t u2MaxPage)
{
	uint32_t u4Val;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_MASK;
	u4Val |= (u2MaxPage <<
		  WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_SHFT) &
		 WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_PSE_PACKET_MAX_SIZE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PACKET_MAX_SIZE_ADDR, u4Val);
}

void mt7961HalDmashdlSetRefill(struct ADAPTER *prAdapter, uint8_t ucGroup,
			       u_int8_t fgEnable)
{
	uint32_t u4Val, u4Mask;

	if (ucGroup >= ENUM_MT7961_DMASHDL_GROUP_NUM)
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

void mt7961HalDmashdlSetMaxQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MaxQuota)
{
	uint32_t u4Addr, u4Val;

	if (ucGroup >= ENUM_MT7961_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Addr = WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_MASK;
	u4Val |= (u2MaxQuota <<
		  WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_SHFT) &
		 WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MAX_QUOTA_MASK;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void mt7961HalDmashdlSetMinQuota(struct ADAPTER *prAdapter, uint8_t ucGroup,
				 uint16_t u2MinQuota)
{
	uint32_t u4Addr, u4Val;

	if (ucGroup >= ENUM_MT7961_DMASHDL_GROUP_NUM)
		ASSERT(0);

	u4Addr = WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_ADDR + (ucGroup << 2);

	HAL_MCR_RD(prAdapter, u4Addr, &u4Val);

	u4Val &= ~WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_MASK;
	u4Val |= (u2MinQuota <<
		  WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_SHFT) &
		 WF_HIF_DMASHDL_TOP_GROUP0_CONTROL_GROUP0_MIN_QUOTA_MASK;

	HAL_MCR_WR(prAdapter, u4Addr, u4Val);
}

void mt7961HalDmashdlSetQueueMapping(struct ADAPTER *prAdapter, uint8_t ucQueue,
				     uint8_t ucGroup)
{
	uint32_t u4Addr, u4Val, u4Mask, u4Shft;

	if (ucQueue >= 32)
		ASSERT(0);

	if (ucGroup >= ENUM_MT7961_DMASHDL_GROUP_NUM)
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

void mt7961HalDmashdlSetSlotArbiter(struct ADAPTER *prAdapter,
				    u_int8_t fgEnable)
{
	uint32_t u4Val;

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR, &u4Val);

	if (fgEnable)
		u4Val |=
		 WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;
	else
		u4Val &=
		~WF_HIF_DMASHDL_TOP_PAGE_SETTING_GROUP_SEQUENCE_ORDER_TYPE_MASK;

	HAL_MCR_WR(prAdapter, WF_HIF_DMASHDL_TOP_PAGE_SETTING_ADDR, u4Val);
}

void mt7961HalDmashdlSetUserDefinedPriority(struct ADAPTER *prAdapter,
					    uint8_t ucPriority, uint8_t ucGroup)
{
	uint32_t u4Addr, u4Val, u4Mask, u4Shft;

	ASSERT(ucPriority < 16);
	ASSERT(ucGroup < ENUM_MT7961_DMASHDL_GROUP_NUM);

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

void mt7961DmashdlInit(struct ADAPTER *prAdapter)
{
	uint32_t idx;

	mt7961HalDmashdlSetPlePktMaxPage(prAdapter,
					 rMT7961DmashdlCfg.u2PktPleMaxPage);

	mt7961HalDmashdlSetPsePktMaxPage(prAdapter,
					 rMT7961DmashdlCfg.u2PktPseMaxPage);

	for (idx = 0; idx < ENUM_MT7961_DMASHDL_GROUP_NUM; idx++) {
		mt7961HalDmashdlSetRefill(prAdapter, idx,
					  rMT7961DmashdlCfg.afgRefillEn[idx]);

		mt7961HalDmashdlSetMaxQuota(prAdapter, idx,
					    rMT7961DmashdlCfg.au2MaxQuota[idx]);

		mt7961HalDmashdlSetMinQuota(prAdapter, idx,
					    rMT7961DmashdlCfg.au2MinQuota[idx]);
	}

	for (idx = 0; idx < 32; idx++)
		mt7961HalDmashdlSetQueueMapping(prAdapter, idx,
					 rMT7961DmashdlCfg.aucQueue2Group[idx]);

	for (idx = 0; idx < 16; idx++)
		mt7961HalDmashdlSetUserDefinedPriority(prAdapter, idx,
				      rMT7961DmashdlCfg.aucPriority2Group[idx]);

	mt7961HalDmashdlSetSlotArbiter(prAdapter,
				       rMT7961DmashdlCfg.fgSlotArbiterEn);
}

#endif /* defined(_HIF_PCIE) || defined(_HIF_AXI) || defined(_HIF_USB) */
#endif /* MT7961 */
