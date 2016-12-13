/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_fltrt.h"
#include "ipahal/ipahal_hw_stats.h"
#include "../ipa_rm_i.h"

#define IPA_V3_0_CLK_RATE_SVS2 (37.5 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_SVS (75 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_NOMINAL (150 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_TURBO (200 * 1000 * 1000UL)

#define IPA_V3_5_CLK_RATE_SVS2 (100 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_SVS (200 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_NOMINAL (400 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_TURBO (42640 * 10 * 1000UL)

#define IPA_V4_0_CLK_RATE_SVS2 (60 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_SVS (125 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_NOMINAL (220 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_TURBO (250 * 1000 * 1000UL)

#define IPA_V3_0_MAX_HOLB_TMR_VAL (4294967296 - 1)

#define IPA_V3_0_BW_THRESHOLD_TURBO_MBPS (1000)
#define IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS (600)
#define IPA_V3_0_BW_THRESHOLD_SVS_MBPS (310)

#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK 0xFF0000
#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT 0x10

/* Max pipes + ICs for TAG process */
#define IPA_TAG_MAX_DESC (IPA3_MAX_NUM_PIPES + 6)

#define IPA_TAG_SLEEP_MIN_USEC (1000)
#define IPA_TAG_SLEEP_MAX_USEC (2000)
#define IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT (10 * HZ)
#define IPA_BCR_REG_VAL_v3_0 (0x00000001)
#define IPA_BCR_REG_VAL_v3_5 (0x0000003B)
#define IPA_BCR_REG_VAL_v4_0 (0x00000039)
#define IPA_CLKON_CFG_v4_0 (0x30000000)
#define IPA_AGGR_GRAN_MIN (1)
#define IPA_AGGR_GRAN_MAX (32)
#define IPA_EOT_COAL_GRAN_MIN (1)
#define IPA_EOT_COAL_GRAN_MAX (16)

#define IPA_DMA_TASK_FOR_GSI_TIMEOUT_MSEC (15)

#define IPA_AGGR_BYTE_LIMIT (\
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT)
#define IPA_AGGR_PKT_LIMIT (\
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT)

/* In IPAv3 only endpoints 0-3 can be configured to deaggregation */
#define IPA_EP_SUPPORTS_DEAGGR(idx) ((idx) >= 0 && (idx) <= 3)

/* configure IPA spare register 1 in order to have correct IPA version
 * set bits 0,2,3 and 4. see SpareBits documentation.xlsx
 */

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

#define QMB_MASTER_SELECT_DDR  (0)
#define QMB_MASTER_SELECT_PCIE (1)

/* Resource Group index*/
#define IPA_v3_0_GROUP_UL		(0)
#define IPA_v3_0_GROUP_DL		(1)
#define IPA_v3_0_GROUP_DPL		IPA_v3_0_GROUP_DL
#define IPA_v3_0_GROUP_DIAG		(2)
#define IPA_v3_0_GROUP_DMA		(3)
#define IPA_v3_0_GROUP_IMM_CMD		IPA_v3_0_GROUP_UL
#define IPA_v3_0_GROUP_Q6ZIP		(4)
#define IPA_v3_0_GROUP_Q6ZIP_GENERAL	IPA_v3_0_GROUP_Q6ZIP
#define IPA_v3_0_GROUP_UC_RX_Q		(5)
#define IPA_v3_0_GROUP_Q6ZIP_ENGINE	IPA_v3_0_GROUP_UC_RX_Q
#define IPA_v3_0_GROUP_MAX		(6)

#define IPA_v3_5_GROUP_LWA_DL		(0) /* currently not used */
#define IPA_v3_5_MHI_GROUP_PCIE	IPA_v3_5_GROUP_LWA_DL
#define IPA_v3_5_GROUP_UL_DL		(1)
#define IPA_v3_5_MHI_GROUP_DDR		IPA_v3_5_GROUP_UL_DL
#define IPA_v3_5_MHI_GROUP_DMA		(2)
#define IPA_v3_5_GROUP_UC_RX_Q		(3) /* currently not used */
#define IPA_v3_5_SRC_GROUP_MAX		(4)
#define IPA_v3_5_DST_GROUP_MAX		(3)

#define IPA_v4_0_GROUP_LWA_DL		(0)
#define IPA_v4_0_MHI_GROUP_PCIE		(0)
#define IPA_v4_0_ETHERNET		(0)
#define IPA_v4_0_GROUP_UL_DL		(1)
#define IPA_v4_0_MHI_GROUP_DDR		(1)
#define IPA_v4_0_MHI_GROUP_DMA		(2)
#define IPA_v4_0_GROUP_UC_RX_Q		(3)
#define IPA_v4_0_SRC_GROUP_MAX		(4)
#define IPA_v4_0_DST_GROUP_MAX		(4)

#define IPA_GROUP_MAX IPA_v3_0_GROUP_MAX

enum ipa_rsrc_grp_type_src {
	IPA_v3_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDR_SECTORS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER,
	IPA_v3_0_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX,

	IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS = 0,
	IPA_v3_5_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_MAX,

	IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS = 0,
	IPA_v4_0_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX
};

#define IPA_RSRC_GRP_TYPE_SRC_MAX IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX

enum ipa_rsrc_grp_type_dst {
	IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTORS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_MAX,

	IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS = 0,
	IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v3_5_RSRC_GRP_TYPE_DST_MAX,

	IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS = 0,
	IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v4_0_RSRC_GRP_TYPE_DST_MAX,
};
#define IPA_RSRC_GRP_TYPE_DST_MAX IPA_v3_0_RSRC_GRP_TYPE_DST_MAX

enum ipa_rsrc_grp_type_rx {
	IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ,
	IPA_RSRC_GRP_TYPE_RX_MAX
};

enum ipa_rsrc_grp_rx_hps_weight_config {
	IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG,
	IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_MAX
};

struct rsrc_min_max {
	u32 min;
	u32 max;
};

enum ipa_ver {
	IPA_3_0,
	IPA_3_5,
	IPA_3_5_MHI,
	IPA_3_5_1,
	IPA_4_0,
	IPA_4_0_MHI,
	IPA_VER_MAX,
};

static const struct rsrc_min_max ipa3_rsrc_src_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_SRC_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/*UL	DL	DIAG	DMA	Not Used	uC Rx*/
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 255}, {3, 255}, {1, 255}, {1, 255}, {1, 255}, {2, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDR_SECTORS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5},  {0, 0}, {8, 8} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{19, 19}, {26, 26}, {3, 3}, {7, 7}, {0, 0}, {8, 8} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5}, {0, 0}, {8, 8} },
	},
	[IPA_3_5] = {
		/* LWA_DL  UL_DL    not used  UC_RX_Q, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{0, 0}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{0, 0}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{0, 0}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{0, 0}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE  DDR     DMA  not used, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{4, 4}, {5, 5}, {1, 1}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {12, 12}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {14, 14}, {14, 14}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_1] = {
		/* LWA_DL  UL_DL    not used  UC_RX_Q, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 255}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/* LWA_DL  UL_DL    not used  UC_RX_Q, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 255}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/* PCIE  DDR     DMA  not used, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{4, 4}, {5, 5}, {1, 1}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRS_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {12, 12}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {14, 14}, {14, 14}, {0, 0}, {0, 0}, {0, 0} },
	},
};

static const struct rsrc_min_max ipa3_rsrc_dst_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_DST_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL/DPL	DIAG	DMA  Q6zip_gen Q6zip_eng */
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{2, 2}, {3, 3}, {0, 0}, {2, 2}, {3, 3}, {3, 3} },
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {0, 0} },
	},
	[IPA_3_5] = {
		/* unused UL/DL/DPL unused N/A    N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE  DDR     DMA     N/A     N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL/DL/DPL unused N/A   N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/*LWA_DL UL/DL/DPL uC, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/*LWA_DL UL/DL/DPL uC, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 2}, {0, 0}, {0, 0} },
	},
};

static const struct rsrc_min_max ipa3_rsrc_rx_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_RX_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL	DIAG	DMA	Unused	uC Rx */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{16, 16}, {24, 24}, {8, 8}, {8, 8}, {0, 0}, {8, 8} },
	},
	[IPA_3_5] = {
		/* unused UL_DL	unused UC_RX_Q   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{0, 0}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{ 3, 3 }, { 7, 7 }, { 2, 2 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL_DL	unused   UC_RX_Q N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/* LWA_DL UL_DL	not used UC_RX_Q, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{ 3, 3 }, { 7, 7 }, { 2, 2 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
	},
};

static const u32 ipa3_rsrc_rx_grp_hps_weight_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL	DIAG	DMA	Unused	uC Rx */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 0, 0, 0, 0, 0, 0 },
	},
	[IPA_3_5] = {
		/* unused UL_DL	unused UC_RX_Q   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_3_5_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 3, 5, 1, 1, 0, 0 },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL_DL	unused   UC_RX_Q N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_4_0] = {
		/* LWA_DL UL_DL	not used UC_RX_Q, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_4_0_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 3, 5, 1, 1, 0, 0 },
	},
};

enum ipa_ees {
	IPA_EE_AP = 0,
	IPA_EE_Q6 = 1,
	IPA_EE_UC = 2,
};

struct ipa_ep_configuration {
	bool valid;
	int group_num;
	bool support_flt;
	int sequencer_type;
	u8 qmb_master_sel;
	struct ipa_gsi_ep_config ipa_gsi_ep_info;
};

/* clients not included in the list below are considered as invalid */
static const struct ipa_ep_configuration ipa3_ep_mapping
					[IPA_VER_MAX][IPA_CLIENT_MAX] = {
	[IPA_3_0][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 1, 8, 16, IPA_EE_UC } },
	[IPA_3_0][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 14, 11, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 16, 32, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_0_GROUP_IMM_CMD, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 22, 6, 18, 28, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 9, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 0, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 4, 8, 12, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_0_GROUP_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_CMD_PROD] = {
			true, IPA_v3_0_GROUP_IMM_CMD, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 18, 28, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_PROD]      = {
			true, IPA_v3_0_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 2, 0, 0, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_PROD]     = {
			true, IPA_v3_0_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 3, 0, 0, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_PCIE,
			{ 12, 9, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_PCIE,
			{ 13, 10, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_ETHERNET_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{2, 0, 8, 16, IPA_EE_UC} },
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 16, 32, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 9, 8, 16, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 10, 8, 16, IPA_EE_AP } },

	[IPA_3_0][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 4, 8, 8, IPA_EE_UC } },
	[IPA_3_0][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 4, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 13, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_WLAN4_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 14, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_0_GROUP_DPL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 2, 8, 12, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 8, 12, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 8, 8, 12, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 23, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 6, 8, 12, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 5, 8, 12, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_DUN_CONS]         = {
			true, IPA_v3_0_GROUP_DIAG, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 7, 4, 4, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_CONS] = {
			true, IPA_v3_0_GROUP_Q6ZIP, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 8, 4, 4, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_CONS] = {
			true, IPA_v3_0_GROUP_Q6ZIP, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 4, 9, 4, 4, IPA_EE_Q6 } },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 28, 13, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 14, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_ETHERNET_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{24, 3, 8, 8, IPA_EE_UC} },
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 4, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 13, 8, 8, IPA_EE_AP } },
	[IPA_3_0][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 14, 8, 8, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_0][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_3_5 */
	[IPA_3_5][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 8, 16, IPA_EE_UC } },
	[IPA_3_5][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_UC } },
	[IPA_3_5][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_3_5][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 } },
	/* Only for test purpose */
	[IPA_3_5][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{7, 8, 8, 16, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP } },

	[IPA_3_5][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 3, 8, 8, IPA_EE_UC } },
	[IPA_3_5][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 13, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 11, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 10, 4, 6, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 } },
	[IPA_3_5][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 } },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_3_5][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 11, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_5][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_3_5_MHI */
	[IPA_3_5_MHI][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_WAN_PROD]   = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v3_5_MHI_GROUP_PCIE, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 4, 10, 30, IPA_EE_Q6 } },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 } },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 8, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP } },
	/* Only for test purpose */
	[IPA_3_5_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST1_PROD]          = {
			0, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_PCIE, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{7, 8, 8, 16, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP } },

	[IPA_3_5_MHI][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 3, 8, 8, IPA_EE_UC } },
	[IPA_3_5_MHI][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 11, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 10, 4, 6, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 } },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 } },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP } },
	/* Only for test purpose */
	[IPA_3_5_MHI][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 11, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 8, 8, IPA_EE_AP } },
	[IPA_3_5_MHI][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_3_5_1 */
	[IPA_3_5_1][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 1, 8, 16, IPA_EE_UC } },
	[IPA_3_5_1][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 7, 8, 16, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_APPS_CMD_PROD]		= {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_3_5_1][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 4, 12, 30, IPA_EE_Q6 } },
	[IPA_3_5_1][IPA_CLIENT_Q6_CMD_PROD]	    = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 } },
	/* Only for test purpose */
	[IPA_3_5_1][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 } },
	[IPA_3_5_1][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_UC } },

	[IPA_3_5_1][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 3, 8, 8, IPA_EE_UC } },
	[IPA_3_5_1][IPA_CLIENT_WLAN2_CONS]          =  {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_WLAN3_CONS]          =  {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 2, 4, 6, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 } },
	[IPA_3_5_1][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 } },
	/* Only for test purpose */
	[IPA_3_5_1][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 8, 8, IPA_EE_AP } },
	[IPA_3_5_1][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 2, 4, 6, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5_1][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_4_0 */
	[IPA_4_0][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 24, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_ETHERNET_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 0, 8, 16, IPA_EE_UC } },
	[IPA_4_0][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 2, 12, 24, IPA_EE_Q6 } },
	[IPA_4_0][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_4_0][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 24, IPA_EE_Q6 } },
	/* Only for test purpose */
	[IPA_4_0][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{7, 9, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP } },


	[IPA_4_0][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 12, 6, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 14, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 15, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 5, 5, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 1, 17, 17, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 1, 17, 17, IPA_EE_UC } },
	[IPA_4_0][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 4, 9, 9, IPA_EE_Q6 } },
	[IPA_4_0][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 9, 9, IPA_EE_Q6 } },
	[IPA_4_0][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 5, 9, 9, IPA_EE_Q6 } },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_0][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 12, 2, 5, 5, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 5, 5, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 6, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 14, 9, 9, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 21, 15, 9, 9, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_0][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_4_0_MHI */
	[IPA_4_0_MHI][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_WAN_PROD]   = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 24, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 2, 12, 24, IPA_EE_Q6 } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 24, IPA_EE_Q6 } },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP } },
	/* Only for test purpose */
	[IPA_4_0_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{7, 9, 8, 16, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP } },

	[IPA_4_0_MHI][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 13, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 5, 5, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 1, 17, 17, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 4, 9, 9, IPA_EE_Q6 } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 9, 9, IPA_EE_Q6 } },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 20, 14, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 21, 15, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 5, 9, 9, IPA_EE_Q6 } },
	/* Only for test purpose */
	[IPA_4_0_MHI][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 12, 2, 5, 5, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 5, 5, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 6, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 14, 9, 9, IPA_EE_AP } },
	[IPA_4_0_MHI][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 21, 15, 9, 9, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_0_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },
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
		.num_paths = ARRAY_SIZE(ipa_init_vectors_v3_0),
		.vectors = ipa_init_vectors_v3_0,
	},
	{
		.num_paths = ARRAY_SIZE(ipa_nominal_perf_vectors_v3_0),
		.vectors = ipa_nominal_perf_vectors_v3_0,
	},
};

static struct msm_bus_scale_pdata ipa_bus_client_pdata_v3_0 = {
	.usecase = ipa_usecases_v3_0,
	.num_usecases = ARRAY_SIZE(ipa_usecases_v3_0),
	.name = "ipa",
};

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
	case IPA_RM_RESOURCE_ODU_ADAPT_CONS:
		clients->names[i++] = IPA_CLIENT_ODU_EMB_CONS;
		clients->names[i++] = IPA_CLIENT_ODU_TETH_CONS;
		break;
	case IPA_RM_RESOURCE_ETHERNET_CONS:
		clients->names[i++] = IPA_CLIENT_ETHERNET_CONS;
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
	case IPA_RM_RESOURCE_ODU_ADAPT_PROD:
		clients->names[i++] = IPA_CLIENT_ODU_PROD;
		break;
	case IPA_RM_RESOURCE_ETHERNET_PROD:
		clients->names[i++] = IPA_CLIENT_ETHERNET_PROD;
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

	/*
	 * starting IPA 4.0 pipe no longer can be suspended. Instead,
	 * the corresponding GSI channel should be stopped. Usually client
	 * driver will take care of stopping the channel. For client drivers
	 * that are not stopping the channel, IPA RM will do that based on
	 * ipa3_should_pipe_channel_be_stopped().
	 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		return false;

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_USB_CONS     ||
	    client == IPA_CLIENT_USB_DPL_CONS ||
	    client == IPA_CLIENT_MHI_CONS     ||
	    client == IPA_CLIENT_HSIC1_CONS   ||
	    client == IPA_CLIENT_WLAN1_CONS   ||
	    client == IPA_CLIENT_WLAN2_CONS   ||
	    client == IPA_CLIENT_WLAN3_CONS   ||
	    client == IPA_CLIENT_WLAN4_CONS   ||
	    client == IPA_CLIENT_ODU_EMB_CONS ||
	    client == IPA_CLIENT_ODU_TETH_CONS ||
	    client == IPA_CLIENT_ETHERNET_CONS)
		return true;

	return false;
}

/**
 * ipa3_should_pipe_channel_be_stopped() - returns true when the client's
 * channel should be stopped during a power save scenario. False otherwise.
 * Most client already stops the GSI channel on suspend, and are not included
 * in the list below.
 *
 * @client: [IN] IPA client
 */
static bool ipa3_should_pipe_channel_be_stopped(enum ipa_client_type client)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
		return false;

	ipa_ep_idx = ipa3_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		WARN_ON(1);
		return false;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_ODU_EMB_CONS ||
	    client == IPA_CLIENT_ODU_TETH_CONS)
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

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* Stop GSI channel */
				res = ipa3_stop_gsi_channel(ipa_ep_idx);
				if (res) {
					IPAERR("failed stop gsi ch %lu\n",
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
					return res;
				}
			}
		}
	}
	/* Sleep ~1 msec */
	if (pipe_suspended)
		usleep_range(1000, 2000);

	/* before gating IPA clocks do TAG process */
	ipa3_ctx->tag_process_before_gating = true;
	IPA_ACTIVE_CLIENTS_DEC_RESOURCE(ipa_rm_resource_str(resource));

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
	struct ipa_active_client_logging_info log_info;

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

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			res = -EPERM;
			goto bail;
		}
	}

	if (res == 0) {
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info,
				ipa_rm_resource_str(resource));
		/* before gating IPA clocks do TAG process */
		ipa3_ctx->tag_process_before_gating = true;
		ipa3_dec_client_disable_clks_no_block(&log_info);
	}
bail:
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

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				res = gsi_start_channel(
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
				if (res) {
					IPAERR("failed to start gsi ch %lu\n",
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
					return res;
				}
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
	struct ipahal_reg_shared_mem_size smem_sz;

	memset(&smem_sz, 0, sizeof(smem_sz));

	ipahal_read_reg_fields(IPA_SHARED_MEM_SIZE, &smem_sz);

	ipa3_ctx->smem_restricted_bytes = smem_sz.shared_mem_baddr;
	ipa3_ctx->smem_sz = smem_sz.shared_mem_sz;

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
 * ipa3_cfg_route() - configure IPA route
 * @route: IPA route
 *
 * Return codes:
 * 0: success
 */
int ipa3_cfg_route(struct ipahal_reg_route *route)
{

	IPADBG("disable_route_block=%d, default_pipe=%d, default_hdr_tbl=%d\n",
		route->route_dis,
		route->route_def_pipe,
		route->route_def_hdr_table);
	IPADBG("default_hdr_ofst=%d, default_frag_pipe=%d\n",
		route->route_def_hdr_ofst,
		route->route_frag_def_pipe);

	IPADBG("default_retain_hdr=%d\n",
		route->route_def_retain_hdr);

	if (route->route_dis) {
		IPAERR("Route disable is not supported!\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipahal_write_reg_fields(IPA_ROUTE, route);

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
	IPAERR_RL("Filter disable is not supported!\n");
	return -EPERM;
}

/**
 * ipa3_cfg_qsb() - Configure IPA QSB maximal reads and writes
 *
 * Returns:	None
 */
void ipa3_cfg_qsb(void)
{
	struct ipahal_reg_qsb_max_reads max_reads = { 0 };
	struct ipahal_reg_qsb_max_writes max_writes = { 0 };

	max_reads.qmb_0_max_reads = 8,
	max_reads.qmb_1_max_reads = 8,

	max_writes.qmb_0_max_writes = 8;
	max_writes.qmb_1_max_writes = 2;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5) {
		max_writes.qmb_1_max_writes = 4;
		max_reads.qmb_1_max_reads = 12;
	}

	ipahal_write_reg_fields(IPA_QSB_MAX_WRITES, &max_writes);
	ipahal_write_reg_fields(IPA_QSB_MAX_READS, &max_reads);
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
	u32 val;

	/* Read IPA version and make sure we have access to the registers */
	ipa_version = ipahal_read_reg(IPA_VERSION);
	if (ipa_version == 0)
		return -EFAULT;

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		val = IPA_BCR_REG_VAL_v3_0;
		break;
	case IPA_HW_v3_5:
	case IPA_HW_v3_5_1:
		val = IPA_BCR_REG_VAL_v3_5;
		break;
	case IPA_HW_v4_0:
		val = IPA_BCR_REG_VAL_v4_0;
		break;
	default:
		IPAERR("unknown HW type in dts\n");
		return -EFAULT;
	}

	ipahal_write_reg(IPA_BCR, val);

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		struct ipahal_reg_tx_cfg cfg;

		ipahal_write_reg(IPA_CLKON_CFG, IPA_CLKON_CFG_v4_0);
		ipahal_read_reg_fields(IPA_TX_CFG, &cfg);
		/* disable PA_MASK_EN to allow holb drop */
		cfg.pa_mask_en = 0;
		ipahal_write_reg_fields(IPA_TX_CFG, &cfg);
	}

	ipa3_cfg_qsb();

	return 0;
}

/**
 * ipa3_get_hw_type_index() - Get HW type index which is used as the entry index
 *	for ep\resource groups related arrays .
 *
 * Return value: HW type index
 */
u8 ipa3_get_hw_type_index(void)
{
	u8 hw_type_index;

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_index = IPA_3_0;
		break;
	case IPA_HW_v3_5:
		hw_type_index = IPA_3_5;
		/*
		 *this flag is initialized only after fw load trigger from
		 * user space (ipa3_write)
		 */
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_3_5_MHI;
		break;
	case IPA_HW_v3_5_1:
		hw_type_index = IPA_3_5_1;
		break;
	case IPA_HW_v4_0:
		hw_type_index = IPA_4_0;
		/*
		 *this flag is initialized only after fw load trigger from
		 * user space (ipa3_write)
		 */
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_4_0_MHI;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_index = IPA_3_0;
		break;
	}

	return hw_type_index;
}

/**
 * ipa3_get_ep_mapping() - provide endpoint mapping
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa3_get_ep_mapping(enum ipa_client_type client)
{
	int ipa_ep_idx;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR_RL("Bad client number! client =%d\n", client);
		return IPA_EP_NOT_ALLOCATED;
	}

	if (!ipa3_ep_mapping[ipa3_get_hw_type_index()][client].valid)
		return IPA_EP_NOT_ALLOCATED;

	ipa_ep_idx = ipa3_ep_mapping[ipa3_get_hw_type_index()][client].
		ipa_gsi_ep_info.ipa_ep_num;
	if (ipa_ep_idx < 0 || (ipa_ep_idx >= IPA3_MAX_NUM_PIPES
		&& client != IPA_CLIENT_DUMMY_CONS))
		return IPA_EP_NOT_ALLOCATED;

	return ipa_ep_idx;
}

/**
 * ipa3_get_gsi_ep_info() - provide gsi ep information
 * @client: IPA client value
 *
 * Return value: pointer to ipa_gsi_ep_info
 */
const struct ipa_gsi_ep_config *ipa3_get_gsi_ep_info
	(enum ipa_client_type client)
{
	int ep_idx;

	ep_idx = ipa3_get_ep_mapping(client);
	if (ep_idx == IPA_EP_NOT_ALLOCATED)
		return NULL;

	if (!ipa3_ep_mapping[ipa3_get_hw_type_index()][client].valid)
		return NULL;

	return &(ipa3_ep_mapping[ipa3_get_hw_type_index()]
		[client].ipa_gsi_ep_info);
}

/**
 * ipa_get_ep_group() - provide endpoint group by client
 * @client: client type
 *
 * Return value: endpoint group
 */
int ipa_get_ep_group(enum ipa_client_type client)
{
	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (!ipa3_ep_mapping[ipa3_get_hw_type_index()][client].valid)
		return -EINVAL;

	return ipa3_ep_mapping[ipa3_get_hw_type_index()][client].group_num;
}

/**
 * ipa3_get_qmb_master_sel() - provide QMB master selection for the client
 * @client: client type
 *
 * Return value: QMB master index
 */
u8 ipa3_get_qmb_master_sel(enum ipa_client_type client)
{
	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (!ipa3_ep_mapping[ipa3_get_hw_type_index()][client].valid)
		return -EINVAL;

	return ipa3_ep_mapping[ipa3_get_hw_type_index()]
		[client].qmb_master_sel;
}

/* ipa3_set_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */

void ipa3_set_client(int index, enum ipacm_client_enum client, bool uplink)
{
	if (client > IPACM_CLIENT_MAX || client < IPACM_CLIENT_USB) {
		IPAERR("Bad client number! client =%d\n", client);
	} else if (index >= IPA3_MAX_NUM_PIPES || index < 0) {
		IPAERR("Bad pipe index! index =%d\n", index);
	} else {
		ipa3_ctx->ipacm_client[index].client_enum = client;
		ipa3_ctx->ipacm_client[index].uplink = uplink;
	}
}

/* ipa3_get_wlan_stats() - get ipa wifi stats
 *
 * Return value: success or failure
 */
int ipa3_get_wlan_stats(struct ipa_get_wdi_sap_stats *wdi_sap_stats)
{
	if (ipa3_ctx->uc_wdi_ctx.stats_notify) {
		ipa3_ctx->uc_wdi_ctx.stats_notify(IPA_GET_WDI_SAP_STATS,
			wdi_sap_stats);
	} else {
		IPAERR("uc_wdi_ctx.stats_notify NULL\n");
		return -EFAULT;
	}
	return 0;
}

int ipa3_set_wlan_quota(struct ipa_set_wifi_quota *wdi_quota)
{
	if (ipa3_ctx->uc_wdi_ctx.stats_notify) {
		ipa3_ctx->uc_wdi_ctx.stats_notify(IPA_SET_WIFI_QUOTA,
			wdi_quota);
	} else {
		IPAERR("uc_wdi_ctx.stats_notify NULL\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * ipa3_get_client() - provide client mapping
 * @client: client type
 *
 * Return value: client mapping enum
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
	if (pipe_idx < 0 || pipe_idx >= IPA3_MAX_NUM_PIPES) {
		IPAERR("invalid pipe idx %d\n", pipe_idx);
		return false;
	}

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
	u8 hw_type_idx = ipa3_get_hw_type_index();
	u32 bitmap;
	u32 pipe_num;

	bitmap = 0;

	BUG_ON(ipa3_ctx->ep_flt_bitmap);

	for (cl = 0; cl < IPA_CLIENT_MAX ; cl++) {
		if (ipa3_ep_mapping[hw_type_idx][cl].support_flt) {
			pipe_num = ipa3_ep_mapping[hw_type_idx][cl].
				ipa_gsi_ep_info.ipa_ep_num;
			bitmap |= (1U << pipe_num);
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
 * ipa3_cfg_ep_seq() - IPA end-point HPS/DPS sequencer type configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_seq(u32 clnt_hdl, const struct ipa_ep_cfg_seq *seq_cfg)
{
	int type;

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

	if (seq_cfg->set_dynamic)
		type = seq_cfg->seq_type;
	else
		type = ipa3_ep_mapping[ipa3_get_hw_type_index()]
			[ipa3_ctx->ep[clnt_hdl].client].sequencer_type;

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
		ipahal_write_reg_n(IPA_ENDP_INIT_SEQ_n, clnt_hdl, type);

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
 * This includes nat, IPv6CT, header, mode, aggregation and route settings and
 * is a one shot API to configure the IPA end-point fully
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

		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
			result = ipa3_cfg_ep_conn_track(clnt_hdl,
				&ipa_ep_cfg->conn_track);
			if (result)
				return result;
		}

		result = ipa3_cfg_ep_mode(clnt_hdl, &ipa_ep_cfg->mode);
		if (result)
			return result;

		result = ipa3_cfg_ep_seq(clnt_hdl, &ipa_ep_cfg->seq);
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

static const char *ipa3_get_nat_en_str(enum ipa_nat_en_type nat_en)
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

static const char *ipa3_get_ipv6ct_en_str(enum ipa_ipv6ct_en_type ipv6ct_en)
{
	switch (ipv6ct_en) {
	case (IPA_BYPASS_IPV6CT):
		return "ipv6ct disabled";
	case (IPA_ENABLE_IPV6CT):
		return "ipv6ct enabled";
	}

	return "undefined";
}

/**
 * ipa3_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_nat:	[in] IPA NAT end-point configuration params
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

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_NAT_n, clnt_hdl, ep_nat);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_conn_track() - IPA end-point IPv6CT configuration
 * @clnt_hdl:		[in] opaque client handle assigned by IPA to client
 * @ep_conn_track:	[in] IPA IPv6CT end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_conn_track(u32 clnt_hdl,
	const struct ipa_ep_cfg_conn_track *ep_conn_track)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_conn_track == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("IPv6CT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d, conn_track_en=%d(%s)\n",
		clnt_hdl,
		ep_conn_track->conn_track_en,
		ipa3_get_ipv6ct_en_str(ep_conn_track->conn_track_en));

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.conn_track = *ep_conn_track;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CONN_TRACK_n, clnt_hdl,
		ep_conn_track);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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
int ipa3_cfg_ep_status(u32 clnt_hdl,
	const struct ipahal_reg_ep_cfg_status *ep_status)
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

	ipahal_write_reg_n_fields(IPA_ENDP_STATUS_n, clnt_hdl, ep_status);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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
	u8 qmb_master_sel;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || cfg == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.cfg = *cfg;

	/* Override QMB master selection */
	qmb_master_sel = ipa3_get_qmb_master_sel(ipa3_ctx->ep[clnt_hdl].client);
	ipa3_ctx->ep[clnt_hdl].cfg.cfg.gen_qmb_master_sel = qmb_master_sel;
	IPADBG(
	       "pipe=%d, frag_ofld_en=%d cs_ofld_en=%d mdata_hdr_ofst=%d gen_qmb_master_sel=%d\n",
			clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.frag_offload_en,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.cs_offload_en,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.cs_metadata_hdr_offset,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.gen_qmb_master_sel);

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CFG_n, clnt_hdl,
				  &ipa3_ctx->ep[clnt_hdl].cfg.cfg);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_METADATA_MASK_n,
		clnt_hdl, metadata_mask);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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
	IPADBG("pipe=%d metadata_reg_valid=%d\n",
		clnt_hdl,
		ep_hdr->hdr_metadata_reg_valid);

	IPADBG("remove_additional=%d, a5_mux=%d, ofst_pkt_size=0x%x\n",
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

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_n, clnt_hdl, &ep->cfg.hdr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_EXT_n, clnt_hdl,
		&ep->cfg.hdr_ext);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_ctrl() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes || ep_ctrl == NULL) {
		IPAERR("bad parm, clnt_hdl = %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0 && ep_ctrl->ipa_ep_suspend) {
		IPAERR("pipe suspend is not supported\n");
		WARN_ON(1);
		return -EPERM;
	}

	IPADBG("pipe=%d ep_suspend=%d, ep_delay=%d\n",
		clnt_hdl,
		ep_ctrl->ipa_ep_suspend,
		ep_ctrl->ipa_ep_delay);

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n, clnt_hdl, ep_ctrl);

	if (ep_ctrl->ipa_ep_suspend == true &&
			IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client))
		ipa3_suspend_active_aggr_wa(clnt_hdl);

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
	struct ipahal_reg_endp_init_mode init_mode;

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

	init_mode.dst_pipe_number = ipa3_ctx->ep[clnt_hdl].dst_pipe_index;
	init_mode.ep_mode = *ep_mode;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_MODE_n, clnt_hdl, &init_mode);

	 /* Configure sequencers type for test clients*/
	if (IPA_CLIENT_IS_TEST(ipa3_ctx->ep[clnt_hdl].client)) {
		if (ep_mode->mode == IPA_DMA)
			type = IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY;
		else
			type =
			   IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP;

		IPADBG(" set sequencers to sequance 0x%x, ep = %d\n", type,
				clnt_hdl);
		ipahal_write_reg_n(IPA_ENDP_INIT_SEQ_n, clnt_hdl, type);
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
	IPADBG("hard_byte_limit_en=%d aggr_sw_eof_active=%d\n",
		ep_aggr->aggr_hard_byte_limit_en,
		ep_aggr->aggr_sw_eof_active);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.aggr = *ep_aggr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_AGGR_n, clnt_hdl, ep_aggr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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
	struct ipahal_reg_endp_init_route init_rt;

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

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

		init_rt.route_table_index = ipa3_ctx->ep[clnt_hdl].rt_tbl_idx;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_ROUTE_n,
			clnt_hdl, &init_rt);

		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	}

	return 0;
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

	ipa3_ctx->ep[clnt_hdl].holb = *ep_holb;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n, clnt_hdl,
		ep_holb);

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n, clnt_hdl,
		ep_holb);

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

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_DEAGGR_n, clnt_hdl,
		&ep->cfg.deaggr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
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
	u32 qmap_id = 0;
	struct ipa_ep_cfg_metadata ep_md_reg_wrt;

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

	ep_md_reg_wrt = *ep_md;
	qmap_id = (ep_md->qmap_id <<
		IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT) &
		IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK;

	ep_md_reg_wrt.qmap_id = qmap_id;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_METADATA_n, clnt_hdl,
		&ep_md_reg_wrt);
	ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_metadata_reg_valid = 1;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_n, clnt_hdl,
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
		IPAERR_RL("bad parm client:%d\n", param_in->client);
		goto fail;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(param_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR_RL("Invalid client.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (!ep->valid) {
		IPAERR_RL("EP not allocated.\n");
		goto fail;
	}

	meta.qmap_id = param_in->qmap_id;
	if (param_in->client == IPA_CLIENT_USB_PROD ||
	    param_in->client == IPA_CLIENT_HSIC1_PROD ||
	    param_in->client == IPA_CLIENT_ODU_PROD ||
	    param_in->client == IPA_CLIENT_ETHERNET_PROD) {
		result = ipa3_cfg_ep_metadata(ipa_ep_idx, &meta);
	} else if (param_in->client == IPA_CLIENT_WLAN1_PROD) {
		ipa3_ctx->ep[ipa_ep_idx].cfg.meta = meta;
		result = ipa3_write_qmapid_wdi_pipe(ipa_ep_idx, meta.qmap_id);
		if (result)
			IPAERR_RL("qmap_id %d write failed on ep=%d\n",
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
 * ipa3_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa3_set_aggr_mode(enum ipa_aggr_mode mode)
{
	struct ipahal_reg_qcncm qcncm;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		if (mode != IPA_MBIM_AGGR) {
			IPAERR("Only MBIM mode is supported staring 4.0\n");
			return -EPERM;
		}
	} else {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		ipahal_read_reg_fields(IPA_QCNCM, &qcncm);
		qcncm.mode_en = mode;
		ipahal_write_reg_fields(IPA_QCNCM, &qcncm);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	}

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
	struct ipahal_reg_qcncm qcncm;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		IPAERR("QCNCM mode is not supported staring 4.0\n");
		return -EPERM;
	}

	if (sig == NULL) {
		IPAERR("bad argument for ipa3_set_qcncm_ndp_sig/n");
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipahal_read_reg_fields(IPA_QCNCM, &qcncm);
	qcncm.mode_val = ((sig[0] << 16) | (sig[1] << 8) | sig[2]);
	ipahal_write_reg_fields(IPA_QCNCM, &qcncm);
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
	struct ipahal_reg_single_ndp_mode mode;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		IPAERR("QCNCM mode is not supported staring 4.0\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipahal_read_reg_fields(IPA_SINGLE_NDP_MODE, &mode);
	mode.single_ndp_en = enable;
	ipahal_write_reg_fields(IPA_SINGLE_NDP_MODE, &mode);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_straddle_boundary() - Checks whether a memory buffer straddles a
 * boundary
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
 * ipa3_init_mem_partition() - Reads IPA memory map from DTS, performs alignment
 * checks and logs the fetched values.
 *
 * Returns:	0 on success
 */
int ipa3_init_mem_partition(struct device_node *node)
{
	const size_t ram_mmap_current_version_size =
		sizeof(ipa3_ctx->ctrl->mem_partition) / sizeof(u32);
	int result;

	memset(&ipa3_ctx->ctrl->mem_partition, 0,
		sizeof(ipa3_ctx->ctrl->mem_partition));

	IPADBG("Reading from DTS as u32 array\n");

	/*
	 * The size of ipa-ram-mmap array depends on the IPA version. The
	 * actual size can't be assumed because of possible DTS versions
	 * mismatch. The size of the array monotonically increasing because the
	 * obsolete entries are set to zero rather than deleted, so the
	 * possible sizes are in range
	 *	[1, ram_mmap_current_version_size]
	 */
	result = of_property_read_variable_u32_array(node, "qcom,ipa-ram-mmap",
		(u32 *)&ipa3_ctx->ctrl->mem_partition,
		1, ram_mmap_current_version_size);

	if (IPA_MEM_PART(uc_event_ring_ofst) & 1023) {
		IPAERR("UC EVENT RING OFST 0x%x is unaligned\n",
			IPA_MEM_PART(uc_event_ring_ofst));
		return -ENODEV;
	}

	IPADBG("UC EVENT RING OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(uc_event_ring_ofst),
		IPA_MEM_PART(uc_event_ring_size));

	IPADBG("NAT OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(nat_ofst),
		IPA_MEM_PART(nat_size));

	if (IPA_MEM_PART(uc_info_ofst) & 3) {
		IPAERR("UC INFO OFST 0x%x is unaligned\n",
			IPA_MEM_PART(uc_info_ofst));
		return -ENODEV;
	}

	IPADBG("UC INFO OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(uc_info_ofst), IPA_MEM_PART(uc_info_size));

	IPADBG("RAM OFST 0x%x\n", IPA_MEM_PART(ofst_start));

	if (IPA_MEM_PART(v4_flt_hash_ofst) & 7) {
		IPAERR("V4 FLT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_flt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_hash_ofst),
		IPA_MEM_PART(v4_flt_hash_size),
		IPA_MEM_PART(v4_flt_hash_size_ddr));

	if (IPA_MEM_PART(v4_flt_nhash_ofst) & 7) {
		IPAERR("V4 FLT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_flt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_nhash_ofst),
		IPA_MEM_PART(v4_flt_nhash_size),
		IPA_MEM_PART(v4_flt_nhash_size_ddr));

	if (IPA_MEM_PART(v6_flt_hash_ofst) & 7) {
		IPAERR("V6 FLT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_flt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_hash_ofst), IPA_MEM_PART(v6_flt_hash_size),
		IPA_MEM_PART(v6_flt_hash_size_ddr));

	if (IPA_MEM_PART(v6_flt_nhash_ofst) & 7) {
		IPAERR("V6 FLT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_flt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_nhash_ofst),
		IPA_MEM_PART(v6_flt_nhash_size),
		IPA_MEM_PART(v6_flt_nhash_size_ddr));

	IPADBG("V4 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v4_rt_num_index));

	IPADBG("V4 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_modem_rt_index_lo),
		IPA_MEM_PART(v4_modem_rt_index_hi));

	IPADBG("V4 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_apps_rt_index_lo),
		IPA_MEM_PART(v4_apps_rt_index_hi));

	if (IPA_MEM_PART(v4_rt_hash_ofst) & 7) {
		IPAERR("V4 RT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_rt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v4_rt_hash_ofst));

	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_hash_size),
		IPA_MEM_PART(v4_rt_hash_size_ddr));

	if (IPA_MEM_PART(v4_rt_nhash_ofst) & 7) {
		IPAERR("V4 RT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_rt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_ofst));

	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_size),
		IPA_MEM_PART(v4_rt_nhash_size_ddr));

	IPADBG("V6 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v6_rt_num_index));

	IPADBG("V6 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_modem_rt_index_lo),
		IPA_MEM_PART(v6_modem_rt_index_hi));

	IPADBG("V6 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_apps_rt_index_lo),
		IPA_MEM_PART(v6_apps_rt_index_hi));

	if (IPA_MEM_PART(v6_rt_hash_ofst) & 7) {
		IPAERR("V6 RT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_rt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v6_rt_hash_ofst));

	IPADBG("V6 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_hash_size),
		IPA_MEM_PART(v6_rt_hash_size_ddr));

	if (IPA_MEM_PART(v6_rt_nhash_ofst) & 7) {
		IPAERR("V6 RT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_rt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_ofst));

	IPADBG("V6 RT NON-HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_size),
		IPA_MEM_PART(v6_rt_nhash_size_ddr));

	if (IPA_MEM_PART(modem_hdr_ofst) & 7) {
		IPAERR("MODEM HDR OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_hdr_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM HDR OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_PART(modem_hdr_size));

	if (IPA_MEM_PART(apps_hdr_ofst) & 7) {
		IPAERR("APPS HDR OFST 0x%x is unaligned\n",
			IPA_MEM_PART(apps_hdr_ofst));
		return -ENODEV;
	}

	IPADBG("APPS HDR OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_ofst), IPA_MEM_PART(apps_hdr_size),
		IPA_MEM_PART(apps_hdr_size_ddr));

	if (IPA_MEM_PART(modem_hdr_proc_ctx_ofst) & 7) {
		IPAERR("MODEM HDR PROC CTX OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM HDR PROC CTX OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst),
		IPA_MEM_PART(modem_hdr_proc_ctx_size));

	if (IPA_MEM_PART(apps_hdr_proc_ctx_ofst) & 7) {
		IPAERR("APPS HDR PROC CTX OFST 0x%x is unaligned\n",
			IPA_MEM_PART(apps_hdr_proc_ctx_ofst));
		return -ENODEV;
	}

	IPADBG("APPS HDR PROC CTX OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_proc_ctx_ofst),
		IPA_MEM_PART(apps_hdr_proc_ctx_size),
		IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr));

	if (IPA_MEM_PART(pdn_config_ofst) & 7) {
		IPAERR("PDN CONFIG OFST 0x%x is unaligned\n",
			IPA_MEM_PART(pdn_config_ofst));
		return -ENODEV;
	}

	IPADBG("PDN CONFIG OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(pdn_config_ofst),
		IPA_MEM_PART(pdn_config_size));

	if (IPA_MEM_PART(modem_ofst) & 7) {
		IPAERR("MODEM OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(modem_ofst),
		IPA_MEM_PART(modem_size));

	IPADBG("V4 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_hash_ofst),
		IPA_MEM_PART(apps_v4_flt_hash_size));

	IPADBG("V4 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_nhash_ofst),
		IPA_MEM_PART(apps_v4_flt_nhash_size));

	IPADBG("V6 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_hash_ofst),
		IPA_MEM_PART(apps_v6_flt_hash_size));

	IPADBG("V6 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_nhash_ofst),
		IPA_MEM_PART(apps_v6_flt_nhash_size));

	IPADBG("RAM END OFST 0x%x\n",
		IPA_MEM_PART(end_ofst));

	IPADBG("V4 APPS HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_rt_hash_ofst),
		IPA_MEM_PART(apps_v4_rt_hash_size));

	IPADBG("V4 APPS NON-HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_rt_nhash_ofst),
		IPA_MEM_PART(apps_v4_rt_nhash_size));

	IPADBG("V6 APPS HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_rt_hash_ofst),
		IPA_MEM_PART(apps_v6_rt_hash_size));

	IPADBG("V6 APPS NON-HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_rt_nhash_ofst),
		IPA_MEM_PART(apps_v6_rt_nhash_size));

	return 0;
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
	if (hw_type >= IPA_HW_v4_0) {
		ctrl->ipa_clk_rate_turbo = IPA_V4_0_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V4_0_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V4_0_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V4_0_CLK_RATE_SVS2;
	} else if (hw_type >= IPA_HW_v3_5) {
		ctrl->ipa_clk_rate_turbo = IPA_V3_5_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V3_5_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V3_5_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V3_5_CLK_RATE_SVS2;
	} else {
		ctrl->ipa_clk_rate_turbo = IPA_V3_0_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V3_0_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V3_0_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V3_0_CLK_RATE_SVS2;
	}

	ctrl->ipa_init_rt4 = _ipa_init_rt4_v3;
	ctrl->ipa_init_rt6 = _ipa_init_rt6_v3;
	ctrl->ipa_init_flt4 = _ipa_init_flt4_v3;
	ctrl->ipa_init_flt6 = _ipa_init_flt6_v3;
	ctrl->ipa3_read_ep_reg = _ipa_read_ep_reg_v3_0;
	ctrl->ipa3_commit_flt = __ipa_commit_flt_v3;
	ctrl->ipa3_commit_rt = __ipa_commit_rt_v3;
	ctrl->ipa3_commit_hdr = __ipa_commit_hdr_v3_0;
	ctrl->ipa3_enable_clks = _ipa_enable_clks_v3_0;
	ctrl->ipa3_disable_clks = _ipa_disable_clks_v3_0;
	ctrl->msm_bus_data_ptr = &ipa_bus_client_pdata_v3_0;
	ctrl->clock_scaling_bw_threshold_svs =
		IPA_V3_0_BW_THRESHOLD_SVS_MBPS;
	ctrl->clock_scaling_bw_threshold_nominal =
		IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS;
	ctrl->clock_scaling_bw_threshold_turbo =
		IPA_V3_0_BW_THRESHOLD_TURBO_MBPS;
	ctrl->ipa_reg_base_ofst = ipahal_get_reg_base();
	ctrl->ipa_init_sram = _ipa_init_sram_v3;
	ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v3_0;
	ctrl->ipa_init_hdr = _ipa_init_hdr_v3_0;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		ctrl->ipa3_read_ep_reg = _ipa_read_ep_reg_v4_0;

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
	/* There is two groups of rule-Ids, Modem ones and Apps ones.
	 * Distinction by high bit: Modem Ids are high bit asserted.
	 */
	return idr_alloc(rule_ids, NULL,
		ipahal_get_low_rule_id(), ipahal_get_rule_id_hi_bit(),
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

void ipa3_tag_destroy_imm(void *user1, int user2)
{
	ipahal_destroy_imm_cmd(user1);
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
	struct ipahal_imm_cmd_ip_packet_init pktinit_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	struct ipahal_imm_cmd_ip_packet_tag_status status;
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
		return -ENOMEM;
	}

	/* Copy the required descriptors from the client now */
	if (desc) {
		memcpy(&(tag_desc[0]), desc, descs_num *
			sizeof(tag_desc[0]));
		desc_idx += descs_num;
	}

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	cmd_pyld = ipahal_construct_nop_imm_cmd(
		false, IPAHAL_FULL_PIPELINE_CLEAR, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct NOP imm cmd\n");
		res = -ENOMEM;
		goto fail_free_tag_desc;
	}
	tag_desc[desc_idx].opcode = cmd_pyld->opcode;
	tag_desc[desc_idx].pyld = cmd_pyld->data;
	tag_desc[desc_idx].len = cmd_pyld->len;
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
	tag_desc[desc_idx].user1 = cmd_pyld;
	desc_idx++;

	/* IP_PACKET_INIT IC for tag status to be sent to apps */
	pktinit_cmd.destination_pipe_index =
		ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_PACKET_INIT, &pktinit_cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct ip_packet_init imm cmd\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	tag_desc[desc_idx].opcode = cmd_pyld->opcode;
	tag_desc[desc_idx].pyld = cmd_pyld->data;
	tag_desc[desc_idx].len = cmd_pyld->len;
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
	tag_desc[desc_idx].user1 = cmd_pyld;
	desc_idx++;

	/* status IC */
	status.tag = IPA_COOKIE;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_PACKET_TAG_STATUS, &status, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct ip_packet_tag_status imm cmd\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	tag_desc[desc_idx].opcode = cmd_pyld->opcode;
	tag_desc[desc_idx].pyld = cmd_pyld->data;
	tag_desc[desc_idx].len = cmd_pyld->len;
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
	tag_desc[desc_idx].user1 = cmd_pyld;
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
		goto fail_free_comp;
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
		goto fail_free_comp;
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

	/*
	 * sleep for short period to ensure IPA wrote all packets to
	 * the transport
	 */
	usleep_range(IPA_TAG_SLEEP_MIN_USEC, IPA_TAG_SLEEP_MAX_USEC);

	return 0;

fail_free_comp:
	kfree(comp);
fail_free_desc:
	/*
	 * Free only the first descriptors allocated here.
	 * [nop, pkt_init, status, dummy_skb]
	 * The user is responsible to free his allocations
	 * in case of failure.
	 * The min is required because we may fail during
	 * of the initial allocations above
	 */
	for (i = descs_num;
		i < min(REQUIRED_TAG_PROCESS_DESCRIPTORS, desc_idx); i++)
		if (tag_desc[i].callback)
			tag_desc[i].callback(tag_desc[i].user1,
				tag_desc[i].user2);
fail_free_tag_desc:
	kfree(tag_desc);
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
	struct ipa_ep_cfg_aggr ep_aggr;
	int desc_idx = 0;
	int res;
	struct ipahal_imm_cmd_register_write reg_write_agg_close;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_reg_valmask valmask;

	for (i = start_pipe; i < end_pipe; i++) {
		ipahal_read_reg_n_fields(IPA_ENDP_INIT_AGGR_n, i, &ep_aggr);
		if (!ep_aggr.aggr_en)
			continue;
		IPADBG("Force close ep: %d\n", i);
		if (desc_idx + 1 > desc_size) {
			IPAERR("Internal error - no descriptors\n");
			res = -EFAULT;
			goto fail_no_desc;
		}

		reg_write_agg_close.skip_pipeline_clear = false;
		reg_write_agg_close.pipeline_clear_options =
			IPAHAL_FULL_PIPELINE_CLEAR;
		reg_write_agg_close.offset =
			ipahal_get_reg_ofst(IPA_AGGR_FORCE_CLOSE);
		ipahal_get_aggr_force_close_valmask(i, &valmask);
		reg_write_agg_close.value = valmask.val;
		reg_write_agg_close.value_mask = valmask.mask;
		cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
			&reg_write_agg_close, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct register_write imm cmd\n");
			res = -ENOMEM;
			goto fail_alloc_reg_write_agg_close;
		}

		desc[desc_idx].opcode = cmd_pyld->opcode;
		desc[desc_idx].pyld = cmd_pyld->data;
		desc[desc_idx].len = cmd_pyld->len;
		desc[desc_idx].type = IPA_IMM_CMD_DESC;
		desc[desc_idx].callback = ipa3_tag_destroy_imm;
		desc[desc_idx].user1 = cmd_pyld;
		desc_idx++;
	}

	return desc_idx;

fail_alloc_reg_write_agg_close:
	for (i = 0; i < desc_idx; i++)
		if (desc[desc_idx].callback)
			desc[desc_idx].callback(desc[desc_idx].user1,
				desc[desc_idx].user2);
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
	if (!ipa3_is_ready())
		return;

	mutex_lock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
	if (ipa3_ctx->q6_proxy_clk_vote_valid) {
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_valid = false;
	}
	mutex_unlock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
}

/**
 * ipa3_proxy_clk_vote() - called to add IPA clock proxy vote
 *
 * Return value: none
 */
void ipa3_proxy_clk_vote(void)
{
	if (!ipa3_is_ready())
		return;

	mutex_lock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
	if (!ipa3_ctx->q6_proxy_clk_vote_valid) {
		IPA_ACTIVE_CLIENTS_INC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_valid = true;
	}
	mutex_unlock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
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
 * ipa3_get_transport_type()
 *
 * Return value: enum ipa_transport_type
 */
enum ipa_transport_type ipa3_get_transport_type(void)
{
	return IPA_TRANSPORT_TYPE_GSI;
}

u32 ipa3_get_num_pipes(void)
{
	return ipahal_read_reg(IPA_ENABLED_PIPES);
}

/**
 * ipa3_disable_apps_wan_cons_deaggr()-
 * set ipa_ctx->ipa_client_apps_wan_cons_agg_gro
 *
 * Return value: 0 or negative in case of failure
 */
int ipa3_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count)
{
	int res = -1;
	u32 limit;

	/* checking if IPA-HW can support */
	limit = ipahal_aggr_get_max_byte_limit();
	if ((agg_size >> 10) > limit) {
		IPAERR("IPA-AGG byte limit %d\n", limit);
		IPAERR("exceed aggr_byte_limit\n");
		return res;
	}
	limit = ipahal_aggr_get_max_pkt_limit();
	if (agg_count > limit) {
		IPAERR("IPA-AGG pkt limit %d\n", limit);
		IPAERR("exceed aggr_pkt_limit\n");
		return res;
	}

	if (ipa3_ctx) {
		ipa3_ctx->ipa_client_apps_wan_cons_agg_gro = true;
		return 0;
	}
	return res;
}

static void *ipa3_get_ipc_logbuf(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->logbuf;

	return NULL;
}

static void *ipa3_get_ipc_logbuf_low(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->logbuf_low;

	return NULL;
}

static void ipa3_get_holb(int ep_idx, struct ipa_ep_cfg_holb *holb)
{
	*holb = ipa3_ctx->ep[ep_idx].holb;
}

static void ipa3_set_tag_process_before_gating(bool val)
{
	ipa3_ctx->tag_process_before_gating = val;
}

int ipa3_bind_api_controller(enum ipa_hw_type ipa_hw_type,
	struct ipa_api_controller *api_ctrl)
{
	if (ipa_hw_type < IPA_HW_v3_0) {
		IPAERR("Unsupported IPA HW version %d\n", ipa_hw_type);
		WARN_ON(1);
		return -EPERM;
	}

	api_ctrl->ipa_connect = NULL;
	api_ctrl->ipa_disconnect = NULL;
	api_ctrl->ipa_reset_endpoint = NULL;
	api_ctrl->ipa_clear_endpoint_delay = ipa3_clear_endpoint_delay;
	api_ctrl->ipa_disable_endpoint = NULL;
	api_ctrl->ipa_cfg_ep = ipa3_cfg_ep;
	api_ctrl->ipa_cfg_ep_nat = ipa3_cfg_ep_nat;
	api_ctrl->ipa_cfg_ep_conn_track = ipa3_cfg_ep_conn_track;
	api_ctrl->ipa_cfg_ep_hdr = ipa3_cfg_ep_hdr;
	api_ctrl->ipa_cfg_ep_hdr_ext = ipa3_cfg_ep_hdr_ext;
	api_ctrl->ipa_cfg_ep_mode = ipa3_cfg_ep_mode;
	api_ctrl->ipa_cfg_ep_aggr = ipa3_cfg_ep_aggr;
	api_ctrl->ipa_cfg_ep_deaggr = ipa3_cfg_ep_deaggr;
	api_ctrl->ipa_cfg_ep_route = ipa3_cfg_ep_route;
	api_ctrl->ipa_cfg_ep_holb = ipa3_cfg_ep_holb;
	api_ctrl->ipa_get_holb = ipa3_get_holb;
	api_ctrl->ipa_set_tag_process_before_gating =
			ipa3_set_tag_process_before_gating;
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
	api_ctrl->ipa_broadcast_wdi_quota_reach_ind =
			ipa3_broadcast_wdi_quota_reach_ind;
	api_ctrl->ipa_uc_wdi_get_dbpa = ipa3_uc_wdi_get_dbpa;
	api_ctrl->ipa_uc_reg_rdyCB = ipa3_uc_reg_rdyCB;
	api_ctrl->ipa_uc_dereg_rdyCB = ipa3_uc_dereg_rdyCB;
	api_ctrl->teth_bridge_init = ipa3_teth_bridge_init;
	api_ctrl->teth_bridge_disconnect = ipa3_teth_bridge_disconnect;
	api_ctrl->teth_bridge_connect = ipa3_teth_bridge_connect;
	api_ctrl->ipa_set_client = ipa3_set_client;
	api_ctrl->ipa_get_client = ipa3_get_client;
	api_ctrl->ipa_get_client_uplink = ipa3_get_client_uplink;
	api_ctrl->ipa_dma_init = ipa3_dma_init;
	api_ctrl->ipa_dma_enable = ipa3_dma_enable;
	api_ctrl->ipa_dma_disable = ipa3_dma_disable;
	api_ctrl->ipa_dma_sync_memcpy = ipa3_dma_sync_memcpy;
	api_ctrl->ipa_dma_async_memcpy = ipa3_dma_async_memcpy;
	api_ctrl->ipa_dma_uc_memcpy = ipa3_dma_uc_memcpy;
	api_ctrl->ipa_dma_destroy = ipa3_dma_destroy;
	api_ctrl->ipa_mhi_init_engine = ipa3_mhi_init_engine;
	api_ctrl->ipa_connect_mhi_pipe = ipa3_connect_mhi_pipe;
	api_ctrl->ipa_disconnect_mhi_pipe = ipa3_disconnect_mhi_pipe;
	api_ctrl->ipa_mhi_stop_gsi_channel = ipa3_mhi_stop_gsi_channel;
	api_ctrl->ipa_uc_mhi_reset_channel = ipa3_uc_mhi_reset_channel;
	api_ctrl->ipa_qmi_enable_force_clear_datapath_send =
			ipa3_qmi_enable_force_clear_datapath_send;
	api_ctrl->ipa_qmi_disable_force_clear_datapath_send =
			ipa3_qmi_disable_force_clear_datapath_send;
	api_ctrl->ipa_mhi_reset_channel_internal =
			ipa3_mhi_reset_channel_internal;
	api_ctrl->ipa_mhi_start_channel_internal =
			ipa3_mhi_start_channel_internal;
	api_ctrl->ipa_mhi_query_ch_info = ipa3_mhi_query_ch_info;
	api_ctrl->ipa_mhi_resume_channels_internal =
			ipa3_mhi_resume_channels_internal;
	api_ctrl->ipa_has_open_aggr_frame = ipa3_has_open_aggr_frame;
	api_ctrl->ipa_mhi_destroy_channel = ipa3_mhi_destroy_channel;
	api_ctrl->ipa_uc_mhi_send_dl_ul_sync_info =
			ipa3_uc_mhi_send_dl_ul_sync_info;
	api_ctrl->ipa_uc_mhi_init = ipa3_uc_mhi_init;
	api_ctrl->ipa_uc_mhi_suspend_channel = ipa3_uc_mhi_suspend_channel;
	api_ctrl->ipa_uc_mhi_stop_event_update_channel =
			ipa3_uc_mhi_stop_event_update_channel;
	api_ctrl->ipa_uc_mhi_cleanup = ipa3_uc_mhi_cleanup;
	api_ctrl->ipa_uc_state_check = ipa3_uc_state_check;
	api_ctrl->ipa_write_qmap_id = ipa3_write_qmap_id;
	api_ctrl->ipa_add_interrupt_handler = ipa3_add_interrupt_handler;
	api_ctrl->ipa_remove_interrupt_handler = ipa3_remove_interrupt_handler;
	api_ctrl->ipa_restore_suspend_handler = ipa3_restore_suspend_handler;
	api_ctrl->ipa_bam_reg_dump = NULL;
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
	api_ctrl->ipa_get_dma_dev = ipa3_get_dma_dev;
	api_ctrl->ipa_release_wdi_mapping = ipa3_release_wdi_mapping;
	api_ctrl->ipa_create_wdi_mapping = ipa3_create_wdi_mapping;
	api_ctrl->ipa_get_gsi_ep_info = ipa3_get_gsi_ep_info;
	api_ctrl->ipa_stop_gsi_channel = ipa3_stop_gsi_channel;
	api_ctrl->ipa_start_gsi_channel = ipa3_start_gsi_channel;
	api_ctrl->ipa_register_ipa_ready_cb = ipa3_register_ipa_ready_cb;
	api_ctrl->ipa_inc_client_enable_clks = ipa3_inc_client_enable_clks;
	api_ctrl->ipa_dec_client_disable_clks = ipa3_dec_client_disable_clks;
	api_ctrl->ipa_inc_client_enable_clks_no_block =
		ipa3_inc_client_enable_clks_no_block;
	api_ctrl->ipa_suspend_resource_no_block =
		ipa3_suspend_resource_no_block;
	api_ctrl->ipa_resume_resource = ipa3_resume_resource;
	api_ctrl->ipa_suspend_resource_sync = ipa3_suspend_resource_sync;
	api_ctrl->ipa_set_required_perf_profile =
		ipa3_set_required_perf_profile;
	api_ctrl->ipa_get_ipc_logbuf = ipa3_get_ipc_logbuf;
	api_ctrl->ipa_get_ipc_logbuf_low = ipa3_get_ipc_logbuf_low;
	api_ctrl->ipa_rx_poll = ipa3_rx_poll;
	api_ctrl->ipa_recycle_wan_skb = ipa3_recycle_wan_skb;
	api_ctrl->ipa_setup_uc_ntn_pipes = ipa3_setup_uc_ntn_pipes;
	api_ctrl->ipa_tear_down_uc_offload_pipes =
		ipa3_tear_down_uc_offload_pipes;
	api_ctrl->ipa_get_pdev = ipa3_get_pdev;
	api_ctrl->ipa_ntn_uc_reg_rdyCB = ipa3_ntn_uc_reg_rdyCB;
	api_ctrl->ipa_ntn_uc_dereg_rdyCB = ipa3_ntn_uc_dereg_rdyCB;
	api_ctrl->ipa_conn_wdi3_pipes = ipa3_conn_wdi3_pipes;
	api_ctrl->ipa_disconn_wdi3_pipes = ipa3_disconn_wdi3_pipes;
	api_ctrl->ipa_enable_wdi3_pipes = ipa3_enable_wdi3_pipes;
	api_ctrl->ipa_disable_wdi3_pipes = ipa3_disable_wdi3_pipes;

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
			enum ipa_rsrc_grp_type_src n, bool src,
			struct ipahal_reg_rsrc_grp_cfg *val) {
	u8 hw_type_idx;

	hw_type_idx = ipa3_get_hw_type_index();

	switch (hw_type_idx) {
	case IPA_3_0:
		if (src) {
			switch (group_index) {
			case IPA_v3_0_GROUP_UL:
			case IPA_v3_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_DIAG:
			case IPA_v3_0_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_Q6ZIP:
			case IPA_v3_0_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v3_0_GROUP_UL:
			case IPA_v3_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_DIAG:
			case IPA_v3_0_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_Q6ZIP_GENERAL:
			case IPA_v3_0_GROUP_Q6ZIP_ENGINE:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_3_5:
	case IPA_3_5_MHI:
	case IPA_3_5_1:
		if (src) {
			switch (group_index) {
			case IPA_v3_5_GROUP_LWA_DL:
			case IPA_v3_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_5_MHI_GROUP_DMA:
			case IPA_v3_5_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v3_5_GROUP_LWA_DL:
			case IPA_v3_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_5_MHI_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_0:
	case IPA_4_0_MHI:
		if (src) {
			switch (group_index) {
			case IPA_v4_0_GROUP_LWA_DL:
			case IPA_v4_0_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_0_MHI_GROUP_DMA:
			case IPA_v4_0_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_0_GROUP_LWA_DL:
			case IPA_v4_0_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_0_MHI_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	default:
		IPAERR("invalid hw type\n");
		WARN_ON(1);
		return;
	}
}

static void ipa3_configure_rx_hps_clients(int depth, bool min)
{
	int i;
	struct ipahal_reg_rx_hps_clients val;
	u8 hw_type_idx;

	hw_type_idx = ipa3_get_hw_type_index();

	/*
	 * depth 0 contains 4 first clients out of 6
	 * depth 1 contains 2 last clients out of 6
	 */
	for (i = 0 ; i < (depth ? 2 : 4) ; i++) {
		if (min)
			val.client_minmax[i] =
				ipa3_rsrc_rx_grp_config
				[hw_type_idx]
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[!depth ? i : 4 + i].min;
		else
			val.client_minmax[i] =
				ipa3_rsrc_rx_grp_config
				[hw_type_idx]
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[!depth ? i : 4 + i].max;
	}
	if (depth) {
		ipahal_write_reg_fields(min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_1 :
					IPA_RX_HPS_CLIENTS_MAX_DEPTH_1,
					&val);
	} else {
		ipahal_write_reg_fields(min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_0 :
					IPA_RX_HPS_CLIENTS_MAX_DEPTH_0,
					&val);
	}
}

static void ipa3_configure_rx_hps_weight(void)
{
	struct ipahal_reg_rx_hps_weights val;
	u8 hw_type_idx;

	hw_type_idx = ipa3_get_hw_type_index();

	val.hps_queue_weight_0 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[0];
	val.hps_queue_weight_1 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[1];
	val.hps_queue_weight_2 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[2];
	val.hps_queue_weight_3 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[3];

	ipahal_write_reg_fields(IPA_HPS_FTCH_ARB_QUEUE_WEIGHT, &val);
}

void ipa3_set_resorce_groups_min_max_limits(void)
{
	int i;
	int j;
	int src_rsrc_type_max;
	int dst_rsrc_type_max;
	int src_grp_idx_max;
	int dst_grp_idx_max;
	struct ipahal_reg_rsrc_grp_cfg val;
	u8 hw_type_idx;

	IPADBG("ENTER\n");
	IPADBG("Assign source rsrc groups min-max limits\n");

	hw_type_idx = ipa3_get_hw_type_index();
	switch (hw_type_idx) {
	case IPA_3_0:
		src_rsrc_type_max = IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v3_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v3_0_GROUP_MAX;
		dst_grp_idx_max = IPA_v3_0_GROUP_MAX;
		break;
	case IPA_3_5:
	case IPA_3_5_MHI:
	case IPA_3_5_1:
		src_rsrc_type_max = IPA_v3_5_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v3_5_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v3_5_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v3_5_DST_GROUP_MAX;
		break;
	case IPA_4_0:
	case IPA_4_0_MHI:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_0_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_0_DST_GROUP_MAX;
		break;
	default:
		IPAERR("invalid hw type index\n");
		WARN_ON(1);
		return;
	}

	for (i = 0; i < src_rsrc_type_max; i++) {
		for (j = 0; j < src_grp_idx_max; j = j + 2) {
			val.x_min =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j].min;
			val.x_max =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j].max;
			val.y_min =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j + 1].min;
			val.y_max =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j + 1].max;
			ipa3_write_rsrc_grp_type_reg(j, i, true, &val);
		}
	}

	IPADBG("Assign destination rsrc groups min-max limits\n");

	for (i = 0; i < dst_rsrc_type_max; i++) {
		for (j = 0; j < dst_grp_idx_max; j = j + 2) {
			val.x_min =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j].min;
			val.x_max =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j].max;
			val.y_min =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j + 1].min;
			val.y_max =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j + 1].max;
			ipa3_write_rsrc_grp_type_reg(j, i, false, &val);
		}
	}

	/* move resource group configuration from HLOS to TZ */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_1) {
		IPAERR("skip configuring ipa_rx_hps_clients from HLOS\n");
		return;
	}

	IPADBG("Assign RX_HPS CMDQ rsrc groups min-max limits\n");

	ipa3_configure_rx_hps_clients(0, true);
	ipa3_configure_rx_hps_clients(0, false);

	/* only hw_type v3_0\3_1 have 6 RX_HPS_CMDQ and needs depth 1*/
	if (ipa3_ctx->ipa_hw_type <= IPA_HW_v3_1) {
		ipa3_configure_rx_hps_clients(1, true);
		ipa3_configure_rx_hps_clients(1, false);
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5)
		ipa3_configure_rx_hps_weight();

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
		if (!atomic_read(&ep->sys->curr_polling_state))
			__ipa_gsi_irq_rx_scedule_poll(ep->sys);
	}
}

void ipa3_suspend_apps_pipes(bool suspend)
{
	struct ipa_ep_cfg_ctrl cfg;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	int res;

	memset(&cfg, 0, sizeof(cfg));
	cfg.ipa_ep_suspend = suspend;

	ipa_ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	if (ipa_ep_idx < 0) {
		IPAERR("IPA client mapping failed\n");
		ipa_assert();
		return;
	}
	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid) {
		IPADBG("%s pipe %d\n", suspend ? "suspend" : "unsuspend",
			ipa_ep_idx);
		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
			if (suspend) {
				res = ipa3_stop_gsi_channel(ipa_ep_idx);
				if (res) {
					IPAERR("failed to stop LAN channel\n");
					ipa_assert();
				}
			} else {
				res = gsi_start_channel(ep->gsi_chan_hdl);
				if (res) {
					IPAERR("failed to start LAN channel\n");
					ipa_assert();
				}
			}
		} else {
			ipa3_cfg_ep_ctrl(ipa_ep_idx, &cfg);
		}
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
		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
			if (suspend) {
				res = ipa3_stop_gsi_channel(ipa_ep_idx);
				if (res) {
					IPAERR("failed to stop WAN channel\n");
					ipa_assert();
				}
			} else {
				res = gsi_start_channel(ep->gsi_chan_hdl);
				if (res) {
					IPAERR("failed to start WAN channel\n");
					ipa_assert();
				}
			}
		} else {
			ipa3_cfg_ep_ctrl(ipa_ep_idx, &cfg);
		}
		if (suspend)
			ipa3_gsi_poll_after_suspend(ep);
		else if (!atomic_read(&ep->sys->curr_polling_state))
			gsi_config_channel_mode(ep->gsi_chan_hdl,
				GSI_CHAN_MODE_CALLBACK);
	}
}

int ipa3_allocate_dma_task_for_gsi(void)
{
	struct ipahal_imm_cmd_dma_task_32b_addr cmd = { 0 };

	IPADBG("Allocate mem\n");
	ipa3_ctx->dma_task_info.mem.size = IPA_GSI_CHANNEL_STOP_PKT_SIZE;
	ipa3_ctx->dma_task_info.mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		ipa3_ctx->dma_task_info.mem.size,
		&ipa3_ctx->dma_task_info.mem.phys_base,
		GFP_KERNEL);
	if (!ipa3_ctx->dma_task_info.mem.base) {
		IPAERR("no mem\n");
		return -EFAULT;
	}

	cmd.flsh = 1;
	cmd.size1 = ipa3_ctx->dma_task_info.mem.size;
	cmd.addr1 = ipa3_ctx->dma_task_info.mem.phys_base;
	cmd.packet_size = ipa3_ctx->dma_task_info.mem.size;
	ipa3_ctx->dma_task_info.cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_TASK_32B_ADDR, &cmd, false);
	if (!ipa3_ctx->dma_task_info.cmd_pyld) {
		IPAERR("failed to construct dma_task_32b_addr cmd\n");
		dma_free_coherent(ipa3_ctx->pdev,
			ipa3_ctx->dma_task_info.mem.size,
			ipa3_ctx->dma_task_info.mem.base,
			ipa3_ctx->dma_task_info.mem.phys_base);
		memset(&ipa3_ctx->dma_task_info, 0,
			sizeof(ipa3_ctx->dma_task_info));
		return -EFAULT;
	}

	return 0;
}

void ipa3_free_dma_task_for_gsi(void)
{
	dma_free_coherent(ipa3_ctx->pdev,
		ipa3_ctx->dma_task_info.mem.size,
		ipa3_ctx->dma_task_info.mem.base,
		ipa3_ctx->dma_task_info.mem.phys_base);
	ipahal_destroy_imm_cmd(ipa3_ctx->dma_task_info.cmd_pyld);
	memset(&ipa3_ctx->dma_task_info, 0, sizeof(ipa3_ctx->dma_task_info));
}

/**
 * ipa3_inject_dma_task_for_gsi()- Send DMA_TASK to IPA for GSI stop channel
 *
 * Send a DMA_TASK of 1B to IPA to unblock GSI channel in STOP_IN_PROG.
 * Return value: 0 on success, negative otherwise
 */
int ipa3_inject_dma_task_for_gsi(void)
{
	struct ipa3_desc desc = {0};

	desc.opcode = ipa3_ctx->dma_task_info.cmd_pyld->opcode;
	desc.pyld = ipa3_ctx->dma_task_info.cmd_pyld->data;
	desc.len = ipa3_ctx->dma_task_info.cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

	IPADBG("sending 1B packet to IPA\n");
	if (ipa3_send_cmd_timeout(1, &desc,
		IPA_DMA_TASK_FOR_GSI_TIMEOUT_MSEC)) {
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
	struct ipa_mem_buffer mem;
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
		IPADBG("Calling gsi_stop_channel ch:%lu\n",
			ep->gsi_chan_hdl);
		res = gsi_stop_channel(ep->gsi_chan_hdl);
		IPADBG("gsi_stop_channel ch: %lu returned %d\n",
			ep->gsi_chan_hdl, res);
		goto end_sequence;
	}

	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		IPADBG("Calling gsi_stop_channel ch:%lu\n",
			ep->gsi_chan_hdl);
		res = gsi_stop_channel(ep->gsi_chan_hdl);
		IPADBG("gsi_stop_channel ch: %lu returned %d\n",
			ep->gsi_chan_hdl, res);
		if (res != -GSI_STATUS_AGAIN && res != -GSI_STATUS_TIMED_OUT)
			goto end_sequence;

		IPADBG("Inject a DMA_TASK with 1B packet to IPA\n");
		/* Send a 1B packet DMA_TASK to IPA and try again */
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

static int ipa3_load_single_fw(const struct firmware *firmware,
	const struct elf32_phdr *phdr)
{
	uint32_t *fw_mem_base;
	int index;
	const uint32_t *elf_data_ptr;

	if (phdr->p_offset > firmware->size) {
		IPAERR("Invalid ELF: offset=%u is beyond elf_size=%zu\n",
			phdr->p_offset, firmware->size);
		return -EINVAL;
	}
	if ((firmware->size - phdr->p_offset) < phdr->p_filesz) {
		IPAERR("Invalid ELF: offset=%u filesz=%u elf_size=%zu\n",
			phdr->p_offset, phdr->p_filesz, firmware->size);
		return -EINVAL;
	}

	if (phdr->p_memsz % sizeof(uint32_t)) {
		IPAERR("FW mem size %u doesn't align to 32bit\n",
			phdr->p_memsz);
		return -EFAULT;
	}

	if (phdr->p_filesz > phdr->p_memsz) {
		IPAERR("FW image too big src_size=%u dst_size=%u\n",
			phdr->p_filesz, phdr->p_memsz);
		return -EFAULT;
	}

	fw_mem_base = ioremap(phdr->p_vaddr, phdr->p_memsz);
	if (!fw_mem_base) {
		IPAERR("Failed to map 0x%x for the size of %u\n",
			phdr->p_vaddr, phdr->p_memsz);
		return -ENOMEM;
	}

	/* Set the entire region to 0s */
	memset(fw_mem_base, 0, phdr->p_memsz);

	elf_data_ptr = (uint32_t *)(firmware->data + phdr->p_offset);

	/* Write the FW */
	for (index = 0; index < phdr->p_filesz/sizeof(uint32_t); index++) {
		writel_relaxed(*elf_data_ptr, &fw_mem_base[index]);
		elf_data_ptr++;
	}

	iounmap(fw_mem_base);

	return 0;
}

/**
 * ipa3_load_fws() - Load the IPAv3 FWs into IPA&GSI SRAM.
 *
 * @firmware: Structure which contains the FW data from the user space.
 * @gsi_mem_base: GSI base address
 *
 * Return value: 0 on success, negative otherwise
 *
 */
int ipa3_load_fws(const struct firmware *firmware, phys_addr_t gsi_mem_base)
{
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	unsigned long gsi_iram_ofst;
	unsigned long gsi_iram_size;
	phys_addr_t ipa_reg_mem_base;
	u32 ipa_reg_ofst;
	int rc;

	if (!gsi_mem_base) {
		IPAERR("Invalid GSI base address\n");
		return -EINVAL;
	}

	ipa_assert_on(!firmware);
	/* One program header per FW image: GSI, DPS and HPS */
	if (firmware->size < (sizeof(*ehdr) + 3 * sizeof(*phdr))) {
		IPAERR("Missing ELF and Program headers firmware size=%zu\n",
			firmware->size);
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *) firmware->data;
	ipa_assert_on(!ehdr);
	if (ehdr->e_phnum != 3) {
		IPAERR("Unexpected number of ELF program headers\n");
		return -EINVAL;
	}
	phdr = (struct elf32_phdr *)(firmware->data + sizeof(*ehdr));

	/*
	 * Each ELF program header represents a FW image and contains:
	 *  p_vaddr : The starting address to which the FW needs to loaded.
	 *  p_memsz : The size of the IRAM (where the image loaded)
	 *  p_filesz: The size of the FW image embedded inside the ELF
	 *  p_offset: Absolute offset to the image from the head of the ELF
	 */

	/* Load GSI FW image */
	gsi_get_inst_ram_offset_and_size(&gsi_iram_ofst, &gsi_iram_size);
	if (phdr->p_vaddr != (gsi_mem_base + gsi_iram_ofst)) {
		IPAERR(
			"Invalid GSI FW img load addr vaddr=0x%x gsi_mem_base=%pa gsi_iram_ofst=0x%lx\n"
			, phdr->p_vaddr, &gsi_mem_base, gsi_iram_ofst);
		return -EINVAL;
	}
	if (phdr->p_memsz > gsi_iram_size) {
		IPAERR("Invalid GSI FW img size memsz=%d gsi_iram_size=%lu\n",
			phdr->p_memsz, gsi_iram_size);
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	phdr++;
	ipa_reg_mem_base = ipa3_ctx->ipa_wrapper_base + ipahal_get_reg_base();

	/* Load IPA DPS FW image */
	ipa_reg_ofst = ipahal_get_reg_ofst(IPA_DPS_SEQUENCER_FIRST);
	if (phdr->p_vaddr != (ipa_reg_mem_base + ipa_reg_ofst)) {
		IPAERR(
			"Invalid IPA DPS img load addr vaddr=0x%x ipa_reg_mem_base=%pa ipa_reg_ofst=%u\n"
			, phdr->p_vaddr, &ipa_reg_mem_base, ipa_reg_ofst);
		return -EINVAL;
	}
	if (phdr->p_memsz > ipahal_get_dps_img_mem_size()) {
		IPAERR("Invalid IPA DPS img size memsz=%d dps_mem_size=%u\n",
			phdr->p_memsz, ipahal_get_dps_img_mem_size());
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	phdr++;

	/* Load IPA HPS FW image */
	ipa_reg_ofst = ipahal_get_reg_ofst(IPA_HPS_SEQUENCER_FIRST);
	if (phdr->p_vaddr != (ipa_reg_mem_base + ipa_reg_ofst)) {
		IPAERR(
			"Invalid IPA HPS img load addr vaddr=0x%x ipa_reg_mem_base=%pa ipa_reg_ofst=%u\n"
			, phdr->p_vaddr, &ipa_reg_mem_base, ipa_reg_ofst);
		return -EINVAL;
	}
	if (phdr->p_memsz > ipahal_get_hps_img_mem_size()) {
		IPAERR("Invalid IPA HPS img size memsz=%d dps_mem_size=%u\n",
			phdr->p_memsz, ipahal_get_hps_img_mem_size());
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	IPADBG("IPA FWs (GSI FW, DPS and HPS) loaded successfully\n");
	return 0;
}

/**
 * ipa3_is_msm_device() - Is the running device a MSM or MDM?
 *  Determine according to IPA version
 *
 * Return value: true if MSM, false if MDM
 *
 */
bool ipa3_is_msm_device(void)
{
	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_5:
	case IPA_HW_v4_0:
		return false;
	case IPA_HW_v3_1:
	case IPA_HW_v3_5_1:
		return true;
	default:
		IPAERR("unknown HW type %d\n", ipa3_ctx->ipa_hw_type);
		ipa_assert();
	}

	return false;
}

/**
* ipa3_disable_prefetch() - disable\enable tx prefetch
*
* @client: the client which is related to the TX where prefetch will be
*          disabled
*
* Return value: Non applicable
*
*/
void ipa3_disable_prefetch(enum ipa_client_type client)
{
	struct ipahal_reg_tx_cfg cfg;
	u8 qmb;

	qmb = ipa3_get_qmb_master_sel(client);

	IPADBG("disabling prefetch for qmb %d\n", (int)qmb);

	ipahal_read_reg_fields(IPA_TX_CFG, &cfg);
	/* QMB0 (DDR) correlates with TX0, QMB1(PCIE) correlates with TX1 */
	if (qmb == QMB_MASTER_SELECT_DDR)
		cfg.tx0_prefetch_disable = true;
	else
		cfg.tx1_prefetch_disable = true;
	ipahal_write_reg_fields(IPA_TX_CFG, &cfg);
}

/**
 * ipa3_get_pdev() - return a pointer to IPA dev struct
 *
 * Return value: a pointer to IPA dev struct
 *
 */
struct device *ipa3_get_pdev(void)
{
	if (!ipa3_ctx)
		return NULL;

	return ipa3_ctx->pdev;
}

/**
 * ipa3_enable_dcd() - enable dynamic clock division on IPA
 *
 * Return value: Non applicable
 *
 */
void ipa3_enable_dcd(void)
{
	struct ipahal_reg_idle_indication_cfg idle_indication_cfg;

	/* recommended values for IPA 3.5 according to IPA HPG */
	idle_indication_cfg.const_non_idle_enable = 0;
	idle_indication_cfg.enter_idle_debounce_thresh = 256;

	ipahal_write_reg_fields(IPA_IDLE_INDICATION_CFG,
			&idle_indication_cfg);
}
