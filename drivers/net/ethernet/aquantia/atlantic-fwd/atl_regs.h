/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_REGS_H_
#define _ATL_REGS_H_

#define ATL_REG_STRIDE(base, stride, idx) ((base) + (stride) * (idx))

/* Ring registers common for Rx and Tx */
#define ATL_RING_OFFT(ring, offt)				\
	(((struct atl_hw_ring *)(ring))->reg_base + (offt))
#define ATL_RING_BASE_LSW(ring) ATL_RING_OFFT(ring, 0)
#define ATL_RING_BASE_MSW(ring) ATL_RING_OFFT(ring, 4)
#define ATL_RING_CTL(ring) ATL_RING_OFFT(ring, 8)
#define ATL_RING_HEAD(ring) ATL_RING_OFFT(ring, 0xc)
#define ATL_RING_TAIL(ring) ATL_RING_OFFT(ring, 0x10)
#define ATL_RING_STS(ring) ATL_RING_OFFT(ring, 0x14)

/* MIF @ 0x0000*/
#define ATL_GLOBAL_STD_CTRL 0
#define ATL_GLOBAL_FW_ID 0xc
#define ATL_GLOBAL_CHIP_ID 0x10
#define ATL_GLOBAL_CHIP_REV 0x14
#define ATL_GLOBAL_FW_IMAGE_ID 0x18
#define ATL_GLOBAL_MIF_ID 0x1c
#define ATL_GLOBAL_MBOX_CTRL 0x200
#define ATL_GLOBAL_MBOX_CRC 0x204
#define ATL_GLOBAL_MBOX_ADDR 0x208
#define ATL_GLOBAL_MBOX_DATA 0x20c
#define ATL_GLOBAL_MDIO_CTL 0x280
#define ATL_GLOBAL_MDIO_CMD 0x284
#define ATL_GLOBAL_MDIO_WDATA 0x288
#define ATL_GLOBAL_MDIO_ADDR 0x28c
#define ATL_GLOBAL_MDIO_RDATA 0x290
#define ATL2_MIF_BOOT_REG_ADR 0x3040
#define ATL2_MCP_HOST_REQ_INT 0x0F00
#define ATL2_MCP_HOST_REQ_INT_CLR 0x0F08
#define ATL2_MCP_HOST_REQ_INT_MASK(idx) ATL_REG_STRIDE(0x0f0c, 4, (idx))
#define ATL2_MIF_SHARED_BUFFER_IN(idx) ATL_REG_STRIDE(0x00012000, 0x4, (idx))
#define ATL2_MIF_SHARED_BUFFER_OUT(idx) ATL_REG_STRIDE(0x00013000, 0x4, (idx))
#define ATL2_MIF_HOST_FINISHED_WRITE 0x0e00
#define ATL2_MIF_MCP_FINISHED_READ 0x0e04

/* Scratch pads numbered starting from 1 */
#define ATL_MCP_SCRATCH(idx) ATL_REG_STRIDE(0x300 - 0x4, 0x4, idx)
#define ATL_MCP_SEM(idx) ATL_REG_STRIDE(0x3a0, 0x4, idx)
#define ATL_MCP_SEM_MDIO 0
#define ATL_MCP_SEM_MSM 1
#define ATL2_MCP_SEM_ACT_RSLVR 3
#define ATL_GLOBAL_CTRL2 0x404
#define ATL_GLOBAL_DAISY_CHAIN_STS1 0x704

enum mcp_scratchpad {
	FW2_MBOX_DATA = 11,	/* 0x328 */
	FW2_MBOX_CMD = 12,	/* 0x32c */
	FW2_RPC_DATA = 14, 	/* 0x334 */
	FW_STAT_STRUCT = 25, 	/* 0x360 */
	FW2_EFUSE_SHADOW = 26,	/* 0x364 */
	FW1_LINK_REQ = 27,
	FW2_LINK_REQ_LOW = 27,	/* 0x368 */
	FW1_LINK_STS = 28,
	FW2_LINK_REQ_HIGH = 28,	/* 0x36c */
	FW2_LINK_RES_LOW = 29,	/* 0x370 */
	FW1_EFUSE_SHADOW = 30,
	FW2_LINK_RES_HIGH = 30,	/* 0x374 */
	RBL_STS = 35,		/* 0x388 */
};

/* INTR @ 0x2000 */
#define ATL_INTR_STS 0x2000
#define ATL_INTR_MSK 0x2010
#define ATL_INTR_MSK_SET 0x2060
#define ATL_INTR_MSK_CLEAR 0x2070
#define ATL_INTR_AUTO_CLEAR 0x2080
#define ATL_INTR_AUTO_MASK 0x2090
#define ATL_INTR_RING_INTR_MAP(idx) ATL_REG_STRIDE(0x2100, 0x4, (idx) >> 1)
#define ATL_INTR_GEN_INTR_MAP4 0x218c
#define ATL_INTR_RSC_EN 0x2200
#define ATL_INTR_RSC_DELAY 0x2204
#define ATL_INTR_CTRL 0x2300
#define ATL_INTR_THRTL(idx) ATL_REG_STRIDE(0x2800, 4, idx)

/* MPI @ 0x4000 */
#define ATL_MPI_CTRL1 0x4000
#define ATL_MPI_MSM_ADDR 0x4400
#define ATL_MPI_MSM_WR 0x4404
#define ATL_MPI_MSM_RD 0x4408

/* RX @ 0x5000 */
#define ATL_RX_CTRL1 0x5000
#define ATL2_RX_FLT_L2_BC_TAG 0x50F0
#define ATL_RX_FLT_CTRL1 0x5100
#define ATL_RX_FLT_CTRL2 0x5104
#define ATL_UC_FLT_NUM 37
#define ATL_RX_UC_FLT_REG1(idx) ATL_REG_STRIDE(0x5110, 8, idx)
#define ATL_RX_UC_FLT_REG2(idx) ATL_REG_STRIDE(0x5114, 8, idx)
#define ATL_MC_FLT_NUM 8
#define ATL_RX_MC_FLT(idx) ATL_REG_STRIDE(0x5250, 4, idx)
#define ATL_RX_MC_FLT_MSK 0x5270
#define ATL_RX_VLAN_FLT_CTRL1 0x5280
#define ATL_VLAN_FLT_NUM 16
#define ATL_RX_VLAN_FLT(idx) ATL_REG_STRIDE(0x5290, 4, idx)
#define ATL_RX_ETYPE_FLT(idx) ATL_REG_STRIDE(0x5300, 4, idx)
#define ATL2_RX_ETYPE_TAG(idx) ATL_REG_STRIDE(0x5340, 4, idx)
#define ATL_ETYPE_FLT_NUM 15
#define ATL_NTUPLE_CTRL(idx) ATL_REG_STRIDE(0x5380, 4, idx)
#define ATL_NTUPLE_SADDR(idx) ATL_REG_STRIDE(0x53b0, 4, idx)
#define ATL_NTUPLE_DADDR(idx) ATL_REG_STRIDE(0x53d0, 4, idx)
#define ATL_NTUPLE_SPORT(idx) ATL_REG_STRIDE(0x5400, 4, idx)
#define ATL_NTUPLE_DPORT(idx) ATL_REG_STRIDE(0x5420, 4, idx)
#define ATL_NTUPLE_FLT_NUM 8
#define ATL_RX_FLEX_FLT_CTRL(idx) ATL_REG_STRIDE(0x5460, 0x20, idx)
#define ATL_FLEX_FLT_NUM 2
#define ATL_RX_RSS_CTRL 0x54c0
#define ATL_RX_RSS_KEY_ADDR 0x54d0
#define ATL_RX_RSS_KEY_WR_DATA 0x54d4
#define ATL_RX_RSS_KEY_RD_DATA 0x54d8
#define ATL_RX_RSS_TBL_ADDR 0x54e0
#define ATL_RX_RSS_TBL_WR_DATA 0x54e4
#define ATL_RX_RSS_TBL_RD_DATA 0x54e8
#define ATL2_RX_RSS_HASH_TYPE_ADR 0x54C8
#define ATL_RX_RPF_DBG_CNT_CTRL 0x5518
#define ATL_RX_RPF_HOST_CNT_LO 0x552c
#define ATL_RX_RPF_HOST_CNT_HI 0x5530
#define ATL_RX_RPF_LOST_CNT_LO 0x554c
#define ATL_RX_RPF_LOST_CNT_HI 0x5550
#define ATL_RX_PO_CTRL1 0x5580
#define ATL_RX_LRO_CTRL1 0x5590
#define ATL_RX_LRO_CTRL2 0x5594
#define ATL_RX_LRO_PKT_LIM_EN 0x5598
#define ATL_RX_LRO_PKT_LIM(idx) ATL_REG_STRIDE(0x55a0, 4, (idx) >> 3)
#define ATL_RX_LRO_TMRS 0x5620
#define ATL_RX_PBUF_CTRL1 0x5700
#define ATL_RX_PBUF_REG1(idx) ATL_REG_STRIDE(0x5710, 0x10, idx)
#define ATL_RX_PBUF_REG2(idx) ATL_REG_STRIDE(0x5714, 0x10, idx)
#define ATL2_RX_Q_TO_TC_MAP(tc) ATL_REG_STRIDE(0x5900, 0x4, tc)
#define ATL_RX_INTR_CTRL 0x5a30
#define ATL_RX_INTR_MOD_CTRL(idx) ATL_REG_STRIDE(0x5a40, 4, idx)
#define ATL2_RPF_RSS_REDIR(TC, INDEX) (0x6200 + \
				       (0x100 * (TC > 3 ? 1 : 0)) + (INDEX) * 4)
#define ATL2_RPF_L3_FLT(filter) ATL_REG_STRIDE(0x6500, 0x4, filter)
#define ATL2_RPF_L3_SA(filter) ATL_REG_STRIDE(0x6400, 0x10, filter)
#define ATL2_RPF_L3_DA(filter) ATL_REG_STRIDE(0x6480, 0x10, filter)
#define ATL2_RPF_L4_FLT(filter) ATL_REG_STRIDE(0x6520, 0x4, filter)
#define ATL2_RPF_REC_TAB_EN 0x00006ff0
#define ATL2_RPF_ACT_RSLVR_REQ_TAG(filter) ATL_REG_STRIDE(0x14000, 0x10, filter)
#define ATL2_RPF_ACT_RSLVR_TAG_MASK(filter) ATL_REG_STRIDE(0x14004, 0x10, filter)
#define ATL2_RPF_ACT_RSLVR_ACTN(filter) ATL_REG_STRIDE(0x14008, 0x10, filter)

/* Rx rings */
#define ATL_RX_RING(idx) ATL_REG_STRIDE(0x5b00, 0x20, idx)
#define ATL_RX_RING_BASE_LSW(ring) ATL_RING_BASE_LSW(ring)
#define ATL_RX_RING_BASE_MSW(ring) ATL_RING_BASE_MSW(ring)
#define ATL_RX_RING_CTL(ring) ATL_RING_CTL(ring)
#define ATL_RX_RING_HEAD(ring) ATL_RING_HEAD(ring)
#define ATL_RX_RING_TAIL(ring) ATL_RING_TAIL(ring)
#define ATL_RX_RING_STS(ring) ATL_RING_STS(ring)
#define ATL_RX_RING_BUF_SIZE(ring) ATL_RING_OFFT(ring, 0x18)
#define ATL_RX_RING_THRESH(ring) ATL_RING_OFFT(ring, 0x1c)

#define ATL_RX_DMA_STATS_CNT7 0x6818

/* TX @ 0x7000 */
#define ATL_TX_CTRL1 0x7000
#define ATL_TX_PO_CTRL1 0x7800
#define ATL_TX_LSO_CTRL 0x7810
#define ATL_TX_LSO_TCP_CTRL1 0x7820
#define ATL_TX_LSO_TCP_CTRL2 0x7824
#define ATL_TX_PBUF_CTRL1 0x7900
#define ATL_TX_PBUF_REG1(idx) ATL_REG_STRIDE(0x7910, 0x10, idx)
#define ATL_TX_PBUF_REG2(idx) ATL_REG_STRIDE(0x7914, 0x10, idx)
#define ATL_TX_INTR_CTRL 0x7b40
#define ATL2_TX_Q_TO_TC_MAP(tc) ATL_REG_STRIDE(0x799c, 0x4, tc)

/* Tx rings */
#define ATL_TX_RING(idx) ATL_REG_STRIDE(0x7c00, 0x40, idx)
#define ATL_TX_RING_BASE_LSW(ring) ATL_RING_BASE_LSW(ring)
#define ATL_TX_RING_BASE_MSW(ring) ATL_RING_BASE_MSW(ring)
#define ATL_TX_RING_CTL(ring) ATL_RING_CTL(ring)
#define ATL_TX_RING_HEAD(ring) ATL_RING_HEAD(ring)
#define ATL_TX_RING_TAIL(ring) ATL_RING_TAIL(ring)
#define ATL_TX_RING_STS(ring) ATL_RING_STS(ring)
#define ATL_TX_RING_THRESH(ring) ATL_RING_OFFT(ring, 0x18)
#define ATL_TX_RING_HEAD_WB_LSW(ring) ATL_RING_OFFT(ring, 0x1c)
#define ATL_TX_RING_HEAD_WB_MSW(ring) ATL_RING_OFFT(ring, 0x20)

#define ATL_TX_INTR_MOD_CTRL(idx) ATL_REG_STRIDE(0x8980, 0x4, idx)
#define ATL2_TX_INTR_MOD_CTRL(idx) ATL_REG_STRIDE(0x7c28, 0x40, idx)

/* MSM */
#define ATL_MSM_GEN_CTRL 0x8
#define ATL_MSM_GEN_STS 0x40
#define ATL_MSM_TX_LPI_DELAY 0x78
#define ATL_MSM_CTR_RX_PKTS_GOOD 0x88
#define ATL_MSM_CTR_RX_FCS_ERRS 0x90
#define ATL_MSM_CTR_RX_ALIGN_ERRS 0x98
#define ATL_MSM_CTR_TX_PAUSE 0xa0
#define ATL_MSM_CTR_RX_PAUSE 0xa8
#define ATL_MSM_CTR_RX_OCTETS_LO 0xd8
#define ATL_MSM_CTR_RX_OCTETS_HI 0xdc
#define ATL_MSM_CTR_RX_MULTICAST 0xE8
#define ATL_MSM_CTR_RX_BROADCAST 0xF0
#define ATL_MSM_CTR_RX_ERRS 0x120


#endif
