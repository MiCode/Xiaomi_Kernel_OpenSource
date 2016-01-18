/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <net/ip.h>
#include <linux/genalloc.h>	/* gen_pool_alloc() */
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/msm_gsi.h>
#include <linux/elf.h>
#include "ipa_i.h"

#define IPA_V3_0_CLK_RATE_SVS (75 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_NOMINAL (150 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_TURBO (200 * 1000 * 1000UL)
#define IPA_V3_0_MAX_HOLB_TMR_VAL (4294967296 - 1)

#define IPA_V3_0_BW_THRESHOLD_TURBO_MBPS (1200)
#define IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS (600)

/* Max pipes + ICs for TAG process */
#define IPA_TAG_MAX_DESC (IPA3_MAX_NUM_PIPES + 6)

#define IPA_TAG_SLEEP_MIN_USEC (1000)
#define IPA_TAG_SLEEP_MAX_USEC (2000)
#define IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT (10 * HZ)
#define IPA_BCR_REG_VAL (0x00000001)
#define IPA_AGGR_GRAN_MIN (1)
#define IPA_AGGR_GRAN_MAX (32)
#define IPA_EOT_COAL_GRAN_MIN (1)
#define IPA_EOT_COAL_GRAN_MAX (16)

#define IPA_AGGR_BYTE_LIMIT (\
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT)
#define IPA_AGGR_PKT_LIMIT (\
		IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_SHFT)

/* In IPAv3 only endpoints 0-3 can be configured to deaggregation */
#define IPA_EP_SUPPORTS_DEAGGR(idx) ((idx) >= 0 && (idx) <= 3)

/* configure IPA spare register 1 in order to have correct IPA version
 * set bits 0,2,3 and 4. see SpareBits documentation.xlsx
 */
#define IPA_SPARE_REG_1_VAL (0x0000081D)


/* HPS, DPS sequencers Types*/
#define IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY  0x00000000
/* DMA + DECIPHER/CIPHER */
#define IPA_DPS_HPS_SEQ_TYPE_DMA_DEC 0x00000011
/* Packet Processing + no decipher + uCP (for Ethernet Bridging) */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP 0x00000002
/* Packet Processing + decipher + uCP */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_DEC_UCP 0x00000013
/* 2 Packet Processing pass + no decipher + uCP */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP 0x00000004
/* 2 Packet Processing pass + decipher + uCP */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP 0x00000015
/* Packet Processing + no decipher + no uCP */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP 0x00000006
/* Packet Processing + no decipher + no uCP */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_DEC_NO_UCP 0x00000017
/* COMP/DECOMP */
#define IPA_DPS_HPS_SEQ_TYPE_DMA_COMP_DECOMP 0x00000020
/* Invalid sequencer type */
#define IPA_DPS_HPS_SEQ_TYPE_INVALID 0xFFFFFFFF

#define IPA_DPS_HPS_SEQ_TYPE_IS_DMA(seq_type) \
	(seq_type == IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY || \
	seq_type == IPA_DPS_HPS_SEQ_TYPE_DMA_DEC || \
	seq_type == IPA_DPS_HPS_SEQ_TYPE_DMA_COMP_DECOMP)

#define IPA_CLIENT_NOT_USED {-1, -1, false, IPA_DPS_HPS_SEQ_TYPE_INVALID}

/* Resource Group index*/
#define IPA_GROUP_UL		(0)
#define IPA_GROUP_DL		(1)
#define IPA_GROUP_DPL		IPA_GROUP_DL
#define IPA_GROUP_DIAG		(2)
#define IPA_GROUP_DMA		(3)
#define IPA_GROUP_IMM_CMD	IPA_GROUP_DMA
#define IPA_GROUP_Q6ZIP		(4)
#define IPA_GROUP_Q6ZIP_GENERAL	IPA_GROUP_Q6ZIP
#define IPA_GROUP_UC_RX_Q	(5)
#define IPA_GROUP_Q6ZIP_ENGINE	IPA_GROUP_UC_RX_Q
#define IPA_GROUP_MAX		(6)

enum ipa_rsrc_grp_type_src {
	IPA_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS,
	IPA_RSRC_GRP_TYPE_SRC_HDR_SECTORS,
	IPA_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER,
	IPA_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS,
	IPA_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS,
	IPA_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_RSRC_GRP_TYPE_SRC_MAX,
};
enum ipa_rsrc_grp_type_dst {
	IPA_RSRC_GRP_TYPE_DST_DATA_SECTORS,
	IPA_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS,
	IPA_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_RSRC_GRP_TYPE_DST_MAX,
};
enum ipa_rsrc_grp_type_rx {
	IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ,
	IPA_RSRC_GRP_TYPE_RX_MAX
};
struct rsrc_min_max {
	u32 min;
	u32 max;
};

static const struct rsrc_min_max ipa3_rsrc_src_grp_config
			[IPA_RSRC_GRP_TYPE_SRC_MAX][IPA_GROUP_MAX] = {
		/*UL	DL	DIAG	DMA	Not Used	uC Rx*/
	[IPA_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 255}, {3, 255}, {1, 255}, {1, 255}, {1, 255}, {2, 255} },
	[IPA_RSRC_GRP_TYPE_SRC_HDR_SECTORS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
	[IPA_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
	[IPA_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5},  {0, 0}, {8, 8} },
	[IPA_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{19, 19}, {26, 26}, {5, 5}, {5, 5}, {0, 0}, {8, 8} },
	[IPA_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
	[IPA_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
	[IPA_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5}, {0, 0}, {8, 8} },
};
static const struct rsrc_min_max ipa3_rsrc_dst_grp_config
			[IPA_RSRC_GRP_TYPE_DST_MAX][IPA_GROUP_MAX] = {
		/*UL	DL/DPL	DIAG	DMA  Q6zip_gen Q6zip_eng*/
	[IPA_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{2, 2}, {3, 3}, {0, 0}, {2, 2}, {3, 3}, {3, 3} },
	[IPA_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
	[IPA_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {0, 0} },
};
static const struct rsrc_min_max ipa3_rsrc_rx_grp_config
			[IPA_RSRC_GRP_TYPE_RX_MAX][IPA_GROUP_MAX] = {
		/*UL	DL	DIAG	DMA	Not Used	uC Rx*/
	[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{16, 16}, {24, 24}, {8, 8}, {8, 8}, {0, 0}, {8, 8} },
};

static const int ipa_ofst_meq32[] = { IPA_OFFSET_MEQ32_0,
					IPA_OFFSET_MEQ32_1, -1 };
static const int ipa_ofst_meq128[] = { IPA_OFFSET_MEQ128_0,
					IPA_OFFSET_MEQ128_1, -1 };
static const int ipa_ihl_ofst_rng16[] = { IPA_IHL_OFFSET_RANGE16_0,
					IPA_IHL_OFFSET_RANGE16_1, -1 };
static const int ipa_ihl_ofst_meq32[] = { IPA_IHL_OFFSET_MEQ32_0,
					IPA_IHL_OFFSET_MEQ32_1, -1 };
enum ipa_ver {
	IPA_3_0,
	IPA_VER_MAX,
};

struct ipa_ep_configuration {
	int pipe_num;
	int group_num;
	bool support_flt;
	int sequencer_type;
};

static const struct ipa_ep_configuration ipa3_ep_mapping
					[IPA_VER_MAX][IPA_CLIENT_MAX] = {
	[IPA_3_0][IPA_CLIENT_HSIC1_PROD]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_WLAN1_PROD]          = {10, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_HSIC2_PROD]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB2_PROD]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_HSIC3_PROD]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB3_PROD]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_HSIC4_PROD]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB4_PROD]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_HSIC5_PROD]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB_PROD]            = {1, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_UC_USB_PROD]         = {2, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_A5_WLAN_AMPDU_PROD]  = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_A2_EMBEDDED_PROD]    = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_A2_TETHERED_PROD]    = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_APPS_LAN_WAN_PROD]   = {14, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_APPS_CMD_PROD]
			= {22, IPA_GROUP_IMM_CMD, false,
				IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY},
	[IPA_3_0][IPA_CLIENT_ODU_PROD]            = {12, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_MHI_PROD]            = {0, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_Q6_LAN_PROD]         = {9, IPA_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_Q6_WAN_PROD]         = {5, IPA_GROUP_DL,
			true, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_Q6_CMD_PROD]
			= {6, IPA_GROUP_IMM_CMD, false,
				IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_PROD]      = {7, IPA_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_PROD]     = {8, IPA_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD]
			= {12, IPA_GROUP_DMA, false,
				IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY},
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD]
			= {13, IPA_GROUP_DMA, false,
				IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY},
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_PROD]           = {1, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_TEST1_PROD]          = {1, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_TEST2_PROD]          = {3, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_TEST3_PROD]          = {12, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_TEST4_PROD]          = {13, IPA_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP},

	[IPA_3_0][IPA_CLIENT_HSIC1_CONS]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_WLAN1_CONS]          = {25, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_HSIC2_CONS]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB2_CONS]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_WLAN2_CONS]          = {27, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_HSIC3_CONS]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB3_CONS]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_WLAN3_CONS]          = {28, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_HSIC4_CONS]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB4_CONS]           = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_WLAN4_CONS]          = {29, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP},
	[IPA_3_0][IPA_CLIENT_HSIC5_CONS]          = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_USB_CONS]            = {26, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_USB_DPL_CONS]        = {17, IPA_GROUP_DPL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_A2_EMBEDDED_CONS]    = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_A2_TETHERED_CONS]    = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_A5_LAN_WAN_CONS]     = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_APPS_LAN_CONS]       = {15, IPA_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_APPS_WAN_CONS]       = {16, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_ODU_EMB_CONS]        = {23, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_ODU_TETH_CONS]       = IPA_CLIENT_NOT_USED,
	[IPA_3_0][IPA_CLIENT_MHI_CONS]            = {23, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_LAN_CONS]         = {19, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_WAN_CONS]         = {18, IPA_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_DUN_CONS]         = {30, IPA_GROUP_DIAG,
			false, IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_CONS]
			= {21, IPA_GROUP_Q6ZIP, false,
					IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_CONS]
			= {4, IPA_GROUP_Q6ZIP, false,
					IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS]
			= {28, IPA_GROUP_DMA, false,
					IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS]
			= {29, IPA_GROUP_DMA, false,
					IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS]     = IPA_CLIENT_NOT_USED,
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_CONS]           = {16, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_TEST1_CONS]          = {16, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_TEST2_CONS]          = {27, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_TEST3_CONS]          = {28, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
	[IPA_3_0][IPA_CLIENT_TEST4_CONS]          = {29, IPA_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID},
};

/* this array include information tuple:
  {ipa_ep_num, ipa_gsi_chan_num, ipa_if_tlv, ipa_if_aos, ee} */
static struct ipa_gsi_ep_config ipa_gsi_ep_info[] = {
	{0, 0, 8, 16, 0},
	{1, 3, 8, 16, 0},
	{3, 5, 16, 32, 0},
	{4, 9, 4, 4, 1},
	{5, 0, 16, 32, 1},
	{6, 1, 18, 28, 1},
	{7, 2, 0, 0, 1},
	{8, 3, 0, 0, 1},
	{9, 4, 8, 12, 1},
	{10, 1, 8, 16, 3},
	{12, 9, 8, 16, 0},
	{13, 10, 8, 16, 0},
	{14, 11, 8, 16, 0},
	{15, 7, 8, 12, 0},
	{16, 8, 8, 12, 0},
	{17, 2, 8, 12, 0},
	{18, 5, 8, 12, 1},
	{19, 6, 8, 12, 1},
	{21, 8, 4, 4, 1},
	{22, 6, 18, 28, 0},
	{23, 1, 8, 8, 0},
	{25, 4, 8, 8, 3},
	{26, 12, 8, 8, 0},
	{27, 4, 8, 8, 0},
	{28, 13, 8, 8, 0},
	{29, 14, 8, 8, 0},
	{30, 7, 4, 4, 1},
	{-1, -1, -1, -1, -1}
};

static struct msm_bus_vectors ipa_init_vectors_v3_0[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors ipa_nominal_perf_vectors_v3_0[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 100000000,
		.ib = 1300000000,
	},
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 100000000,
		.ib = 1300000000,
	},
};

static struct msm_bus_paths ipa_usecases_v3_0[]  = {
	{
		ARRAY_SIZE(ipa_init_vectors_v3_0),
		ipa_init_vectors_v3_0,
	},
	{
		ARRAY_SIZE(ipa_nominal_perf_vectors_v3_0),
		ipa_nominal_perf_vectors_v3_0,
	},
};

static struct msm_bus_scale_pdata ipa_bus_client_pdata_v3_0 = {
	ipa_usecases_v3_0,
	ARRAY_SIZE(ipa_usecases_v3_0),
	.name = "ipa",
};

void ipa3_active_clients_lock(void)
{
	unsigned long flags;

	mutex_lock(&ipa3_ctx->ipa3_active_clients.mutex);
	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients.spinlock, flags);
	ipa3_ctx->ipa3_active_clients.mutex_locked = true;
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients.spinlock, flags);
}

int ipa3_active_clients_trylock(unsigned long *flags)
{
	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients.spinlock, *flags);
	if (ipa3_ctx->ipa3_active_clients.mutex_locked) {
		spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients.spinlock,
					 *flags);
		return 0;
	}

	return 1;
}

void ipa3_active_clients_trylock_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients.spinlock, *flags);
}

void ipa3_active_clients_unlock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients.spinlock, flags);
	ipa3_ctx->ipa3_active_clients.mutex_locked = false;
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients.spinlock, flags);
	mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
}

/**
 * ipa3_get_clients_from_rm_resource() - get IPA clients which are related to an
 * IPA_RM resource
 *
 * @resource: [IN] IPA Resource Manager resource
 * @clients: [OUT] Empty array which will contain the list of clients. The
 *         caller must initialize this array.
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_get_clients_from_rm_resource(
	enum ipa_rm_resource_name resource,
	struct ipa3_client_names *clients)
{
	int i = 0;

	if (resource < 0 ||
	    resource >= IPA_RM_RESOURCE_MAX ||
	    !clients) {
		IPAERR("Bad parameters\n");
		return -EINVAL;
	}

	switch (resource) {
	case IPA_RM_RESOURCE_USB_CONS:
		clients->names[i++] = IPA_CLIENT_USB_CONS;
		break;
	case IPA_RM_RESOURCE_USB_DPL_CONS:
		clients->names[i++] = IPA_CLIENT_USB_DPL_CONS;
		break;
	case IPA_RM_RESOURCE_HSIC_CONS:
		clients->names[i++] = IPA_CLIENT_HSIC1_CONS;
		break;
	case IPA_RM_RESOURCE_WLAN_CONS:
		clients->names[i++] = IPA_CLIENT_WLAN1_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN2_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN3_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN4_CONS;
		break;
	case IPA_RM_RESOURCE_MHI_CONS:
		clients->names[i++] = IPA_CLIENT_MHI_CONS;
		break;
	case IPA_RM_RESOURCE_USB_PROD:
		clients->names[i++] = IPA_CLIENT_USB_PROD;
		break;
	case IPA_RM_RESOURCE_HSIC_PROD:
		clients->names[i++] = IPA_CLIENT_HSIC1_PROD;
		break;
	case IPA_RM_RESOURCE_MHI_PROD:
		clients->names[i++] = IPA_CLIENT_MHI_PROD;
		break;
	default:
		break;
	}
	clients->length = i;

	return 0;
}

/**
 * ipa3_should_pipe_be_suspended() - returns true when the client's pipe should
 * be suspended during a power save scenario. False otherwise.
 *
 * @client: [IN] IPA client
 */
bool ipa3_should_pipe_be_suspended(enum ipa_client_type client)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;

	ipa_ep_idx = ipa3_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		WARN_ON(1);
		return false;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_USB_CONS     ||
	    client == IPA_CLIENT_USB_DPL_CONS ||
	    client == IPA_CLIENT_MHI_CONS     ||
	    client == IPA_CLIENT_HSIC1_CONS   ||
	    client == IPA_CLIENT_WLAN1_CONS   ||
	    client == IPA_CLIENT_WLAN2_CONS   ||
	    client == IPA_CLIENT_WLAN3_CONS   ||
	    client == IPA_CLIENT_WLAN4_CONS)
		return true;

	return false;
}

/**
 * ipa3_suspend_resource_sync() - suspend client endpoints related to the IPA_RM
 * resource and decrement active clients counter, which may result in clock
 * gating of IPA clocks.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_suspend_resource_sync(enum ipa_rm_resource_name resource)
{
	struct ipa3_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;
	bool pipe_suspended = false;

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("Bad params.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa3_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		ipa3_ctx->resume_on_connect[client] = false;
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa3_cfg_ep_ctrl(ipa_ep_idx, &suspend);
				pipe_suspended = true;
			}
		}
	}
	/* Sleep ~1 msec */
	if (pipe_suspended)
		usleep_range(1000, 2000);

	/* before gating IPA clocks do TAG process */
	ipa3_ctx->tag_process_before_gating = true;
	IPA_ACTIVE_CLIENTS_DEC_RESOURCE(ipa3_rm_resource_str(resource));

	return 0;
}

/**
 * ipa3_suspend_resource_no_block() - suspend client endpoints related to the
 * IPA_RM resource and decrement active clients counter. This function is
 * guaranteed to avoid sleeping.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_suspend_resource_no_block(enum ipa_rm_resource_name resource)
{
	int res;
	struct ipa3_client_names clients;
	int index;
	enum ipa_client_type client;
	struct ipa_ep_cfg_ctrl suspend;
	int ipa_ep_idx;
	unsigned long flags;
	struct ipa3_active_client_logging_info log_info;

	if (ipa3_active_clients_trylock(&flags) == 0)
		return -EPERM;
	if (ipa3_ctx->ipa3_active_clients.cnt == 1) {
		res = -EPERM;
		goto bail;
	}

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR(
			"ipa3_get_clients_from_rm_resource() failed, name = %d.\n",
			resource);
		goto bail;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa3_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		ipa3_ctx->resume_on_connect[client] = false;
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa3_cfg_ep_ctrl(ipa_ep_idx, &suspend);
			}
		}
	}

	if (res == 0) {
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info,
				ipa3_rm_resource_str(resource));
		ipa3_active_clients_log_dec(&log_info, true);
		ipa3_ctx->ipa3_active_clients.cnt--;
		IPADBG("active clients = %d\n",
		       ipa3_ctx->ipa3_active_clients.cnt);
	}
bail:
	ipa3_active_clients_trylock_unlock(&flags);

	return res;
}

/**
 * ipa3_resume_resource() - resume client endpoints related to the IPA_RM
 * resource.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_resume_resource(enum ipa_rm_resource_name resource)
{

	struct ipa3_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("ipa3_get_clients_from_rm_resource() failed.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa3_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		/*
		 * The related ep, will be resumed on connect
		 * while its resource is granted
		 */
		ipa3_ctx->resume_on_connect[client] = true;
		IPADBG("%d will be resumed on connect.\n", client);
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = false;
				ipa3_cfg_ep_ctrl(ipa_ep_idx, &suspend);
			}
		}
	}

	return res;
}

/**
 * _ipa_sram_settings_read_v3_0() - Read SRAM settings from HW
 *
 * Returns:	None
 */
void _ipa_sram_settings_read_v3_0(void)
{
	ipa3_ctx->smem_restricted_bytes = ipa_read_reg_field(ipa3_ctx->mmio,
		IPA_SHARED_MEM_SIZE_OFST_v3_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK_v3_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT_v3_0);
	ipa3_ctx->smem_sz = ipa_read_reg_field(ipa3_ctx->mmio,
		IPA_SHARED_MEM_SIZE_OFST_v3_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK_v3_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT_v3_0);
	/* reg fields are in 8B units */
	ipa3_ctx->smem_restricted_bytes *= 8;
	ipa3_ctx->smem_sz *= 8;
	ipa3_ctx->smem_reqd_sz = IPA_MEM_PART(end_ofst);
	ipa3_ctx->hdr_tbl_lcl = 0;
	ipa3_ctx->hdr_proc_ctx_tbl_lcl = 1;

	/*
	 * when proc ctx table is located in internal memory,
	 * modem entries resides first.
	 */
	if (ipa3_ctx->hdr_proc_ctx_tbl_lcl) {
		ipa3_ctx->hdr_proc_ctx_tbl.start_offset =
			IPA_MEM_PART(modem_hdr_proc_ctx_size);
	}
	ipa3_ctx->ip4_rt_tbl_hash_lcl = 0;
	ipa3_ctx->ip4_rt_tbl_nhash_lcl = 0;
	ipa3_ctx->ip6_rt_tbl_hash_lcl = 0;
	ipa3_ctx->ip6_rt_tbl_nhash_lcl = 0;
	ipa3_ctx->ip4_flt_tbl_hash_lcl = 0;
	ipa3_ctx->ip4_flt_tbl_nhash_lcl = 0;
	ipa3_ctx->ip6_flt_tbl_hash_lcl = 0;
	ipa3_ctx->ip6_flt_tbl_nhash_lcl = 0;
}

/**
 * _ipa_cfg_route_v3_0() - Configure the IPA default route register
 * @route: IPA route
 *
 * Returns:	None
 */
void _ipa_cfg_route_v3_0(struct ipa3_route *route)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, route->route_dis,
			IPA_ROUTE_ROUTE_DIS_SHFT,
			IPA_ROUTE_ROUTE_DIS_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_pipe,
			IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_table,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_ofst,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_frag_def_pipe,
			IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_retain_hdr,
			IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_SHFT,
			IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_BMSK);

	ipa_write_reg(ipa3_ctx->mmio, IPA_ROUTE_OFST_v3_0, reg_val);
}

/**
 * ipa3_cfg_route() - configure IPA route
 * @route: IPA route
 *
 * Return codes:
 * 0: success
 */
int ipa3_cfg_route(struct ipa3_route *route)
{

	IPADBG("disable_route_block=%d, default_pipe=%d, default_hdr_tbl=%d\n",
		route->route_dis,
		route->route_def_pipe,
		route->route_def_hdr_table);
	IPADBG("default_hdr_ofst=%d, default_frag_pipe=%d\n",
		route->route_def_hdr_ofst,
		route->route_frag_def_pipe);

	if (route->route_dis) {
		IPAERR("Route disable is not supported!\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipa3_ctx->ctrl->ipa3_cfg_route(route);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_cfg_filter() - configure filter
 * @disable: disable value
 *
 * Return codes:
 * 0: success
 */
int ipa3_cfg_filter(u32 disable)
{
	IPAERR("Filter disable is not supported!\n");
	return -EPERM;
}

/**
 * ipa3_init_hw() - initialize HW
 *
 * Return codes:
 * 0: success
 */
int ipa3_init_hw(void)
{
	u32 ipa_version = 0;

	/* Read IPA version and make sure we have access to the registers */
	ipa_version = ipa_read_reg(ipa3_ctx->mmio, IPA_VERSION_OFST);
	if (ipa_version == 0)
		return -EFAULT;

	/* using old BCR configuration(IPAv2.6)*/
	ipa_write_reg(ipa3_ctx->mmio, IPA_BCR_OFST, IPA_BCR_REG_VAL);

	return 0;
}

/**
 * ipa3_get_ep_mapping() - provide endpoint mapping
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa3_get_ep_mapping(enum ipa_client_type client)
{
	u8 hw_type_index;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_index = IPA_3_0;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_index = IPA_3_0;
		break;
	}

	return ipa3_ep_mapping[hw_type_index][client].pipe_num;
}

/**
 * ipa3_get_gsi_ep_info() - provide gsi ep information
 * @ipa_ep_idx: IPA endpoint index
 *
 * Return value: pointer to ipa_gsi_ep_info
 */
struct ipa_gsi_ep_config *ipa3_get_gsi_ep_info(int ipa_ep_idx)
{
	int i;

	for (i = 0; ; i++) {
		if (ipa_gsi_ep_info[i].ipa_ep_num < 0)
			break;

		if (ipa_gsi_ep_info[i].ipa_ep_num ==
			ipa_ep_idx)
			return &(ipa_gsi_ep_info[i]);
	}

	return NULL;
}

/**
 * ipa_get_ep_group() - provide endpoint group by client
 * @client: client type
 *
 * Return value: endpoint group
 */
int ipa_get_ep_group(enum ipa_client_type client)
{
	u8 hw_type_index;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_index = IPA_3_0;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_index = IPA_3_0;
		break;
	}

	return ipa3_ep_mapping[hw_type_index][client].group_num;
}

/* ipa3_set_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */

void ipa3_set_client(int index, enum ipacm_client_enum client, bool uplink)
{
	if (client >= IPACM_CLIENT_MAX || client < IPACM_CLIENT_USB) {
		IPAERR("Bad client number! client =%d\n", client);
	} else if (index >= IPA3_MAX_NUM_PIPES || index < 0) {
		IPAERR("Bad pipe index! index =%d\n", index);
	} else {
		ipa3_ctx->ipacm_client[index].client_enum = client;
		ipa3_ctx->ipacm_client[index].uplink = uplink;
	}
}

/**
 * ipa3_get_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */
enum ipacm_client_enum ipa3_get_client(int pipe_idx)
{
	if (pipe_idx >= IPA3_MAX_NUM_PIPES || pipe_idx < 0) {
		IPAERR("Bad pipe index! pipe_idx =%d\n", pipe_idx);
		return IPACM_CLIENT_MAX;
	} else {
		return ipa3_ctx->ipacm_client[pipe_idx].client_enum;
	}
}

/**
 * ipa2_get_client_uplink() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */
bool ipa3_get_client_uplink(int pipe_idx)
{
	return ipa3_ctx->ipacm_client[pipe_idx].uplink;
}

/**
 * ipa3_get_rm_resource_from_ep() - get the IPA_RM resource which is related to
 * the supplied pipe index.
 *
 * @pipe_idx:
 *
 * Return value: IPA_RM resource related to the pipe, -1 if a resource was not
 * found.
 */
enum ipa_rm_resource_name ipa3_get_rm_resource_from_ep(int pipe_idx)
{
	int i;
	int j;
	enum ipa_client_type client;
	struct ipa3_client_names clients;
	bool found = false;

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return -EINVAL;
	}

	client = ipa3_ctx->ep[pipe_idx].client;

	for (i = 0; i < IPA_RM_RESOURCE_MAX; i++) {
		memset(&clients, 0, sizeof(clients));
		ipa3_get_clients_from_rm_resource(i, &clients);
		for (j = 0; j < clients.length; j++) {
			if (clients.names[j] == client) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}

	if (!found)
		return -EFAULT;

	return i;
}

/**
 * ipa3_get_client_mapping() - provide client mapping
 * @pipe_idx: IPA end-point number
 *
 * Return value: client mapping
 */
enum ipa_client_type ipa3_get_client_mapping(int pipe_idx)
{
	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return -EINVAL;
	}

	return ipa3_ctx->ep[pipe_idx].client;
}

/**
 * ipa_init_ep_flt_bitmap() - Initialize the bitmap
 * that represents the End-points that supports filtering
 */
void ipa_init_ep_flt_bitmap(void)
{
	enum ipa_client_type cl;
	u8 hw_type_idx;
	u32 bitmap;

	bitmap = 0;

	BUG_ON(ipa3_ctx->ep_flt_bitmap);

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_idx = IPA_3_0;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_idx = IPA_3_0;
		break;
	}

	for (cl = 0; cl < IPA_CLIENT_MAX ; cl++) {
		if (ipa3_ep_mapping[hw_type_idx][cl].support_flt) {
			bitmap |=
				(1U<<ipa3_ep_mapping[hw_type_idx][cl].pipe_num);
			if (bitmap != ipa3_ctx->ep_flt_bitmap) {
				ipa3_ctx->ep_flt_bitmap = bitmap;
				ipa3_ctx->ep_flt_num++;
			}
		}
	}

}

/**
 * ipa_is_ep_support_flt() - Given an End-point check
 * whether it supports filtering or not.
 *
 * @pipe_idx:
 *
 * Return values:
 * true if supports and false if not
 */
bool ipa_is_ep_support_flt(int pipe_idx)
{
	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return false;
	}

	return ipa3_ctx->ep_flt_bitmap & (1U<<pipe_idx);
}

/**
 * ipa3_write_64() - convert 64 bit value to byte array
 * @w: 64 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa3_write_64(u64 w, u8 *dest)
{
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);
	*dest++ = (u8)((w >> 32) & 0xFF);
	*dest++ = (u8)((w >> 40) & 0xFF);
	*dest++ = (u8)((w >> 48) & 0xFF);
	*dest++ = (u8)((w >> 56) & 0xFF);

	return dest;
}

/**
 * ipa3_write_32() - convert 32 bit value to byte array
 * @w: 32 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa3_write_32(u32 w, u8 *dest)
{
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);

	return dest;
}

/**
 * ipa3_write_16() - convert 16 bit value to byte array
 * @hw: 16 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa3_write_16(u16 hw, u8 *dest)
{
	*dest++ = (u8)((hw) & 0xFF);
	*dest++ = (u8)((hw >> 8) & 0xFF);

	return dest;
}

/**
 * ipa3_write_8() - convert 8 bit value to byte array
 * @hw: 8 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa3_write_8(u8 b, u8 *dest)
{
	*dest++ = (b) & 0xFF;

	return dest;
}

/**
 * ipa3_pad_to_32() - pad byte array to 32 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa3_pad_to_32(u8 *dest)
{
	int i = (long)dest & 0x3;
	int j;

	if (i)
		for (j = 0; j < (4 - i); j++)
			*dest++ = 0;

	return dest;
}

/**
 * ipa3_pad_to_64() - pad byte array to 64 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa3_pad_to_64(u8 *dest)
{
	int i = (long)dest & 0x7;
	int j;

	if (i)
		for (j = 0; j < (8 - i); j++)
			*dest++ = 0;

	return dest;
}

void ipa3_generate_mac_addr_hw_rule(u8 **extra, u8 **rest,
	u8 hdr_mac_addr_offset,
	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN])
{
	int i;

	*extra = ipa3_write_8(hdr_mac_addr_offset, *extra);

	/* LSB MASK and ADDR */
	*rest = ipa3_write_64(0, *rest);
	*rest = ipa3_write_64(0, *rest);

	/* MSB MASK and ADDR */
	*rest = ipa3_write_16(0, *rest);
	for (i = 5; i >= 0; i--)
		*rest = ipa3_write_8(mac_addr_mask[i], *rest);
	*rest = ipa3_write_16(0, *rest);
	for (i = 5; i >= 0; i--)
		*rest = ipa3_write_8(mac_addr[i], *rest);
}

/**
 * ipa_rule_generation_err_check() - check basic validity on the rule
 *  attribs before starting building it
 *  checks if not not using ipv4 attribs on ipv6 and vice-versa
 * @ip: IP address type
 * @attrib: IPA rule attribute
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_rule_generation_err_check(
	enum ipa_ip_type ip, const struct ipa_rule_attrib *attrib)
{
	if (ip == IPA_IP_v4) {
		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR ||
		    attrib->attrib_mask & IPA_FLT_TC ||
		    attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
			IPAERR("v6 attrib's specified for v4 rule\n");
			return -EPERM;
		}
	} else if (ip == IPA_IP_v6) {
		if (attrib->attrib_mask & IPA_FLT_TOS ||
		    attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			IPAERR("v4 attrib's specified for v6 rule\n");
			return -EPERM;
		}
	} else {
		IPAERR("unsupported ip %d\n", ip);
		return -EPERM;
	}

	return 0;
}

static int ipa3_generate_hw_rule_ip4(u16 *en_rule,
	const struct ipa_rule_attrib *attrib,
	u8 **extra_wrds, u8 **rest_wrds)
{
	u8 *extra = *extra_wrds;
	u8 *rest = *rest_wrds;
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	int rc = 0;

	if (attrib->attrib_mask & IPA_FLT_TOS) {
		*en_rule |= IPA_TOS_EQ;
		extra = ipa3_write_8(attrib->u.v4.tos, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
		*en_rule |= IPA_PROTOCOL_EQ;
		extra = ipa3_write_8(attrib->u.v4.protocol, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-14,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-8,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-22,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-16,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		/* 0 => offset of TOS in v4 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_32((attrib->tos_mask << 16), rest);
		rest = ipa3_write_32((attrib->tos_value << 16), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		/* 12 => offset of src ip in v4 header */
		extra = ipa3_write_8(12, extra);
		rest = ipa3_write_32(attrib->u.v4.src_addr_mask, rest);
		rest = ipa3_write_32(attrib->u.v4.src_addr, rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		/* 16 => offset of dst ip in v4 header */
		extra = ipa3_write_8(16, extra);
		rest = ipa3_write_32(attrib->u.v4.dst_addr_mask, rest);
		rest = ipa3_write_32(attrib->u.v4.dst_addr, rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		/* -2 => offset of ether type in L2 hdr */
		extra = ipa3_write_8((u8)-2, extra);
		rest = ipa3_write_16(0, rest);
		rest = ipa3_write_16(htons(attrib->ether_type), rest);
		rest = ipa3_write_16(0, rest);
		rest = ipa3_write_16(htons(attrib->ether_type), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 0  => offset of type after v4 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_32(0xFF, rest);
		rest = ipa3_write_32(attrib->type, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 1  => offset of code after v4 header */
		extra = ipa3_write_8(1, extra);
		rest = ipa3_write_32(0xFF, rest);
		rest = ipa3_write_32(attrib->code, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 0  => offset of SPI after v4 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_32(0xFFFFFFFF, rest);
		rest = ipa3_write_32(attrib->spi, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_METADATA_COMPARE;
		rest = ipa3_write_32(attrib->meta_data_mask, rest);
		rest = ipa3_write_32(attrib->meta_data, rest);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAERR("bad src port range param\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 0  => offset of src port after v4 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_16(attrib->src_port_hi, rest);
		rest = ipa3_write_16(attrib->src_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAERR("bad dst port range param\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 2  => offset of dst port after v4 header */
		extra = ipa3_write_8(2, extra);
		rest = ipa3_write_16(attrib->dst_port_hi, rest);
		rest = ipa3_write_16(attrib->dst_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 0  => offset of src port after v4 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_16(attrib->src_port, rest);
		rest = ipa3_write_16(attrib->src_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 2  => offset of dst port after v4 header */
		extra = ipa3_write_8(2, extra);
		rest = ipa3_write_16(attrib->dst_port, rest);
		rest = ipa3_write_16(attrib->dst_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT)
		*en_rule |= IPA_IS_FRAG;

	goto done;

err:
	rc = -EPERM;
done:
	*extra_wrds = extra;
	*rest_wrds = rest;
	return rc;
}

static int ipa3_generate_hw_rule_ip6(u16 *en_rule,
	const struct ipa_rule_attrib *attrib,
	u8 **extra_wrds, u8 **rest_wrds)
{
	u8 *extra = *extra_wrds;
	u8 *rest = *rest_wrds;
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	int rc = 0;

	/* v6 code below assumes no extension headers TODO: fix this */

	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
		*en_rule |= IPA_PROTOCOL_EQ;
		extra = ipa3_write_8(attrib->u.v6.next_hdr, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_TC) {
		*en_rule |= IPA_TC_EQ;
		extra = ipa3_write_8(attrib->u.v6.tc, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		/* 8 => offset of src ip in v6 header */
		extra = ipa3_write_8(8, extra);
		rest = ipa3_write_32(attrib->u.v6.src_addr_mask[3], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr_mask[2], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr[3], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr[2], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr_mask[1], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr_mask[0], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr[1], rest);
		rest = ipa3_write_32(attrib->u.v6.src_addr[0], rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		/* 24 => offset of dst ip in v6 header */
		extra = ipa3_write_8(24, extra);
		rest = ipa3_write_32(attrib->u.v6.dst_addr_mask[3], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr_mask[2], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr[3], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr[2], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr_mask[1], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr_mask[0], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr[1], rest);
		rest = ipa3_write_32(attrib->u.v6.dst_addr[0], rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		/* 0 => offset of TOS in v6 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_64(0, rest);
		rest = ipa3_write_64(0, rest);
		rest = ipa3_write_32(0, rest);
		rest = ipa3_write_32((attrib->tos_mask << 20), rest);
		rest = ipa3_write_32(0, rest);
		rest = ipa3_write_32((attrib->tos_value << 20), rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-14,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-8,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-22,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa3_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-16,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		/* -2 => offset of ether type in L2 hdr */
		extra = ipa3_write_8((u8)-2, extra);
		rest = ipa3_write_16(0, rest);
		rest = ipa3_write_16(htons(attrib->ether_type), rest);
		rest = ipa3_write_16(0, rest);
		rest = ipa3_write_16(htons(attrib->ether_type), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 0  => offset of type after v6 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_32(0xFF, rest);
		rest = ipa3_write_32(attrib->type, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 1  => offset of code after v6 header */
		extra = ipa3_write_8(1, extra);
		rest = ipa3_write_32(0xFF, rest);
		rest = ipa3_write_32(attrib->code, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		/* 0  => offset of SPI after v6 header FIXME */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_32(0xFFFFFFFF, rest);
		rest = ipa3_write_32(attrib->spi, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_METADATA_COMPARE;
		rest = ipa3_write_32(attrib->meta_data_mask, rest);
		rest = ipa3_write_32(attrib->meta_data, rest);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 0  => offset of src port after v6 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_16(attrib->src_port, rest);
		rest = ipa3_write_16(attrib->src_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 2  => offset of dst port after v6 header */
		extra = ipa3_write_8(2, extra);
		rest = ipa3_write_16(attrib->dst_port, rest);
		rest = ipa3_write_16(attrib->dst_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAERR("bad src port range param\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 0  => offset of src port after v6 header */
		extra = ipa3_write_8(0, extra);
		rest = ipa3_write_16(attrib->src_port_hi, rest);
		rest = ipa3_write_16(attrib->src_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAERR("bad dst port range param\n");
			goto err;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		/* 2  => offset of dst port after v6 header */
		extra = ipa3_write_8(2, extra);
		rest = ipa3_write_16(attrib->dst_port_hi, rest);
		rest = ipa3_write_16(attrib->dst_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
		*en_rule |= IPA_FL_EQ;
		rest = ipa3_write_32(attrib->u.v6.flow_label & 0xFFFFF,
			rest);
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT)
		*en_rule |= IPA_IS_FRAG;

	goto done;

err:
	rc = -EPERM;
done:
	*extra_wrds = extra;
	*rest_wrds = rest;
	return rc;
}

static u8 *ipa3_copy_mem(u8 *src, u8 *dst, int cnt)
{
	while (cnt--)
		*dst++ = *src++;

	return dst;
}

/**
 * ipa3_generate_hw_rule() - generate HW rule
 * @ip: IP address type
 * @attrib: IPA rule attribute
 * @buf: output buffer
 * @en_rule: enable rule
 *
 * Return codes:
 * 0: success
 * -EPERM: wrong input
 */
int ipa3_generate_hw_rule(enum ipa_ip_type ip,
	const struct ipa_rule_attrib *attrib, u8 **buf, u16 *en_rule)
{
	int sz;
	int rc = 0;
	u8 *extra_wrd_buf;
	u8 *rest_wrd_buf;
	u8 *extra_wrd_start;
	u8 *rest_wrd_start;
	u8 *extra_wrd_i;
	u8 *rest_wrd_i;

	sz = IPA_HW_TBL_WIDTH * 2 + IPA_HW_RULE_START_ALIGNMENT;
	extra_wrd_buf = kzalloc(sz, GFP_KERNEL);
	if (!extra_wrd_buf) {
		IPAERR("failed to allocate %d bytes\n", sz);
		rc = -ENOMEM;
		goto fail_extra_alloc;
	}

	sz = IPA_RT_FLT_HW_RULE_BUF_SIZE + IPA_HW_RULE_START_ALIGNMENT;
	rest_wrd_buf = kzalloc(sz, GFP_KERNEL);
	if (!rest_wrd_buf) {
		IPAERR("failed to allocate %d bytes\n", sz);
		rc = -ENOMEM;
		goto fail_rest_alloc;
	}

	extra_wrd_start = extra_wrd_buf + IPA_HW_RULE_START_ALIGNMENT;
	extra_wrd_start = (u8 *)((long)extra_wrd_start &
		~IPA_HW_RULE_START_ALIGNMENT);

	rest_wrd_start = rest_wrd_buf + IPA_HW_RULE_START_ALIGNMENT;
	rest_wrd_start = (u8 *)((long)rest_wrd_start &
		~IPA_HW_RULE_START_ALIGNMENT);

	extra_wrd_i = extra_wrd_start;
	rest_wrd_i = rest_wrd_start;

	rc = ipa_rule_generation_err_check(ip, attrib);
	if (rc) {
		IPAERR("ipa_rule_generation_err_check() failed\n");
		goto fail_err_check;
	}

	if (ip == IPA_IP_v4) {
		if (ipa3_generate_hw_rule_ip4(en_rule, attrib,
			&extra_wrd_i, &rest_wrd_i)) {
			IPAERR("failed to build ipv4 hw rule\n");
			rc = -EPERM;
			goto fail_err_check;
		}

	} else if (ip == IPA_IP_v6) {
		if (ipa3_generate_hw_rule_ip6(en_rule, attrib,
			&extra_wrd_i, &rest_wrd_i)) {

			IPAERR("failed to build ipv6 hw rule\n");
			rc = -EPERM;
			goto fail_err_check;
		}
	} else {
		IPAERR("unsupported ip %d\n", ip);
		goto fail_err_check;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		IPADBG("building default rule\n");
		*en_rule |= ipa_ofst_meq32[0];
		extra_wrd_i = ipa3_write_8(0, extra_wrd_i);  /* offset */
		rest_wrd_i = ipa3_write_32(0, rest_wrd_i);   /* mask */
		rest_wrd_i = ipa3_write_32(0, rest_wrd_i);   /* val */
	}

	IPADBG("extra_word_1 0x%llx\n", *(u64 *)extra_wrd_start);
	IPADBG("extra_word_2 0x%llx\n",
		*(u64 *)(extra_wrd_start + IPA_HW_TBL_WIDTH));

	extra_wrd_i = ipa3_pad_to_64(extra_wrd_i);
	sz = extra_wrd_i - extra_wrd_start;
	IPADBG("extra words params sz %d\n", sz);
	*buf = ipa3_copy_mem(extra_wrd_start, *buf, sz);

	rest_wrd_i = ipa3_pad_to_64(rest_wrd_i);
	sz = rest_wrd_i - rest_wrd_start;
	IPADBG("non extra words params sz %d\n", sz);
	*buf = ipa3_copy_mem(rest_wrd_start, *buf, sz);

fail_err_check:
	kfree(rest_wrd_buf);
fail_rest_alloc:
	kfree(extra_wrd_buf);
fail_extra_alloc:
	return rc;
}

void ipa3_generate_flt_mac_addr_eq(struct ipa_ipfltri_rule_eq *eq_atrb,
	u8 hdr_mac_addr_offset,	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN], u8 ofst_meq128)
{
	int i;

	eq_atrb->offset_meq_128[ofst_meq128].offset = hdr_mac_addr_offset;

	/* LSB MASK and ADDR */
	memset(eq_atrb->offset_meq_128[ofst_meq128].mask, 0, 8);
	memset(eq_atrb->offset_meq_128[ofst_meq128].value, 0, 8);

	/* MSB MASK and ADDR */
	memset(eq_atrb->offset_meq_128[ofst_meq128].mask + 8, 0, 2);
	for (i = 0; i <= 5; i++)
		eq_atrb->offset_meq_128[ofst_meq128].mask[15 - i] =
			mac_addr_mask[i];

	memset(eq_atrb->offset_meq_128[ofst_meq128].value + 8, 0, 2);
	for (i = 0; i <= 0; i++)
		eq_atrb->offset_meq_128[ofst_meq128].value[15 - i] =
			mac_addr[i];
}

int ipa3_generate_flt_eq_ip4(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	u16 eq_bitmap = 0;
	u16 *en_rule = &eq_bitmap;

	if (attrib->attrib_mask & IPA_FLT_TOS) {
		*en_rule |= IPA_TOS_EQ;
		eq_atrb->tos_eq_present = 1;
		eq_atrb->tos_eq = attrib->u.v4.tos;
	}

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
		*en_rule |= IPA_PROTOCOL_EQ;
		eq_atrb->protocol_eq_present = 1;
		eq_atrb->protocol_eq = attrib->u.v4.protocol;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -14,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -8,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -22,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -16,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = 0;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->tos_mask << 16;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->tos_value << 16;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = 12;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->u.v4.src_addr_mask;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->u.v4.src_addr;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = 16;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->u.v4.dst_addr_mask;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->u.v4.dst_addr;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			htons(attrib->ether_type);
		eq_atrb->offset_meq_32[ofst_meq32].value =
			htons(attrib->ether_type);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->type;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->code;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
			0xFFFFFFFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->spi;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_METADATA_COMPARE;
		eq_atrb->metadata_meq32_present = 1;
		eq_atrb->metadata_meq32.offset = 0;
		eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
		eq_atrb->metadata_meq32.value = attrib->meta_data;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAERR("bad src port range param\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAERR("bad dst port range param\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
		*en_rule |= IPA_IS_FRAG;
		eq_atrb->ipv4_frag_eq_present = 1;
	}

	eq_atrb->rule_eq_bitmap = *en_rule;
	eq_atrb->num_offset_meq_32 = ofst_meq32;
	eq_atrb->num_ihl_offset_range_16 = ihl_ofst_rng16;
	eq_atrb->num_ihl_offset_meq_32 = ihl_ofst_meq32;
	eq_atrb->num_offset_meq_128 = ofst_meq128;

	return 0;
}

int ipa3_generate_flt_eq_ip6(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	u16 eq_bitmap = 0;
	u16 *en_rule = &eq_bitmap;

	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
		*en_rule |= IPA_PROTOCOL_EQ;
		eq_atrb->protocol_eq_present = 1;
		eq_atrb->protocol_eq = attrib->u.v6.next_hdr;
	}

	if (attrib->attrib_mask & IPA_FLT_TC) {
		*en_rule |= IPA_FLT_TC;
		eq_atrb->tc_eq_present = 1;
		eq_atrb->tc_eq = attrib->u.v6.tc;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		eq_atrb->offset_meq_128[ofst_meq128].offset = 8;
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
			= attrib->u.v6.src_addr_mask[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
			= attrib->u.v6.src_addr_mask[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
			= attrib->u.v6.src_addr_mask[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->u.v6.src_addr_mask[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
			= attrib->u.v6.src_addr[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
			= attrib->u.v6.src_addr[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
			= attrib->u.v6.src_addr[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->u.v6.src_addr[0];
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		eq_atrb->offset_meq_128[ofst_meq128].offset = 24;
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
			= attrib->u.v6.dst_addr_mask[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
			= attrib->u.v6.dst_addr_mask[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
			= attrib->u.v6.dst_addr_mask[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->u.v6.dst_addr_mask[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
			= attrib->u.v6.dst_addr[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
			= attrib->u.v6.dst_addr[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
			= attrib->u.v6.dst_addr[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->u.v6.dst_addr[0];
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];
		eq_atrb->offset_meq_128[ofst_meq128].offset = 0;
		memset(eq_atrb->offset_meq_128[ofst_meq128].mask, 0, 12);
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->tos_mask << 20;
		memset(eq_atrb->offset_meq_128[ofst_meq128].value, 0, 12);
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->tos_value << 20;
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -14,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -8,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -22,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (ipa_ofst_meq128[ofst_meq128] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq128[ofst_meq128];

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa3_generate_flt_mac_addr_eq(eq_atrb, -16,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			htons(attrib->ether_type);
		eq_atrb->offset_meq_32[ofst_meq32].value =
			htons(attrib->ether_type);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->type;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->code;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
			IPAERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
			0xFFFFFFFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->spi;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_METADATA_COMPARE;
		eq_atrb->metadata_meq32_present = 1;
		eq_atrb->metadata_meq32.offset = 0;
		eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
		eq_atrb->metadata_meq32.value = attrib->meta_data;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAERR("bad src port range param\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
			IPAERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAERR("bad dst port range param\n");
			return -EPERM;
		}
		*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
		*en_rule |= IPA_FLT_FLOW_LABEL;
		eq_atrb->fl_eq_present = 1;
		eq_atrb->fl_eq = attrib->u.v6.flow_label;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
		*en_rule |= IPA_IS_FRAG;
		eq_atrb->ipv4_frag_eq_present = 1;
	}

	eq_atrb->rule_eq_bitmap = *en_rule;
	eq_atrb->num_offset_meq_32 = ofst_meq32;
	eq_atrb->num_ihl_offset_range_16 = ihl_ofst_rng16;
	eq_atrb->num_ihl_offset_meq_32 = ihl_ofst_meq32;
	eq_atrb->num_offset_meq_128 = ofst_meq128;

	return 0;
}

int ipa3_generate_flt_eq(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	if (ipa_rule_generation_err_check(ip, attrib))
		return -EPERM;

	if (ip == IPA_IP_v4) {
		if (ipa3_generate_flt_eq_ip4(ip, attrib, eq_atrb)) {
			IPAERR("failed to build ipv4 flt eq rule\n");
			return -EPERM;
		}

	} else if (ip == IPA_IP_v6) {
		if (ipa3_generate_flt_eq_ip6(ip, attrib, eq_atrb)) {
			IPAERR("failed to build ipv6 flt eq rule\n");
			return -EPERM;
		}
	} else {
		IPAERR("unsupported ip %d\n", ip);
		return  -EPERM;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		eq_atrb->rule_eq_bitmap = 0;
		eq_atrb->rule_eq_bitmap |= ipa_ofst_meq32[0];
		eq_atrb->offset_meq_32[0].offset = 0;
		eq_atrb->offset_meq_32[0].mask = 0;
		eq_atrb->offset_meq_32[0].value = 0;
	}

	return 0;
}

/**
 * ipa3_cfg_ep_seq() - IPA end-point HPS/DPS sequencer type configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_seq(u32 clnt_hdl)
{
	int type;
	u8 hw_type_index;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad param, clnt_hdl = %d", clnt_hdl);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("SEQ does not apply to IPA consumer EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	/*
	 * Skip Configure sequencers type for test clients.
	 * These are configured dynamically in ipa3_cfg_ep_mode
	 */
	if (IPA_CLIENT_IS_TEST(ipa3_ctx->ep[clnt_hdl].client)) {
		IPADBG("Skip sequencers configuration for test clients\n");
		return 0;
	}

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_index = IPA_3_0;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_index = IPA_3_0;
		break;
	}

	type = ipa3_ep_mapping[hw_type_index][ipa3_ctx->ep[clnt_hdl].client]
						.sequencer_type;
	if (type != IPA_DPS_HPS_SEQ_TYPE_INVALID) {
		if (ipa3_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA &&
			!IPA_DPS_HPS_SEQ_TYPE_IS_DMA(type)) {
			IPAERR("Configuring non-DMA SEQ type to DMA pipe\n");
			BUG();
		}
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
		/* Configure sequencers type*/

		IPADBG("set sequencers to sequence 0x%x, ep = %d\n", type,
				clnt_hdl);
		ipa_write_reg(ipa3_ctx->mmio,
				IPA_ENDP_INIT_SEQ_n_OFST(clnt_hdl), type);

		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	} else {
		IPADBG("should not set sequencer type of ep = %d\n", clnt_hdl);
	}
	return 0;
}

/**
 * ipa3_cfg_ep - IPA end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * This includes nat, header, mode, aggregation and route settings and is a one
 * shot API to configure the IPA end-point fully
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg)
{
	int result = -EINVAL;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	result = ipa3_cfg_ep_hdr(clnt_hdl, &ipa_ep_cfg->hdr);
	if (result)
		return result;

	result = ipa3_cfg_ep_hdr_ext(clnt_hdl, &ipa_ep_cfg->hdr_ext);
	if (result)
		return result;

	result = ipa3_cfg_ep_aggr(clnt_hdl, &ipa_ep_cfg->aggr);
	if (result)
		return result;

	result = ipa3_cfg_ep_cfg(clnt_hdl, &ipa_ep_cfg->cfg);
	if (result)
		return result;

	if (IPA_CLIENT_IS_PROD(ipa3_ctx->ep[clnt_hdl].client)) {
		result = ipa3_cfg_ep_nat(clnt_hdl, &ipa_ep_cfg->nat);
		if (result)
			return result;

		result = ipa3_cfg_ep_mode(clnt_hdl, &ipa_ep_cfg->mode);
		if (result)
			return result;

		result = ipa3_cfg_ep_seq(clnt_hdl);
		if (result)
			return result;

		result = ipa3_cfg_ep_route(clnt_hdl, &ipa_ep_cfg->route);
		if (result)
			return result;

		result = ipa3_cfg_ep_deaggr(clnt_hdl, &ipa_ep_cfg->deaggr);
		if (result)
			return result;
	} else {
		result = ipa3_cfg_ep_metadata_mask(clnt_hdl,
				&ipa_ep_cfg->metadata_mask);
		if (result)
			return result;
	}

	return 0;
}

const char *ipa3_get_nat_en_str(enum ipa_nat_en_type nat_en)
{
	switch (nat_en) {
	case (IPA_BYPASS_NAT):
		return "NAT disabled";
	case (IPA_SRC_NAT):
		return "Source NAT";
	case (IPA_DST_NAT):
		return "Dst NAT";
	}

	return "undefined";
}

void _ipa_cfg_ep_nat_v3_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ep_nat)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_nat->nat_en,
			IPA_ENDP_INIT_NAT_N_NAT_EN_SHFT,
			IPA_ENDP_INIT_NAT_N_NAT_EN_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_NAT_N_OFST_v3_0(clnt_hdl),
			reg_val);
}

/**
 * ipa3_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ep_nat)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_nat == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("NAT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d, nat_en=%d(%s)\n",
			clnt_hdl,
			ep_nat->nat_en,
			ipa3_get_nat_en_str(ep_nat->nat_en));

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.nat = *ep_nat;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_nat(clnt_hdl, ep_nat);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

static void _ipa_cfg_ep_status_v3_0(u32 clnt_hdl,
		const struct ipa3_ep_cfg_status *ep_status)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_status->status_en,
			IPA_ENDP_STATUS_n_STATUS_EN_SHFT,
			IPA_ENDP_STATUS_n_STATUS_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_status->status_ep,
			IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT,
			IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_status->status_location,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_SHFT,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_STATUS_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa3_cfg_ep_status() - IPA end-point status configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_status(u32 clnt_hdl, const struct ipa3_ep_cfg_status *ep_status)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_status == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, status_en=%d status_ep=%d status_location=%d\n",
			clnt_hdl,
			ep_status->status_en,
			ep_status->status_ep,
			ep_status->status_location);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].status = *ep_status;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_status(clnt_hdl, ep_status);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

static void _ipa_cfg_ep_cfg_v3_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_cfg *cfg)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, cfg->frag_offload_en,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(reg_val, cfg->cs_offload_en,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(reg_val, cfg->cs_metadata_hdr_offset,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_BMSK);

	ipa_write_reg(ipa3_ctx->mmio, IPA_ENDP_INIT_CFG_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa3_cfg_ep_cfg() - IPA end-point cfg configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *cfg)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || cfg == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, frag_ofld_en=%d cs_ofld_en=%d mdata_hdr_ofst=%d\n",
			clnt_hdl,
			cfg->frag_offload_en,
			cfg->cs_offload_en,
			cfg->cs_metadata_hdr_offset);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.cfg = *cfg;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_cfg(clnt_hdl, cfg);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

static void _ipa_cfg_ep_metadata_mask_v3_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *metadata_mask)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, metadata_mask->metadata_mask,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa3_cfg_ep_metadata_mask() - IPA end-point meta-data mask configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_metadata_mask(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask
		*metadata_mask)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || metadata_mask == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, metadata_mask=0x%x\n",
			clnt_hdl,
			metadata_mask->metadata_mask);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.metadata_mask = *metadata_mask;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_metadata_mask(clnt_hdl, metadata_mask);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa_cfg_ep_hdr_v3_0() -  IPA end-point header configuration
 * @pipe_number:[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
void _ipa_cfg_ep_hdr_v3_0(u32 pipe_number,
		const struct ipa_ep_cfg_hdr *ep_hdr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_metadata_reg_valid,
			IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_SHFT_v2,
			IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_BMSK_v2);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_remove_additional,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_SHFT_v2,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_BMSK_v2);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_a5_mux,
			IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_pkt_size,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_pkt_size_valid,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_additional_const_len,
			IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_metadata,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_metadata_valid,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_len,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HDR_N_OFST_v3_0(pipe_number), reg_val);
}

/**
 * ipa3_cfg_ep_hdr() -  IPA end-point header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ep_hdr)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_hdr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}
	IPADBG("pipe=%d remove_additional=%d, a5_mux=%d, ofst_pkt_size=0x%x\n",
		clnt_hdl,
		ep_hdr->hdr_remove_additional,
		ep_hdr->hdr_a5_mux,
		ep_hdr->hdr_ofst_pkt_size);

	IPADBG("ofst_pkt_size_valid=%d, additional_const_len=0x%x\n",
		ep_hdr->hdr_ofst_pkt_size_valid,
		ep_hdr->hdr_additional_const_len);

	IPADBG("ofst_metadata=0x%x, ofst_metadata_valid=%d, len=0x%x",
		ep_hdr->hdr_ofst_metadata,
		ep_hdr->hdr_ofst_metadata_valid,
		ep_hdr->hdr_len);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr = *ep_hdr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_hdr(clnt_hdl, &ep->cfg.hdr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

static int _ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext, u32 reg_val)
{
	u8 hdr_endianness = ep_hdr_ext->hdr_little_endian ? 0 : 1;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad_offset,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_payload_len_inc_padding,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad_valid,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, hdr_endianness,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
		IPA_ENDP_INIT_HDR_EXT_n_OFST_v3_0(clnt_hdl), reg_val);

	return 0;
}

static int _ipa_cfg_ep_hdr_ext_v3_0(u32 clnt_hdl,
				const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_pad_to_alignment,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_BMSK_v3_0);

	return _ipa_cfg_ep_hdr_ext(clnt_hdl, ep_hdr_ext, reg_val);
}

/**
 * ipa3_cfg_ep_hdr_ext() -  IPA end-point extended header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_hdr_ext:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_hdr_ext(u32 clnt_hdl,
		       const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_hdr_ext == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d hdr_pad_to_alignment=%d\n",
		clnt_hdl,
		ep_hdr_ext->hdr_pad_to_alignment);

	IPADBG("hdr_total_len_or_pad_offset=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_offset);

	IPADBG("hdr_payload_len_inc_padding=%d hdr_total_len_or_pad=%d\n",
		ep_hdr_ext->hdr_payload_len_inc_padding,
		ep_hdr_ext->hdr_total_len_or_pad);

	IPADBG("hdr_total_len_or_pad_valid=%d hdr_little_endian=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_valid,
		ep_hdr_ext->hdr_little_endian);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr_ext = *ep_hdr_ext;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_hdr_ext(clnt_hdl, &ep->cfg.hdr_ext);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_hdr() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	u32 reg_val = 0;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes || ep_ctrl == NULL) {
		IPAERR("bad parm, clnt_hdl = %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d ep_suspend=%d, ep_delay=%d\n",
		clnt_hdl,
		ep_ctrl->ipa_ep_suspend,
		ep_ctrl->ipa_ep_delay);

	IPA_SETFIELD_IN_REG(reg_val, ep_ctrl->ipa_ep_suspend,
		IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_SHFT,
		IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_ctrl->ipa_ep_delay,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
		IPA_ENDP_INIT_CTRL_N_OFST(clnt_hdl), reg_val);

	if (ep_ctrl->ipa_ep_suspend == true &&
			IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client))
		ipa3_suspend_active_aggr_wa(clnt_hdl);

	return 0;
}

/**
 * ipa3_cfg_aggr_cntr_granularity() - granularity of the AGGR timer configuration
 * @aggr_granularity:     [in] defines the granularity of AGGR timers
 *			  number of units of 1/32msec
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_aggr_cntr_granularity(u8 aggr_granularity)
{
	u32 reg_val = 0;

	if (aggr_granularity <= IPA_AGGR_GRAN_MIN ||
			aggr_granularity > IPA_AGGR_GRAN_MAX) {
		IPAERR("bad param, aggr_granularity = %d\n",
				aggr_granularity);
		return -EINVAL;
	}
	IPADBG("aggr_granularity=%d\n", aggr_granularity);

	reg_val = ipa_read_reg(ipa3_ctx->mmio, IPA_COUNTER_CFG_OFST);
	reg_val = (reg_val & ~IPA_COUNTER_CFG_AGGR_GRAN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, aggr_granularity - 1,
			IPA_COUNTER_CFG_AGGR_GRAN_SHFT,
			IPA_COUNTER_CFG_AGGR_GRAN_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_COUNTER_CFG_OFST, reg_val);

	return 0;

}

/**
 * ipa3_cfg_eot_coal_cntr_granularity() - granularity of EOT_COAL timer
 *					 configuration
 * @eot_coal_granularity: defines the granularity of EOT_COAL timers
 *			  number of units of 1/32msec
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_eot_coal_cntr_granularity(u8 eot_coal_granularity)
{
	u32 reg_val = 0;

	if (eot_coal_granularity <= IPA_EOT_COAL_GRAN_MIN ||
			eot_coal_granularity > IPA_EOT_COAL_GRAN_MAX) {
		IPAERR("bad parm, eot_coal_granularity = %d\n",
				eot_coal_granularity);
		return -EINVAL;
	}
	IPADBG("eot_coal_granularity=%d\n", eot_coal_granularity);

	reg_val = ipa_read_reg(ipa3_ctx->mmio, IPA_COUNTER_CFG_OFST);
	reg_val = (reg_val & ~IPA_COUNTER_CFG_EOT_COAL_GRAN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, eot_coal_granularity - 1,
			IPA_COUNTER_CFG_EOT_COAL_GRAN_SHFT,
			IPA_COUNTER_CFG_EOT_COAL_GRAN_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_COUNTER_CFG_OFST, reg_val);

	return 0;

}

const char *ipa3_get_mode_type_str(enum ipa_mode_type mode)
{
	switch (mode) {
	case (IPA_BASIC):
		return "Basic";
	case (IPA_ENABLE_FRAMING_HDLC):
		return "HDLC framing";
	case (IPA_ENABLE_DEFRAMING_HDLC):
		return "HDLC de-framing";
	case (IPA_DMA):
		return "DMA";
	}

	return "undefined";
}

/**
 * _ipa_cfg_ep_mode_v3_0() - IPA end-point mode configuration
 * @pipe_number:[in] pipe number to set the mode on
 * @dst_pipe_number:[in] destination pipe, valid only when the mode is DMA
 * @ep_mode:	[in] IPA end-point configuration params
 *
 * Returns: None
 */
void _ipa_cfg_ep_mode_v3_0(u32 pipe_number, u32 dst_pipe_number,
		const struct ipa_ep_cfg_mode *ep_mode)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_mode->mode,
			IPA_ENDP_INIT_MODE_N_MODE_SHFT,
			IPA_ENDP_INIT_MODE_N_MODE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, dst_pipe_number,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_SHFT_v3_0,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_BMSK_v3_0);

	ipa_write_reg(ipa3_ctx->mmio,
		IPA_ENDP_INIT_MODE_N_OFST_v3_0(pipe_number), reg_val);
}

/**
 * ipa3_cfg_ep_mode() - IPA end-point mode configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ep_mode)
{
	int ep;
	int type;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_mode == NULL) {
		IPAERR("bad params clnt_hdl=%d , ep_valid=%d ep_mode=%p\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid,
				ep_mode);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("MODE does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	ep = ipa3_get_ep_mapping(ep_mode->dst);
	if (ep == -1 && ep_mode->mode == IPA_DMA) {
		IPAERR("dst %d does not exist in DMA mode\n", ep_mode->dst);
		return -EINVAL;
	}

	WARN_ON(ep_mode->mode == IPA_DMA && IPA_CLIENT_IS_PROD(ep_mode->dst));

	if (!IPA_CLIENT_IS_CONS(ep_mode->dst))
		ep = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);

	IPADBG("pipe=%d mode=%d(%s), dst_client_number=%d",
			clnt_hdl,
			ep_mode->mode,
			ipa3_get_mode_type_str(ep_mode->mode),
			ep_mode->dst);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.mode = *ep_mode;
	ipa3_ctx->ep[clnt_hdl].dst_pipe_index = ep;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_mode(clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].dst_pipe_index,
			ep_mode);

	 /* Configure sequencers type for test clients*/
	if (IPA_CLIENT_IS_TEST(ipa3_ctx->ep[clnt_hdl].client)) {
		if (ep_mode->mode == IPA_DMA)
			type = IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY;
		else
			type = IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP;

		IPADBG(" set sequencers to sequance 0x%x, ep = %d\n", type,
				clnt_hdl);
		ipa_write_reg(ipa3_ctx->mmio,
				IPA_ENDP_INIT_SEQ_n_OFST(clnt_hdl), type);
	}
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

const char *ipa3_get_aggr_enable_str(enum ipa_aggr_en_type aggr_en)
{
	switch (aggr_en) {
	case (IPA_BYPASS_AGGR):
			return "no aggregation";
	case (IPA_ENABLE_AGGR):
			return "aggregation enabled";
	case (IPA_ENABLE_DEAGGR):
		return "de-aggregation enabled";
	}

	return "undefined";
}

const char *ipa3_get_aggr_type_str(enum ipa_aggr_type aggr_type)
{
	switch (aggr_type) {
	case (IPA_MBIM_16):
			return "MBIM_16";
	case (IPA_HDLC):
		return "HDLC";
	case (IPA_TLP):
			return "TLP";
	case (IPA_RNDIS):
			return "RNDIS";
	case (IPA_GENERIC):
			return "GENERIC";
	case (IPA_QCMAP):
			return "QCMAP";
	}
	return "undefined";
}

/**
 * _ipa_cfg_ep_aggr_v3_0() - IPA end-point aggregation configuration
 * @pipe_number:[in] pipe number
 * @ep_aggr:[in] IPA end-point configuration params
 */
void _ipa_cfg_ep_aggr_v3_0(u32 pipe_number,
		const struct ipa_ep_cfg_aggr *ep_aggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_en,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_byte_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_time_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_pkt_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_PKT_LIMIT_BMSK);

	/* set byte-limit aggregation behavior to soft-byte limit */
	IPA_SETFIELD_IN_REG(reg_val, 0,
			IPA_ENDP_INIT_AGGR_N_AGGR_HARD_BYTE_LIMIT_ENABLE_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_HARD_BYTE_LIMIT_ENABLE_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v3_0(pipe_number), reg_val);
}

/**
 * ipa3_cfg_ep_aggr() - IPA end-point aggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ep_aggr)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_aggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (ep_aggr->aggr_en == IPA_ENABLE_DEAGGR &&
	    !IPA_EP_SUPPORTS_DEAGGR(clnt_hdl)) {
		IPAERR("pipe=%d cannot be configured to DEAGGR\n", clnt_hdl);
		WARN_ON(1);
		return -EINVAL;
	}

	IPADBG("pipe=%d en=%d(%s), type=%d(%s), byte_limit=%d, time_limit=%d\n",
			clnt_hdl,
			ep_aggr->aggr_en,
			ipa3_get_aggr_enable_str(ep_aggr->aggr_en),
			ep_aggr->aggr,
			ipa3_get_aggr_type_str(ep_aggr->aggr),
			ep_aggr->aggr_byte_limit,
			ep_aggr->aggr_time_limit);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.aggr = *ep_aggr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_aggr(clnt_hdl, ep_aggr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * _ipa_cfg_ep_route_v3_0() - IPA end-point routing configuration
 * @pipe_index:[in] pipe index
 * @rt_tbl_index:[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
void _ipa_cfg_ep_route_v3_0(u32 pipe_index, u32 rt_tbl_index)
{
	int reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, rt_tbl_index,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_SHFT,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_N_OFST_v3_0(pipe_index),
			reg_val);
}

/**
 * ipa3_cfg_ep_route() - IPA end-point routing configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ep_route)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_route == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("ROUTE does not apply to IPA out EP %d\n",
				clnt_hdl);
		return -EINVAL;
	}

	/*
	 * if DMA mode was configured previously for this EP, return with
	 * success
	 */
	if (ipa3_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA) {
		IPADBG("DMA enabled for ep %d, dst pipe is part of DMA\n",
				clnt_hdl);
		return 0;
	}

	if (ep_route->rt_tbl_hdl)
		IPAERR("client specified non-zero RT TBL hdl - ignore it\n");

	IPADBG("pipe=%d, rt_tbl_hdl=%d\n",
			clnt_hdl,
			ep_route->rt_tbl_hdl);

	/* always use "default" routing table when programming EP ROUTE reg */
	ipa3_ctx->ep[clnt_hdl].rt_tbl_idx =
		IPA_MEM_PART(v4_apps_rt_index_lo);

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_route(clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].rt_tbl_idx);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * _ipa_cfg_ep_holb_v3_0() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @pipe_number	[in] pipe number
 * @ep_holb:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
void _ipa_cfg_ep_holb_v3_0(u32 pipe_number,
			const struct ipa_ep_cfg_holb *ep_holb)
{
	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v3_0(pipe_number),
			ep_holb->en);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v3_0(pipe_number),
			(u16)ep_holb->tmr_val);
}

/**
 * ipa3_cfg_ep_holb() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ep_holb)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_holb == NULL ||
	    ep_holb->tmr_val > ipa3_ctx->ctrl->max_holb_tmr_val ||
	    ep_holb->en > 1) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_PROD(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("HOLB does not apply to IPA in EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (!ipa3_ctx->ctrl->ipa3_cfg_ep_holb) {
		IPAERR("HOLB is not supported for this IPA core\n");
		return -EINVAL;
	}

	ipa3_ctx->ep[clnt_hdl].holb = *ep_holb;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_holb(clnt_hdl, ep_holb);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("cfg holb %u ep=%d tmr=%d\n", ep_holb->en, clnt_hdl,
				ep_holb->tmr_val);

	return 0;
}

/**
 * ipa3_cfg_ep_holb_by_client() - IPA end-point holb configuration
 *
 * Wrapper function for ipa3_cfg_ep_holb() with client name instead of
 * client handle. This function is used for clients that does not have
 * client handle.
 *
 * @client:	[in] client name
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ep_holb)
{
	return ipa3_cfg_ep_holb(ipa3_get_ep_mapping(client), ep_holb);
}

static int _ipa_cfg_ep_deaggr_v3_0(u32 clnt_hdl,
				   const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->deaggr_hdr_len,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->packet_offset_valid,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->packet_offset_location,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->max_packet_len,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_BMSK);

	ipa_write_reg(ipa3_ctx->mmio,
		IPA_ENDP_INIT_DEAGGR_n_OFST_v3_0(clnt_hdl), reg_val);

	return 0;
}

/**
 * ipa3_cfg_ep_deaggr() -  IPA end-point deaggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_deaggr:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_deaggr(u32 clnt_hdl,
			const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_deaggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d deaggr_hdr_len=%d\n",
		clnt_hdl,
		ep_deaggr->deaggr_hdr_len);

	IPADBG("packet_offset_valid=%d\n",
		ep_deaggr->packet_offset_valid);

	IPADBG("packet_offset_location=%d max_packet_len=%d\n",
		ep_deaggr->packet_offset_location,
		ep_deaggr->max_packet_len);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.deaggr = *ep_deaggr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_deaggr(clnt_hdl, &ep->cfg.deaggr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

static void _ipa_cfg_ep_metadata_v3_0(u32 pipe_number,
					const struct ipa_ep_cfg_metadata *meta)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, meta->qmap_id,
			IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK);

	ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HDR_METADATA_n_OFST(pipe_number),
			reg_val);
}

/**
 * ipa3_cfg_ep_metadata() - IPA end-point metadata configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_metadata(u32 clnt_hdl, const struct ipa_ep_cfg_metadata *ep_md)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_md == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, mux id=%d\n", clnt_hdl, ep_md->qmap_id);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.meta = *ep_md;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_ctx->ctrl->ipa3_cfg_ep_metadata(clnt_hdl, ep_md);
	ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_metadata_reg_valid = 1;
	ipa3_ctx->ctrl->ipa3_cfg_ep_hdr(clnt_hdl,
			&ipa3_ctx->ep[clnt_hdl].cfg.hdr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

int ipa3_write_qmap_id(struct ipa_ioc_write_qmapid *param_in)
{
	struct ipa_ep_cfg_metadata meta;
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (param_in->client  >= IPA_CLIENT_MAX) {
		IPAERR("bad parm client:%d\n", param_in->client);
		goto fail;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(param_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (!ep->valid) {
		IPAERR("EP not allocated.\n");
		goto fail;
	}

	meta.qmap_id = param_in->qmap_id;
	if (param_in->client == IPA_CLIENT_USB_PROD ||
	    param_in->client == IPA_CLIENT_HSIC1_PROD ||
	    param_in->client == IPA_CLIENT_ODU_PROD) {
		result = ipa3_cfg_ep_metadata(ipa_ep_idx, &meta);
	} else if (param_in->client == IPA_CLIENT_WLAN1_PROD) {
		ipa3_ctx->ep[ipa_ep_idx].cfg.meta = meta;
		result = ipa3_write_qmapid_wdi_pipe(ipa_ep_idx, meta.qmap_id);
		if (result)
			IPAERR("qmap_id %d write failed on ep=%d\n",
					meta.qmap_id, ipa_ep_idx);
		result = 0;
	}

fail:
	return result;
}

/**
 * ipa3_dump_buff_internal() - dumps buffer for debug purposes
 * @base: buffer base address
 * @phy_base: buffer physical base address
 * @size: size of the buffer
 */
void ipa3_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size)
{
	int i;
	u32 *cur = (u32 *)base;
	u8 *byt;

	IPADBG("system phys addr=%pa len=%u\n", &phy_base, size);
	for (i = 0; i < size / 4; i++) {
		byt = (u8 *)(cur + i);
		IPADBG("%2d %08x   %02x %02x %02x %02x\n", i, *(cur + i),
				byt[0], byt[1], byt[2], byt[3]);
	}
	IPADBG("END\n");
}

/**
 * ipa3_pipe_mem_init() - initialize the pipe memory
 * @start_ofst: start offset
 * @size: size
 *
 * Return value:
 * 0: success
 * -ENOMEM: no memory
 */
int ipa3_pipe_mem_init(u32 start_ofst, u32 size)
{
	int res;
	u32 aligned_start_ofst;
	u32 aligned_size;
	struct gen_pool *pool;

	if (!size) {
		IPAERR("no IPA pipe memory allocated\n");
		goto fail;
	}

	aligned_start_ofst = IPA_HW_TABLE_ALIGNMENT(start_ofst);
	aligned_size = size - (aligned_start_ofst - start_ofst);

	IPADBG("start_ofst=%u aligned_start_ofst=%u size=%u aligned_size=%u\n",
	       start_ofst, aligned_start_ofst, size, aligned_size);

	/* allocation order of 8 i.e. 128 bytes, global pool */
	pool = gen_pool_create(8, -1);
	if (!pool) {
		IPAERR("Failed to create a new memory pool.\n");
		goto fail;
	}

	res = gen_pool_add(pool, aligned_start_ofst, aligned_size, -1);
	if (res) {
		IPAERR("Failed to add memory to IPA pipe pool\n");
		goto err_pool_add;
	}

	ipa3_ctx->pipe_mem_pool = pool;
	return 0;

err_pool_add:
	gen_pool_destroy(pool);
fail:
	return -ENOMEM;
}

/**
 * ipa3_pipe_mem_alloc() - allocate pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa3_pipe_mem_alloc(u32 *ofst, u32 size)
{
	u32 vaddr;
	int res = -1;

	if (!ipa3_ctx->pipe_mem_pool || !size) {
		IPAERR("failed size=%u pipe_mem_pool=%p\n", size,
				ipa3_ctx->pipe_mem_pool);
		return res;
	}

	vaddr = gen_pool_alloc(ipa3_ctx->pipe_mem_pool, size);

	if (vaddr) {
		*ofst = vaddr;
		res = 0;
		IPADBG("size=%u ofst=%u\n", size, vaddr);
	} else {
		IPAERR("size=%u failed\n", size);
	}

	return res;
}

/**
 * ipa3_pipe_mem_free() - free pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa3_pipe_mem_free(u32 ofst, u32 size)
{
	IPADBG("size=%u ofst=%u\n", size, ofst);
	if (ipa3_ctx->pipe_mem_pool && size)
		gen_pool_free(ipa3_ctx->pipe_mem_pool, ofst, size);
	return 0;
}

/**
 * ipa3_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa3_set_aggr_mode(enum ipa_aggr_mode mode)
{
	u32 reg_val;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	reg_val = ipa_read_reg(ipa3_ctx->mmio, IPA_QCNCM_OFST);
	ipa_write_reg(ipa3_ctx->mmio, IPA_QCNCM_OFST, (mode & 0x1) |
			(reg_val & 0xfffffffe));
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_set_qcncm_ndp_sig(char sig[3])
{
	u32 reg_val;

	if (sig == NULL) {
		IPAERR("bad argument for ipa3_set_qcncm_ndp_sig/n");
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	reg_val = ipa_read_reg(ipa3_ctx->mmio, IPA_QCNCM_OFST);
	ipa_write_reg(ipa3_ctx->mmio, IPA_QCNCM_OFST, sig[0] << 20 |
			(sig[1] << 12) | (sig[2] << 4) |
			(reg_val & 0xf000000f));
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa3_set_single_ndp_per_mbim(bool enable)
{
	u32 reg_val;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	reg_val = ipa_read_reg(ipa3_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST);
	ipa_write_reg(ipa3_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST,
			(enable & 0x1) | (reg_val & 0xfffffffe));
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_straddle_boundary() - Checks whether a memory buffer straddles a boundary
 * @start: start address of the memory buffer
 * @end: end address of the memory buffer
 * @boundary: boundary
 *
 * Return value:
 * 1: if the interval [start, end] straddles boundary
 * 0: otherwise
 */
int ipa3_straddle_boundary(u32 start, u32 end, u32 boundary)
{
	u32 next_start;
	u32 prev_end;

	IPADBG("start=%u end=%u boundary=%u\n", start, end, boundary);

	next_start = (start + (boundary - 1)) & ~(boundary - 1);
	prev_end = ((end + (boundary - 1)) & ~(boundary - 1)) - boundary;

	while (next_start < prev_end)
		next_start += boundary;

	if (next_start == prev_end)
		return 1;
	else
		return 0;
}

/**
 * ipa3_bam_reg_dump() - Dump selected BAM registers for IPA.
 * The API is right now used only to dump IPA registers towards USB.
 *
 * Function is rate limited to avoid flooding kernel log buffer
 */
void ipa3_bam_reg_dump(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 500*HZ, 1);

	if (__ratelimit(&_rs)) {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		pr_err("IPA BAM START\n");
		sps_get_bam_debug_info(ipa3_ctx->bam_handle, 93,
			(SPS_BAM_PIPE(ipa3_get_ep_mapping(IPA_CLIENT_USB_CONS))
			|
			SPS_BAM_PIPE(ipa3_get_ep_mapping(IPA_CLIENT_USB_PROD))),
			0, 2);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	}
}

static void ipa_init_mem_partition_v3_0(void)
{
	IPADBG("Memory partition IPA 3.0\n");
	IPA_MEM_PART(nat_ofst) = IPA_RAM_NAT_OFST;
	IPA_MEM_PART(nat_size) = IPA_RAM_NAT_SIZE;
	IPADBG("NAT OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(nat_ofst),
		IPA_MEM_PART(nat_size));

	IPA_MEM_PART(uc_info_ofst) = IPA_MEM_v3_0_RAM_UC_INFO_OFST;
	IPA_MEM_PART(uc_info_size) = IPA_MEM_v3_0_RAM_UC_INFO_SIZE;
	IPADBG("UC INFO OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(uc_info_ofst),
		IPA_MEM_PART(uc_info_size));

	IPA_MEM_PART(ofst_start) = IPA_MEM_v3_0_RAM_OFST_START;
	IPADBG("RAM OFST 0x%x\n", IPA_MEM_PART(ofst_start));

	IPA_MEM_PART(v4_flt_hash_ofst) = IPA_MEM_v3_0_RAM_V4_FLT_HASH_OFST;
	IPA_MEM_PART(v4_flt_hash_size) = IPA_MEM_v3_0_RAM_V4_FLT_HASH_SIZE;
	IPA_MEM_PART(v4_flt_hash_size_ddr) = IPA_MEM_RAM_V4_FLT_HASH_SIZE_DDR;
	IPADBG("V4 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_hash_ofst),
		IPA_MEM_PART(v4_flt_hash_size),
		IPA_MEM_PART(v4_flt_hash_size_ddr));

	IPA_MEM_PART(v4_flt_nhash_ofst) = IPA_MEM_v3_0_RAM_V4_FLT_NHASH_OFST;
	IPA_MEM_PART(v4_flt_nhash_size) = IPA_MEM_v3_0_RAM_V4_FLT_NHASH_SIZE;
	IPA_MEM_PART(v4_flt_nhash_size_ddr) = IPA_MEM_RAM_V4_FLT_NHASH_SIZE_DDR;
	IPADBG("V4 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_nhash_ofst),
		IPA_MEM_PART(v4_flt_nhash_size),
		IPA_MEM_PART(v4_flt_nhash_size_ddr));

	IPA_MEM_PART(v6_flt_hash_ofst) = IPA_MEM_v3_0_RAM_V6_FLT_HASH_OFST;
	IPA_MEM_PART(v6_flt_hash_size) = IPA_MEM_v3_0_RAM_V6_FLT_HASH_SIZE;
	IPA_MEM_PART(v6_flt_hash_size_ddr) = IPA_MEM_RAM_V6_FLT_HASH_SIZE_DDR;
	IPADBG("V6 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_hash_ofst), IPA_MEM_PART(v6_flt_hash_size),
		IPA_MEM_PART(v6_flt_hash_size_ddr));

	IPA_MEM_PART(v6_flt_nhash_ofst) = IPA_MEM_v3_0_RAM_V6_FLT_NHASH_OFST;
	IPA_MEM_PART(v6_flt_nhash_size) = IPA_MEM_v3_0_RAM_V6_FLT_NHASH_SIZE;
	IPA_MEM_PART(v6_flt_nhash_size_ddr) = IPA_MEM_RAM_V6_FLT_NHASH_SIZE_DDR;
	IPADBG("V6 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_nhash_ofst),
		IPA_MEM_PART(v6_flt_nhash_size),
		IPA_MEM_PART(v6_flt_nhash_size_ddr));

	IPA_MEM_PART(v4_rt_num_index) = IPA_MEM_v3_0_RAM_V4_RT_NUM_INDEX;
	IPADBG("V4 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v4_rt_num_index));

	IPA_MEM_PART(v4_modem_rt_index_lo) =
		IPA_MEM_v3_0_V4_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v4_modem_rt_index_hi) =
		IPA_MEM_v3_0_V4_MODEM_RT_INDEX_HI;
	IPADBG("V4 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_modem_rt_index_lo),
		IPA_MEM_PART(v4_modem_rt_index_hi));

	IPA_MEM_PART(v4_apps_rt_index_lo) =
		IPA_MEM_v3_0_V4_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v4_apps_rt_index_hi) =
		IPA_MEM_v3_0_V4_APPS_RT_INDEX_HI;
	IPADBG("V4 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_apps_rt_index_lo),
		IPA_MEM_PART(v4_apps_rt_index_hi));

	IPA_MEM_PART(v4_rt_hash_ofst) = IPA_MEM_v3_0_RAM_V4_RT_HASH_OFST;
	IPADBG("V4 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v4_rt_hash_ofst));

	IPA_MEM_PART(v4_rt_hash_size) = IPA_MEM_v3_0_RAM_V4_RT_HASH_SIZE;
	IPA_MEM_PART(v4_rt_hash_size_ddr) = IPA_MEM_RAM_V4_RT_HASH_SIZE_DDR;
	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_hash_size),
		IPA_MEM_PART(v4_rt_hash_size_ddr));

	IPA_MEM_PART(v4_rt_nhash_ofst) = IPA_MEM_v3_0_RAM_V4_RT_NHASH_OFST;
	IPADBG("V4 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_ofst));

	IPA_MEM_PART(v4_rt_nhash_size) = IPA_MEM_v3_0_RAM_V4_RT_NHASH_SIZE;
	IPA_MEM_PART(v4_rt_nhash_size_ddr) = IPA_MEM_RAM_V4_RT_NHASH_SIZE_DDR;
	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_size),
		IPA_MEM_PART(v4_rt_nhash_size_ddr));

	IPA_MEM_PART(v6_rt_num_index) = IPA_MEM_v3_0_RAM_V6_RT_NUM_INDEX;
	IPADBG("V6 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v6_rt_num_index));

	IPA_MEM_PART(v6_modem_rt_index_lo) =
		IPA_MEM_v3_0_V6_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v6_modem_rt_index_hi) =
		IPA_MEM_v3_0_V6_MODEM_RT_INDEX_HI;
	IPADBG("V6 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_modem_rt_index_lo),
		IPA_MEM_PART(v6_modem_rt_index_hi));

	IPA_MEM_PART(v6_apps_rt_index_lo) =
		IPA_MEM_v3_0_V6_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v6_apps_rt_index_hi) =
		IPA_MEM_v3_0_V6_APPS_RT_INDEX_HI;
	IPADBG("V6 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_apps_rt_index_lo),
		IPA_MEM_PART(v6_apps_rt_index_hi));

	IPA_MEM_PART(v6_rt_hash_ofst) = IPA_MEM_v3_0_RAM_V6_RT_HASH_OFST;
	IPADBG("V6 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v6_rt_hash_ofst));

	IPA_MEM_PART(v6_rt_hash_size) = IPA_MEM_v3_0_RAM_V6_RT_HASH_SIZE;
	IPA_MEM_PART(v6_rt_hash_size_ddr) = IPA_MEM_RAM_V6_RT_HASH_SIZE_DDR;
	IPADBG("V6 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_hash_size),
		IPA_MEM_PART(v6_rt_hash_size_ddr));

	IPA_MEM_PART(v6_rt_nhash_ofst) = IPA_MEM_v3_0_RAM_V6_RT_NHASH_OFST;
	IPADBG("V6 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_ofst));

	IPA_MEM_PART(v6_rt_nhash_size) = IPA_MEM_v3_0_RAM_V6_RT_NHASH_SIZE;
	IPA_MEM_PART(v6_rt_nhash_size_ddr) = IPA_MEM_RAM_V6_RT_NHASH_SIZE_DDR;
	IPADBG("V6 RT NON-HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_size),
		IPA_MEM_PART(v6_rt_nhash_size_ddr));

	IPA_MEM_PART(modem_hdr_ofst) = IPA_MEM_v3_0_RAM_MODEM_HDR_OFST;
	IPA_MEM_PART(modem_hdr_size) = IPA_MEM_v3_0_RAM_MODEM_HDR_SIZE;
	IPADBG("MODEM HDR OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_PART(modem_hdr_size));

	IPA_MEM_PART(apps_hdr_ofst) = IPA_MEM_v3_0_RAM_APPS_HDR_OFST;
	IPA_MEM_PART(apps_hdr_size) = IPA_MEM_v3_0_RAM_APPS_HDR_SIZE;
	IPA_MEM_PART(apps_hdr_size_ddr) = IPA_MEM_v3_0_RAM_HDR_SIZE_DDR;
	IPADBG("APPS HDR OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_ofst), IPA_MEM_PART(apps_hdr_size),
		IPA_MEM_PART(apps_hdr_size_ddr));

	IPA_MEM_PART(modem_hdr_proc_ctx_ofst) =
		IPA_MEM_v3_0_RAM_MODEM_HDR_PROC_CTX_OFST;
	IPA_MEM_PART(modem_hdr_proc_ctx_size) =
		IPA_MEM_v3_0_RAM_MODEM_HDR_PROC_CTX_SIZE;
	IPADBG("MODEM HDR PROC CTX OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst),
		IPA_MEM_PART(modem_hdr_proc_ctx_size));

	IPA_MEM_PART(apps_hdr_proc_ctx_ofst) =
		IPA_MEM_v3_0_RAM_APPS_HDR_PROC_CTX_OFST;
	IPA_MEM_PART(apps_hdr_proc_ctx_size) =
		IPA_MEM_v3_0_RAM_APPS_HDR_PROC_CTX_SIZE;
	IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr) =
		IPA_MEM_RAM_HDR_PROC_CTX_SIZE_DDR;
	IPADBG("APPS HDR PROC CTX OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_proc_ctx_ofst),
		IPA_MEM_PART(apps_hdr_proc_ctx_size),
		IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr));

	IPA_MEM_PART(modem_ofst) = IPA_MEM_v3_0_RAM_MODEM_OFST;
	IPA_MEM_PART(modem_size) = IPA_MEM_v3_0_RAM_MODEM_SIZE;
	IPADBG("MODEM OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(modem_ofst),
		IPA_MEM_PART(modem_size));

	IPA_MEM_PART(apps_v4_flt_hash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V4_FLT_HASH_OFST;
	IPA_MEM_PART(apps_v4_flt_hash_size) =
		IPA_MEM_v3_0_RAM_APPS_V4_FLT_HASH_SIZE;
	IPADBG("V4 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_hash_ofst),
		IPA_MEM_PART(apps_v4_flt_hash_size));

	IPA_MEM_PART(apps_v4_flt_nhash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V4_FLT_NHASH_OFST;
	IPA_MEM_PART(apps_v4_flt_nhash_size) =
		IPA_MEM_v3_0_RAM_APPS_V4_FLT_NHASH_SIZE;
	IPADBG("V4 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_nhash_ofst),
		IPA_MEM_PART(apps_v4_flt_nhash_size));

	IPA_MEM_PART(apps_v6_flt_hash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V6_FLT_HASH_OFST;
	IPA_MEM_PART(apps_v6_flt_hash_size) =
		IPA_MEM_v3_0_RAM_APPS_V6_FLT_HASH_SIZE;
	IPADBG("V6 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_hash_ofst),
		IPA_MEM_PART(apps_v6_flt_hash_size));

	IPA_MEM_PART(apps_v6_flt_nhash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V6_FLT_NHASH_OFST;
	IPA_MEM_PART(apps_v6_flt_nhash_size) =
		IPA_MEM_v3_0_RAM_APPS_V6_FLT_NHASH_SIZE;
	IPADBG("V6 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_nhash_ofst),
		IPA_MEM_PART(apps_v6_flt_nhash_size));

	IPA_MEM_PART(end_ofst) = IPA_MEM_v3_0_RAM_END_OFST;
	IPA_MEM_PART(apps_v4_rt_hash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V4_RT_HASH_OFST;
	IPA_MEM_PART(apps_v4_rt_hash_size) =
		IPA_MEM_v3_0_RAM_APPS_V4_RT_HASH_SIZE;
	IPA_MEM_PART(apps_v4_rt_nhash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V4_RT_NHASH_OFST;
	IPA_MEM_PART(apps_v4_rt_nhash_size) =
		IPA_MEM_v3_0_RAM_APPS_V4_RT_NHASH_SIZE;
	IPA_MEM_PART(apps_v6_rt_hash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V6_RT_HASH_OFST;
	IPA_MEM_PART(apps_v6_rt_hash_size) =
		IPA_MEM_v3_0_RAM_APPS_V6_RT_HASH_SIZE;
	IPA_MEM_PART(apps_v6_rt_nhash_ofst) =
		IPA_MEM_v3_0_RAM_APPS_V6_RT_NHASH_OFST;
	IPA_MEM_PART(apps_v6_rt_nhash_size) =
		IPA_MEM_v3_0_RAM_APPS_V6_RT_NHASH_SIZE;
}

/**
 * ipa_ctrl_static_bind() - set the appropriate methods for
 *  IPA Driver based on the HW version
 *
 *  @ctrl: data structure which holds the function pointers
 *  @hw_type: the HW type in use
 *
 *  This function can avoid the runtime assignment by using C99 special
 *  struct initialization - hard decision... time.vs.mem
 */
int ipa3_controller_static_bind(struct ipa3_controller *ctrl,
		enum ipa_hw_type hw_type)
{
	ipa_init_mem_partition_v3_0();

	ctrl->ipa_init_rt4 = _ipa_init_rt4_v3;
	ctrl->ipa_init_rt6 = _ipa_init_rt6_v3;
	ctrl->ipa_init_flt4 = _ipa_init_flt4_v3;
	ctrl->ipa_init_flt6 = _ipa_init_flt6_v3;
	ctrl->ipa3_cfg_ep_hdr = _ipa_cfg_ep_hdr_v3_0;
	ctrl->ipa3_cfg_ep_nat = _ipa_cfg_ep_nat_v3_0;
	ctrl->ipa3_cfg_ep_aggr = _ipa_cfg_ep_aggr_v3_0;
	ctrl->ipa3_cfg_ep_deaggr = _ipa_cfg_ep_deaggr_v3_0;
	ctrl->ipa3_cfg_ep_mode = _ipa_cfg_ep_mode_v3_0;
	ctrl->ipa3_cfg_ep_route = _ipa_cfg_ep_route_v3_0;
	ctrl->ipa3_cfg_route = _ipa_cfg_route_v3_0;
	ctrl->ipa3_cfg_ep_status = _ipa_cfg_ep_status_v3_0;
	ctrl->ipa3_cfg_ep_cfg = _ipa_cfg_ep_cfg_v3_0;
	ctrl->ipa3_cfg_ep_metadata_mask = _ipa_cfg_ep_metadata_mask_v3_0;
	ctrl->ipa_clk_rate_turbo = IPA_V3_0_CLK_RATE_TURBO;
	ctrl->ipa_clk_rate_nominal = IPA_V3_0_CLK_RATE_NOMINAL;
	ctrl->ipa_clk_rate_svs = IPA_V3_0_CLK_RATE_SVS;
	ctrl->ipa3_read_gen_reg = _ipa_read_gen_reg_v3_0;
	ctrl->ipa3_read_ep_reg = _ipa_read_ep_reg_v3_0;
	ctrl->ipa3_write_dbg_cnt = _ipa_write_dbg_cnt_v3_0;
	ctrl->ipa3_read_dbg_cnt = _ipa_read_dbg_cnt_v3_0;
	ctrl->ipa3_commit_flt = __ipa_commit_flt_v3;
	ctrl->ipa3_commit_rt = __ipa_commit_rt_v3;
	ctrl->ipa3_commit_hdr = __ipa_commit_hdr_v3_0;
	ctrl->ipa3_enable_clks = _ipa_enable_clks_v3_0;
	ctrl->ipa3_disable_clks = _ipa_disable_clks_v3_0;
	ctrl->msm_bus_data_ptr = &ipa_bus_client_pdata_v3_0;
	ctrl->ipa3_cfg_ep_metadata = _ipa_cfg_ep_metadata_v3_0;
	ctrl->clock_scaling_bw_threshold_nominal =
		IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS;
	ctrl->clock_scaling_bw_threshold_turbo =
		IPA_V3_0_BW_THRESHOLD_TURBO_MBPS;
	ctrl->ipa_reg_base_ofst = IPA_REG_BASE_OFST_v3_0;
	ctrl->ipa_init_sram = _ipa_init_sram_v3_0;
	ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v3_0;

	ctrl->ipa_init_hdr = _ipa_init_hdr_v3_0;
	ctrl->ipa3_cfg_ep_hdr_ext = _ipa_cfg_ep_hdr_ext_v3_0;
	ctrl->ipa3_cfg_ep_holb = _ipa_cfg_ep_holb_v3_0;
	ctrl->ipa_generate_rt_hw_rule = __ipa_generate_rt_hw_rule_v3_0;

	return 0;
}

void ipa3_skb_recycle(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo;

	shinfo = skb_shinfo(skb);
	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
	atomic_set(&shinfo->dataref, 1);

	memset(skb, 0, offsetof(struct sk_buff, tail));
	skb->data = skb->head + NET_SKB_PAD;
	skb_reset_tail_pointer(skb);
}

int ipa3_alloc_rule_id(struct idr *rule_ids)
{
	return idr_alloc(rule_ids, NULL,
		IPA_RULE_ID_MIN_VAL, IPA_RULE_ID_MAX_VAL + 1,
		GFP_KERNEL);
}

int ipa3_id_alloc(void *ptr)
{
	int id;

	idr_preload(GFP_KERNEL);
	spin_lock(&ipa3_ctx->idr_lock);
	id = idr_alloc(&ipa3_ctx->ipa_idr, ptr, 0, 0, GFP_NOWAIT);
	spin_unlock(&ipa3_ctx->idr_lock);
	idr_preload_end();

	return id;
}

void *ipa3_id_find(u32 id)
{
	void *ptr;

	spin_lock(&ipa3_ctx->idr_lock);
	ptr = idr_find(&ipa3_ctx->ipa_idr, id);
	spin_unlock(&ipa3_ctx->idr_lock);

	return ptr;
}

void ipa3_id_remove(u32 id)
{
	spin_lock(&ipa3_ctx->idr_lock);
	idr_remove(&ipa3_ctx->ipa_idr, id);
	spin_unlock(&ipa3_ctx->idr_lock);
}

void ipa3_tag_free_buf(void *user1, int user2)
{
	kfree(user1);
}

static void ipa3_tag_free_skb(void *user1, int user2)
{
	dev_kfree_skb_any((struct sk_buff *)user1);
}

#define REQUIRED_TAG_PROCESS_DESCRIPTORS 4

/* ipa3_tag_process() - Initiates a tag process. Incorporates the input
 * descriptors
 *
 * @desc:	descriptors with commands for IC
 * @desc_size:	amount of descriptors in the above variable
 *
 * Note: The descriptors are copied (if there's room), the client needs to
 * free his descriptors afterwards
 *
 * Return: 0 or negative in case of failure
 */
int ipa3_tag_process(struct ipa3_desc desc[],
	int descs_num,
	unsigned long timeout)
{
	struct ipa3_sys_context *sys;
	struct ipa3_desc *tag_desc;
	int desc_idx = 0;
	struct ipa3_ip_packet_init *pkt_init;
	struct ipa3_register_write *reg_write_nop;
	struct ipa3_ip_packet_tag_status *status;
	int i;
	struct sk_buff *dummy_skb;
	int res;
	struct ipa3_tag_completion *comp;
	int ep_idx;

	/* Not enough room for the required descriptors for the tag process */
	if (IPA_TAG_MAX_DESC - descs_num < REQUIRED_TAG_PROCESS_DESCRIPTORS) {
		IPAERR("up to %d descriptors are allowed (received %d)\n",
		       IPA_TAG_MAX_DESC - REQUIRED_TAG_PROCESS_DESCRIPTORS,
		       descs_num);
		return -ENOMEM;
	}

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}
	sys = ipa3_ctx->ep[ep_idx].sys;

	tag_desc = kzalloc(sizeof(*tag_desc) * IPA_TAG_MAX_DESC, GFP_KERNEL);
	if (!tag_desc) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_alloc_desc;
	}

	/* Copy the required descriptors from the client now */
	if (desc) {
		memcpy(&(tag_desc[0]), desc, descs_num *
			sizeof(tag_desc[0]));
		desc_idx += descs_num;
	}

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	reg_write_nop = kzalloc(sizeof(*reg_write_nop), GFP_KERNEL);
	if (!reg_write_nop) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}

	reg_write_nop->skip_pipeline_clear = 0;
	reg_write_nop->pipeline_clear_options = IPA_FULL_PIPELINE_CLEAR;
	reg_write_nop->value_mask = 0x0;

	tag_desc[desc_idx].opcode = IPA_REGISTER_WRITE;
	tag_desc[desc_idx].pyld = reg_write_nop;
	tag_desc[desc_idx].len = sizeof(*reg_write_nop);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_free_buf;
	tag_desc[desc_idx].user1 = reg_write_nop;
	desc_idx++;

	/* IP_PACKET_INIT IC for tag status to be sent to apps */
	pkt_init = kzalloc(sizeof(*pkt_init), GFP_KERNEL);
	if (!pkt_init) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_alloc_pkt_init;
	}

	pkt_init->destination_pipe_index =
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);

	tag_desc[desc_idx].opcode = IPA_IP_PACKET_INIT;
	tag_desc[desc_idx].pyld = pkt_init;
	tag_desc[desc_idx].len = sizeof(*pkt_init);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_free_buf;
	tag_desc[desc_idx].user1 = pkt_init;
	desc_idx++;

	/* status IC */
	status = kzalloc(sizeof(*status), GFP_KERNEL);
	if (!status) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}

	status->tag = IPA_COOKIE;

	tag_desc[desc_idx].opcode = IPA_IP_PACKET_TAG_STATUS;
	tag_desc[desc_idx].pyld = status;
	tag_desc[desc_idx].len = sizeof(*status);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_free_buf;
	tag_desc[desc_idx].user1 = status;
	desc_idx++;

	comp = kzalloc(sizeof(*comp), GFP_KERNEL);
	if (!comp) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	init_completion(&comp->comp);

	/* completion needs to be released from both here and rx handler */
	atomic_set(&comp->cnt, 2);

	/* dummy packet to send to IPA. packet payload is a completion object */
	dummy_skb = alloc_skb(sizeof(comp), GFP_KERNEL);
	if (!dummy_skb) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_free_skb;
	}

	memcpy(skb_put(dummy_skb, sizeof(comp)), &comp, sizeof(comp));

	tag_desc[desc_idx].pyld = dummy_skb->data;
	tag_desc[desc_idx].len = dummy_skb->len;
	tag_desc[desc_idx].type = IPA_DATA_DESC_SKB;
	tag_desc[desc_idx].callback = ipa3_tag_free_skb;
	tag_desc[desc_idx].user1 = dummy_skb;
	desc_idx++;

	/* send all descriptors to IPA with single EOT */
	res = ipa3_send(sys, desc_idx, tag_desc, true);
	if (res) {
		IPAERR("failed to send TAG packets %d\n", res);
		res = -ENOMEM;
		goto fail_send;
	}
	kfree(tag_desc);
	tag_desc = NULL;

	IPADBG("waiting for TAG response\n");
	res = wait_for_completion_timeout(&comp->comp, timeout);
	if (res == 0) {
		IPAERR("timeout (%lu msec) on waiting for TAG response\n",
			timeout);
		WARN_ON(1);
		if (atomic_dec_return(&comp->cnt) == 0)
			kfree(comp);
		return -ETIME;
	}

	IPADBG("TAG response arrived!\n");
	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);

	/* sleep for short period to ensure IPA wrote all packets to BAM */
	usleep_range(IPA_TAG_SLEEP_MIN_USEC, IPA_TAG_SLEEP_MAX_USEC);

	return 0;

fail_send:
	dev_kfree_skb_any(dummy_skb);
	desc_idx--;
fail_free_skb:
	kfree(comp);
fail_free_desc:
	/*
	 * Free only the first descriptors allocated here.
	 * [pkt_init, status, nop]
	 * The user is responsible to free his allocations
	 * in case of failure.
	 * The min is required because we may fail during
	 * of the initial allocations above
	 */
	for (i = 0; i < min(REQUIRED_TAG_PROCESS_DESCRIPTORS-1, desc_idx); i++)
		kfree(tag_desc[i].user1);

fail_alloc_pkt_init:
	kfree(tag_desc);
fail_alloc_desc:
	return res;
}

/**
 * ipa3_tag_generate_force_close_desc() - generate descriptors for force close
 *					 immediate command
 *
 * @desc: descriptors for IC
 * @desc_size: desc array size
 * @start_pipe: first pipe to close aggregation
 * @end_pipe: last (non-inclusive) pipe to close aggregation
 *
 * Return: number of descriptors written or negative in case of failure
 */
static int ipa3_tag_generate_force_close_desc(struct ipa3_desc desc[],
	int desc_size, int start_pipe, int end_pipe)
{
	int i;
	u32 aggr_init;
	int desc_idx = 0;
	int res;
	struct ipa3_register_write *reg_write_agg_close;

	for (i = start_pipe; i < end_pipe; i++) {
		aggr_init = ipa_read_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v3_0(i));
		if (((aggr_init & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) >>
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT) != IPA_ENABLE_AGGR)
			continue;
		IPADBG("Force close ep: %d\n", i);
		if (desc_idx + 1 > desc_size) {
			IPAERR("Internal error - no descriptors\n");
			res = -EFAULT;
			goto fail_no_desc;
		}

		reg_write_agg_close = kzalloc(sizeof(*reg_write_agg_close),
			GFP_KERNEL);
		if (!reg_write_agg_close) {
			IPAERR("no mem\n");
			res = -ENOMEM;
			goto fail_alloc_reg_write_agg_close;
		}

		reg_write_agg_close->skip_pipeline_clear = 0;
		reg_write_agg_close->pipeline_clear_options =
			IPA_FULL_PIPELINE_CLEAR;

		reg_write_agg_close->offset = IPA_AGGR_FORCE_CLOSE_OFST;
		IPA_SETFIELD_IN_REG(reg_write_agg_close->value, 1 << i,
		   IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT,
		   IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK);
		reg_write_agg_close->value_mask =
		IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK <<
		IPA_AGGR_FORCE_CLOSE_OFST_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT;

		desc[desc_idx].opcode = IPA_REGISTER_WRITE;
		desc[desc_idx].pyld = reg_write_agg_close;
		desc[desc_idx].len = sizeof(*reg_write_agg_close);
		desc[desc_idx].type = IPA_IMM_CMD_DESC;
		desc[desc_idx].callback = ipa3_tag_free_buf;
		desc[desc_idx].user1 = reg_write_agg_close;
		desc_idx++;
	}

	return desc_idx;

fail_alloc_reg_write_agg_close:
	for (i = 0; i < desc_idx; i++)
		kfree(desc[desc_idx].user1);
fail_no_desc:
	return res;
}

/**
 * ipa3_tag_aggr_force_close() - Force close aggregation
 *
 * @pipe_num: pipe number or -1 for all pipes
 */
int ipa3_tag_aggr_force_close(int pipe_num)
{
	struct ipa3_desc *desc;
	int res = -1;
	int start_pipe;
	int end_pipe;
	int num_descs;
	int num_aggr_descs;

	if (pipe_num < -1 || pipe_num >= (int)ipa3_ctx->ipa_num_pipes) {
		IPAERR("Invalid pipe number %d\n", pipe_num);
		return -EINVAL;
	}

	if (pipe_num == -1) {
		start_pipe = 0;
		end_pipe = ipa3_ctx->ipa_num_pipes;
	} else {
		start_pipe = pipe_num;
		end_pipe = pipe_num + 1;
	}

	num_descs = end_pipe - start_pipe;

	desc = kcalloc(num_descs, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("no mem\n");
		return -ENOMEM;
	}

	/* Force close aggregation on all valid pipes with aggregation */
	num_aggr_descs = ipa3_tag_generate_force_close_desc(desc, num_descs,
						start_pipe, end_pipe);
	if (num_aggr_descs < 0) {
		IPAERR("ipa3_tag_generate_force_close_desc failed %d\n",
			num_aggr_descs);
		goto fail_free_desc;
	}

	res = ipa3_tag_process(desc, num_aggr_descs,
			      IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT);

fail_free_desc:
	kfree(desc);

	return res;
}

/**
 * ipa3_is_ready() - check if IPA module was initialized
 * successfully
 *
 * Return value: true for yes; false for no
 */
bool ipa3_is_ready(void)
{
	bool complete;

	if (ipa3_ctx == NULL)
		return false;
	mutex_lock(&ipa3_ctx->lock);
	complete = ipa3_ctx->ipa_initialization_complete;
	mutex_unlock(&ipa3_ctx->lock);
	return complete;
}

/**
 * ipa3_is_client_handle_valid() - check if IPA client handle is valid handle
 *
 * Return value: true for yes; false for no
 */
bool ipa3_is_client_handle_valid(u32 clnt_hdl)
{
	if (clnt_hdl >= 0 && clnt_hdl < ipa3_ctx->ipa_num_pipes)
		return true;
	return false;
}

/**
 * ipa3_proxy_clk_unvote() - called to remove IPA clock proxy vote
 *
 * Return value: none
 */
void ipa3_proxy_clk_unvote(void)
{
	if (ipa3_is_ready() && ipa3_ctx->q6_proxy_clk_vote_valid) {
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_valid = false;
	}
}

/**
 * ipa3_proxy_clk_vote() - called to add IPA clock proxy vote
 *
 * Return value: none
 */
void ipa3_proxy_clk_vote(void)
{
	if (ipa3_is_ready() && !ipa3_ctx->q6_proxy_clk_vote_valid) {
		IPA_ACTIVE_CLIENTS_INC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_valid = true;
	}
}

/**
 * ipa3_get_smem_restr_bytes()- Return IPA smem restricted bytes
 *
 * Return value: u16 - number of IPA smem restricted bytes
 */
u16 ipa3_get_smem_restr_bytes(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->smem_restricted_bytes;

	IPAERR("IPA Driver not initialized\n");

	return 0;
}

/**
 * ipa3_get_modem_cfg_emb_pipe_flt()- Return ipa3_ctx->modem_cfg_emb_pipe_flt
 *
 * Return value: true if modem configures embedded pipe flt, false otherwise
 */
bool ipa3_get_modem_cfg_emb_pipe_flt(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->modem_cfg_emb_pipe_flt;

	IPAERR("IPA driver has not been initialized\n");

	return false;
}

/**
 * ipa3_get_transport_type()- Return ipa3_ctx->transport_prototype
 *
 * Return value: enum ipa_transport_type
 */
enum ipa_transport_type ipa3_get_transport_type(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->transport_prototype;

	IPAERR("IPA driver has not been initialized\n");
	return IPA_TRANSPORT_TYPE_GSI;
}

u32 ipa3_get_num_pipes(void)
{
	return ipa_read_reg(ipa3_ctx->mmio, IPA_ENABLED_PIPES_OFST);
}

/**
 * ipa3_disable_apps_wan_cons_deaggr()- set ipa_ctx->ipa_client_apps_wan_cons_agg_gro
 *
 * Return value: 0 or negative in case of failure
 */
int ipa3_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count)
{
	int res = -1;

	/* checking if IPA-HW can support */
	if ((agg_size >> 10) >
		IPA_AGGR_BYTE_LIMIT) {
		IPAERR("IPA-AGG byte limit %d\n",
		IPA_AGGR_BYTE_LIMIT);
		IPAERR("exceed aggr_byte_limit\n");
		return res;
		}
	if (agg_count >
		IPA_AGGR_PKT_LIMIT) {
		IPAERR("IPA-AGG pkt limit %d\n",
		IPA_AGGR_PKT_LIMIT);
		IPAERR("exceed aggr_pkt_limit\n");
		return res;
	}

	if (ipa3_ctx) {
		ipa3_ctx->ipa_client_apps_wan_cons_agg_gro = true;
		return 0;
	}
	return res;
}


int ipa3_bind_api_controller(enum ipa_hw_type ipa_hw_type,
	struct ipa_api_controller *api_ctrl)
{
	if (ipa_hw_type < IPA_HW_v3_0) {
		IPAERR("Unsupported IPA HW version %d\n", ipa_hw_type);
		WARN_ON(1);
		return -EPERM;
	}

	api_ctrl->ipa_connect = ipa3_connect;
	api_ctrl->ipa_disconnect = ipa3_disconnect;
	api_ctrl->ipa_reset_endpoint = ipa3_reset_endpoint;
	api_ctrl->ipa_clear_endpoint_delay = ipa3_clear_endpoint_delay;
	api_ctrl->ipa_cfg_ep = ipa3_cfg_ep;
	api_ctrl->ipa_cfg_ep_nat = ipa3_cfg_ep_nat;
	api_ctrl->ipa_cfg_ep_hdr = ipa3_cfg_ep_hdr;
	api_ctrl->ipa_cfg_ep_hdr_ext = ipa3_cfg_ep_hdr_ext;
	api_ctrl->ipa_cfg_ep_mode = ipa3_cfg_ep_mode;
	api_ctrl->ipa_cfg_ep_aggr = ipa3_cfg_ep_aggr;
	api_ctrl->ipa_cfg_ep_deaggr = ipa3_cfg_ep_deaggr;
	api_ctrl->ipa_cfg_ep_route = ipa3_cfg_ep_route;
	api_ctrl->ipa_cfg_ep_holb = ipa3_cfg_ep_holb;
	api_ctrl->ipa_cfg_ep_cfg = ipa3_cfg_ep_cfg;
	api_ctrl->ipa_cfg_ep_metadata_mask = ipa3_cfg_ep_metadata_mask;
	api_ctrl->ipa_cfg_ep_holb_by_client = ipa3_cfg_ep_holb_by_client;
	api_ctrl->ipa_cfg_ep_ctrl = ipa3_cfg_ep_ctrl;
	api_ctrl->ipa_add_hdr = ipa3_add_hdr;
	api_ctrl->ipa_del_hdr = ipa3_del_hdr;
	api_ctrl->ipa_commit_hdr = ipa3_commit_hdr;
	api_ctrl->ipa_reset_hdr = ipa3_reset_hdr;
	api_ctrl->ipa_get_hdr = ipa3_get_hdr;
	api_ctrl->ipa_put_hdr = ipa3_put_hdr;
	api_ctrl->ipa_copy_hdr = ipa3_copy_hdr;
	api_ctrl->ipa_add_hdr_proc_ctx = ipa3_add_hdr_proc_ctx;
	api_ctrl->ipa_del_hdr_proc_ctx = ipa3_del_hdr_proc_ctx;
	api_ctrl->ipa_add_rt_rule = ipa3_add_rt_rule;
	api_ctrl->ipa_del_rt_rule = ipa3_del_rt_rule;
	api_ctrl->ipa_commit_rt = ipa3_commit_rt;
	api_ctrl->ipa_reset_rt = ipa3_reset_rt;
	api_ctrl->ipa_get_rt_tbl = ipa3_get_rt_tbl;
	api_ctrl->ipa_put_rt_tbl = ipa3_put_rt_tbl;
	api_ctrl->ipa_query_rt_index = ipa3_query_rt_index;
	api_ctrl->ipa_mdfy_rt_rule = ipa3_mdfy_rt_rule;
	api_ctrl->ipa_add_flt_rule = ipa3_add_flt_rule;
	api_ctrl->ipa_del_flt_rule = ipa3_del_flt_rule;
	api_ctrl->ipa_mdfy_flt_rule = ipa3_mdfy_flt_rule;
	api_ctrl->ipa_commit_flt = ipa3_commit_flt;
	api_ctrl->ipa_reset_flt = ipa3_reset_flt;
	api_ctrl->allocate_nat_device = ipa3_allocate_nat_device;
	api_ctrl->ipa_nat_init_cmd = ipa3_nat_init_cmd;
	api_ctrl->ipa_nat_dma_cmd = ipa3_nat_dma_cmd;
	api_ctrl->ipa_nat_del_cmd = ipa3_nat_del_cmd;
	api_ctrl->ipa_send_msg = ipa3_send_msg;
	api_ctrl->ipa_register_pull_msg = ipa3_register_pull_msg;
	api_ctrl->ipa_deregister_pull_msg = ipa3_deregister_pull_msg;
	api_ctrl->ipa_register_intf = ipa3_register_intf;
	api_ctrl->ipa_register_intf_ext = ipa3_register_intf_ext;
	api_ctrl->ipa_deregister_intf = ipa3_deregister_intf;
	api_ctrl->ipa_set_aggr_mode = ipa3_set_aggr_mode;
	api_ctrl->ipa_set_qcncm_ndp_sig = ipa3_set_qcncm_ndp_sig;
	api_ctrl->ipa_set_single_ndp_per_mbim = ipa3_set_single_ndp_per_mbim;
	api_ctrl->ipa_tx_dp = ipa3_tx_dp;
	api_ctrl->ipa_tx_dp_mul = ipa3_tx_dp_mul;
	api_ctrl->ipa_free_skb = ipa3_free_skb;
	api_ctrl->ipa_setup_sys_pipe = ipa3_setup_sys_pipe;
	api_ctrl->ipa_teardown_sys_pipe = ipa3_teardown_sys_pipe;
	api_ctrl->ipa_sys_setup = ipa3_sys_setup;
	api_ctrl->ipa_sys_teardown = ipa3_sys_teardown;
	api_ctrl->ipa_sys_update_gsi_hdls = ipa3_sys_update_gsi_hdls;
	api_ctrl->ipa_connect_wdi_pipe = ipa3_connect_wdi_pipe;
	api_ctrl->ipa_disconnect_wdi_pipe = ipa3_disconnect_wdi_pipe;
	api_ctrl->ipa_enable_wdi_pipe = ipa3_enable_wdi_pipe;
	api_ctrl->ipa_disable_wdi_pipe = ipa3_disable_wdi_pipe;
	api_ctrl->ipa_resume_wdi_pipe = ipa3_resume_wdi_pipe;
	api_ctrl->ipa_suspend_wdi_pipe = ipa3_suspend_wdi_pipe;
	api_ctrl->ipa_get_wdi_stats = ipa3_get_wdi_stats;
	api_ctrl->ipa_get_smem_restr_bytes = ipa3_get_smem_restr_bytes;
	api_ctrl->ipa_uc_wdi_get_dbpa = ipa3_uc_wdi_get_dbpa;
	api_ctrl->ipa_uc_reg_rdyCB = ipa3_uc_reg_rdyCB;
	api_ctrl->ipa_uc_dereg_rdyCB = ipa3_uc_dereg_rdyCB;
	api_ctrl->ipa_rm_create_resource = ipa3_rm_create_resource;
	api_ctrl->ipa_rm_delete_resource = ipa3_rm_delete_resource;
	api_ctrl->ipa_rm_register = ipa3_rm_register;
	api_ctrl->ipa_rm_deregister = ipa3_rm_deregister;
	api_ctrl->ipa_rm_set_perf_profile = ipa3_rm_set_perf_profile;
	api_ctrl->ipa_rm_add_dependency = ipa3_rm_add_dependency;
	api_ctrl->ipa_rm_delete_dependency = ipa3_rm_delete_dependency;
	api_ctrl->ipa_rm_request_resource = ipa3_rm_request_resource;
	api_ctrl->ipa_rm_release_resource = ipa3_rm_release_resource;
	api_ctrl->ipa_rm_notify_completion = ipa3_rm_notify_completion;
	api_ctrl->ipa_rm_inactivity_timer_init =
		ipa3_rm_inactivity_timer_init;
	api_ctrl->ipa_rm_inactivity_timer_destroy =
		ipa3_rm_inactivity_timer_destroy;
	api_ctrl->ipa_rm_inactivity_timer_request_resource =
		ipa3_rm_inactivity_timer_request_resource;
	api_ctrl->ipa_rm_inactivity_timer_release_resource =
		ipa3_rm_inactivity_timer_release_resource;
	api_ctrl->teth_bridge_init = ipa3_teth_bridge_init;
	api_ctrl->teth_bridge_disconnect = ipa3_teth_bridge_disconnect;
	api_ctrl->teth_bridge_connect = ipa3_teth_bridge_connect;
	api_ctrl->ipa_set_client = ipa3_set_client;
	api_ctrl->ipa_get_client = ipa3_get_client;
	api_ctrl->ipa_get_client_uplink = ipa3_get_client_uplink;
	api_ctrl->odu_bridge_init = ipa3_odu_bridge_init;
	api_ctrl->odu_bridge_connect = ipa3_odu_bridge_connect;
	api_ctrl->odu_bridge_disconnect = ipa3_odu_bridge_disconnect;
	api_ctrl->odu_bridge_tx_dp = ipa3_odu_bridge_tx_dp;
	api_ctrl->odu_bridge_cleanup = ipa3_odu_bridge_cleanup;
	api_ctrl->ipa_dma_init = ipa3_dma_init;
	api_ctrl->ipa_dma_enable = ipa3_dma_enable;
	api_ctrl->ipa_dma_disable = ipa3_dma_disable;
	api_ctrl->ipa_dma_sync_memcpy = ipa3_dma_sync_memcpy;
	api_ctrl->ipa_dma_async_memcpy = ipa3_dma_async_memcpy;
	api_ctrl->ipa_dma_uc_memcpy = ipa3_dma_uc_memcpy;
	api_ctrl->ipa_dma_destroy = ipa3_dma_destroy;
	api_ctrl->ipa_mhi_init = ipa3_mhi_init;
	api_ctrl->ipa_mhi_start = ipa3_mhi_start;
	api_ctrl->ipa_mhi_connect_pipe = ipa3_mhi_connect_pipe;
	api_ctrl->ipa_mhi_disconnect_pipe = ipa3_mhi_disconnect_pipe;
	api_ctrl->ipa_mhi_suspend = ipa3_mhi_suspend;
	api_ctrl->ipa_mhi_resume = ipa3_mhi_resume;
	api_ctrl->ipa_mhi_destroy = ipa3_mhi_destroy;
	api_ctrl->ipa_write_qmap_id = ipa3_write_qmap_id;
	api_ctrl->ipa_add_interrupt_handler = ipa3_add_interrupt_handler;
	api_ctrl->ipa_remove_interrupt_handler = ipa3_remove_interrupt_handler;
	api_ctrl->ipa_restore_suspend_handler = ipa3_restore_suspend_handler;
	api_ctrl->ipa_bam_reg_dump = ipa3_bam_reg_dump;
	api_ctrl->ipa_get_ep_mapping = ipa3_get_ep_mapping;
	api_ctrl->ipa_is_ready = ipa3_is_ready;
	api_ctrl->ipa_proxy_clk_vote = ipa3_proxy_clk_vote;
	api_ctrl->ipa_proxy_clk_unvote = ipa3_proxy_clk_unvote;
	api_ctrl->ipa_is_client_handle_valid = ipa3_is_client_handle_valid;
	api_ctrl->ipa_get_client_mapping = ipa3_get_client_mapping;
	api_ctrl->ipa_get_rm_resource_from_ep = ipa3_get_rm_resource_from_ep;
	api_ctrl->ipa_get_modem_cfg_emb_pipe_flt =
		ipa3_get_modem_cfg_emb_pipe_flt;
	api_ctrl->ipa_get_transport_type = ipa3_get_transport_type;
	api_ctrl->ipa_ap_suspend = ipa3_ap_suspend;
	api_ctrl->ipa_ap_resume = ipa3_ap_resume;
	api_ctrl->ipa_get_smmu_domain = ipa3_get_smmu_domain;
	api_ctrl->ipa_disable_apps_wan_cons_deaggr =
		ipa3_disable_apps_wan_cons_deaggr;
	api_ctrl->ipa_rm_add_dependency_sync = ipa3_rm_add_dependency_sync;
	api_ctrl->ipa_get_dma_dev = ipa3_get_dma_dev;
	api_ctrl->ipa_release_wdi_mapping = ipa3_release_wdi_mapping;
	api_ctrl->ipa_create_wdi_mapping = ipa3_create_wdi_mapping;
	api_ctrl->ipa_get_gsi_ep_info = ipa3_get_gsi_ep_info;
	api_ctrl->ipa_stop_gsi_channel = ipa3_stop_gsi_channel;
	api_ctrl->ipa_usb_init_teth_prot = ipa3_usb_init_teth_prot;
	api_ctrl->ipa_usb_xdci_connect = ipa3_usb_xdci_connect;
	api_ctrl->ipa_usb_xdci_disconnect = ipa3_usb_xdci_disconnect;
	api_ctrl->ipa_usb_deinit_teth_prot = ipa3_usb_deinit_teth_prot;
	api_ctrl->ipa_usb_xdci_suspend = ipa3_usb_xdci_suspend;
	api_ctrl->ipa_usb_xdci_resume = ipa3_usb_xdci_resume;
	api_ctrl->ipa_register_ipa_ready_cb = ipa3_register_ipa_ready_cb;

	return 0;
}

/**
 * ipa_is_modem_pipe()- Checks if pipe is owned by the modem
 *
 * @pipe_idx: pipe number
 * Return value: true if owned by modem, false otherwize
 */
bool ipa_is_modem_pipe(int pipe_idx)
{
	int client_idx;

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return false;
	}

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (!IPA_CLIENT_IS_Q6_CONS(client_idx) &&
			!IPA_CLIENT_IS_Q6_PROD(client_idx))
			continue;
		if (ipa3_get_ep_mapping(client_idx) == pipe_idx)
			return true;
	}

	return false;
}

static void ipa3_write_rsrc_grp_type_reg(int group_index,
			enum ipa_rsrc_grp_type_src n, bool src, u32 val) {

	if (src) {
		switch (group_index) {
		case IPA_GROUP_UL:
		case IPA_GROUP_DL:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n(n), val);
			break;
		case IPA_GROUP_DIAG:
		case IPA_GROUP_DMA:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n(n), val);
			break;
		case IPA_GROUP_Q6ZIP:
		case IPA_GROUP_UC_RX_Q:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n(n), val);
			break;
		default:
			IPAERR(
			" Invalid source resource group,index #%d\n",
			group_index);
			break;
		}
	} else {
		switch (group_index) {
		case IPA_GROUP_UL:
		case IPA_GROUP_DL:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_DST_RSRC_GRP_01_RSRC_TYPE_n(n), val);
			break;
		case IPA_GROUP_DIAG:
		case IPA_GROUP_DMA:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_DST_RSRC_GRP_23_RSRC_TYPE_n(n), val);
			break;
		case IPA_GROUP_Q6ZIP_GENERAL:
		case IPA_GROUP_Q6ZIP_ENGINE:
			ipa_write_reg(ipa3_ctx->mmio,
				IPA_DST_RSRC_GRP_45_RSRC_TYPE_n(n), val);
			break;
		default:
			IPAERR(
			" Invalid destination resource group,index #%d\n",
			group_index);
			break;
		}
	}
}

static void ipa3_configure_rx_hps_clients(int depth, bool min)
{
	int i;
	int val;
	u32 reg;

	/*
	 * depth 0 contains 4 first clients out of 6
	 * depth 1 contains 2 last clients out of 6
	 */
	for (i = 0 ; i < (depth ? 2 : 4) ; i++) {
		if (min)
			val = ipa3_rsrc_rx_grp_config
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[!depth ? i : 4 + i].min;
		else
			val = ipa3_rsrc_rx_grp_config
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[!depth ? i : 4 + i].max;

		IPA_SETFIELD_IN_REG(reg, val,
			IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(i),
			IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(i));
	}
	if (depth) {
		ipa_write_reg(ipa3_ctx->mmio,
			min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_1 :
			IPA_RX_HPS_CLIENTS_MAX_DEPTH_1,
			reg);
	} else {
		ipa_write_reg(ipa3_ctx->mmio,
			min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_0 :
			IPA_RX_HPS_CLIENTS_MAX_DEPTH_0,
			reg);
	}
}

void ipa3_set_resorce_groups_min_max_limits(void)
{
	int i;
	int j;
	u32 reg;

	IPADBG("ENTER\n");
	IPADBG("Assign source rsrc groups min-max limits\n");

	for (i = 0; i < IPA_RSRC_GRP_TYPE_SRC_MAX; i++) {
		for (j = 0; j < IPA_GROUP_MAX; j = j + 2) {
			reg = 0;
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_src_grp_config[i][j].min,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_src_grp_config[i][j].max,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_src_grp_config[i][j + 1].min,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_src_grp_config[i][j + 1].max,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_BMSK);
			ipa3_write_rsrc_grp_type_reg(j, i, true, reg);
		}
	}

	IPADBG("Assign destination rsrc groups min-max limits\n");

	for (i = 0; i < IPA_RSRC_GRP_TYPE_DST_MAX; i++) {
		for (j = 0; j < IPA_GROUP_MAX; j = j + 2) {
			reg = 0;
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_dst_grp_config[i][j].min,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_dst_grp_config[i][j].max,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_dst_grp_config[i][j + 1].min,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_BMSK);
			IPA_SETFIELD_IN_REG(reg,
				ipa3_rsrc_dst_grp_config[i][j + 1].max,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_SHFT,
				IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_BMSK);
			ipa3_write_rsrc_grp_type_reg(j, i, false, reg);
		}
	}

	IPADBG("Assign RX_HPS CMDQ rsrc groups min-max limits\n");

	ipa3_configure_rx_hps_clients(0, true);
	ipa3_configure_rx_hps_clients(1, true);
	ipa3_configure_rx_hps_clients(0, false);
	ipa3_configure_rx_hps_clients(1, false);

	IPADBG("EXIT\n");
}

static void ipa3_gsi_poll_after_suspend(struct ipa3_ep_context *ep)
{
	bool empty;

	IPADBG("switch ch %ld to poll\n", ep->gsi_chan_hdl);
	gsi_config_channel_mode(ep->gsi_chan_hdl, GSI_CHAN_MODE_POLL);
	gsi_is_channel_empty(ep->gsi_chan_hdl, &empty);
	if (!empty) {
		IPADBG("ch %ld not empty\n", ep->gsi_chan_hdl);
		/* queue a work to start polling if don't have one */
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (!atomic_read(&ep->sys->curr_polling_state)) {
			atomic_set(&ep->sys->curr_polling_state, 1);
			queue_work(ep->sys->wq, &ep->sys->work);
		}
	}
}

void ipa3_suspend_apps_pipes(bool suspend)
{
	struct ipa_ep_cfg_ctrl cfg;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;

	memset(&cfg, 0, sizeof(cfg));
	cfg.ipa_ep_suspend = suspend;

	ipa_ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid) {
		IPADBG("%s pipe %d\n", suspend ? "suspend" : "unsuspend",
			ipa_ep_idx);
		ipa3_cfg_ep_ctrl(ipa_ep_idx, &cfg);
		if (suspend)
			ipa3_gsi_poll_after_suspend(ep);
		else if (!atomic_read(&ep->sys->curr_polling_state))
			gsi_config_channel_mode(ep->gsi_chan_hdl,
				GSI_CHAN_MODE_CALLBACK);
	}

	ipa_ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
	/* Considering the case for SSR. */
	if (ipa_ep_idx == -1) {
		IPADBG("Invalid client.\n");
		return;
	}
	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid) {
		IPADBG("%s pipe %d\n", suspend ? "suspend" : "unsuspend",
			ipa_ep_idx);
		ipa3_cfg_ep_ctrl(ipa_ep_idx, &cfg);
		if (suspend)
			ipa3_gsi_poll_after_suspend(ep);
		else if (!atomic_read(&ep->sys->curr_polling_state))
			gsi_config_channel_mode(ep->gsi_chan_hdl,
				GSI_CHAN_MODE_CALLBACK);
	}
}

/**
 * ipa3_inject_dma_task_for_gsi()- Send DMA_TASK to IPA for GSI stop channel
 *
 * Send a DMA_TASK of 1B to IPA to unblock GSI channel in STOP_IN_PROG.
 * Return value: 0 on success, negative otherwise
 */
int ipa3_inject_dma_task_for_gsi(void)
{
	static struct ipa3_mem_buffer mem = {0};
	static struct ipa3_hw_imm_cmd_dma_task_32b_addr cmd = {0};
	struct ipa3_desc desc = {0};

	/* allocate the memory only for the very first time */
	if (!mem.base) {
		IPADBG("Allocate mem\n");
		mem.size = IPA_GSI_CHANNEL_STOP_PKT_SIZE;
		mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
			mem.size,
			&mem.phys_base,
			GFP_KERNEL);
		if (!mem.base) {
			IPAERR("no mem\n");
			return -EFAULT;
		}

		cmd.flsh = 1;
		cmd.size1 = mem.size;
		cmd.addr1 = mem.phys_base;
		cmd.packet_size = mem.size;
	}

	desc.opcode = IPA_DMA_TASK_32B_ADDR(1);
	desc.pyld = &cmd;
	desc.len = sizeof(cmd);
	desc.type = IPA_IMM_CMD_DESC;

	IPADBG("sending 1B packet to IPA\n");
	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("ipa3_send_cmd failed\n");
		return -EFAULT;
	}

	return 0;
}

/**
 * ipa3_stop_gsi_channel()- Stops a GSI channel in IPA
 * @chan_hdl: GSI channel handle
 *
 * This function implements the sequence to stop a GSI channel
 * in IPA. This function returns when the channel is is STOP state.
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa3_stop_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_mem_buffer mem;
	int res = 0;
	int i;
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	memset(&mem, 0, sizeof(mem));

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		res = gsi_stop_channel(ep->gsi_chan_hdl);
		goto end_sequence;
	}

	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		IPADBG("Calling gsi_stop_channel\n");
		res = gsi_stop_channel(ep->gsi_chan_hdl);
		IPADBG("gsi_stop_channel returned %d\n", res);
		if (res != -GSI_STATUS_AGAIN && res != -GSI_STATUS_TIMED_OUT)
			goto end_sequence;

		IPADBG("Inject a DMA_TASK with 1B packet to IPA and retry\n");
		/* Send a 1B packet DMA_RASK to IPA and try again*/
		res = ipa3_inject_dma_task_for_gsi();
		if (res) {
			IPAERR("Failed to inject DMA TASk for GSI\n");
			goto end_sequence;
		}

		/* sleep for short period to flush IPA */
		usleep_range(IPA_GSI_CHANNEL_STOP_SLEEP_MIN_USEC,
			IPA_GSI_CHANNEL_STOP_SLEEP_MAX_USEC);
	}

	IPAERR("Failed  to stop GSI channel with retries\n");
	res = -EFAULT;
end_sequence:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return res;
}

/**
 * ipa3_calc_extra_wrd_bytes()- Calculate the number of extra words for eq
 * @attrib: equation attribute
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa3_calc_extra_wrd_bytes(const struct ipa_ipfltri_rule_eq *attrib)
{
	int num = 0;

	if (attrib->tos_eq_present)
		num++;
	if (attrib->protocol_eq_present)
		num++;
	if (attrib->tc_eq_present)
		num++;
	num += attrib->num_offset_meq_128;
	num += attrib->num_offset_meq_32;
	num += attrib->num_ihl_offset_meq_32;
	num += attrib->num_ihl_offset_range_16;
	if (attrib->ihl_offset_eq_32_present)
		num++;
	if (attrib->ihl_offset_eq_16_present)
		num++;

	IPADBG("extra bytes number %d\n", num);

	return num;
}

/**
 * ipa3_calc_extra_wrd_bytes()- generate an equation from rule read from IPA HW
 * @attrib: equation attribute
 * @buf: raw rule in IPA SRAM
 * @rule_size: size of the rule pointed by buf
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa3_generate_eq_from_hw_rule(
	struct ipa_ipfltri_rule_eq *attrib, u8 *buf, u8 *rule_size)
{
	int num_offset_meq_32;
	int num_ihl_offset_range_16;
	int num_ihl_offset_meq_32;
	int num_offset_meq_128;
	int extra_bytes;
	u8 *extra;
	u8 *rest;
	int i;

	IPADBG("rule_eq_bitmap=0x%x\n", attrib->rule_eq_bitmap);
	if (attrib->rule_eq_bitmap & IPA_TOS_EQ)
		attrib->tos_eq_present = true;
	if (attrib->rule_eq_bitmap & IPA_PROTOCOL_EQ)
		attrib->protocol_eq_present = true;
	if (attrib->rule_eq_bitmap & IPA_OFFSET_MEQ32_0)
		attrib->num_offset_meq_32++;
	if (attrib->rule_eq_bitmap & IPA_OFFSET_MEQ32_1)
		attrib->num_offset_meq_32++;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_RANGE16_0)
		attrib->num_ihl_offset_range_16++;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_RANGE16_1)
		attrib->num_ihl_offset_range_16++;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_EQ_16)
		attrib->ihl_offset_eq_16_present = true;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_EQ_32)
		attrib->ihl_offset_eq_32_present = true;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_MEQ32_0)
		attrib->num_ihl_offset_meq_32++;
	if (attrib->rule_eq_bitmap & IPA_OFFSET_MEQ128_0)
		attrib->num_offset_meq_128++;
	if (attrib->rule_eq_bitmap & IPA_OFFSET_MEQ128_1)
		attrib->num_offset_meq_128++;
	if (attrib->rule_eq_bitmap & IPA_TC_EQ)
		attrib->tc_eq_present = true;
	if (attrib->rule_eq_bitmap & IPA_FL_EQ)
		attrib->fl_eq_present = true;
	if (attrib->rule_eq_bitmap & IPA_PROTOCOL_EQ)
		attrib->protocol_eq_present = true;
	if (attrib->rule_eq_bitmap & IPA_IHL_OFFSET_MEQ32_1)
		attrib->num_ihl_offset_meq_32++;
	if (attrib->rule_eq_bitmap & IPA_METADATA_COMPARE)
		attrib->metadata_meq32_present = true;
	if (attrib->rule_eq_bitmap & IPA_IS_FRAG)
		attrib->ipv4_frag_eq_present = true;

	extra_bytes = ipa3_calc_extra_wrd_bytes(attrib);
	/*
	 * only 3 eq does not have extra word param, 13 out of 16 is the number
	 * of equations that needs extra word param
	 */
	if (extra_bytes > 13) {
		IPAERR("too much extra bytes\n");
		return -EPERM;
	} else if (extra_bytes > IPA_HW_TBL_HDR_WIDTH) {
		/* two extra words */
		extra = buf;
		rest = buf + IPA_HW_TBL_HDR_WIDTH * 2;
	} else if (extra_bytes > 0) {
		/* single exra word */
		extra = buf;
		rest = buf + IPA_HW_TBL_HDR_WIDTH;
	} else {
		/* no extra words */
		extra = NULL;
		rest = buf;
	}
	IPADBG("buf=0x%p extra=0x%p rest=0x%p\n", buf, extra, rest);

	num_offset_meq_32 = attrib->num_offset_meq_32;
	num_ihl_offset_range_16 = attrib->num_ihl_offset_range_16;
	num_ihl_offset_meq_32 = attrib->num_ihl_offset_meq_32;
	num_offset_meq_128 = attrib->num_offset_meq_128;

	if (attrib->tos_eq_present)
		attrib->tos_eq = *extra++;

	if (attrib->protocol_eq_present)
		attrib->protocol_eq = *extra++;

	if (attrib->tc_eq_present)
		attrib->tc_eq = *extra++;

	if (num_offset_meq_128) {
		attrib->offset_meq_128[0].offset = *extra++;
		for (i = 0; i < 8; i++)
			attrib->offset_meq_128[0].mask[i] = *rest++;
		for (i = 0; i < 8; i++)
			attrib->offset_meq_128[0].value[i] = *rest++;
		for (i = 8; i < 16; i++)
			attrib->offset_meq_128[0].mask[i] = *rest++;
		for (i = 8; i < 16; i++)
			attrib->offset_meq_128[0].value[i] = *rest++;
		num_offset_meq_128--;
	}

	if (num_offset_meq_128) {
		attrib->offset_meq_128[1].offset = *extra++;
		for (i = 0; i < 8; i++)
			attrib->offset_meq_128[1].mask[i] = *rest++;
		for (i = 0; i < 8; i++)
			attrib->offset_meq_128[1].value[i] = *rest++;
		for (i = 8; i < 16; i++)
			attrib->offset_meq_128[1].mask[i] = *rest++;
		for (i = 8; i < 16; i++)
			attrib->offset_meq_128[1].value[i] = *rest++;
		num_offset_meq_128--;
	}

	if (num_offset_meq_32) {
		attrib->offset_meq_32[0].offset = *extra++;
		attrib->offset_meq_32[0].mask = *((u32 *)rest);
		rest += 4;
		attrib->offset_meq_32[0].value = *((u32 *)rest);
		rest += 4;
		num_offset_meq_32--;
	}
	IPADBG("buf=0x%p extra=0x%p rest=0x%p\n", buf, extra, rest);

	if (num_offset_meq_32) {
		attrib->offset_meq_32[1].offset = *extra++;
		attrib->offset_meq_32[1].mask = *((u32 *)rest);
		rest += 4;
		attrib->offset_meq_32[1].value = *((u32 *)rest);
		rest += 4;
		num_offset_meq_32--;
	}
	IPADBG("buf=0x%p extra=0x%p rest=0x%p\n", buf, extra, rest);

	if (num_ihl_offset_meq_32) {
		attrib->ihl_offset_meq_32[0].offset = *extra++;
		attrib->ihl_offset_meq_32[0].mask = *((u32 *)rest);
		rest += 4;
		attrib->ihl_offset_meq_32[0].value = *((u32 *)rest);
		rest += 4;
		num_ihl_offset_meq_32--;
	}

	if (num_ihl_offset_meq_32) {
		attrib->ihl_offset_meq_32[1].offset = *extra++;
		attrib->ihl_offset_meq_32[1].mask = *((u32 *)rest);
		rest += 4;
		attrib->ihl_offset_meq_32[1].value = *((u32 *)rest);
		rest += 4;
		num_ihl_offset_meq_32--;
	}

	if (attrib->metadata_meq32_present) {
		attrib->metadata_meq32.mask = *((u32 *)rest);
		rest += 4;
		attrib->metadata_meq32.value = *((u32 *)rest);
		rest += 4;
	}

	if (num_ihl_offset_range_16) {
		attrib->ihl_offset_range_16[0].offset = *extra++;
		attrib->ihl_offset_range_16[0].range_high = *((u16 *)rest);
		rest += 2;
		attrib->ihl_offset_range_16[0].range_low = *((u16 *)rest);
		rest += 2;
		num_ihl_offset_range_16--;
	}
	if (num_ihl_offset_range_16) {
		attrib->ihl_offset_range_16[1].offset = *extra++;
		attrib->ihl_offset_range_16[1].range_high = *((u16 *)rest);
		rest += 2;
		attrib->ihl_offset_range_16[1].range_low = *((u16 *)rest);
		rest += 2;
		num_ihl_offset_range_16--;
	}

	if (attrib->ihl_offset_eq_32_present) {
		attrib->ihl_offset_eq_32.offset = *extra++;
		attrib->ihl_offset_eq_32.value = *((u32 *)rest);
		rest += 4;
	}

	if (attrib->ihl_offset_eq_16_present) {
		attrib->ihl_offset_eq_16.offset = *extra++;
		attrib->ihl_offset_eq_16.value = *((u16 *)rest);
		rest += 4;
	}

	if (attrib->fl_eq_present) {
		attrib->fl_eq = *((u32 *)rest);
		rest += 4;
	}

	IPADBG("before align buf=0x%p extra=0x%p rest=0x%p\n",
		buf, extra, rest);
	/* align to 64 bit */
	rest = (u8 *)(((unsigned long)rest + IPA_HW_RULE_START_ALIGNMENT) &
		~IPA_HW_RULE_START_ALIGNMENT);

	IPADBG("after align buf=0x%p extra=0x%p rest=0x%p\n",
		buf, extra, rest);

	*rule_size = rest - buf;

	IPADBG("rest - buf=0x%llx\n", (u64) (rest - buf));
	IPADBG("*rule_size=0x%x\n", *rule_size);

	return 0;
}

/**
 * ipa3_load_fws() - Load the IPAv3 FWs into IPA&GSI SRAM.
 *
 * @firmware: Structure which contains the FW data from the user space.
 *
 * Return value: 0 on success, negative otherwise
 *
 */
int ipa3_load_fws(const struct firmware *firmware)
{
	/*
	 * TODO: Do we need to use a generic elf_hdr/elf_phdr
	 * so we won't have issues with 64bit systems?
	 */
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const uint8_t *elf_phdr_ptr;
	uint32_t *elf_data_ptr;
	int phdr_idx, index;
	uint32_t *fw_mem_base;

	ehdr = (struct elf32_hdr *) firmware->data;

	elf_phdr_ptr = firmware->data + sizeof(*ehdr);

	for (phdr_idx = 0; phdr_idx < ehdr->e_phnum; phdr_idx++) {
		/*
		 * The ELF program header will contain the starting
		 * address to which the firmware needs to copied.
		 * TODO: Shall we rely on that, or rely on the order
		 * of which the FWs reside in the ELF, and use
		 * registers/defines in here?
		 */
		phdr = (struct elf32_phdr *)elf_phdr_ptr;

		/*
		 * p_addr will contain the physical address to which the
		 * FW needs to be loaded.
		 * p_memsz will contain the size of the FW image.
		 */
		fw_mem_base = ioremap(phdr->p_paddr, phdr->p_memsz);
		if (!fw_mem_base) {
			IPAERR("Failed to map 0x%x for the size of %u\n",
				phdr->p_paddr, phdr->p_memsz);
				return -ENOMEM;
		}

		/*
		 * p_offset will contain and absolute offset from the beginning
		 * of the ELF file.
		 */
		elf_data_ptr = (uint32_t *)
				((uint8_t *)firmware->data + phdr->p_offset);

		if (phdr->p_memsz % sizeof(uint32_t)) {
			IPAERR("FW size %u doesn't align to 32bit\n",
				phdr->p_memsz);
			return -EFAULT;
		}

		for (index = 0; index < phdr->p_memsz/sizeof(uint32_t);
			index++) {
			writel_relaxed(*elf_data_ptr, &fw_mem_base[index]);
			elf_data_ptr++;
		}

		iounmap(fw_mem_base);

		elf_phdr_ptr = elf_phdr_ptr + sizeof(*phdr);
	}
	IPADBG("IPA FWs (GSI FW, HPS and DPS) were loaded\n");
	return 0;
}
