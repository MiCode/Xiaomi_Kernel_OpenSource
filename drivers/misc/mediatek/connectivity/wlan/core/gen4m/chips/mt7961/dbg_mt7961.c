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
 *[File]             dbg_mt7961.c
 *[Version]          v1.0
 *[Revision Date]    2019-04-09
 *[Author]
 *[Description]
 *    The program provides WIFI FALCON MAC Debug APIs
 *[Copyright]
 *    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/

#ifdef MT7961
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "coda/mt7961/wf_ple_top.h"
#include "coda/mt7961/wf_pse_top.h"
#include "precomp.h"
#include "mt_dmac.h"
#include "wf_ple.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct pse_group_info {
	char name[8];
	u_int32_t quota_addr;
	u_int32_t pg_info_addr;
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
	{"HIF0", WF_PSE_TOP_PG_HIF0_GROUP_ADDR, WF_PSE_TOP_HIF0_PG_INFO_ADDR},
	{"HIF1", WF_PSE_TOP_PG_HIF1_GROUP_ADDR, WF_PSE_TOP_HIF1_PG_INFO_ADDR},
	{"HIF2", WF_PSE_TOP_PG_HIF2_GROUP_ADDR, WF_PSE_TOP_HIF2_PG_INFO_ADDR},
	{"CPU",  WF_PSE_TOP_PG_CPU_GROUP_ADDR,  WF_PSE_TOP_CPU_PG_INFO_ADDR},
	{"PLE",  WF_PSE_TOP_PG_PLE_GROUP_ADDR,  WF_PSE_TOP_PLE_PG_INFO_ADDR},
	{"PLE1", WF_PSE_TOP_PG_PLE1_GROUP_ADDR, WF_PSE_TOP_PLE1_PG_INFO_ADDR},
	{"LMAC0", WF_PSE_TOP_PG_LMAC0_GROUP_ADDR,
			WF_PSE_TOP_LMAC0_PG_INFO_ADDR},
	{"LMAC1", WF_PSE_TOP_PG_LMAC1_GROUP_ADDR,
			WF_PSE_TOP_LMAC1_PG_INFO_ADDR},
	{"LMAC2", WF_PSE_TOP_PG_LMAC2_GROUP_ADDR,
			WF_PSE_TOP_LMAC2_PG_INFO_ADDR},
	{"LMAC3", WF_PSE_TOP_PG_LMAC3_GROUP_ADDR,
			WF_PSE_TOP_LMAC3_PG_INFO_ADDR},
	{"MDP",  WF_PSE_TOP_PG_MDP_GROUP_ADDR,  WF_PSE_TOP_MDP_PG_INFO_ADDR},
	{"MDP1", WF_PSE_TOP_PG_MDP1_GROUP_ADDR, WF_PSE_TOP_MDP1_PG_INFO_ADDR},
	{"MDP2", WF_PSE_TOP_PG_MDP2_GROUP_ADDR, WF_PSE_TOP_MDP2_PG_INFO_ADDR},
};

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
void mt7961_show_ple_info(
	struct ADAPTER *prAdapter,
	u_int8_t fgDumpTxd)
{
	u_int32_t ple_buf_ctrl, pg_sz, pg_num;
	u_int32_t ple_stat[25] = {0}, pg_flow_ctrl[8] = {0};
	u_int32_t sta_pause[6] = {0}, dis_sta_map[6] = {0};
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail, hif_max_q, hif_min_q;
	u_int32_t rpg_hif, upg_hif, cpu_max_q, cpu_min_q, rpg_cpu, upg_cpu;
	u_int32_t i, j;
#if 0
	u_int32_t ple_txcmd_stat;
#endif

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
	/* Configuration Info */
	DBGLOG(HAL, INFO, "PLE Configuration Info:\n");

	DBGLOG(HAL, INFO, "\tPacket Buffer Control(0x%08x): 0x%08x\n",
		WF_PLE_TOP_PBUF_CTRL_ADDR,
		ple_buf_ctrl);
	pg_sz = (ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_MASK) >>
		WF_PLE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_SHFT;
	DBGLOG(HAL, INFO, "\t\tPage Size=%d(%d bytes per page)\n", pg_sz,
	       (pg_sz == 1 ? 128 : 64));
	DBGLOG(HAL, INFO, "\t\tPage Offset=%d(in unit of 2KB)\n",
	       (ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_PBUF_OFFSET_MASK) >>
		       WF_PLE_TOP_PBUF_CTRL_PBUF_OFFSET_SHFT);
	pg_num = (ple_buf_ctrl & WF_PLE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_MASK) >>
		 WF_PLE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_SHFT;
	DBGLOG(HAL, INFO, "\t\tTotal Page=%d pages\n", pg_num);

	/* Page Flow Control */
	DBGLOG(HAL, INFO, "PLE Page Flow Control:\n");
	DBGLOG(HAL, INFO, "\tFree page counter(0x%08x): 0x%08x\n",
		WF_PLE_TOP_FREEPG_CNT_ADDR,
		pg_flow_ctrl[0]);
	fpg_cnt = (pg_flow_ctrl[0] & WF_PLE_TOP_FREEPG_CNT_FREEPG_CNT_MASK) >>
		WF_PLE_TOP_FREEPG_CNT_FREEPG_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe toal page number of free=0x%03x\n", fpg_cnt);
	ffa_cnt = (pg_flow_ctrl[0] & WF_PLE_TOP_FREEPG_CNT_FFA_CNT_MASK) >>
		  WF_PLE_TOP_FREEPG_CNT_FFA_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe free page numbers of free for all=0x%03x\n",
	       ffa_cnt);
	DBGLOG(HAL, INFO, "\tFree page head and tail(0x%08x): 0x%08x\n",
		WF_PLE_TOP_FREEPG_HEAD_TAIL_ADDR,
		pg_flow_ctrl[1]);
	fpg_head = (pg_flow_ctrl[1] &
		    WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_MASK) >>
		   WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_SHFT;
	fpg_tail = (pg_flow_ctrl[1] &
		    WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_MASK) >>
		   WF_PLE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe tail/head page of free page list=0x%03x/0x%03x\n",
	       fpg_tail, fpg_head);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of HIF group(0x%08x): 0x%08x\n",
		WF_PLE_TOP_PG_HIF_GROUP_ADDR,
		pg_flow_ctrl[2]);
	DBGLOG(HAL, INFO, "\tHIF group page status(0x%08x): 0x%08x\n",
		WF_PLE_TOP_HIF_PG_INFO_ADDR,
		pg_flow_ctrl[3]);
	hif_min_q = (pg_flow_ctrl[2] &
		     WF_PLE_TOP_PG_HIF_GROUP_HIF_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_GROUP_HIF_MIN_QUOTA_SHFT;
	hif_max_q = (pg_flow_ctrl[2] &
		     WF_PLE_TOP_PG_HIF_GROUP_HIF_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_GROUP_HIF_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of HIF group=0x%03x/0x%03x\n",
	       hif_max_q, hif_min_q);
	rpg_hif = (pg_flow_ctrl[3] & WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_RSV_CNT_SHFT;
	upg_hif = (pg_flow_ctrl[3] & WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_MASK) >>
		  WF_PLE_TOP_HIF_PG_INFO_HIF_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of HIF group=0x%03x/0x%03x\n",
	       upg_hif, rpg_hif);

	DBGLOG(HAL, INFO,
	"\tReserved page counter of HIF_TXCMD group(0x%08x): 0x%08x\n",
	WF_PLE_TOP_PG_HIF_WMTXD_GROUP_ADDR,
	pg_flow_ctrl[6]);
	DBGLOG(HAL, INFO, "\tHIF_TXCMD group page status(0x%08x): 0x%08x\n",
		WF_PLE_TOP_HIF_WMTXD_PG_INFO_ADDR,
		pg_flow_ctrl[7]);
	cpu_min_q = (pg_flow_ctrl[6] &
		     WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MIN_QUOTA_SHFT;
	cpu_max_q = (pg_flow_ctrl[6] &
		     WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_HIF_TXCMD_GROUP_HIF_TXCMD_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of HIF_TXCMD group=0x%03x/0x%03x\n",
	       cpu_max_q, cpu_min_q);
	rpg_cpu = (pg_flow_ctrl[7] &
		   WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_SRC_CNT_MASK) >>
		  WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_SRC_CNT_SHFT;
	upg_cpu = (pg_flow_ctrl[7] &
		   WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_RSV_CNT_MASK) >>
		  WF_PLE_TOP_HIF_TXCMD_PG_INFO_HIF_TXCMD_RSV_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of HIF_TXCMD group=0x%03x/0x%03x\n",
	       upg_cpu, rpg_cpu);

	DBGLOG(HAL, INFO,
		"\tReserved page counter of CPU group(0x%08x): 0x%08x\n",
		WF_PLE_TOP_PG_CPU_GROUP_ADDR,
		pg_flow_ctrl[4]);
	DBGLOG(HAL, INFO, "\tCPU group page status(0x%08x): 0x%08x\n",
		WF_PLE_TOP_CPU_PG_INFO_ADDR,
	       pg_flow_ctrl[5]);
	cpu_min_q = (pg_flow_ctrl[4] &
		     WF_PLE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_SHFT;
	cpu_max_q = (pg_flow_ctrl[4] &
		     WF_PLE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_MASK) >>
		    WF_PLE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of CPU group=0x%03x/0x%03x\n",
	       cpu_max_q, cpu_min_q);
	rpg_cpu = (pg_flow_ctrl[5] & WF_PLE_TOP_CPU_PG_INFO_CPU_RSV_CNT_MASK) >>
		  WF_PLE_TOP_CPU_PG_INFO_CPU_RSV_CNT_SHFT;
	upg_cpu = (pg_flow_ctrl[5] & WF_PLE_TOP_CPU_PG_INFO_CPU_SRC_CNT_MASK) >>
		  WF_PLE_TOP_CPU_PG_INFO_CPU_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of CPU group=0x%03x/0x%03x\n",
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

		DBGLOG(HAL, INFO, "\n");
	}

	DBGLOG(HAL, INFO, "Nonempty Q info:\n");

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

void mt7961_show_pse_info(
	struct ADAPTER *prAdapter)
{
	u_int32_t pse_buf_ctrl, pg_sz, pg_num;
	u_int32_t pse_stat;
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail;
	u_int32_t max_q, min_q, rsv_pg, used_pg;
	u_int32_t i, group_cnt;
	u_int32_t group_quota, group_info, freepg_cnt, freepg_head_tail;
	struct pse_group_info *group;
	char *str;

	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PBUF_CTRL_ADDR, &pse_buf_ctrl);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_QUEUE_EMPTY_ADDR, &pse_stat);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_CNT_ADDR, &freepg_cnt);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR,
		   &freepg_head_tail);

	/* Configuration Info */
	DBGLOG(HAL, INFO, "PSE Configuration Info:\n");
	DBGLOG(HAL, INFO, "\tPacket Buffer Control(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PBUF_CTRL_ADDR,
		pse_buf_ctrl);
	pg_sz = (pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_MASK) >>
		WF_PSE_TOP_PBUF_CTRL_PAGE_SIZE_CFG_SHFT;
	DBGLOG(HAL, INFO, "\t\tPage Size=%d(%d bytes per page)\n", pg_sz,
	       (pg_sz == 1 ? 256 : 128));
	DBGLOG(HAL, INFO, "\t\tPage Offset=%d(in unit of 64KB)\n",
	       (pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_PBUF_OFFSET_MASK) >>
		       WF_PSE_TOP_PBUF_CTRL_PBUF_OFFSET_SHFT);
	pg_num = (pse_buf_ctrl & WF_PSE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_MASK) >>
		 WF_PSE_TOP_PBUF_CTRL_TOTAL_PAGE_NUM_SHFT;
	DBGLOG(HAL, INFO, "\t\tTotal page numbers=%d pages\n", pg_num);
	/* Page Flow Control */
	DBGLOG(HAL, INFO, "PSE Page Flow Control:\n");
	DBGLOG(HAL, INFO, "\tFree page counter(0x%08x): 0x%08x\n",
		WF_PSE_TOP_FREEPG_CNT_ADDR, freepg_cnt);
	fpg_cnt = (freepg_cnt & WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe toal page number of free=0x%03x\n", fpg_cnt);
	ffa_cnt = (freepg_cnt & WF_PSE_TOP_FREEPG_CNT_FFA_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FFA_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe free page numbers of free for all=0x%03x\n",
		ffa_cnt);
	DBGLOG(HAL, INFO, "\tFree page head and tail(0x%08x): 0x%08x\n",
		WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR, freepg_head_tail);
	fpg_head = (freepg_head_tail &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_SHFT;
	fpg_tail = (freepg_head_tail &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe tail/head page of free page list=0x%03x/0x%03x\n",
	       fpg_tail, fpg_head);

	group_cnt = sizeof(pse_group) / sizeof(struct pse_group_info);
	for (i = 0; i < group_cnt; i++) {
		group = &pse_group[i];
		HAL_MCR_RD(prAdapter, group->quota_addr, &group_quota);
		HAL_MCR_RD(prAdapter, group->pg_info_addr, &group_info);

		DBGLOG(HAL, INFO,
		       "\tReserved page counter of %s group(0x%08x): 0x%08x\n",
		       group->name, group->quota_addr, group_quota);
		DBGLOG(HAL, INFO, "\t%s group page status(0x%08x): 0x%08x\n",
			group->name, group->pg_info_addr, group_info);
		min_q = (group_quota &
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_MASK) >>
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_SHFT;
		max_q = (group_quota &
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_MASK) >>
			WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_SHFT;
		DBGLOG(HAL, INFO,
		     "\t\tThe max/min quota pages of %s group=0x%03x/0x%03x\n",
		       group->name, max_q, min_q);
		rsv_pg =
		(group_info & WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_SHFT;
		used_pg =
		(group_info & WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_SHFT;
		DBGLOG(HAL, INFO,
		       "\t\tThe used/reserved pages of %s group=0x%03x/0x%03x\n",
		       group->name, used_pg, rsv_pg);
	}

	/* Queue Empty Status */
	DBGLOG(HAL, INFO, "PSE Queue Empty Status:\n");
	DBGLOG(HAL, INFO, "\tQUEUE_EMPTY(0x%08x): 0x%08x\n",
		WF_PSE_TOP_QUEUE_EMPTY_ADDR,
		pse_stat);
	DBGLOG(HAL, INFO, "\t\tCPU Q0/1/2/3 empty=%d/%d/%d/%d\n",
	       (pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q0_EMPTY_MASK) >>
		       WF_PSE_TOP_QUEUE_EMPTY_CPU_Q0_EMPTY_SHFT,
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q1_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q1_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q2_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q2_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_CPU_Q3_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_CPU_Q3_EMPTY_SHFT));
	str = "\t\tHIF Q0/1/2/3/4/5/6/7/8/9/10/11";
	DBGLOG(HAL, INFO,
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
	DBGLOG(HAL, INFO, "\t\tLMAC TX Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_LMAC_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_LMAC_TX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tMDP TX Q/RX Q empty=%d/%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_MDP_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TX_QUEUE_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_MDP_RX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tSEC TX Q/RX Q empty=%d/%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_TX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_TX_QUEUE_EMPTY_SHFT),
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_RX_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_RX_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tSFD PARK Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SFD_PARK_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SFD_PARK_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tMDP TXIOC Q/RXIOC Q empty=%d/%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC_QUEUE_EMPTY_SHFT),
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tMDP TX1 Q empty=%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TX1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TX1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tSEC TX1 Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_SEC_TX1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_SEC_TX1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tMDP TXIOC1 Q/RXIOC1 Q empty=%d/%d\n",
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_TXIOC1_QUEUE_EMPTY_SHFT),
	       ((pse_stat &
		 WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC1_QUEUE_EMPTY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_MDP_RXIOC1_QUEUE_EMPTY_SHFT));
	DBGLOG(HAL, INFO, "\t\tRLS Q empty=%d\n",
	       ((pse_stat & WF_PSE_TOP_QUEUE_EMPTY_RLS_Q_EMTPY_MASK) >>
		WF_PSE_TOP_QUEUE_EMPTY_RLS_Q_EMTPY_SHFT));
	DBGLOG(HAL, INFO, "Nonempty Q info:\n");

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
#endif /* MT7961 */
