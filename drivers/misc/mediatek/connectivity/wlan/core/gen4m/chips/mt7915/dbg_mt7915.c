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
 *[File]             dbg_mt7915.c
 *[Version]          v1.0
 *[Revision Date]    2019-04-09
 *[Author]
 *[Description]
 *    The program provides WIFI FALCON MAC Debug APIs
 *[Copyright]
 *    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/

#ifdef MT7915
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "coda/mt7915/wf_ple_top.h"
#include "coda/mt7915/wf_pse_top.h"
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
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},			     /* 4~7 not defined */
	{"HIF Q0", ENUM_UMAC_HIF_PORT_0, 0}, /* Q8 */
	{"HIF Q1", ENUM_UMAC_HIF_PORT_0, 1},
	{"HIF Q2", ENUM_UMAC_HIF_PORT_0, 2},
	{"HIF Q3", ENUM_UMAC_HIF_PORT_0, 3},
	{"HIF Q4", ENUM_UMAC_HIF_PORT_0, 4},
	{"HIF Q5", ENUM_UMAC_HIF_PORT_0, 5},
	{NULL, 0, 0},
	{NULL, 0, 0}, /* 14~15 not defined */
	{"LMAC Q", ENUM_UMAC_LMAC_PORT_2, 0},
	{"MDP TX Q", ENUM_UMAC_LMAC_PORT_2, 1},
	{"MDP RX Q", ENUM_UMAC_LMAC_PORT_2, 2},
	{"SEC TX Q", ENUM_UMAC_LMAC_PORT_2, 3},
	{"SEC RX Q", ENUM_UMAC_LMAC_PORT_2, 4},
	{"SFD_PARK Q", ENUM_UMAC_LMAC_PORT_2, 5},
	{"MDP_TXIOC Q", ENUM_UMAC_LMAC_PORT_2, 6},
	{"MDP_RXIOC Q", ENUM_UMAC_LMAC_PORT_2, 7},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0},
	{NULL, 0, 0}, /* 24~30 not defined */
	{"RLS Q", ENUM_PLE_CTRL_PSE_PORT_3, ENUM_UMAC_PLE_CTRL_P3_Q_0X1F} };

static u_int8_t *sta_ctrl_reg[] = {"ENABLE", "DISABLE", "PAUSE"};

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
void mt7915_show_ple_info(
	struct ADAPTER *prAdapter,
	u_int8_t fgDumpTxd)
{
	u_int32_t ple_buf_ctrl, pg_sz, pg_num;
	u_int32_t ple_stat[25] = {0}, pg_flow_ctrl[8] = {0};
	u_int32_t sta_pause[6] = {0}, dis_sta_map[6] = {0};
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail, hif_max_q, hif_min_q;
	u_int32_t rpg_hif, upg_hif, cpu_max_q, cpu_min_q, rpg_cpu, upg_cpu;
	u_int32_t i, j;
	u_int32_t ple_txcmd_stat;

	HAL_MCR_RD(prAdapter, WF_PLE_TOP_PBUF_CTRL_ADDR, &ple_buf_ctrl);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_QUEUE_EMPTY_ADDR, &ple_stat[0]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY0_ADDR, &ple_stat[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY1_ADDR, &ple_stat[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY2_ADDR, &ple_stat[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY3_ADDR, &ple_stat[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY4_ADDR, &ple_stat[5]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC0_QUEUE_EMPTY5_ADDR, &ple_stat[6]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY0_ADDR, &ple_stat[7]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY1_ADDR, &ple_stat[8]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY2_ADDR, &ple_stat[9]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY3_ADDR, &ple_stat[10]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY4_ADDR, &ple_stat[11]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC1_QUEUE_EMPTY5_ADDR, &ple_stat[12]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY0_ADDR, &ple_stat[13]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY1_ADDR, &ple_stat[14]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY2_ADDR, &ple_stat[15]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY3_ADDR, &ple_stat[16]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY4_ADDR, &ple_stat[17]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC2_QUEUE_EMPTY5_ADDR, &ple_stat[18]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY0_ADDR, &ple_stat[19]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY1_ADDR, &ple_stat[20]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY2_ADDR, &ple_stat[21]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY3_ADDR, &ple_stat[22]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY4_ADDR, &ple_stat[23]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_AC3_QUEUE_EMPTY5_ADDR, &ple_stat[24]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_TXCMD_QUEUE_EMPTY_ADDR,
		   &ple_txcmd_stat);
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
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP1_ADDR, &dis_sta_map[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP2_ADDR, &dis_sta_map[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP3_ADDR, &dis_sta_map[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP4_ADDR, &dis_sta_map[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_DIS_STA_MAP5_ADDR, &dis_sta_map[5]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE0_ADDR, &sta_pause[0]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE1_ADDR, &sta_pause[1]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE2_ADDR, &sta_pause[2]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE3_ADDR, &sta_pause[3]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE4_ADDR, &sta_pause[4]);
	HAL_MCR_RD(prAdapter, WF_PLE_TOP_STATION_PAUSE5_ADDR, &sta_pause[5]);
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
}

void mt7915_show_pse_info(
	struct ADAPTER *prAdapter)
{
	u_int32_t pse_buf_ctrl, pg_sz, pg_num;
	u_int32_t pse_stat, pg_flow_ctrl[20] = {0};
	u_int32_t fpg_cnt, ffa_cnt, fpg_head, fpg_tail;
	u_int32_t max_q, min_q, rsv_pg, used_pg;
	u_int32_t i;

	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PBUF_CTRL_ADDR, &pse_buf_ctrl);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_QUEUE_EMPTY_ADDR, &pse_stat);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_CNT_ADDR, &pg_flow_ctrl[0]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR,
		   &pg_flow_ctrl[1]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_HIF0_GROUP_ADDR, &pg_flow_ctrl[2]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_HIF0_PG_INFO_ADDR, &pg_flow_ctrl[3]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_HIF1_GROUP_ADDR, &pg_flow_ctrl[4]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_HIF1_PG_INFO_ADDR, &pg_flow_ctrl[5]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_CPU_GROUP_ADDR, &pg_flow_ctrl[6]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_CPU_PG_INFO_ADDR, &pg_flow_ctrl[7]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_LMAC0_GROUP_ADDR, &pg_flow_ctrl[8]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_LMAC0_PG_INFO_ADDR, &pg_flow_ctrl[9]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_LMAC1_GROUP_ADDR,
		   &pg_flow_ctrl[10]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_LMAC1_PG_INFO_ADDR, &pg_flow_ctrl[11]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_LMAC2_GROUP_ADDR,
		   &pg_flow_ctrl[12]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_LMAC2_PG_INFO_ADDR, &pg_flow_ctrl[13]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_PLE_GROUP_ADDR, &pg_flow_ctrl[14]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PLE_PG_INFO_ADDR, &pg_flow_ctrl[15]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_LMAC3_GROUP_ADDR,
		   &pg_flow_ctrl[16]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_LMAC3_PG_INFO_ADDR, &pg_flow_ctrl[17]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_PG_MDP_GROUP_ADDR, &pg_flow_ctrl[18]);
	HAL_MCR_RD(prAdapter, WF_PSE_TOP_MDP_PG_INFO_ADDR, &pg_flow_ctrl[19]);
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
		WF_PSE_TOP_FREEPG_CNT_ADDR,
		pg_flow_ctrl[0]);
	fpg_cnt = (pg_flow_ctrl[0] & WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FREEPG_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe toal page number of free=0x%03x\n", fpg_cnt);
	ffa_cnt = (pg_flow_ctrl[0] & WF_PSE_TOP_FREEPG_CNT_FFA_CNT_MASK) >>
		WF_PSE_TOP_FREEPG_CNT_FFA_CNT_SHFT;
	DBGLOG(HAL, INFO, "\t\tThe free page numbers of free for all=0x%03x\n",
		ffa_cnt);
	DBGLOG(HAL, INFO, "\tFree page head and tail(0x%08x): 0x%08x\n",
		WF_PSE_TOP_FREEPG_HEAD_TAIL_ADDR,
		pg_flow_ctrl[1]);
	fpg_head = (pg_flow_ctrl[1] &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_HEAD_SHFT;
	fpg_tail = (pg_flow_ctrl[1] &
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_MASK) >>
		WF_PSE_TOP_FREEPG_HEAD_TAIL_FREEPG_TAIL_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe tail/head page of free page list=0x%03x/0x%03x\n",
	       fpg_tail, fpg_head);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of HIF0 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_HIF0_GROUP_ADDR,
		pg_flow_ctrl[2]);
	DBGLOG(HAL, INFO, "\tHIF0 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_HIF0_PG_INFO_ADDR,
		pg_flow_ctrl[3]);
	min_q = (pg_flow_ctrl[2] &
		WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[2] &
		WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_HIF0_GROUP_HIF0_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of HIF0 group=0x%03x/0x%03x\n",
	       max_q, min_q);
	rsv_pg =
		(pg_flow_ctrl[3] & WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_RSV_CNT_SHFT;
	used_pg =
		(pg_flow_ctrl[3] & WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_MASK) >>
		WF_PSE_TOP_HIF0_PG_INFO_HIF0_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of HIF0 group=0x%03x/0x%03x\n",
	       used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of HIF1 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_HIF1_GROUP_ADDR,
		pg_flow_ctrl[4]);
	DBGLOG(HAL, INFO, "\tHIF1 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_HIF1_PG_INFO_ADDR,
		pg_flow_ctrl[5]);
	min_q = (pg_flow_ctrl[4] &
		WF_PSE_TOP_PG_HIF1_GROUP_HIF1_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_HIF1_GROUP_HIF1_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[4] &
		WF_PSE_TOP_PG_HIF1_GROUP_HIF1_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_HIF1_GROUP_HIF1_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of HIF1 group=0x%03x/0x%03x\n",
	       max_q, min_q);
	rsv_pg =
		(pg_flow_ctrl[5] & WF_PSE_TOP_HIF1_PG_INFO_HIF1_RSV_CNT_MASK) >>
		WF_PSE_TOP_HIF1_PG_INFO_HIF1_RSV_CNT_SHFT;
	used_pg =
		(pg_flow_ctrl[5] & WF_PSE_TOP_HIF1_PG_INFO_HIF1_SRC_CNT_MASK) >>
		WF_PSE_TOP_HIF1_PG_INFO_HIF1_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe used/reserved pages of HIF1 group=0x%03x/0x%03x\n",
		used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of CPU group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_CPU_GROUP_ADDR,
		pg_flow_ctrl[6]);
	DBGLOG(HAL, INFO, "\tCPU group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_CPU_PG_INFO_ADDR,
		pg_flow_ctrl[7]);
	min_q = (pg_flow_ctrl[6] &
		 WF_PSE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_CPU_GROUP_CPU_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[6] &
		 WF_PSE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_CPU_GROUP_CPU_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of CPU group=0x%03x/0x%03x\n",
	       max_q, min_q);
	rsv_pg = (pg_flow_ctrl[7] & WF_PSE_TOP_CPU_PG_INFO_CPU_RSV_CNT_MASK) >>
		 WF_PSE_TOP_CPU_PG_INFO_CPU_RSV_CNT_SHFT;
	used_pg = (pg_flow_ctrl[7] & WF_PSE_TOP_CPU_PG_INFO_CPU_SRC_CNT_MASK) >>
		  WF_PSE_TOP_CPU_PG_INFO_CPU_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of CPU group=0x%03x/0x%03x\n",
	       used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of LMAC0 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_LMAC0_GROUP_ADDR,
		pg_flow_ctrl[8]);
	DBGLOG(HAL, INFO, "\tLMAC0 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_LMAC0_PG_INFO_ADDR,
		pg_flow_ctrl[9]);
	min_q = (pg_flow_ctrl[8] &
		WF_PSE_TOP_PG_LMAC0_GROUP_LMAC0_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC0_GROUP_LMAC0_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[8] &
		WF_PSE_TOP_PG_LMAC0_GROUP_LMAC0_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC0_GROUP_LMAC0_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe max/min quota pages of LMAC0 group=0x%03x/0x%03x\n",
		max_q, min_q);
	rsv_pg = (pg_flow_ctrl[9] &
		WF_PSE_TOP_LMAC0_PG_INFO_LMAC0_RSV_CNT_MASK) >>
		WF_PSE_TOP_LMAC0_PG_INFO_LMAC0_RSV_CNT_SHFT;
	used_pg = (pg_flow_ctrl[9] &
		WF_PSE_TOP_LMAC0_PG_INFO_LMAC0_SRC_CNT_MASK) >>
		WF_PSE_TOP_LMAC0_PG_INFO_LMAC0_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe used/reserved pages of LMAC0 group=0x%03x/0x%03x\n",
		used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of LMAC1 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_LMAC1_GROUP_ADDR,
		pg_flow_ctrl[10]);
	DBGLOG(HAL, INFO, "\tLMAC1 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_LMAC1_PG_INFO_ADDR,
		pg_flow_ctrl[11]);
	min_q = (pg_flow_ctrl[10] &
		WF_PSE_TOP_PG_LMAC1_GROUP_LMAC1_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC1_GROUP_LMAC1_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[10] &
		WF_PSE_TOP_PG_LMAC1_GROUP_LMAC1_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC1_GROUP_LMAC1_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe max/min quota pages of LMAC1 group=0x%03x/0x%03x\n",
		max_q, min_q);
	rsv_pg = (pg_flow_ctrl[11] &
		WF_PSE_TOP_LMAC1_PG_INFO_LMAC1_RSV_CNT_MASK) >>
		WF_PSE_TOP_LMAC1_PG_INFO_LMAC1_RSV_CNT_SHFT;
	used_pg = (pg_flow_ctrl[11] &
		WF_PSE_TOP_LMAC1_PG_INFO_LMAC1_SRC_CNT_MASK) >>
		WF_PSE_TOP_LMAC1_PG_INFO_LMAC1_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe used/reserved pages of LMAC1 group=0x%03x/0x%03x\n",
		used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of LMAC2 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_LMAC2_GROUP_ADDR,
		pg_flow_ctrl[11]);
	DBGLOG(HAL, INFO, "\tLMAC2 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_LMAC2_PG_INFO_ADDR,
		pg_flow_ctrl[12]);
	min_q = (pg_flow_ctrl[12] &
		WF_PSE_TOP_PG_LMAC2_GROUP_LMAC2_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC2_GROUP_LMAC2_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[12] &
		WF_PSE_TOP_PG_LMAC2_GROUP_LMAC2_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC2_GROUP_LMAC2_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe max/min quota pages of LMAC2 group=0x%03x/0x%03x\n",
		max_q, min_q);
	rsv_pg = (pg_flow_ctrl[13] &
		WF_PSE_TOP_LMAC2_PG_INFO_LMAC2_RSV_CNT_MASK) >>
		WF_PSE_TOP_LMAC2_PG_INFO_LMAC2_RSV_CNT_SHFT;
	used_pg = (pg_flow_ctrl[13] &
		WF_PSE_TOP_LMAC2_PG_INFO_LMAC2_SRC_CNT_MASK) >>
		WF_PSE_TOP_LMAC2_PG_INFO_LMAC2_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe used/reserved pages of LMAC2 group=0x%03x/0x%03x\n",
		used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of LMAC3 group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_LMAC3_GROUP_ADDR,
		pg_flow_ctrl[16]);
	DBGLOG(HAL, INFO, "\tLMAC3 group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_LMAC3_PG_INFO_ADDR,
		pg_flow_ctrl[17]);
	min_q = (pg_flow_ctrl[16] &
		 WF_PSE_TOP_PG_LMAC3_GROUP_LMAC3_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC3_GROUP_LMAC3_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[16] &
		 WF_PSE_TOP_PG_LMAC3_GROUP_LMAC3_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_LMAC3_GROUP_LMAC3_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of LMAC3 group=0x%03x/0x%03x\n",
	       max_q, min_q);
	rsv_pg = (pg_flow_ctrl[17] &
		  WF_PSE_TOP_LMAC3_PG_INFO_LMAC3_RSV_CNT_MASK) >>
		 WF_PSE_TOP_LMAC3_PG_INFO_LMAC3_RSV_CNT_SHFT;
	used_pg = (pg_flow_ctrl[17] &
		   WF_PSE_TOP_LMAC3_PG_INFO_LMAC3_SRC_CNT_MASK) >>
		  WF_PSE_TOP_LMAC3_PG_INFO_LMAC3_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe used/reserved pages of LMAC3 group=0x%03x/0x%03x\n",
		used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of PLE group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_PLE_GROUP_ADDR,
		pg_flow_ctrl[14]);
	DBGLOG(HAL, INFO, "\tPLE group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PLE_PG_INFO_ADDR,
		pg_flow_ctrl[15]);
	min_q = (pg_flow_ctrl[14] &
		WF_PSE_TOP_PG_PLE_GROUP_PLE_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_PLE_GROUP_PLE_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[14] &
		WF_PSE_TOP_PG_PLE_GROUP_PLE_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_PLE_GROUP_PLE_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
		"\t\tThe max/min quota pages of PLE group=0x%03x/0x%03x\n",
		max_q, min_q);
	rsv_pg = (pg_flow_ctrl[15] & WF_PSE_TOP_PLE_PG_INFO_PLE_RSV_CNT_MASK) >>
		WF_PSE_TOP_PLE_PG_INFO_PLE_RSV_CNT_SHFT;
	used_pg =
		(pg_flow_ctrl[15] & WF_PSE_TOP_PLE_PG_INFO_PLE_SRC_CNT_MASK) >>
		WF_PSE_TOP_PLE_PG_INFO_PLE_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of PLE group=0x%03x/0x%03x\n",
	       used_pg, rsv_pg);
	DBGLOG(HAL, INFO,
		"\tReserved page counter of MDP group(0x%08x): 0x%08x\n",
		WF_PSE_TOP_PG_MDP_GROUP_ADDR,
		pg_flow_ctrl[18]);
	DBGLOG(HAL, INFO, "\tMDP group page status(0x%08x): 0x%08x\n",
		WF_PSE_TOP_MDP_PG_INFO_ADDR,
		pg_flow_ctrl[19]);
	min_q = (pg_flow_ctrl[18] &
		 WF_PSE_TOP_PG_MDP_GROUP_MDP_MIN_QUOTA_MASK) >>
		WF_PSE_TOP_PG_MDP_GROUP_MDP_MIN_QUOTA_SHFT;
	max_q = (pg_flow_ctrl[18] &
		 WF_PSE_TOP_PG_MDP_GROUP_MDP_MAX_QUOTA_MASK) >>
		WF_PSE_TOP_PG_MDP_GROUP_MDP_MAX_QUOTA_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe max/min quota pages of MDP group=0x%03x/0x%03x\n",
	       max_q, min_q);
	rsv_pg = (pg_flow_ctrl[19] & WF_PSE_TOP_MDP_PG_INFO_MDP_RSV_CNT_MASK) >>
		 WF_PSE_TOP_MDP_PG_INFO_MDP_RSV_CNT_SHFT;
	used_pg =
		(pg_flow_ctrl[19] & WF_PSE_TOP_MDP_PG_INFO_MDP_SRC_CNT_MASK) >>
		WF_PSE_TOP_MDP_PG_INFO_MDP_SRC_CNT_SHFT;
	DBGLOG(HAL, INFO,
	       "\t\tThe used/reserved pages of MDP group=0x%03x/0x%03x\n",
	       used_pg, rsv_pg);
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
	DBGLOG(HAL, INFO, "\t\tHIF Q0/1/2/3/4/5 empty=%d/%d/%d/%d/%d/%d\n",
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
		WF_PSE_TOP_QUEUE_EMPTY_HIF_5_EMPTY_SHFT));
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
#endif /* MT7915 */
