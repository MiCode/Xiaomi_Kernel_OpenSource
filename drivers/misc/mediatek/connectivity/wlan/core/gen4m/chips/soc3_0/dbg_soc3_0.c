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
/******************************************************************************
 *[File]             dbg_soc3_0.c
 *[Version]          v1.0
 *[Revision Date]    2019-09-09
 *[Author]
 *[Description]
 *    The program provides WIFI FALCON MAC Debug APIs
 *[Copyright]
 *    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/

#ifdef SOC3_0
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "coda/soc3_0/wf_ple_top.h"
#include "coda/soc3_0/wf_pse_top.h"
#include "coda/soc3_0/wf_wfdma_host_dma0.h"
#include "coda/soc3_0/wf_wfdma_host_dma1.h"
#include "coda/soc3_0/wf_hif_dmashdl_top.h"
#include "precomp.h"
#include "mt_dmac.h"
#include "wf_ple.h"
#include "hal_dmashdl_soc3_0.h"
#include "soc3_0.h"
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/* TODO : need to Merge related API with non-driver base */
#define WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE 1

/* define  WFDMA CODA here */
#define WF_WFDMA_MCU_DMA0_WPDMA_TX_RING0_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_0_BASE + 0x300)
#define WF_WFDMA_MCU_DMA1_WPDMA_TX_RING0_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_1_BASE + 0x300)
#define WF_WFDMA_MCU_DMA1_WPDMA_TX_RING1_CTRL0_ADDR \
				(CONNAC2X_MCU_WPDMA_1_BASE + 0x310)
#define WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_0_BASE + 0x500)
#define WF_WFDMA_MCU_DMA0_WPDMA_RX_RING1_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_0_BASE + 0x510)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING0_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_1_BASE + 0x500)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING1_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_1_BASE + 0x510)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING2_CTRL0_ADDR \
			(CONNAC2X_MCU_WPDMA_1_BASE + 0x520)

/* define PLE/PSE FSM CR here */
#define WF_PLE_FSM_PEEK_CR_00 (WF_PLE_TOP_BASE + 0x03D0)
#define WF_PLE_FSM_PEEK_CR_01 (WF_PLE_TOP_BASE + 0x03D4)
#define WF_PLE_FSM_PEEK_CR_02 (WF_PLE_TOP_BASE + 0x03D8)
#define WF_PLE_FSM_PEEK_CR_03 (WF_PLE_TOP_BASE + 0x03DC)
#define WF_PLE_FSM_PEEK_CR_04 (WF_PLE_TOP_BASE + 0x03E0)
#define WF_PLE_FSM_PEEK_CR_05 (WF_PLE_TOP_BASE + 0x03E4)
#define WF_PLE_FSM_PEEK_CR_06 (WF_PLE_TOP_BASE + 0x03E8)
#define WF_PLE_FSM_PEEK_CR_07 (WF_PLE_TOP_BASE + 0x03EC)
#define WF_PLE_FSM_PEEK_CR_08 (WF_PLE_TOP_BASE + 0x03F0)
#define WF_PLE_FSM_PEEK_CR_09 (WF_PLE_TOP_BASE + 0x03F4)
#define WF_PLE_FSM_PEEK_CR_10 (WF_PLE_TOP_BASE + 0x03F8)
#define WF_PLE_FSM_PEEK_CR_11 (WF_PLE_TOP_BASE + 0x03FC)

#define WF_PSE_FSM_PEEK_CR_00 (WF_PSE_TOP_BASE + 0x03D0)
#define WF_PSE_FSM_PEEK_CR_01 (WF_PSE_TOP_BASE + 0x03D4)
#define WF_PSE_FSM_PEEK_CR_02 (WF_PSE_TOP_BASE + 0x03D8)
#define WF_PSE_FSM_PEEK_CR_03 (WF_PSE_TOP_BASE + 0x03DC)
#define WF_PSE_FSM_PEEK_CR_04 (WF_PSE_TOP_BASE + 0x03E0)
#define WF_PSE_FSM_PEEK_CR_05 (WF_PSE_TOP_BASE + 0x03E4)
#define WF_PSE_FSM_PEEK_CR_06 (WF_PSE_TOP_BASE + 0x03E8)
#define WF_PSE_FSM_PEEK_CR_07 (WF_PSE_TOP_BASE + 0x03EC)
#define WF_PSE_FSM_PEEK_CR_08 (WF_PSE_TOP_BASE + 0x03F0)
#define WF_PSE_FSM_PEEK_CR_09 (WF_PSE_TOP_BASE + 0x03F4)

#if WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE
/* define for read Driver Base CR */
#define CONNAC2X_MCU_WPDMA_0_DRIVER_BASE		0x18402000
#define CONNAC2X_MCU_WPDMA_1_DRIVER_BASE		0x18403000
#define CONNAC2X_HOST_WPDMA_0_DRIVER_BASE		0x18024000
#define CONNAC2X_HOST_WPDMA_1_DRIVER_BASE		0x18025000

#define CONNAC2X_HOST_EXT_CONN_HIF_WRAP_DRIVER_BASE	0x18027000
#define CONNAC2X_MCU_INT_CONN_HIF_WRAP_DRIVER_BASE	0x18405000

#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x300) /* 5300 */
#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_CTRL1_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x304) /* 5304 */
#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING16_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x400) /* 5400 */
#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING8_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x380) /* 5380 */
#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING17_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x410) /* 5410 */
#define WF_WFDMA_HOST_DMA1_WPDMA_TX_RING18_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x420) /* 5420 */

#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x500) /* 4500 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x510) /* 4510 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING2_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x520) /* 4520 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING3_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x530) /* 4530 */
#define WF_WFDMA_HOST_DMA1_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x500) /* 5500 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x540) /* 4540 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING5_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x550) /* 4550 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING6_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x560) /* 4560 */
#define WF_WFDMA_HOST_DMA0_WPDMA_RX_RING7_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_0_DRIVER_BASE + 0x570) /* 4570 */
#define WF_WFDMA_HOST_DMA1_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_HOST_WPDMA_1_DRIVER_BASE + 0x510) /* 5510 */

#define WF_WFDMA_MCU_DMA0_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_0_DRIVER_BASE + 0x300)
#define WF_WFDMA_MCU_DMA1_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_0_DRIVER_BASE + 0x300)
#define WF_WFDMA_MCU_DMA1_WPDMA_TX_RING1_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_1_DRIVER_BASE + 0x310)
#define WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_1_DRIVER_BASE + 0x500)
#define WF_WFDMA_MCU_DMA0_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_0_DRIVER_BASE + 0x510)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_1_DRIVER_BASE + 0x500)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_1_DRIVER_BASE + 0x510)
#define WF_WFDMA_MCU_DMA1_WPDMA_RX_RING2_CTRL0_ADDR_DRIVER_BASE \
		(CONNAC2X_MCU_WPDMA_1_DRIVER_BASE + 0x520)

#endif /* WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct pse_group_info {
	char name[20];
	u_int32_t quota_addr;
	u_int32_t pg_info_addr;
};

struct wfdma_group_info {
	char name[20];
	u_int32_t hw_desc_base;
};

enum _ENUM_WFDMA_TYPE_T {
	WFDMA_TYPE_HOST = 0,
	WFDMA_TYPE_WM
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

static struct EMPTY_QUEUE_INFO ple_queue_empty_info[] = {
	{"CPU Q0", MCU_Q0_INDEX, ENUM_UMAC_CTX_Q_0},
	{"CPU Q1", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_1},
	{"CPU Q2", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_2},
	{"CPU Q3", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_3},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0}, /* 4~7 not defined */
	{"ALTX Q0", ENUM_UMAC_LMAC_PORT_2,
	 ENUM_UMAC_LMAC_PLE_TX_Q_ALTX_0}, /* Q16 */
	{"BMC Q0", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_BMC_0},
	{"BCN Q0", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_BNC_0},
	{"PSMP Q0", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_PSMP_0},
	{"ALTX Q1", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_ALTX_1},
	{"BMC Q1", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_BMC_1},
	{"BCN Q1", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_BNC_1},
	{"PSMP Q1", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_PSMP_1},
	{"NAF Q", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_NAF},
	{"NBCN Q", ENUM_UMAC_LMAC_PORT_2, ENUM_UMAC_LMAC_PLE_TX_Q_NBCN},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0}, /* 18~29 not defined */
	{"RLS Q", ENUM_PLE_CTRL_PSE_PORT_3, ENUM_UMAC_PLE_CTRL_P3_Q_0X1E},
	{"RLS2 Q", ENUM_PLE_CTRL_PSE_PORT_3, ENUM_UMAC_PLE_CTRL_P3_Q_0X1F} };

static struct EMPTY_QUEUE_INFO pse_queue_empty_info[] = {
	{"CPU Q0", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_0},
	{"CPU Q1", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_1},
	{"CPU Q2", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_2},
	{"CPU Q3", ENUM_UMAC_CPU_PORT_1, ENUM_UMAC_CTX_Q_3},
	{"HIF Q8", ENUM_UMAC_HIF_PORT_0, 8},
	{"HIF Q9", ENUM_UMAC_HIF_PORT_0, 9},
	{"HIF Q10", ENUM_UMAC_HIF_PORT_0, 10},
	{"HIF Q11", ENUM_UMAC_HIF_PORT_0, 11},
	{"HIF Q0", ENUM_UMAC_HIF_PORT_0, 0}, /*bit 8*/
	{"HIF Q1", ENUM_UMAC_HIF_PORT_0, 1},
	{"HIF Q2", ENUM_UMAC_HIF_PORT_0, 2},
	{"HIF Q3", ENUM_UMAC_HIF_PORT_0, 3},
	{"HIF Q4", ENUM_UMAC_HIF_PORT_0, 4},
	{"HIF Q5", ENUM_UMAC_HIF_PORT_0, 5},
	{"HIF Q6", ENUM_UMAC_HIF_PORT_0, 6},
	{"HIF Q7", ENUM_UMAC_HIF_PORT_0, 7},
	{"LMAC Q", ENUM_UMAC_LMAC_PORT_2, 0}, /*bit 16*/
	{"MDP TX Q", ENUM_UMAC_LMAC_PORT_2, 1},
	{"MDP RX Q", ENUM_UMAC_LMAC_PORT_2, 2},
	{"SEC TX Q", ENUM_UMAC_LMAC_PORT_2, 3},
	{"SEC RX Q", ENUM_UMAC_LMAC_PORT_2, 4},
	{"SFD_PARK Q", ENUM_UMAC_LMAC_PORT_2, 5},
	{"MDP_TXIOC Q", ENUM_UMAC_LMAC_PORT_2, 6},
	{"MDP_RXIOC Q", ENUM_UMAC_LMAC_PORT_2, 7},
	{"MDP_TX1 Q", ENUM_UMAC_LMAC_PORT_2, 17}, /*bit 24*/
	{"SEC_TX1 Q", ENUM_UMAC_LMAC_PORT_2, 19},
	{"MDP_TXIOC1 Q", ENUM_UMAC_LMAC_PORT_2, 22},
	{"MDP_RXIOC1 Q", ENUM_UMAC_LMAC_PORT_2, 23},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0}, /* 28~30 not defined */
	{"RLS Q", ENUM_PLE_CTRL_PSE_PORT_3, ENUM_UMAC_PLE_CTRL_P3_Q_0X1F} };

static u_int8_t *sta_ctrl_reg[] = {"ENABLE", "DISABLE", "PAUSE"};

#if 0
static struct EMPTY_QUEUE_INFO ple_txcmd_queue_empty_info[] = {
	{"AC00Q", ENUM_UMAC_LMAC_PORT_2, 0x40},
	{"AC01Q", ENUM_UMAC_LMAC_PORT_2, 0x41},
	{"AC02Q", ENUM_UMAC_LMAC_PORT_2, 0x42},
	{"AC03Q", ENUM_UMAC_LMAC_PORT_2, 0x43},
	{"AC10Q", ENUM_UMAC_LMAC_PORT_2, 0x44},
	{"AC11Q", ENUM_UMAC_LMAC_PORT_2, 0x45},
	{"AC12Q", ENUM_UMAC_LMAC_PORT_2, 0x46},
	{"AC13Q", ENUM_UMAC_LMAC_PORT_2, 0x47},
	{"AC20Q", ENUM_UMAC_LMAC_PORT_2, 0x48},
	{"AC21Q", ENUM_UMAC_LMAC_PORT_2, 0x49},
	{"AC22Q", ENUM_UMAC_LMAC_PORT_2, 0x4a},
	{"AC23Q", ENUM_UMAC_LMAC_PORT_2, 0x4b},
	{"AC30Q", ENUM_UMAC_LMAC_PORT_2, 0x4c},
	{"AC31Q", ENUM_UMAC_LMAC_PORT_2, 0x4d},
	{"AC32Q", ENUM_UMAC_LMAC_PORT_2, 0x4e},
	{"AC33Q", ENUM_UMAC_LMAC_PORT_2, 0x4f},
	{"ALTX Q0", ENUM_UMAC_LMAC_PORT_2, 0x50},
	{"TF Q0", ENUM_UMAC_LMAC_PORT_2, 0x51},
	{"TWT TSF-TF Q0", ENUM_UMAC_LMAC_PORT_2, 0x52},
	{"TWT DL Q0", ENUM_UMAC_LMAC_PORT_2, 0x53},
	{"TWT UL Q0", ENUM_UMAC_LMAC_PORT_2, 0x54},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0} };
#endif

struct pse_group_info pse_group[] = {
	{"HIF0(TX data)", WF_PSE_TOP_PG_HIF0_GROUP_ADDR,
		WF_PSE_TOP_HIF0_PG_INFO_ADDR},
	{"HIF1(Talos CMD)", WF_PSE_TOP_PG_HIF1_GROUP_ADDR,
		WF_PSE_TOP_HIF1_PG_INFO_ADDR},
#if 0
	{"HIF2", WF_PSE_TOP_PG_HIF2_GROUP_ADDR,
		WF_PSE_TOP_HIF2_PG_INFO_ADDR},
#endif
	{"CPU(I/O r/w)",  WF_PSE_TOP_PG_CPU_GROUP_ADDR,
		WF_PSE_TOP_CPU_PG_INFO_ADDR},
	{"PLE(host report)",  WF_PSE_TOP_PG_PLE_GROUP_ADDR,
		WF_PSE_TOP_PLE_PG_INFO_ADDR},
	{"PLE1(SPL report)", WF_PSE_TOP_PG_PLE1_GROUP_ADDR,
		WF_PSE_TOP_PLE1_PG_INFO_ADDR},
	{"LMAC0(RX data)", WF_PSE_TOP_PG_LMAC0_GROUP_ADDR,
			WF_PSE_TOP_LMAC0_PG_INFO_ADDR},
	{"LMAC1(RX_VEC)", WF_PSE_TOP_PG_LMAC1_GROUP_ADDR,
			WF_PSE_TOP_LMAC1_PG_INFO_ADDR},
	{"LMAC2(TXS)", WF_PSE_TOP_PG_LMAC2_GROUP_ADDR,
			WF_PSE_TOP_LMAC2_PG_INFO_ADDR},
	{"LMAC3(TXCMD/RXRPT)", WF_PSE_TOP_PG_LMAC3_GROUP_ADDR,
			WF_PSE_TOP_LMAC3_PG_INFO_ADDR},
	{"MDP",  WF_PSE_TOP_PG_MDP_GROUP_ADDR,
			WF_PSE_TOP_MDP_PG_INFO_ADDR},
#if 0
	{"MDP1", WF_PSE_TOP_PG_MDP1_GROUP_ADDR,
		WF_PSE_TOP_MDP1_PG_INFO_ADDR},
	{"MDP2", WF_PSE_TOP_PG_MDP2_GROUP_ADDR,
		WF_PSE_TOP_MDP2_PG_INFO_ADDR},
#endif
};

struct wfdma_group_info wfmda_host_tx_group[] = {
	{"P1T0:AP DATA0", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_CTRL0_ADDR},
	{"P1T1:AP DATA1", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING1_CTRL0_ADDR},
	{"P1T16:FWDL", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING16_CTRL0_ADDR},
	{"P1T17:AP CMD", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING17_CTRL0_ADDR},
	{"P1T8:MD DATA", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING8_CTRL0_ADDR},
	{"P1T18:MD CMD", WF_WFDMA_HOST_DMA1_WPDMA_TX_RING18_CTRL0_ADDR},
};

struct wfdma_group_info wfmda_host_rx_group[] = {
	{"P0R0:AP DATA0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR},
	{"P0R1:AP DATA1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING1_CTRL0_ADDR},
	{"P0R2:AP TDONE0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING2_CTRL0_ADDR},
	{"P0R3:AP TDONE1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING3_CTRL0_ADDR},
	{"P1R0:AP EVENT", WF_WFDMA_HOST_DMA1_WPDMA_RX_RING0_CTRL0_ADDR},
	{"P0R4:MD DATA0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_CTRL0_ADDR},
	{"P0R5:MD DATA1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING5_CTRL0_ADDR},
	{"P0R6:MD TDONE0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING6_CTRL0_ADDR},
	{"P0R7:MD TDONE1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING7_CTRL0_ADDR},
	{"P1R1:MD EVENT", WF_WFDMA_HOST_DMA1_WPDMA_RX_RING1_CTRL0_ADDR},
};

struct wfdma_group_info wfmda_wm_tx_group[] = {
	{"P0T0:DATA", WF_WFDMA_MCU_DMA0_WPDMA_TX_RING0_CTRL0_ADDR},
	{"P1T0:AP EVENT", WF_WFDMA_MCU_DMA1_WPDMA_TX_RING0_CTRL0_ADDR},
	{"P1T1:MD EVENT", WF_WFDMA_MCU_DMA1_WPDMA_TX_RING1_CTRL0_ADDR},
};

struct wfdma_group_info wfmda_wm_rx_group[] = {
	{"P0R0:DATA", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR},
	{"P0R1:TXDONE", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING1_CTRL0_ADDR},
	{"P1R0:FWDL", WF_WFDMA_MCU_DMA1_WPDMA_RX_RING0_CTRL0_ADDR},
	{"P1R1:AP CMD", WF_WFDMA_MCU_DMA1_WPDMA_RX_RING1_CTRL0_ADDR},
	{"P1R2:MD CMD", WF_WFDMA_MCU_DMA1_WPDMA_RX_RING2_CTRL0_ADDR},
};

#if WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE
struct wfdma_group_info wfmda_host_tx_group_driver_base[] = {
	{"P1T0:AP DATA0",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P1T1:AP DATA1",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P1T16:FWDL",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING16_CTRL0_ADDR_DRIVER_BASE},
	{"P1T17:AP CMD",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING17_CTRL0_ADDR_DRIVER_BASE},
	{"P1T8:MD DATA",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING8_CTRL0_ADDR_DRIVER_BASE},
	{"P1T18:MD CMD",
		WF_WFDMA_HOST_DMA1_WPDMA_TX_RING18_CTRL0_ADDR_DRIVER_BASE},
};

struct wfdma_group_info wfmda_host_rx_group_driver_base[] = {
	{"P0R0:AP DATA0",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P0R1:AP DATA1",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE},
	{"P0R2:AP TDONE0",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING2_CTRL0_ADDR_DRIVER_BASE},
	{"P0R3:AP TDONE1",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING3_CTRL0_ADDR_DRIVER_BASE},
	{"P1R0:AP EVENT",
		WF_WFDMA_HOST_DMA1_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P0R4:MD DATA0",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_CTRL0_ADDR_DRIVER_BASE},
	{"P0R5:MD DATA1",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING5_CTRL0_ADDR_DRIVER_BASE},
	{"P0R6:MD TDONE0",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING6_CTRL0_ADDR_DRIVER_BASE},
	{"P0R7:MD TDONE1",
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING7_CTRL0_ADDR_DRIVER_BASE},
	{"P1R1:MD EVENT",
		WF_WFDMA_HOST_DMA1_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE},
};

struct wfdma_group_info wfmda_wm_tx_group_driver_base[] = {
	{"P0T0:DATA",
		WF_WFDMA_MCU_DMA0_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P1T0:AP EVENT",
		WF_WFDMA_MCU_DMA1_WPDMA_TX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P1T1:MD EVENT",
		WF_WFDMA_MCU_DMA1_WPDMA_TX_RING1_CTRL0_ADDR_DRIVER_BASE},
};

struct wfdma_group_info wfmda_wm_rx_group_driver_base[] = {
	{"P0R0:DATA",
		WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P0R1:TXDONE",
		WF_WFDMA_MCU_DMA0_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE},
	{"P1R0:FWDL",
		WF_WFDMA_MCU_DMA1_WPDMA_RX_RING0_CTRL0_ADDR_DRIVER_BASE},
	{"P1R1:AP CMD",
		WF_WFDMA_MCU_DMA1_WPDMA_RX_RING1_CTRL0_ADDR_DRIVER_BASE},
	{"P1R2:MD CMD",
		WF_WFDMA_MCU_DMA1_WPDMA_RX_RING2_CTRL0_ADDR_DRIVER_BASE},
};
#endif /* WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE */

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
void soc3_0_show_ple_info(
	struct ADAPTER *prAdapter,
	u_int8_t fgDumpTxd)
{
	u_int32_t int_n9_err = 0;
	u_int32_t int_n9_err1 = 0;
	u_int32_t ple_buf_ctrl = 0, pg_sz, pg_num;
	u_int32_t ple_stat[25] = {0}, pg_flow_ctrl[10] = {0};
	u_int32_t sta_pause[6] = {0}, dis_sta_map[6] = {0};
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail, hif_max_q, hif_min_q;
	u_int32_t rpg_hif, upg_hif, cpu_max_q, cpu_min_q, rpg_cpu, upg_cpu;
	u_int32_t i, j;
	u_int32_t ple_peek[12] = {0};
	u_int32_t ple_empty = 0;
	u_int32_t ple_txd_empty = 0;

#if 0
	u_int32_t ple_txcmd_stat;
#endif

	HAL_MCR_RD(prAdapter, WF_PLE_TOP_INT_N9_ERR_STS_ADDR, &int_n9_err);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_INT_N9_ERR_STS_1_ADDR, &int_n9_err1);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_QUEUE_EMPTY_ADDR, &ple_empty);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_TXD_QUEUE_EMPTY_ADDR, &ple_txd_empty);

	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PBUF_CTRL_ADDR, &ple_buf_ctrl);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_QUEUE_EMPTY_ADDR, &ple_stat[0]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY0_ADDR, &ple_stat[1]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY1_ADDR, &ple_stat[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY2_ADDR, &ple_stat[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY3_ADDR, &ple_stat[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY4_ADDR, &ple_stat[5]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY5_ADDR, &ple_stat[6]);
#endif
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY0_ADDR, &ple_stat[7]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY1_ADDR, &ple_stat[8]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY2_ADDR, &ple_stat[9]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY3_ADDR, &ple_stat[10]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY4_ADDR, &ple_stat[11]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY5_ADDR, &ple_stat[12]);
#endif
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY0_ADDR, &ple_stat[13]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY1_ADDR, &ple_stat[14]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY2_ADDR, &ple_stat[15]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY3_ADDR, &ple_stat[16]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY4_ADDR, &ple_stat[17]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY5_ADDR, &ple_stat[18]);
#endif
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY0_ADDR, &ple_stat[19]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY1_ADDR, &ple_stat[20]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY2_ADDR, &ple_stat[21]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY3_ADDR, &ple_stat[22]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY4_ADDR, &ple_stat[23]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY5_ADDR, &ple_stat[24]);
#endif
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_TXCMD_QUEUE_EMPTY_ADDR,
		   &ple_txcmd_stat);
#endif
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_FREEPG_CNT_ADDR, &pg_flow_ctrl[0]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_FREEPG_HEAD_TAIL_ADDR,
		   &pg_flow_ctrl[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PG_HIF_GROUP_ADDR, &pg_flow_ctrl[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_HIF_PG_INFO_ADDR, &pg_flow_ctrl[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PG_CPU_GROUP_ADDR, &pg_flow_ctrl[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_CPU_PG_INFO_ADDR, &pg_flow_ctrl[5]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PG_HIF_TXCMD_GROUP_ADDR,
		   &pg_flow_ctrl[6]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_HIF_TXCMD_PG_INFO_ADDR,
		   &pg_flow_ctrl[7]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PG_HIF_WMTXD_GROUP_ADDR,
		   &pg_flow_ctrl[8]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_HIF_WMTXD_PG_INFO_ADDR,
		   &pg_flow_ctrl[9]);

	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP0_ADDR, &dis_sta_map[0]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP1_ADDR, &dis_sta_map[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP2_ADDR, &dis_sta_map[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP3_ADDR, &dis_sta_map[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP4_ADDR, &dis_sta_map[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP5_ADDR, &dis_sta_map[5]);
#endif
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE0_ADDR, &sta_pause[0]);
#if 0
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE1_ADDR, &sta_pause[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE2_ADDR, &sta_pause[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE3_ADDR, &sta_pause[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE4_ADDR, &sta_pause[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE5_ADDR, &sta_pause[5]);
#endif

	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_00, &ple_peek[0]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_01, &ple_peek[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_02, &ple_peek[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_03, &ple_peek[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_04, &ple_peek[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_05, &ple_peek[5]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_06, &ple_peek[6]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_07, &ple_peek[7]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_08, &ple_peek[8]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_09, &ple_peek[9]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_10, &ple_peek[10]);
	HAL_MCR_RD(prAdapter, WF_PLE_FSM_PEEK_CR_11, &ple_peek[11]);

	/* Error Status Info */
	DBGLOG(HAL, INFO,
	"PLE Error Status(0x%08x):0x%08x,Error Status1(0x%08x):0x%08x\n",
			WF_PLE_TOP_INT_N9_ERR_STS_ADDR, int_n9_err,
			WF_PLE_TOP_INT_N9_ERR_STS_1_ADDR, int_n9_err1);

	/* FSM PEEK CR */
	DBGLOG(HAL, INFO,
	"00(0x%08x):0x%08x,01(0x%08x):0x%08x,02(0x%08x):0x%08x,03(0x%08x):0x%08x,04(0x%08x):0x%08x,05(0x%08x):0x%08x,",
			WF_PLE_FSM_PEEK_CR_00, ple_peek[0],
			WF_PLE_FSM_PEEK_CR_01, ple_peek[1],
			WF_PLE_FSM_PEEK_CR_02, ple_peek[2],
			WF_PLE_FSM_PEEK_CR_03, ple_peek[3],
			WF_PLE_FSM_PEEK_CR_04, ple_peek[4],
			WF_PLE_FSM_PEEK_CR_05, ple_peek[5]);

	DBGLOG(HAL, INFO,
	"06(0x%08x):0x%08x,07(0x%08x):0x%08x,08(0x%08x):0x%08x,09(0x%08x):0x%08x,10(0x%08x):0x%08x,11(0x%08x):0x%08x\n",
			WF_PLE_FSM_PEEK_CR_06, ple_peek[6],
			WF_PLE_FSM_PEEK_CR_07, ple_peek[7],
			WF_PLE_FSM_PEEK_CR_08, ple_peek[8],
			WF_PLE_FSM_PEEK_CR_09, ple_peek[9],
			WF_PLE_FSM_PEEK_CR_10, ple_peek[10],
			WF_PLE_FSM_PEEK_CR_11, ple_peek[11]);

	/* Configuration Info */

	pg_sz = (ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_MASK) >>
		WF_PLE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_SHFT;
	pg_num = (ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_MASK) >>
			 WF_PLE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_SHFT;

	DBGLOG(HAL, INFO,
	"Buffer Control(0x%08x):0x%08x,Page Size=%d, Page Offset=%d, Total Page=%d\n",
		WF_PLE_TOP_PBUF_CTRL_ADDR,
		ple_buf_ctrl,
		pg_sz,
		(ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_PBUF_OFFSET_MASK) >>
		       WF_PLE_TOP_PBUF_CTRL_PBUF_OFFSET_SHFT,
		pg_num);

	/* Flow-Control: Page Flow Control */
	fpg_cnt = (pg_flow_ctrl[0] & WF_PLE_TOP_FREEPG_CNT_FREEPG_CNT_MASK) >>
		WF_PLE_TOP_FREEPG_CNT_FREEPG_CNT_SHFT;
	ffa_cnt = (pg_flow_ctrl[0] & WF_PLE_TOP_FREEPG_CNT_FFA_CNT_MASK) >>
		  WF_PLE_TOP_FREEPG_CNT_FFA_CNT_SHFT;

	DBGLOG(HAL, INFO,
	"Free page counter(0x%08x):0x%08x,The toal page number of free=0x%03x,The free page numbers of free for all=0x%03x\n",
		WF_PLE_TOP_FREEPG_CNT_ADDR,
		pg_flow_ctrl[0], fpg_cnt, ffa_cnt);

	/* PLE tail / head FID */
	fpg_head = (pg_flow_ctrl[1] &
		    WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_MASK) >>
		   WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_SHFT;
	fpg_tail = (pg_flow_ctrl[1] &
		    WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_MASK) >>
		   WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_SHFT;
	DBGLOG(HAL, INFO,
	"Free page tail/head FID(0x%08x):0x%08x,The tail/head page of free page list=0x%03x/0x%03x\n",
		WF_PLE_TOP_FREEPG_HEAD_TAIL_ADDR,
		pg_flow_ctrl[1], fpg_tail, fpg_head);

	/* Flow-Control: Show PLE HIF Group information */
	DBGLOG(HAL, INFO,
	"Reserved page counter of HIF group(0x%08x):0x%08x,status(0x%08x):0x%08x\n",
		WF_PLE_TOP_PG_HIF_GROUP_ADDR, pg_flow_ctrl[2],
		WF_PLE_TOP_HIF_PG_INFO_ADDR, pg_flow_ctrl[3]);

	hif_min_q = (pg_flow_ctrl[2] &
		     WF_PLE_TOP_PG_HIF_GROUP_HIF_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_GROUP_HIF_MIN_QUOTA_SHFT;
	hif_max_q = (pg_flow_ctrl[2] &
		     WF_PLE_TOP_PG_HIF_GROUP_HIF_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_GROUP_HIF_MAX_QUOTA_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe max/min quota pages of HIF group=0x%03x/0x%03x\n",
	       hif_max_q, hif_min_q);
	rpg_hif = (pg_flow_ctrl[3] & WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_SHFT;
	upg_hif = (pg_flow_ctrl[3] & WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe used/reserved pages of HIF group=0x%03x/0x%03x\n",
	       upg_hif, rpg_hif);

	/* Flow-Control: Show PLE CPU Group information */
	DBGLOG(HAL, INFO,
	"Reserved page counter of CPU group(0x%08x):0x%08x,status(0x%08x):0x%08x\n",
		WF_PLE_TOP_PG_CPU_GROUP_ADDR, pg_flow_ctrl[4],
		WF_PLE_TOP_CPU_PG_INFO_ADDR, pg_flow_ctrl[5]);

	cpu_min_q = (pg_flow_ctrl[4] &
		     WF_PLE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_SHFT;
	cpu_max_q = (pg_flow_ctrl[4] &
		     WF_PLE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe max/min quota pages of CPU group=0x%03x/0x%03x\n",
	       cpu_max_q, cpu_min_q);
	rpg_cpu = (pg_flow_ctrl[5] & WF_PLE_TOP_CPU_PG_INFO_CPU_RSV_CNT_MASK) >>
		  WF_PLE_TOP_CPU_PG_INFO_CPU_RSV_CNT_SHFT;
	upg_cpu = (pg_flow_ctrl[5] & WF_PLE_TOP_CPU_PG_INFO_CPU_SRC_CNT_MASK) >>
		  WF_PLE_TOP_CPU_PG_INFO_CPU_SRC_CNT_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe used/reserved pages of CPU group=0x%03x/0x%03x\n",
	       upg_cpu, rpg_cpu);

	/* Flow-Control: Show PLE WMTXD Group information */
	DBGLOG(HAL, INFO,
	"Reserved page counter of HIF_WMTXD group(0x%08x):0x%08x,status(0x%08x):0x%08x\n",
	WF_PLE_TOP_PG_HIF_WMTXD_GROUP_ADDR, pg_flow_ctrl[8],
	WF_PLE_TOP_HIF_WMTXD_PG_INFO_ADDR, pg_flow_ctrl[9]);
	cpu_min_q = (pg_flow_ctrl[8] &
		     WF_PLE_TOP_PG_HIF_WMTXD_GROUP_HIF_WMTXD_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_WMTXD_GROUP_HIF_WMTXD_MIN_QUOTA_SHFT;
	cpu_max_q = (pg_flow_ctrl[8] &
		     WF_PLE_TOP_PG_HIF_WMTXD_GROUP_HIF_WMTXD_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_WMTXD_GROUP_HIF_WMTXD_MAX_QUOTA_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe max/min quota pages of HIF_WMTXD group=0x%03x/0x%03x\n",
	       cpu_max_q, cpu_min_q);
	rpg_cpu = (pg_flow_ctrl[9] &
		 WF_PLE_TOP_HIF_WMTXD_PG_INFO_HIF_WMTXD_RSV_CNT_MASK) >>
		WF_PLE_TOP_HIF_WMTXD_PG_INFO_HIF_WMTXD_RSV_CNT_SHFT;
	upg_cpu = (pg_flow_ctrl[9] &
		 WF_PLE_TOP_HIF_WMTXD_PG_INFO_HIF_WMTXD_SRC_CNT_MASK) >>
		WF_PLE_TOP_HIF_WMTXD_PG_INFO_HIF_WMTXD_SRC_CNT_SHFT;
	DBGLOG(HAL, TRACE,
	       "\tThe used/reserved pages of HIF_WMTXD group=0x%03x/0x%03x\n",
	       upg_cpu, rpg_cpu);

	/* Flow-Control: Show PLE TXCMD Group information */
	DBGLOG(HAL, INFO,
	"Reserved page counter of HIF_TXCMD group(0x%08x):0x%08x,status(0x%08x):0x%08x\n",
	WF_PLE_TOP_PG_HIF_TXCMD_GROUP_ADDR, pg_flow_ctrl[6],
	WF_PLE_TOP_HIF_TXCMD_PG_INFO_ADDR, pg_flow_ctrl[7]);
	cpu_min_q = (pg_flow_ctrl[6] &
		     WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MIN_QUOTA_SHFT;
	cpu_max_q = (pg_flow_ctrl[6] &
		     WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MAX_QUOTA_SHFT;
	DBGLOG(HAL, TRACE,
	       "\t\tThe max/min quota pages of HIF_TXCMD group=0x%03x/0x%03x\n",
	       cpu_max_q, cpu_min_q);
	rpg_cpu = (pg_flow_ctrl[7] &
		   WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_SRC_CNT_MASK) >>
		  WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_SRC_CNT_SHFT;
	upg_cpu = (pg_flow_ctrl[7] &
		   WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_RSV_CNT_MASK) >>
		  WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_RSV_CNT_SHFT;
	DBGLOG(HAL, TRACE,
	       "\t\tThe used/reserved pages of HIF_TXCMD group=0x%03x/0x%03x\n",
	       upg_cpu, rpg_cpu);

	if ((ple_stat[0] & WF_PLE_TOP_QUEUE_EMPTY_ALL_AC_EMPTY_MASK) == 0) {
		for (j = 0; j < 24; j = j + 6) {
			if (j % 6 == 0) {
				DBGLOG(HAL, INFO,
					"\tNonempty AC%d Q of STA#: ", j / 6);
			}

			for (i = 0; i < 32; i++) {
				if (((ple_stat[j + 1] & (0x1 << i)) >> i) ==
				    0) {
					DBGLOG(HAL, INFO, "%d ",
						i + (j % 6) * 32);
				}
			}
		}
		DBGLOG(HAL, INFO, ", ");
	}

	/* Queue Empty Status */
	DBGLOG(HAL, INFO,
	"QUEUE_EMPTY(0x%08x):0x%08xTXD QUEUE_EMPTY(0x%08x):0x%08x\n",
			WF_PLE_TOP_QUEUE_EMPTY_ADDR, ple_empty,
			WF_PLE_TOP_TXD_QUEUE_EMPTY_ADDR, ple_txd_empty);

	/* Nonempty Queue Status */
	DBGLOG(HAL, INFO, "Nonempty Q info:");

	for (i = 0; i < 31; i++) {
		if (((ple_stat[0] & (0x1 << i)) >> i) == 0) {
			uint32_t hfid, tfid, pktcnt, fl_que_ctrl[3] = {0};

			if (ple_queue_empty_info[i].QueueName != NULL) {
				DBGLOG(HAL, INFO, "\t%s: ",
					ple_queue_empty_info[i].QueueName);
				fl_que_ctrl[0] |=
					WF_PLE_TOP_FL_QUE_CTRL_0_EXECUTE_MASK;
				fl_que_ctrl[0] |=
				(ple_queue_empty_info[i].Portid
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_PID_SHFT);
				fl_que_ctrl[0] |=
				(ple_queue_empty_info[i].Queueid
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_QID_SHFT);
			} else
				continue;

			HAL_MCR_WR(prAdapter, WF_PLE_TOP_FL_QUE_CTRL_0_ADDR,
				   fl_que_ctrl[0]);
			HAL_MCR_RD(prAdapter, WF_PLE_TOP_FL_QUE_CTRL_2_ADDR,
				   &fl_que_ctrl[1]);
			HAL_MCR_RD(prAdapter, WF_PLE_TOP_FL_QUE_CTRL_3_ADDR,
				   &fl_que_ctrl[2]);
			hfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_MASK) >>
			       WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_SHFT;
			tfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_MASK) >>
			       WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_SHFT;
			pktcnt =
				(fl_que_ctrl[2] &
				 WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_SHFT;
			DBGLOG(HAL, INFO,
			"tail/head fid = 0x%03x/0x%03x, pkt cnt = 0x%03x\n",
				tfid, hfid, pktcnt);
			if (pktcnt > 0 && fgDumpTxd)
				connac2x_show_txd_Info(
					prAdapter, hfid);
		}
	}

	for (j = 0; j < 24; j = j + 6) { /* show AC Q info */
		for (i = 0; i < 32; i++) {
			if (((ple_stat[j + 1] & (0x1 << i)) >> i) == 0) {
				uint32_t hfid, tfid, pktcnt, ac_num = j / 6,
							   ctrl = 0;
				uint32_t sta_num = i + (j % 6) * 32,
				       fl_que_ctrl[3] = {0};
				uint32_t wmmidx = 0;

				DBGLOG(HAL, INFO, "\tSTA%d AC%d: ", sta_num,
				       ac_num);

				fl_que_ctrl[0] |=
					WF_PLE_TOP_FL_QUE_CTRL_0_EXECUTE_MASK;
				fl_que_ctrl[0] |=
				(ENUM_UMAC_LMAC_PORT_2
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_PID_SHFT);
				fl_que_ctrl[0] |=
				(ac_num
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_QID_SHFT);
				fl_que_ctrl[0] |=
				(sta_num
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_WLANID_SHFT);
				HAL_MCR_WR(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_0_ADDR,
					   fl_que_ctrl[0]);
				HAL_MCR_RD(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_2_ADDR,
					   &fl_que_ctrl[1]);
				HAL_MCR_RD(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_3_ADDR,
					   &fl_que_ctrl[2]);
				hfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_SHFT;
				tfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_SHFT;

				pktcnt =
				(fl_que_ctrl[2] &
				WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_SHFT;
				DBGLOG(HAL, INFO,
				"tail/head fid = 0x%03x/0x%03x, pkt cnt = %x",
				tfid, hfid, pktcnt);

				if (((sta_pause[j % 6] & 0x1 << i) >> i) == 1)
					ctrl = 2;

				if (((dis_sta_map[j % 6] & 0x1 << i) >> i) == 1)
					ctrl = 1;

				DBGLOG(HAL, INFO, " ctrl = %s",
						   sta_ctrl_reg[ctrl]);
				DBGLOG(HAL, INFO, " (wmmidx=%d)\n",
					wmmidx);
				if (pktcnt > 0 && fgDumpTxd)
					connac2x_show_txd_Info(
						prAdapter, hfid);
			}
		}
	}
#if 0
	if (~ple_txcmd_stat) {
		DBGLOG(HAL, INFO, "Nonempty TXCMD Q info:\n");
		for (i = 0; i < 31; i++) {
			if (((ple_txcmd_stat & (0x1 << i)) >> i) == 0) {
				uint32_t hfid, tfid;
				uint32_t pktcnt, fl_que_ctrl[3] = {0};

				if (ple_txcmd_queue_empty_info[i].QueueName !=
				    NULL) {
					DBGLOG(HAL, INFO, "\t%s: ",
					       ple_txcmd_queue_empty_info[i]
						       .QueueName);
					fl_que_ctrl[0] |=
					WF_PLE_TOP_FL_QUE_CTRL_0_EXECUTE_MASK;
					fl_que_ctrl[0] |=
						(ple_txcmd_queue_empty_info[i]
							 .Portid
				<< WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_PID_SHFT);
					fl_que_ctrl[0] |=
						(ple_txcmd_queue_empty_info[i]
							 .Queueid
				 << WF_PLE_TOP_FL_QUE_CTRL_0_Q_BUF_QID_SHFT);
				} else
					continue;

				HAL_MCR_WR(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_0_ADDR,
					   fl_que_ctrl[0]);
				HAL_MCR_RD(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_2_ADDR,
					   &fl_que_ctrl[1]);
				HAL_MCR_RD(prAdapter,
					   WF_PLE_TOP_FL_QUE_CTRL_3_ADDR,
					   &fl_que_ctrl[2]);
				hfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_SHFT;
				tfid = (fl_que_ctrl[1] &
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_SHFT;
				pktcnt =
				(fl_que_ctrl[2] &
				WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_MASK) >>
				WF_PLE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_SHFT;
				DBGLOG(HAL, INFO, "tail/head fid =");
				DBGLOG(HAL, INFO, "0x%03x/0x%03x,", tfid, hfid);
				DBGLOG(HAL, INFO, "pkt cnt = 0x%03x\n", pktcnt);
			}
		}
	}
#endif
}

void soc3_0_show_pse_info(
	struct ADAPTER *prAdapter)
{
	u_int32_t int_n9_err = 0;
	u_int32_t int_n9_err1 = 0;
	u_int32_t pse_buf_ctrl = 0;
	u_int32_t pg_sz = 0;
	u_int32_t pg_num = 0;
	u_int32_t pse_stat = 0;
	u_int32_t pse_stat_mask = 0;
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail;
	u_int32_t max_q, min_q, rsv_pg, used_pg;
	u_int32_t i, group_cnt;
	u_int32_t group_quota = 0;
	u_int32_t group_info = 0;
	u_int32_t freepg_cnt = 0;
	u_int32_t freepg_head_tail = 0;
	struct pse_group_info *group;
	char *str;
	u_int32_t pse_peek[10] = {0};

	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PBUF_CTRL_ADDR, &pse_buf_ctrl);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_QUEUE_EMPTY_ADDR, &pse_stat);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_QUEUE_EMPTY_MASK_ADDR, &pse_stat_mask);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_CNT_ADDR, &freepg_cnt);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR,
		   &freepg_head_tail);

	HAL_MCR_RD(prAdapter, WF_PSE_TOP_INT_N9_ERR_STS_ADDR, &int_n9_err);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_INT_N9_ERR1_STS_ADDR, &int_n9_err1);

	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_00, &pse_peek[0]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_01, &pse_peek[1]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_02, &pse_peek[2]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_03, &pse_peek[3]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_04, &pse_peek[4]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_05, &pse_peek[5]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_06, &pse_peek[6]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_07, &pse_peek[7]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_08, &pse_peek[8]);
	HAL_MCR_RD(prAdapter, WF_PSE_FSM_PEEK_CR_09, &pse_peek[9]);

	/* Error Status Info */
	DBGLOG(HAL, INFO,
	"PSE Error Status(0x%08x):0x%08x,PSE Error 1 Status(0x%08x):0x%08x\n",
			WF_PSE_TOP_INT_N9_ERR_STS_ADDR, int_n9_err,
			WF_PSE_TOP_INT_N9_ERR1_STS_ADDR, int_n9_err1);

	DBGLOG(HAL, INFO,
	"00(0x%08x):0x%08x,01(0x%08x):0x%08x02(0x%08x):0x%08x,03(0x%08x):0x%08x04(0x%08x):0x%08x,05(0x%08x):0x%08x\n",
				WF_PSE_FSM_PEEK_CR_00, pse_peek[0],
				WF_PSE_FSM_PEEK_CR_01, pse_peek[1],
				WF_PSE_FSM_PEEK_CR_02, pse_peek[2],
				WF_PSE_FSM_PEEK_CR_03, pse_peek[3],
				WF_PSE_FSM_PEEK_CR_04, pse_peek[4],
				WF_PSE_FSM_PEEK_CR_05, pse_peek[5]);

	DBGLOG(HAL, INFO,
	"06(0x%08x):0x%08x,07(0x%08x):0x%08x08(0x%08x):0x%08x,09(0x%08x):0x%08x\n",
				WF_PSE_FSM_PEEK_CR_06, pse_peek[6],
				WF_PSE_FSM_PEEK_CR_07, pse_peek[7],
				WF_PSE_FSM_PEEK_CR_08, pse_peek[8],
				WF_PSE_FSM_PEEK_CR_09, pse_peek[9]);

	/* Configuration Info */
	pg_sz = (pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_MASK) >>
		WF_PSE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_SHFT;
	pg_num = (pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_MASK) >>
		 WF_PSE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_SHFT;

	DBGLOG(HAL, INFO,
	"Packet Buffer Control(0x%08x): 0x%08x,Page Size=%d, Page Offset=%d, Total page=%d\n",
		WF_PSE_TOP_PBUF_CTRL_ADDR,
		pse_buf_ctrl, pg_sz,
		((pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_PBUF_OFFSET_MASK) >>
		       WF_PSE_TOP_PBUF_CTRL_PBUF_OFFSET_SHFT),
		pg_num);

	/* Page Flow Control */
	fpg_cnt = (freepg_cnt & WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_SHFT;

	ffa_cnt = (freepg_cnt & WF_PSE_TOP_FREEPG_CNT_FFA_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FFA_CNT_SHFT;


	DBGLOG(HAL, INFO,
	"Free page counter(0x%08x): 0x%08x,The toal page number of free=0x%03x,The free page numbers of free for all=0x%03x\n",
		WF_PSE_TOP_FREEPG_CNT_ADDR, freepg_cnt,
		fpg_cnt, ffa_cnt);

	/* PLE tail / head FID */
	fpg_head = (freepg_head_tail &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_SHFT;
	fpg_tail = (freepg_head_tail &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_SHFT;

	DBGLOG(HAL, INFO,
		"Free page tail/head(0x%08x): 0x%08x,The tail/head page of free page list=0x%03x/0x%03x\n",
		WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR, freepg_head_tail,
		fpg_tail, fpg_head);

	/*Each Group page status */
	group_cnt = sizeof(pse_group) / sizeof(struct pse_group_info);
	for (i = 0; i < group_cnt; i++) {
		group = &pse_group[i];
		HAL_MCR_RD(prAdapter, group->quota_addr, &group_quota);
		HAL_MCR_RD(prAdapter, group->pg_info_addr, &group_info);

		DBGLOG(HAL, INFO,
		"Reserved page counter of %s group(0x%08x):0x%08x,status(0x%08x):0x%08x\n",
		       group->name, group->quota_addr, group_quota,
		       group->pg_info_addr, group_info);
		min_q = (group_quota &
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_MASK) >>
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_SHFT;
		max_q = (group_quota &
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_MASK) >>
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_SHFT;
		DBGLOG(HAL, TRACE,
		     "\tThe max/min quota pages of %s group=0x%03x/0x%03x\n",
		       group->name, max_q, min_q);
		rsv_pg =
		(group_info & WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_SHFT;
		used_pg =
		(group_info & WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_SHFT;
		DBGLOG(HAL, TRACE,
		       "\tThe used/reserved pages of %s group=0x%03x/0x%03x\n",
		       group->name, used_pg, rsv_pg);
	}

	/* Queue Empty Status */
	DBGLOG(HAL, INFO,
	"QUEUE_EMPTY(0x%08x):0x%08x,QUEUE_EMPTY_MASK(0x%08x):0x%08x\n",
		WF_PSE_TOP_QUEUE_EMPTY_ADDR, pse_stat,
		WF_PSE_TOP_QUEUE_EMPTY_MASK_ADDR, pse_stat_mask);

	DBGLOG(HAL, TRACE, "\t\tCPU Q0/1/2/3 empty=%d/%d/%d/%d\n",
	       (pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q0_EMPTY_MASK) >>
		       WF_PSE_TOP_QUEUE_EMPTY_CPU_Q0_EMPTY_SHFT,
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q1_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q1_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q2_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q2_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q3_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q3_EMPTY_SHFT));
	str = "\t\tHIF Q0/1/2/3/4/5/6/7/8/9/10/11";
	DBGLOG(HAL, TRACE,
		"%s empty=%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d\n", str,
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_0_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_0_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_1_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_1_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_2_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_2_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_3_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_3_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_4_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_4_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_5_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_5_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_6_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_6_EMPTY_SHFT),
		((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_7_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_7_EMPTY_SHFT),
		((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_8_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_8_EMPTY_SHFT),
		((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_9_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_9_EMPTY_SHFT),
		((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_10_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_10_EMPTY_SHFT),
		((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_HIF_11_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_HIF_11_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tLMAC TX Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_LMAC_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_LMAC_TX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tMDP TX Q/RX Q empty=%d/%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_MDP_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TX_QUEUE_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_MDP_RX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tSEC TX Q/RX Q empty=%d/%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_TX_QUEUE_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_RX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_RX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tSFD PARK Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SFD_PARK_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SFD_PARK_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tMDP TXIOC Q/RXIOC Q empty=%d/%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC_QUEUE_EMPTY_SHFT),
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tMDP TX1 Q empty=%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TX1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TX1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tSEC TX1 Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_TX1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_TX1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tMDP TXIOC1 Q/RXIOC1 Q empty=%d/%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC1_QUEUE_EMPTY_SHFT),
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, TRACE, "\t\tRLS Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_RLS_Q_EMTPY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_RLS_Q_EMTPY_SHFT));

	/* Nonempty Queue Status */
	DBGLOG(HAL, INFO, "Nonempty Q info:");
	for (i = 0; i < 31; i++) {
		if (((pse_stat & (0x1 << i)) >> i) == 0) {
			uint32_t hfid, tfid, pktcnt, fl_que_ctrl[3] = {0};

			if (pse_queue_empty_info[i].QueueName != NULL) {
				DBGLOG(HAL, INFO, "\t%s: ",
				       pse_queue_empty_info[i].QueueName);
				fl_que_ctrl[0] |=
					WF_PSE_TOP_FL_QUE_CTRL_0_EXECUTE_MASK;
				fl_que_ctrl[0] |=
				(pse_queue_empty_info[i].Portid
				 << WF_PSE_TOP_FL_QUE_CTRL_0_Q_BUF_PID_SHFT);
				fl_que_ctrl[0] |=
				(pse_queue_empty_info[i].Queueid
				 << WF_PSE_TOP_FL_QUE_CTRL_0_Q_BUF_QID_SHFT);
			} else
				continue;

			fl_que_ctrl[0] |= (0x1 << 31);
			HAL_MCR_WR(prAdapter, WF_PSE_TOP_FL_QUE_CTRL_0_ADDR,
				   fl_que_ctrl[0]);
			HAL_MCR_RD(prAdapter, WF_PSE_TOP_FL_QUE_CTRL_2_ADDR,
				   &fl_que_ctrl[1]);
			HAL_MCR_RD(prAdapter, WF_PSE_TOP_FL_QUE_CTRL_3_ADDR,
				   &fl_que_ctrl[2]);
			hfid = (fl_que_ctrl[1] &
				WF_PSE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_MASK) >>
			       WF_PSE_TOP_FL_QUE_CTRL_2_QUEUE_HEAD_FID_SHFT;
			tfid = (fl_que_ctrl[1] &
				WF_PSE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_MASK) >>
			       WF_PSE_TOP_FL_QUE_CTRL_2_QUEUE_TAIL_FID_SHFT;
			pktcnt =
				(fl_que_ctrl[2] &
				 WF_PSE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_MASK) >>
				WF_PSE_TOP_FL_QUE_CTRL_3_QUEUE_PKT_NUM_SHFT;
			DBGLOG(HAL, INFO,
		       "tail/head fid = 0x%03x/0x%03x, pkt cnt = 0x%03x\n",
			       tfid, hfid, pktcnt);
		}
	}
}

static void DumpPPDebugCr(struct ADAPTER *prAdapter)
{
	uint32_t ReadRegValue[4];
	uint32_t u4Value[4] = {0};

	/* 0x820CC0F0 : PP DBG_CTRL */
	ReadRegValue[0] = 0x820CC0F0;
	HAL_MCR_RD(prAdapter, ReadRegValue[0], &u4Value[0]);

	/* 0x820CC0F8 : PP DBG_CS0 */
	ReadRegValue[1] = 0x820CC0F8;
	HAL_MCR_RD(prAdapter, ReadRegValue[1], &u4Value[1]);

	/* 0x820CC0FC : PP DBG_CS1 */
	ReadRegValue[2] = 0x820CC0FC;
	HAL_MCR_RD(prAdapter, ReadRegValue[2], &u4Value[2]);

	/* 0x820CC100 : PP DBG_CS2 */
	ReadRegValue[3] = 0x820CC100;
	HAL_MCR_RD(prAdapter, ReadRegValue[3], &u4Value[3]);

	DBGLOG(HAL, INFO,
	"PP[0x%08x]=0x%08x,[0x%08x]=0x%08x,[0x%08x]=0x%08x,[0x%08x]=0x%08x,",
		ReadRegValue[0], u4Value[0],
		ReadRegValue[1], u4Value[1],
		ReadRegValue[2], u4Value[2],
		ReadRegValue[3], u4Value[3]);
}

void show_wfdma_interrupt_info(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	uint32_t idx;
	uint32_t u4hostBaseCrAddr;
	uint32_t u4DmaCfgCrAddr = 0;
	uint32_t u4DmaCfgCrAddrByWFDMA[CONNAC2X_WFDMA_COUNT];
	uint32_t u4RegValue = 0;
	uint32_t u4RegValueByWFDMA[CONNAC2X_WFDMA_COUNT] = {0};

	/* Dump Interrupt Status info */
	if (enum_wfdma_type == WFDMA_TYPE_HOST) {
		/* Dump Global Status CR only in WFMDA HOST*/
		u4hostBaseCrAddr = CONNAC2X_HOST_EXT_CONN_HIF_WRAP;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_EXT_INT_STA(u4hostBaseCrAddr);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr, &u4RegValue);
	}

	/* Dump PDMA Status CR */
	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
				CONNAC2X_HOST_WPDMA_1_BASE :
				CONNAC2X_HOST_WPDMA_0_BASE;
		else
			u4hostBaseCrAddr = idx ?
				CONNAC2X_MCU_WPDMA_1_BASE :
				CONNAC2X_MCU_WPDMA_0_BASE;

		u4DmaCfgCrAddrByWFDMA[idx] =
			CONNAC2X_WPDMA_INT_STA(u4hostBaseCrAddr);

		HAL_MCR_RD(prAdapter,
			u4DmaCfgCrAddrByWFDMA[idx], &u4RegValueByWFDMA[idx]);
	}

	DBGLOG(INIT, INFO,
	"G_INT_S(0x%08x):0x%08x,W_%d(0x%08x):0x%08x, W_%d(0x%08x):0x%08x\n",
		u4DmaCfgCrAddr, u4RegValue,
		0, u4DmaCfgCrAddrByWFDMA[0], u4RegValueByWFDMA[0],
		1, u4DmaCfgCrAddrByWFDMA[1], u4RegValueByWFDMA[1]);

	/* Dump Interrupt Enable Info */
	if (enum_wfdma_type == WFDMA_TYPE_HOST) {

		/* Dump Global Enable CR */
		u4hostBaseCrAddr = CONNAC2X_HOST_EXT_CONN_HIF_WRAP;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_EXT_INT_MASK(u4hostBaseCrAddr);

		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr, &u4RegValue);
	}

	/* Dump PDMA Enable CR */
	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
				CONNAC2X_HOST_WPDMA_1_BASE :
				CONNAC2X_HOST_WPDMA_0_BASE;
		else
			u4hostBaseCrAddr = idx ?
				CONNAC2X_MCU_WPDMA_1_BASE :
				CONNAC2X_MCU_WPDMA_0_BASE;

		u4DmaCfgCrAddrByWFDMA[idx] =
			CONNAC2X_WPDMA_INT_MASK(u4hostBaseCrAddr);

		HAL_MCR_RD(prAdapter,
			u4DmaCfgCrAddrByWFDMA[idx], &u4RegValueByWFDMA[idx]);
	}

	DBGLOG(INIT, INFO,
	"G_INT_E(0x%08x):0x%08x,W_%d(0x%08x):0x%08x, W_%d(0x%08x):0x%08x\n",
		u4DmaCfgCrAddr, u4RegValue,
		0, u4DmaCfgCrAddrByWFDMA[0], u4RegValueByWFDMA[0],
		1, u4DmaCfgCrAddrByWFDMA[1], u4RegValueByWFDMA[1]);
}

void show_wfdma_glo_info(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	uint32_t idx;
	uint32_t u4hostBaseCrAddr;
	uint32_t u4DmaCfgCrAddr = 0;
	union WPDMA_GLO_CFG_STRUCT GloCfgValue;

	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
			CONNAC2X_HOST_WPDMA_1_BASE :
			CONNAC2X_HOST_WPDMA_0_BASE;
		else
			u4hostBaseCrAddr = idx ?
			CONNAC2X_MCU_WPDMA_1_BASE :
			CONNAC2X_MCU_WPDMA_0_BASE;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_GLO_CFG(u4hostBaseCrAddr);

		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr, &GloCfgValue.word);

		DBGLOG(INIT, INFO,
		"WFDMA_%d GLO(0x%08x):0x%08x,EN T/R=(%d/%d), Busy T/R=(%d/%d)\n",
			idx, u4DmaCfgCrAddr, GloCfgValue.word,
			GloCfgValue.field_conn2x.tx_dma_en,
			GloCfgValue.field_conn2x.rx_dma_en,
			GloCfgValue.field_conn2x.tx_dma_busy,
			GloCfgValue.field_conn2x.rx_dma_busy);
	}

}

void show_wfdma_ring_info(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{

	uint32_t idx;
	uint32_t group_cnt;
	uint32_t u4DmaCfgCrAddr = 0;
	struct wfdma_group_info *group;
	uint32_t u4_hw_desc_base_value = 0;
	uint32_t u4_hw_cnt_value = 0;
	uint32_t u4_hw_cidx_value = 0;
	uint32_t u4_hw_didx_value = 0;
	uint32_t queue_cnt;

	/* Dump All TX Ring Info */
	DBGLOG(HAL, INFO, "----------- TX Ring Config -----------\n");
	DBGLOG(HAL, INFO, "%4s %16s %8s %10s %6s %6s %6s %6s\n",
		"Idx", "Attr", "Reg", "Base", "Cnt", "CIDX", "DIDX", "QCnt");

	/* Dump TX Ring */
	if (enum_wfdma_type == WFDMA_TYPE_HOST)
		group_cnt = sizeof(wfmda_host_tx_group) /
		sizeof(struct wfdma_group_info);
	else
		group_cnt = sizeof(wfmda_wm_tx_group) /
		sizeof(struct wfdma_group_info);

	for (idx = 0; idx < group_cnt; idx++) {
		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			group = &wfmda_host_tx_group[idx];
		else
			group = &wfmda_wm_tx_group[idx];

		u4DmaCfgCrAddr = group->hw_desc_base;

		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr, &u4_hw_desc_base_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x04, &u4_hw_cnt_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x08, &u4_hw_cidx_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x0c, &u4_hw_didx_value);

		queue_cnt = (u4_hw_cidx_value >= u4_hw_didx_value) ?
			(u4_hw_cidx_value - u4_hw_didx_value) :
			(u4_hw_cidx_value - u4_hw_didx_value + u4_hw_cnt_value);

		DBGLOG(HAL, INFO, "%4d %16s %8x %10x %6x %6x %6x %6x\n",
					idx,
					group->name,
					u4DmaCfgCrAddr, u4_hw_desc_base_value,
					u4_hw_cnt_value, u4_hw_cidx_value,
					u4_hw_didx_value, queue_cnt);

	}

	/* Dump All RX Ring Info */
	DBGLOG(HAL, INFO, "----------- RX Ring Config -----------\n");
	DBGLOG(HAL, INFO, "%4s %16s %8s %10s %6s %6s %6s %6s\n",
		"Idx", "Attr", "Reg", "Base", "Cnt", "CIDX", "DIDX", "QCnt");

	/* Dump RX Ring */
	if (enum_wfdma_type == WFDMA_TYPE_HOST)
		group_cnt = sizeof(wfmda_host_rx_group) /
		sizeof(struct wfdma_group_info);
	else
		group_cnt = sizeof(wfmda_wm_rx_group) /
		sizeof(struct wfdma_group_info);

	for (idx = 0; idx < group_cnt; idx++) {
		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			group = &wfmda_host_rx_group[idx];
		else
			group = &wfmda_wm_rx_group[idx];

		u4DmaCfgCrAddr = group->hw_desc_base;

		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr, &u4_hw_desc_base_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x04, &u4_hw_cnt_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x08, &u4_hw_cidx_value);
		HAL_MCR_RD(prAdapter, u4DmaCfgCrAddr+0x0c, &u4_hw_didx_value);

		queue_cnt = (u4_hw_didx_value > u4_hw_cidx_value) ?
			(u4_hw_didx_value - u4_hw_cidx_value - 1) :
			(u4_hw_didx_value - u4_hw_cidx_value
			+ u4_hw_cnt_value - 1);

		DBGLOG(HAL, INFO, "%4d %16s %8x %10x %6x %6x %6x %6x\n",
					idx,
					group->name,
					u4DmaCfgCrAddr, u4_hw_desc_base_value,
					u4_hw_cnt_value, u4_hw_cidx_value,
					u4_hw_didx_value, queue_cnt);
	}

}

static void dump_wfdma_dbg_value(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type,
	IN uint32_t wfdma_idx)
{
#define BUF_SIZE 1024

	uint32_t pdma_base_cr;
	uint32_t set_debug_flag_value;
	char *buf;
	uint32_t pos = 0;
	uint32_t set_debug_cr, get_debug_cr, get_debug_value = 0;

	if (enum_wfdma_type == WFDMA_TYPE_HOST) {
		if (wfdma_idx == 0)
			pdma_base_cr = CONNAC2X_HOST_WPDMA_0_BASE;
		else
			pdma_base_cr = CONNAC2X_HOST_WPDMA_1_BASE;
	} else {
		if (wfdma_idx == 0)
			pdma_base_cr = CONNAC2X_MCU_WPDMA_0_BASE;
		else
			pdma_base_cr = CONNAC2X_MCU_WPDMA_1_BASE;
	}

	buf = (char *) kalMemAlloc(BUF_SIZE, VIR_MEM_TYPE);
	if (!buf) {
		DBGLOG(HAL, ERROR, "Mem allocation failed.\n");
		return;
	}
	set_debug_cr = pdma_base_cr + 0x124;
	get_debug_cr = pdma_base_cr + 0x128;
	kalMemZero(buf, BUF_SIZE);
	pos += kalSnprintf(buf + pos, 50,
			"set_debug_cr:0x%08x get_debug_cr:0x%08x; ",
			set_debug_cr, get_debug_cr);
	for (set_debug_flag_value = 0x100; set_debug_flag_value <= 0x112;
			set_debug_flag_value++) {
		HAL_MCR_WR(prAdapter, set_debug_cr, set_debug_flag_value);
		HAL_MCR_RD(prAdapter, get_debug_cr, &get_debug_value);
		pos += kalSnprintf(buf + pos, 40, "Set:0x%03x, result=0x%08x%s",
			set_debug_flag_value,
			get_debug_value,
			set_debug_flag_value == 0x112 ? "\n" : "; ");
	}
	DBGLOG(HAL, INFO, "%s", buf);
	kalMemFree(buf, VIR_MEM_TYPE, BUF_SIZE);
}

void show_wfdma_dbg_flag_log(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	dump_wfdma_dbg_value(prAdapter, enum_wfdma_type, 0);
	dump_wfdma_dbg_value(prAdapter, enum_wfdma_type, 1);
}

void show_wfdma_dbg_log(
	IN struct ADAPTER *prAdapter,
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	/* Dump Host WFMDA info */
	DBGLOG(HAL, TRACE, "WFMDA Configuration:\n");
	show_wfdma_interrupt_info(prAdapter, enum_wfdma_type);
	show_wfdma_glo_info(prAdapter, enum_wfdma_type);
	show_wfdma_ring_info(prAdapter, enum_wfdma_type);
}

void soc3_0_show_wfdma_info(IN struct ADAPTER *prAdapter)
{
	/* dump WFDMA info by host or WM*/
	show_wfdma_dbg_log(prAdapter, WFDMA_TYPE_HOST);
	show_wfdma_dbg_log(prAdapter, WFDMA_TYPE_WM);

	/* dump debug flag CR by host or WM*/
	show_wfdma_dbg_flag_log(prAdapter, WFDMA_TYPE_HOST);
	show_wfdma_dbg_flag_log(prAdapter, WFDMA_TYPE_WM);

	DumpPPDebugCr(prAdapter);
}

void soc3_0_show_wfdma_info_by_type(IN struct ADAPTER *prAdapter,
	IN bool bShowWFDMA_type)
{
	if (bShowWFDMA_type) {
		show_wfdma_dbg_log(prAdapter, WFDMA_TYPE_WM);
		show_wfdma_dbg_flag_log(prAdapter, WFDMA_TYPE_WM);
	} else {
		show_wfdma_dbg_log(prAdapter, WFDMA_TYPE_HOST);
		show_wfdma_dbg_flag_log(prAdapter, WFDMA_TYPE_HOST);
	}
}

void soc3_0_show_dmashdl_info(IN struct ADAPTER *prAdapter)
{
	uint32_t value = 0;
	uint8_t idx;
	uint32_t rsv_cnt = 0;
	uint32_t src_cnt = 0;
	uint32_t total_src_cnt = 0;
	uint32_t total_rsv_cnt = 0;
	uint32_t ffa_cnt = 0;
	uint32_t free_pg_cnt = 0;
	uint32_t ple_rpg_hif;
	uint32_t ple_upg_hif;
	uint8_t is_mismatch = FALSE;

	DBGLOG(HAL, INFO, "DMASHDL info:\n");

	mt6885HalDmashdlGetRefill(prAdapter);
	mt6885HalDmashdlGetPktMaxPage(prAdapter);

	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_ERROR_FLAG_CTRL_ADDR, &value);
	DBGLOG(HAL, INFO, "DMASHDL ERR FLAG CTRL(0x%08x): 0x%08x\n",
		WF_HIF_DMASHDL_TOP_ERROR_FLAG_CTRL_ADDR, value);

	for (idx = 0; idx < ENUM_MT6885_DMASHDL_GROUP_2; idx++) {
		DBGLOG(HAL, INFO, "Group %d info:\n", idx);
		mt6885HalDmashdlGetGroupControl(prAdapter, idx);
		rsv_cnt = mt6885HalDmashdlGetRsvCount(prAdapter, idx);
		src_cnt = mt6885HalDmashdlGetSrcCount(prAdapter, idx);
		mt6885HalDmashdlGetPKTCount(prAdapter, idx);
		total_src_cnt += src_cnt;
		total_rsv_cnt += rsv_cnt;
	}
	HAL_MCR_RD(prAdapter, WF_HIF_DMASHDL_TOP_STATUS_RD_ADDR, &value);
	ffa_cnt = (value & WF_HIF_DMASHDL_TOP_STATUS_RD_FFA_CNT_MASK) >>
		WF_HIF_DMASHDL_TOP_STATUS_RD_FFA_CNT_SHFT;
	free_pg_cnt = (value &
		WF_HIF_DMASHDL_TOP_STATUS_RD_FREE_PAGE_CNT_MASK) >>
		WF_HIF_DMASHDL_TOP_STATUS_RD_FREE_PAGE_CNT_SHFT;
	DBGLOG(HAL, INFO, "\tDMASHDL Status_RD(0x%08x): 0x%08x\n",
		WF_HIF_DMASHDL_TOP_STATUS_RD_ADDR, value);
	DBGLOG(HAL, INFO, "\tfree page cnt = 0x%03x, ffa cnt = 0x%03x\n",
		free_pg_cnt, ffa_cnt);

	DBGLOG(HAL, INFO, "\nDMASHDL Counter Check:\n");
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_HIF_PG_INFO_ADDR, &value);
	ple_rpg_hif = (value & WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_SHFT;
	ple_upg_hif = (value & WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\tPLE:The used/reserved pages of PLE HIF group=0x%03x/0x%03x\n",
		 ple_upg_hif, ple_rpg_hif);
	DBGLOG(HAL, INFO,
		"\tDMASHDL:The total used pages of group0~14=0x%03x\n",
		total_src_cnt);

	if (ple_upg_hif != total_src_cnt) {
		DBGLOG(HAL, INFO,
			"\tPLE used pages & total used pages mismatch!\n");
		is_mismatch = TRUE;
	}

	DBGLOG(HAL, INFO,
		"\tThe total reserved pages of group0~14=0x%03x\n",
		total_rsv_cnt);
	DBGLOG(HAL, INFO,
		"\tThe total ffa pages of group0~14=0x%03x\n",
		ffa_cnt);
	DBGLOG(HAL, INFO,
		"\tThe total free pages of group0~14=0x%03x\n",
		free_pg_cnt);

	if (free_pg_cnt != total_rsv_cnt + ffa_cnt) {
		DBGLOG(HAL, INFO,
			"\tmismatch(total_rsv_cnt + ffa_cnt in DMASHDL)\n");
		is_mismatch = TRUE;
	}

	if (free_pg_cnt != ple_rpg_hif) {
		DBGLOG(HAL, INFO, "\tmismatch(reserved pages in PLE)\n");
		is_mismatch = TRUE;
	}


	if (!is_mismatch)
		DBGLOG(HAL, INFO, "DMASHDL: no counter mismatch\n");
}

void soc3_0_dump_mac_info(IN struct ADAPTER *prAdapter)
{
#define BUF_SIZE 1024
#define CR_COUNT 12
#define LOOP_COUNT 30

	uint32_t i = 0, j = 0, pos = 0;
	uint32_t value = 0;
	uint32_t cr_band0[] = {
			0x820ED020,
			0x820E4120,
			0x820E4128,
			0x820E22F0,
			0x820E22F4,
			0x820E22F8,
			0x820E22FC,
			0x820E3190,
			0x820C0220,
			0x820C0114,
			0x820C0154,
			0x820E0024
	};
	uint32_t cr_band1[] = {
			0x820FD020,
			0x820F4120,
			0x820F4128,
			0x820F22F0,
			0x820F22F4,
			0x820F22F8,
			0x820F22FC,
			0x820F3190,
			0x820C0220,
			0x820C0114,
			0x820C0154,
			0x820F0024
	};

	char *buf = (char *) kalMemAlloc(BUF_SIZE, VIR_MEM_TYPE);

	DBGLOG(HAL, INFO, "Dump for band0\n");
	HAL_MCR_WR(prAdapter, 0x7C006100, 0x1F);
	HAL_MCR_WR(prAdapter, 0x7C006104, 0x07070707);
	HAL_MCR_WR(prAdapter, 0x7C006108, 0x0A0A0909);
	HAL_MCR_RD(prAdapter, 0x820D0000, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820D0000 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820E3080, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820E3080 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C0028, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C0028 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C8028, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C8028 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C8030, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C8030 = 0x%08x\n", value);
	/* Band 0 TXV_C and TXV_P */
	for (i = 0x820E412C; i < 0x820E4160; i += 4) {
		HAL_MCR_RD(prAdapter, i, &value);
		DBGLOG(HAL, INFO, "Dump CR: 0x%08x = 0x%08x\n", i, value);
		kalMdelay(1);
	}
	HAL_MCR_RD(prAdapter, 0x820E206C, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820E206C = 0x%08x\n", value);

	if (buf) {
		kalMemZero(buf, BUF_SIZE);
		for (i = 0; i < LOOP_COUNT; i++) {
			for (j = 0; j < CR_COUNT; j++) {
				HAL_MCR_RD(prAdapter, cr_band0[j], &value);
				pos += kalSnprintf(buf + pos, 25,
					"0x%08x = 0x%08x%s", cr_band0[j], value,
					j == CR_COUNT - 1 ? ";" : ",");
			}
			DBGLOG(HAL, INFO, "Dump CR: %s\n", buf);
			pos = 0;
			kalMdelay(1);
		}
	}

	DBGLOG(HAL, INFO, "Dump for band1\n");
	HAL_MCR_WR(prAdapter, 0x7C006400, 0x1F);
	HAL_MCR_WR(prAdapter, 0x7C006404, 0x07070707);
	HAL_MCR_WR(prAdapter, 0x7C006408, 0x0A0A0909);
	HAL_MCR_RD(prAdapter, 0x820D0000, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820D0000 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820F3080, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820F3080 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C0028, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C0028 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C8028, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C8028 = 0x%08x\n", value);
	HAL_MCR_RD(prAdapter, 0x820C8030, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820C8030 = 0x%08x\n", value);
	/* Band 0 TXV_C and TXV_P */
	for (i = 0x820F412C; i < 0x820F4160; i += 4) {
		HAL_MCR_RD(prAdapter, i, &value);
		DBGLOG(HAL, INFO, "Dump CR: 0x%08x = 0x%08x\n", i, value);
		kalMdelay(1);
	}
	HAL_MCR_RD(prAdapter, 0x820F206C, &value);
	DBGLOG(HAL, INFO, "Dump CR: 0x820F206C = 0x%08x\n", value);

	if (buf) {
		kalMemZero(buf, BUF_SIZE);
		for (i = 0; i < LOOP_COUNT; i++) {
			for (j = 0; j < CR_COUNT; j++) {
				HAL_MCR_RD(prAdapter, cr_band1[j], &value);
				pos += kalSnprintf(buf + pos, 25,
					"0x%08x = 0x%08x%s", cr_band1[j], value,
					j == CR_COUNT - 1 ? ";" : ",");
			}
			DBGLOG(HAL, INFO, "Dump CR: %s\n", buf);
			pos = 0;
			kalMdelay(1);
		}
	}

	if (buf)
		kalMemFree(buf, VIR_MEM_TYPE, BUF_SIZE);
}

#if WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE
void show_wfdma_interrupt_info_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	uint32_t idx;
	uint32_t u4hostBaseCrAddr = 0;
	uint32_t u4DmaCfgCrAddr = 0;
	uint32_t u4DmaCfgCrAddrByWFDMA[CONNAC2X_WFDMA_COUNT];
	uint32_t u4RegValue = 0;
	uint32_t u4RegValueByWFDMA[CONNAC2X_WFDMA_COUNT] = {0};

	/* Dump Interrupt Status info */
	if (enum_wfdma_type == WFDMA_TYPE_HOST) {
		/* Dump Global Status CR only in WFMDA HOST*/
		u4hostBaseCrAddr = CONNAC2X_HOST_EXT_CONN_HIF_WRAP_DRIVER_BASE;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_EXT_INT_STA(u4hostBaseCrAddr);
		wf_ioremap_read(u4DmaCfgCrAddr, &u4RegValue);
	}

	/* Dump PDMA Status CR */
	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
				CONNAC2X_HOST_WPDMA_1_DRIVER_BASE :
				CONNAC2X_HOST_WPDMA_0_DRIVER_BASE;
		else
			u4hostBaseCrAddr = idx ?
				CONNAC2X_MCU_WPDMA_1_DRIVER_BASE :
				CONNAC2X_MCU_WPDMA_0_DRIVER_BASE;

		u4DmaCfgCrAddrByWFDMA[idx] =
			CONNAC2X_WPDMA_INT_STA(u4hostBaseCrAddr);

		wf_ioremap_read(u4DmaCfgCrAddrByWFDMA[idx],
			&u4RegValueByWFDMA[idx]);
	}

	DBGLOG(INIT, INFO,
	"G_INT_S(0x%08x):0x%08x,W_%d(0x%08x):0x%08x, W_%d(0x%08x):0x%08x\n",
		u4DmaCfgCrAddr, u4RegValue,
		0, u4DmaCfgCrAddrByWFDMA[0], u4RegValueByWFDMA[0],
		1, u4DmaCfgCrAddrByWFDMA[1], u4RegValueByWFDMA[1]);

	/* Dump Interrupt Enable Info */
	if (enum_wfdma_type == WFDMA_TYPE_HOST) {

		/* Dump Global Enable CR */
		u4hostBaseCrAddr = CONNAC2X_HOST_EXT_CONN_HIF_WRAP_DRIVER_BASE;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_EXT_INT_MASK(u4hostBaseCrAddr);

		wf_ioremap_read(u4DmaCfgCrAddr, &u4RegValue);
	}

	/* Dump PDMA Enable CR */
	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
				CONNAC2X_HOST_WPDMA_1_DRIVER_BASE :
				CONNAC2X_HOST_WPDMA_0_DRIVER_BASE;
		else
			u4hostBaseCrAddr = idx ?
				CONNAC2X_MCU_WPDMA_1_DRIVER_BASE :
				CONNAC2X_MCU_WPDMA_0_DRIVER_BASE;

		u4DmaCfgCrAddrByWFDMA[idx] =
			CONNAC2X_WPDMA_INT_MASK(u4hostBaseCrAddr);

		wf_ioremap_read(u4DmaCfgCrAddrByWFDMA[idx],
			&u4RegValueByWFDMA[idx]);
	}

	DBGLOG(INIT, INFO,
	"G_INT_E(0x%08x):0x%08x,W_%d(0x%08x):0x%08x, W_%d(0x%08x):0x%08x\n",
		u4DmaCfgCrAddr, u4RegValue,
		0, u4DmaCfgCrAddrByWFDMA[0], u4RegValueByWFDMA[0],
		1, u4DmaCfgCrAddrByWFDMA[1], u4RegValueByWFDMA[1]);
}

void show_wfdma_glo_info_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	uint32_t idx;
	uint32_t u4hostBaseCrAddr = 0;
	uint32_t u4DmaCfgCrAddr = 0;
	union WPDMA_GLO_CFG_STRUCT GloCfgValue;

	for (idx = 0; idx < CONNAC2X_WFDMA_COUNT; idx++) {

		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			u4hostBaseCrAddr = idx ?
			CONNAC2X_HOST_WPDMA_1_DRIVER_BASE :
			CONNAC2X_HOST_WPDMA_0_DRIVER_BASE;
		else
			u4hostBaseCrAddr = idx ?
			CONNAC2X_MCU_WPDMA_1_DRIVER_BASE :
			CONNAC2X_MCU_WPDMA_0_DRIVER_BASE;

		u4DmaCfgCrAddr = CONNAC2X_WPDMA_GLO_CFG(u4hostBaseCrAddr);

		wf_ioremap_read(u4DmaCfgCrAddr, &GloCfgValue.word);

		DBGLOG(INIT, INFO,
		"WFDMA_%d GLO(0x%08x):0x%08x,EN T/R=(%d/%d), Busy T/R=(%d/%d)\n",
			idx, u4DmaCfgCrAddr, GloCfgValue.word,
			GloCfgValue.field_conn2x.tx_dma_en,
			GloCfgValue.field_conn2x.rx_dma_en,
			GloCfgValue.field_conn2x.tx_dma_busy,
			GloCfgValue.field_conn2x.rx_dma_busy);
	}

}

void show_wfdma_ring_info_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{

	uint32_t idx = 0;
	uint32_t group_cnt = 0;
	uint32_t u4DmaCfgCrAddr;
	struct wfdma_group_info *group;
	uint32_t u4_hw_desc_base_value = 0;
	uint32_t u4_hw_cnt_value = 0;
	uint32_t u4_hw_cidx_value = 0;
	uint32_t u4_hw_didx_value = 0;
	uint32_t queue_cnt;

	/* Dump All TX Ring Info */
	DBGLOG(HAL, TRACE, "----------- TX Ring Config -----------\n");
	DBGLOG(HAL, TRACE, "%4s %16s %8s %10s %6s %6s %6s %6s\n",
		"Idx", "Attr", "Reg", "Base", "Cnt", "CIDX", "DIDX", "QCnt");

	/* Dump TX Ring */
	if (enum_wfdma_type == WFDMA_TYPE_HOST)
		group_cnt = sizeof(wfmda_host_tx_group_driver_base) /
		sizeof(struct wfdma_group_info);
	else
		group_cnt = sizeof(wfmda_wm_tx_group_driver_base) /
		sizeof(struct wfdma_group_info);

	for (idx = 0; idx < group_cnt; idx++) {
		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			group = &wfmda_host_tx_group_driver_base[idx];
		else
			group = &wfmda_wm_tx_group_driver_base[idx];

		u4DmaCfgCrAddr = group->hw_desc_base;

		wf_ioremap_read(u4DmaCfgCrAddr, &u4_hw_desc_base_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x04, &u4_hw_cnt_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x08, &u4_hw_cidx_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x0c, &u4_hw_didx_value);

		queue_cnt = (u4_hw_cidx_value >= u4_hw_didx_value) ?
			(u4_hw_cidx_value - u4_hw_didx_value) :
			(u4_hw_cidx_value - u4_hw_didx_value + u4_hw_cnt_value);

		DBGLOG(HAL, INFO, "%4d %16s %8x %10x %6x %6x %6x %6x\n",
					idx,
					group->name,
					u4DmaCfgCrAddr, u4_hw_desc_base_value,
					u4_hw_cnt_value, u4_hw_cidx_value,
					u4_hw_didx_value, queue_cnt);

	}

	/* Dump All RX Ring Info */
	DBGLOG(HAL, TRACE, "----------- RX Ring Config -----------\n");
	DBGLOG(HAL, TRACE, "%4s %16s %8s %10s %6s %6s %6s %6s\n",
		"Idx", "Attr", "Reg", "Base", "Cnt", "CIDX", "DIDX", "QCnt");

	/* Dump RX Ring */
	if (enum_wfdma_type == WFDMA_TYPE_HOST)
		group_cnt = sizeof(wfmda_host_rx_group_driver_base) /
		sizeof(struct wfdma_group_info);
	else
		group_cnt = sizeof(wfmda_wm_rx_group_driver_base) /
		sizeof(struct wfdma_group_info);

	for (idx = 0; idx < group_cnt; idx++) {
		if (enum_wfdma_type == WFDMA_TYPE_HOST)
			group = &wfmda_host_rx_group_driver_base[idx];
		else
			group = &wfmda_wm_rx_group_driver_base[idx];

		u4DmaCfgCrAddr = group->hw_desc_base;

		wf_ioremap_read(u4DmaCfgCrAddr, &u4_hw_desc_base_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x04, &u4_hw_cnt_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x08, &u4_hw_cidx_value);
		wf_ioremap_read(u4DmaCfgCrAddr+0x0c, &u4_hw_didx_value);

		queue_cnt = (u4_hw_didx_value > u4_hw_cidx_value) ?
			(u4_hw_didx_value - u4_hw_cidx_value - 1) :
			(u4_hw_didx_value - u4_hw_cidx_value
			+ u4_hw_cnt_value - 1);

		DBGLOG(HAL, INFO, "%4d %16s %8x %10x %6x %6x %6x %6x\n",
					idx,
					group->name,
					u4DmaCfgCrAddr, u4_hw_desc_base_value,
					u4_hw_cnt_value, u4_hw_cidx_value,
					u4_hw_didx_value, queue_cnt);
	}

}

static void dump_dbg_value_without_adapter(
	IN uint32_t pdma_base_cr,
	IN uint32_t set_value,
	IN uint32_t isMandatoryDump)
{
	uint32_t set_debug_cr = 0;
	uint32_t get_debug_cr = 0;
	uint32_t get_debug_value = 0;

	set_debug_cr = pdma_base_cr + 0x124;
	get_debug_cr = pdma_base_cr + 0x128;

	wf_ioremap_write(set_debug_cr, set_value);
	wf_ioremap_read(get_debug_cr, &get_debug_value);

	if (isMandatoryDump == 1) {
		DBGLOG(INIT, INFO, "set(0x%08x):0x%08x, get(0x%08x):0x%08x,",
						set_debug_cr, set_value,
						get_debug_cr, get_debug_value);
	} else {
		DBGLOG(INIT, TRACE, "set(0x%08x):0x%08x, get(0x%08x):0x%08x,",
						set_debug_cr, set_value,
						get_debug_cr, get_debug_value);
	}
}

static void dump_wfdma_dbg_value_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type,
	IN uint32_t wfdma_idx,
	IN uint32_t isMandatoryDump)
{
	uint32_t pdma_base_cr = 0;
	uint32_t set_debug_flag_value = 0;

	if (enum_wfdma_type == WFDMA_TYPE_HOST) {
		if (wfdma_idx == 0)
			pdma_base_cr = CONNAC2X_HOST_WPDMA_0_DRIVER_BASE;
		else
			pdma_base_cr = CONNAC2X_HOST_WPDMA_1_DRIVER_BASE;
	} else{
		if (wfdma_idx == 0)
			pdma_base_cr = CONNAC2X_MCU_WPDMA_0_DRIVER_BASE;
		else
			pdma_base_cr = CONNAC2X_MCU_WPDMA_1_DRIVER_BASE;
	}

	set_debug_flag_value = 0x100;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x101;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x102;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x103;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x104;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x105;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x107;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x10A;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x10D;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x10E;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x10F;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x110;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x111;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

	set_debug_flag_value = 0x112;
	dump_dbg_value_without_adapter(pdma_base_cr,
		set_debug_flag_value, isMandatoryDump);

}

static void show_wfdma_dbg_flag_log_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type,
	IN uint32_t isMandatoryDump)
{
	dump_wfdma_dbg_value_without_adapter(
		enum_wfdma_type, 0, isMandatoryDump);
	dump_wfdma_dbg_value_without_adapter(
		enum_wfdma_type, 1, isMandatoryDump);
}

static void show_wfdma_dbg_log_without_adapter(
	IN enum _ENUM_WFDMA_TYPE_T enum_wfdma_type)
{
	/* Dump Host WFMDA info */
	DBGLOG(HAL, TRACE, "WFMDA Configuration:\n");
	show_wfdma_interrupt_info_without_adapter(enum_wfdma_type);
	show_wfdma_glo_info_without_adapter(enum_wfdma_type);
	show_wfdma_ring_info_without_adapter(enum_wfdma_type);
}

/* bIsHostDMA = 1 (how Host PDMA) else (WM PDMA) */
void soc3_0_show_wfdma_info_by_type_without_adapter(
	IN bool bShowWFDMA_type)
{
	if (bShowWFDMA_type) {
		show_wfdma_dbg_log_without_adapter(WFDMA_TYPE_WM);
		show_wfdma_dbg_flag_log_without_adapter(WFDMA_TYPE_WM, TRUE);
	} else {
		show_wfdma_dbg_log_without_adapter(WFDMA_TYPE_HOST);
		show_wfdma_dbg_flag_log_without_adapter(WFDMA_TYPE_HOST, TRUE);
	}
}

#endif /* WFSYS_SUPPORT_BUS_HANG_READ_FROM_DRIVER_BASE */

#endif /* SOC3_0 */
