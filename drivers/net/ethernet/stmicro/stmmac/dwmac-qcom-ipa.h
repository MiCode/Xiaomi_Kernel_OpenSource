/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#ifndef	_DWMAC_QCOM_IPA_H
#define	_DWMAC_QCOM_IPA_H

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define ETHQOS_IPA_OFFLOAD_VLAN
#include <linux/if_vlan.h>
#endif

#define IPA_LOCK() mutex_lock(&eth_ipa_ctx.ipa_lock)
#define IPA_UNLOCK() mutex_unlock(&eth_ipa_ctx.ipa_lock)

static char * const IPA_OFFLOAD_EVENT_string[] = {
	"EV_INVALID",
	"EV_DEV_OPEN",
	"EV_DEV_CLOSE",
	"EV_IPA_READY",
	"EV_IPA_UC_READY",
	"EV_PHY_LINK_UP",
	"EV_PHY_LINK_DOWN",
	"EV_DPM_SUSPEND",
	"EV_DPM_RESUME",
	"EV_USR_SUSPEND",
	"EV_USR_RESUME",
	"EV_IPA_OFFLOAD_MAX"
};


#define GET_VALUE(data, lbit, lbit2, hbit) ((data >> lbit) & \
		(~(~0 << (hbit - lbit2 + 1))))

#define SET_BITS(e, s, reg, val) do { \
		unsigned int e1 = e;\
		unsigned int s1 = s;\
		unsigned int reg1 = reg;\
		unsigned int val1 = val;\
		unsigned int mask64 = (((e1) - (s1)) == 63 ?\
					   0xffffffffffffffffULL : \
					   ((1ULL << ((e1) - (s1) + 1)) - 1));\
		unsigned int mask32 = (((e1) - (s1)) == 31 ?\
					   0xffffffffffffffffULL : \
					   ((1ULL << ((e1) - (s1) + 1)) - 1));\
		if ((e1) - (s1) > 31) { \
			reg1 = ((((val1) << (s1)) & (mask64\
				<< (s1))) | \
			((reg1) & (~(mask64 << (s1))))); \
		} \
		else { \
			reg1 = ((((val1) << (s1)) & (mask32\
				<< (s1))) | \
			 ((reg1) & (~(mask32 << (s1))))); \
		} \
	} while (0)

#define GET_RX_CURRENT_RCVD_LAST_DESC_INDEX(start_index, offset, desc_cnt)\
		(desc_cnt - 1)
#define GET_RX_DESC_IDX(QINX, desc)\
	(((desc) - eth_ipa_ctx.rx_queue->rx_desc_dma_addrs[0]) / \
	 (sizeof(struct dma_desc)))

#define GET_TX_DESC_IDX(QINX, desc)\
	(((desc) - eth_ipa_ctx.tx_queue->tx_desc_dma_addrs[0]) / \
	 (sizeof(struct dma_desc)))

#define DMA_CR0_RGOFFADDR ((BASE_ADDRESS + 0x1100))

#define ETHQOS_ETH_FRAME_LEN_IPA ((1 << 11)) /*IPA can support 2KB max length*/

#define IPA_TX_DESC_CNT 128 /*Default TX desc count to 128 for IPA offload*/
#define IPA_RX_DESC_CNT 128 /*Default RX desc count to 128 for IPA offload*/

#define  BASE_ADDRESS (ethqos->ioaddr)

#define DMA_TDRLR_RGOFFADDR (BASE_ADDRESS + 0x112c)

#define DMA_TDRLR_RGOFFADDRESS(i) \
	(DMA_TDRLR_RGOFFADDR + ((i - 0) * 128))

#define DMA_TDRLR_RGWR(i, data) \
	writel_relaxed(data, DMA_TDRLR_RGOFFADDRESS(i))

#define DMA_TDLAR_RGOFFADDR (BASE_ADDRESS + 0x1114)

#define DMA_TDLAR_RGOFFADDRESS(i) \
	(DMA_TDLAR_RGOFFADDR + ((i - 0) * 128))

#define DMA_TDLAR_RGWR(i, data) \
	writel_relaxed(data, DMA_TDLAR_RGOFFADDRESS(i))

#define DMA_RDRLR_RGOFFADDR (BASE_ADDRESS + 0x1130)

#define DMA_RDRLR_RGOFFADDRESS(i) \
	(DMA_RDRLR_RGOFFADDR + ((i - 0) * 128))

#define DMA_RDRLR_RGWR(i, data) \
	writel_relaxed(data, DMA_RDRLR_RGOFFADDRESS(i))

#define DMA_RDLAR_RGOFFADDR (BASE_ADDRESS + 0x111c)

#define DMA_RDLAR_RGOFFADDRESS(i) \
	(DMA_RDLAR_RGOFFADDR + ((i - 0) * 128))

#define DMA_RDLAR_RGWR(i, data) \
	writel_relaxed(data, DMA_RDLAR_RGOFFADDRESS(i))

#define  DMA_TCR_MASK_22 (unsigned long)(0x3ff)

/*#define DMA_TCR_RES_Wr_Mask_22 (unsigned long)(~((~(~0<<(10)))<<(22)))*/

#define DMA_TCR_RES_WR_MASK_22 (unsigned long)(0x3fffff)

/*#define  DMA_TCR_Mask_13 (unsigned long)(~(~0<<(3)))*/

#define  DMA_TCR_MASK_13 (unsigned long)(0x7)

/*#define DMA_TCR_RES_Wr_Mask_13 (unsigned long)(~((~(~0<<(3)))<<(13)))*/

#define DMA_TCR_RES_WR_MASK_13 (unsigned long)(0xffff1fff)

/*#define  DMA_TCR_Mask_5 (unsigned long)(~(~0<<(7)))*/

#define  DMA_TCR_MASK_5 (unsigned long)(0x7f)

/*#define DMA_TCR_RES_Wr_Mask_5 (unsigned long)(~((~(~0<<(7)))<<(5)))*/

#define DMA_TCR_RES_WR_MASK_5 (unsigned long)(0xfffff01f)

/*#define DMA_TCR_PBL_Mask (unsigned long)(~(~0<<(6)))*/

#define DMA_TCR_PBL_MASK (unsigned long)(0x3f)

/*#define DMA_TCR_PBL_Wr_Mask (unsigned long)(~((~(~0 << (6))) << (16)))*/

#define DMA_TCR_PBL_WR_MASK (unsigned long)(0xffc0ffff)

#define DMA_TCR_RGOFFADDR (BASE_ADDRESS + 0x1104)

#define DMA_TCR_RGOFFADDRESS(i) \
	(DMA_TCR_RGOFFADDR + ((i - 0) * 128))

#define DMA_TCR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_TCR_RGOFFADDRESS(i)))

#define DMA_TCR_RGWR(i, data) \
	writel_relaxed(data, DMA_TCR_RGOFFADDRESS(i))

#define DMA_TCR_PBL_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_TCR_RGRD(i1, v);\
	v = (v & (DMA_TCR_RES_WR_MASK_22)) | (((0) & (DMA_TCR_MASK_22)) << 22);\
	v = (v & (DMA_TCR_RES_WR_MASK_13)) | (((0) & (DMA_TCR_MASK_13)) << 13);\
	v = (v & (DMA_TCR_RES_WR_MASK_5)) | (((0) & (DMA_TCR_MASK_5)) << 5);\
	v = ((v & DMA_TCR_PBL_WR_MASK) | ((data & DMA_TCR_PBL_MASK) << 16));\
	DMA_TCR_RGWR(i1, v);\
} while (0)

#define DMA_TCR_OSP_MASK (unsigned long)(0x1)

/*#define DMA_TCR_OSP_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (4)))*/

#define DMA_TCR_OSP_WR_MASK (unsigned long)(0xffffffef)

#define DMA_TCR_OSP_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_TCR_RGRD(i1, v);\
	v = (v & (DMA_TCR_RES_WR_MASK_22)) | (((0) & (DMA_TCR_MASK_22)) << 22);\
	v = (v & (DMA_TCR_RES_WR_MASK_13)) | (((0) & (DMA_TCR_MASK_13)) << 13);\
	v = (v & (DMA_TCR_RES_WR_MASK_5)) | (((0) & (DMA_TCR_MASK_5)) << 5);\
	v = ((v & DMA_TCR_OSP_WR_MASK) | ((data & DMA_TCR_OSP_MASK) << 4));\
	DMA_TCR_RGWR(i1, v);\
} while (0)

#define DMA_CR_RGOFFADDR (BASE_ADDRESS + 0x1100)

#define DMA_CR_RGOFFADDRESS(i) \
	(DMA_CR_RGOFFADDR + ((i - 0) * 128))

/*#define  DMA_CR_Mask_25 (unsigned long)(~(~0<<(7)))*/

#define  DMA_CR_MASK_25 (unsigned long)(0x7f)

/*#define DMA_CR_RES_Wr_Mask_25 (unsigned long)(~((~(~0<<(7)))<<(25)))*/

#define DMA_CR_RES_WR_MASK_25 (unsigned long)(0x1ffffff)

/*#define  DMA_CR_Mask_21 (unsigned long)(~(~0<<(2)))*/

#define  DMA_CR_MASK_21 (unsigned long)(0x3)

/*#define DMA_CR_RES_Wr_Mask_21 (unsigned long)(~((~(~0<<(2)))<<(21)))*/

#define DMA_CR_RES_WR_MASK_21 (unsigned long)(0xff9fffff)

/*#define DMA_CR_SPH_Mask (unsigned long)(~(~0<<(1)))*/

#define DMA_CR_SPH_MASK (unsigned long)(0x1)

/*#define DMA_CR_SPH_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (24)))*/

#define DMA_CR_SPH_WR_MASK (unsigned long)(0xfeffffff)

#define DMA_CR_PBLX8_MASK (unsigned long)(0x1)

/*#define DMA_CR_PBLx8_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (16)))*/

#define DMA_CR_PBLX8_WR_MASK (unsigned long)(0xfffeffff)

#define DMA_CR_RGWR(i, data) \
	writel_relaxed(data, DMA_CR_RGOFFADDRESS(i))

#define DMA_CR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_CR_RGOFFADDRESS(i)))

#define DMA_CR_PBLX8_UDFWR(i, data) do {\
	 unsigned long v;\
	unsigned long i1 = i;\
	DMA_CR_RGRD(i1, v);\
	v = (v & (DMA_CR_RES_WR_MASK_25)) | (((0) & (DMA_CR_MASK_25)) << 25);\
	v = (v & (DMA_CR_RES_WR_MASK_21)) | (((0) & (DMA_CR_MASK_21)) << 21);\
	v = ((v & DMA_CR_PBLX8_WR_MASK) | ((data & DMA_CR_PBLX8_MASK) << 16));\
	 DMA_CR_RGWR(i1, v);\
} while (0)

#define DMA_SR_RGOFFADDR (BASE_ADDRESS + 0x1160)

#define DMA_SR_RGOFFADDRESS(i) \
	(DMA_SR_RGOFFADDR + ((i - 0) * 128))

#define DMA_SR_RGWR(i, data) \
	writel_relaxed(data, DMA_SR_RGOFFADDRESS(i))

#define DMA_SR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_SR_RGOFFADDRESS(i)))

#define DMA_IER_RGOFFADDR (BASE_ADDRESS + 0x1134)

#define DMA_IER_RGOFFADDRESS(i) \
	(DMA_IER_RGOFFADDR + ((i - 0) * 128))

#define DMA_IER_RGWR(i, data) \
	writel_relaxed(data, DMA_IER_RGOFFADDRESS(i))

#define DMA_IER_RGRD(i, data) \
	((data) = readl_relaxed(DMA_IER_RGOFFADDRESS(i)))

#define DMA_TCR_ST_MASK (unsigned long)(0x1)

/*#define DMA_TCR_ST_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define DMA_TCR_ST_WR_MASK (unsigned long)(0xfffffffe)

#define DMA_TCR_ST_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_TCR_RGRD(i1, v);\
	v = (v & (DMA_TCR_RES_WR_MASK_22)) | (((0) & (DMA_TCR_MASK_22)) << 22);\
	v = (v & (DMA_TCR_RES_WR_MASK_13)) | (((0) & (DMA_TCR_MASK_13)) << 13);\
	v = (v & (DMA_TCR_RES_WR_MASK_5)) | (((0) & (DMA_TCR_MASK_5)) << 5);\
	v = ((v & DMA_TCR_ST_WR_MASK) | ((data & DMA_TCR_ST_MASK) << 0));\
	DMA_TCR_RGWR(i1, v);\
} while (0)

#define DMA_RCR_RBSZ_MASK (unsigned long)(0x3fff)

/*#define DMA_RCR_RBSZ_Wr_Mask (unsigned long)(~((~(~0 << (14))) << (1)))*/

#define DMA_RCR_RBSZ_WR_MASK (unsigned long)(0xffff8001)

/*#define  DMA_RCR_Mask_28 (unsigned long)(~(~0<<(4)))*/

#define  DMA_RCR_MASK_28 (unsigned long)(0x7)

/*#define DMA_RCR_RES_Wr_Mask_28 (unsigned long)(~((~(~0<<(4)))<<(28)))*/

#define DMA_RCR_RES_WR_MASK_28 (unsigned long)(0x8fffffff)

/*#define  DMA_RCR_Mask_22 (unsigned long)(~(~0<<(3)))*/

#define  DMA_RCR_MASK_22 (unsigned long)(0x7)

/*#define DMA_RCR_RES_Wr_Mask_22 (unsigned long)(~((~(~0<<(3)))<<(22)))*/

#define DMA_RCR_RES_WR_MASK_22 (unsigned long)(0xfe3fffff)

/*#define  DMA_RCR_Mask_15 (unsigned long)(~(~0<<(1)))*/

#define  DMA_RCR_MASK_15 (unsigned long)(0x1)

/*#define DMA_RCR_RES_Wr_Mask_15 (unsigned long)(~((~(~0<<(1)))<<(15)))*/

#define DMA_RCR_RES_WR_MASK_15 (unsigned long)(0xffff7fff)

/*#define DMA_RCR_MAMS_Mask (unsigned long)(~(~0<<(1)))*/

#define DMA_RCR_MAMS_MASK (unsigned long)(0x1)

/*#define DMA_RCR_MAMS_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (27)))*/

#define DMA_RCR_MAMS_WR_MASK (unsigned long)(0xf7ffffff)

#define DMA_RCR_RGOFFADDR (BASE_ADDRESS + 0x1108)

#define DMA_RCR_RGOFFADDRESS(i) \
	(DMA_RCR_RGOFFADDR + ((i - 0) * 128))

#define DMA_RCR_RGWR(i, data) \
	writel_relaxed(data, DMA_RCR_RGOFFADDRESS(i))

#define DMA_RCR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_RCR_RGOFFADDRESS(i)))

#define DMA_RCR_RBSZ_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_RCR_RGRD(i1, v);\
	v = (v & (DMA_RCR_RES_WR_MASK_28)) | (((0) & (DMA_RCR_MASK_28)) << 28);\
	v = (v & (DMA_RCR_RES_WR_MASK_22)) | (((0) & (DMA_RCR_MASK_22)) << 22);\
	v = (v & (DMA_RCR_RES_WR_MASK_15)) | (((0) & (DMA_RCR_MASK_15)) << 15);\
	v = ((v & DMA_RCR_RBSZ_WR_MASK) | ((data & DMA_RCR_RBSZ_MASK) << 1));\
	DMA_RCR_RGWR(i1, v);\
} while (0)

#define DMA_RCR_PBL_MASK (unsigned long)(0x3f)

/*#define DMA_RCR_PBL_Wr_Mask (unsigned long)(~((~(~0 << (6))) << (16)))*/

#define DMA_RCR_PBL_WR_MASK (unsigned long)(0xffc0ffff)

#define DMA_RCR_PBL_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_RCR_RGRD(i1, v);\
	v = (v & (DMA_RCR_RES_WR_MASK_28)) | (((0) & (DMA_RCR_MASK_28)) << 28);\
	v = (v & (DMA_RCR_RES_WR_MASK_22)) | (((0) & (DMA_RCR_MASK_22)) << 22);\
	v = (v & (DMA_RCR_RES_WR_MASK_15)) | (((0) & (DMA_RCR_MASK_15)) << 15);\
	v = ((v & DMA_RCR_PBL_WR_MASK) | ((data & DMA_RCR_PBL_MASK) << 16));\
	DMA_RCR_RGWR(i1, v);\
} while (0)

#define DMA_RCR_ST_MASK (unsigned long)(0x1)

/*#define DMA_RCR_ST_Wr_Mask (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define DMA_RCR_ST_WR_MASK (unsigned long)(0xfffffffe)

#define DMA_RCR_ST_UDFWR(i, data) do {\
	unsigned long v;\
	unsigned long i1 = i;\
	DMA_RCR_RGRD(i1, v);\
	v = (v & (DMA_RCR_RES_WR_MASK_28)) | (((0) & (DMA_RCR_MASK_28)) << 28);\
	v = (v & (DMA_RCR_RES_WR_MASK_22)) | (((0) & (DMA_RCR_MASK_22)) << 22);\
	v = (v & (DMA_RCR_RES_WR_MASK_15)) | (((0) & (DMA_RCR_MASK_15)) << 15);\
	v = ((v & DMA_RCR_ST_WR_MASK) | ((data & DMA_RCR_ST_MASK) << 0));\
	DMA_RCR_RGWR(i1, v);\
} while (0)

#define DMA_RIWTR_RGOFFADDR (BASE_ADDRESS + 0x1138)

#define DMA_RIWTR_RGOFFADDRESS(i) \
	(DMA_RIWTR_RGOFFADDR + ((i - 0) * 128))

#define DMA_RIWTR_RGWR(i, data) \
	writel_relaxed(data, DMA_RIWTR_RGOFFADDRESS(i))

#define DMA_RIWTR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_RIWTR_RGOFFADDRESS(i)))

/*#define  DMA_RIWTR_Mask_8 (unsigned long)(~(~0<<(24)))*/

#define  DMA_RIWTR_MASK_8 (unsigned long)(0xffffff)

/*#define DMA_RIWTR_RES_Wr_Mask_8 (unsigned long)(~((~(~0<<(24)))<<(8)))*/

#define DMA_RIWTR_RES_WR_MASK_8 (unsigned long)(0xff)

/*#define DMA_RIWTR_RWT_Mask (unsigned long)(~(~0<<(8)))*/

#define DMA_RIWTR_RWT_MASK (unsigned long)(0xff)

/*#define DMA_RIWTR_RWT_Wr_Mask (unsigned long)(~((~(~0 << (8))) << (0)))*/

#define DMA_RIWTR_RWT_WR_MASK (unsigned long)(0xffffff00)

#define DMA_RIWTR_RWT_UDFWR(i, data) do {\
	unsigned long v = 0; \
	v = (v & (DMA_RIWTR_RES_WR_MASK_8)) |\
	(((0) & (DMA_RIWTR_MASK_8)) << 8);\
	(v) = ((v & DMA_RIWTR_RWT_WR_MASK) |\
	((data & DMA_RIWTR_RWT_MASK) << 0));\
	DMA_RIWTR_RGWR(i, v);\
} while (0)

#define MTL_RQDCM0R_RGOFFADDR ((BASE_ADDRESS + 0xc30))

#define MTL_RQDCM0R_RGWR(data) \
	writel_relaxed(data, MTL_RQDCM0R_RGOFFADDR)

#define DMA_DSR0_RPS0_LPOS 8
#define DMA_DSR0_RPS0_HPOS 11

#define DMA_DSR0_TPS0_LPOS 12
#define DMA_DSR0_TPS0_HPOS 15

#define DMA_SR_TI_LPOS 0
#define DMA_SR_TI_HPOS 0

#define DMA_SR_TPS_LPOS 1
#define DMA_SR_TPS_HPOS 1

#define DMA_SR_TBU_LPOS 2
#define DMA_SR_TBU_HPOS 2

#define DMA_SR_RI_LPOS 6
#define DMA_SR_RI_HPOS 6

#define DMA_SR_RBU_LPOS 7
#define DMA_SR_RBU_HPOS 7

#define DMA_SR_RPS_LPOS 8
#define DMA_SR_RPS_HPOS 8

#define DMA_SR_RWT_LPOS 9
#define DMA_SR_RWT_HPOS 9

#define DMA_SR_FBE_LPOS 12
#define DMA_SR_FBE_HPOS 12

#define DMA_ISR_DC0IS_LPOS 0
#define DMA_ISR_DC0IS_HPOS 0

#define DMA_ISR_DC1IS_LPOS 1
#define DMA_ISR_DC1IS_HPOS 1

#define DMA_ISR_DC2IS_LPOS 2
#define DMA_ISR_DC2IS_HPOS 2

#define DMA_ISR_DC3IS_LPOS 3
#define DMA_ISR_DC3IS_HPOS 3

#define DMA_ISR_DC4IS_LPOS 4
#define DMA_ISR_DC4IS_HPOS 4

#define DMA_ISR_MTLIS_LPOS 16
#define DMA_ISR_MTLIS_HPOS 16

#define DMA_DSR0_RGOFFADDR ((BASE_ADDRESS + 0x100c))

#define DMA_DSR0_RGRD(data) \
	((data) = readl_relaxed(DMA_DSR0_RGOFFADDR))

#define DMA_CHRDR_RGOFFADDR (BASE_ADDRESS + 0x114c)

#define DMA_CHRDR_RGOFFADDRESS(i)\
	((DMA_CHRDR_RGOFFADDR + ((i - 0) * 128)))

#define DMA_CHRDR_RGRD(i, data) \
	((data) = readl_relaxed(DMA_CHRDR_RGOFFADDRESS(i)))

#define DMA_RDTP_RPDR_RGOFFADDR (BASE_ADDRESS + 0x1128)

#define DMA_RDTP_RPDR_RGOFFADDRESS(i)\
	((DMA_RDTP_RPDR_RGOFFADDR + ((i - 0) * 128)))

#define DMA_RDTP_RPDR_RGWR(i, data)\
		writel_relaxed(data, DMA_RDTP_RPDR_RGOFFADDRESS(i))

#define DMA_RDTP_RPDR_RGRD(i, data) \
		((data) = readl_relaxed(DMA_RDTP_RPDR_RGOFFADDRESS(i)))

#define DMA_IER_RBUE_MASK (unsigned long)(0x1)

#define DMA_IER_RBUE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 7) & DMA_IER_RBUE_MASK);\
} while (0)

#define DMA_IER_ETIE_MASK (unsigned long)(0x1)

#define DMA_IER_ETIE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 10) & DMA_IER_ETIE_MASK);\
} while (0)

#define DMA_CHTDR_RGOFFADDR (BASE_ADDRESS + 0x1144)

#define DMA_CHTDR_RGOFFADDRESS(i)\
	((DMA_CHTDR_RGOFFADDR + ((i - 0) * 128)))

#define DMA_CHTDR_RGRD(i, data) \
		((data) = readl_relaxed(DMA_CHTDR_RGOFFADDRESS(i)))

#define DMA_TDTP_TPDR_RGOFFADDR (BASE_ADDRESS + 0x1120)

#define DMA_TDTP_TPDR_RGOFFADDRESS(i)\
	((DMA_TDTP_TPDR_RGOFFADDR + ((i - 0) * 128)))

#define DMA_TDTP_TPDR_RGWR(i, data) \
		writel_relaxed(data, DMA_TDTP_TPDR_RGOFFADDRESS(i))

#define DMA_TDTP_TPDR_RGRD(i, data) \
		((data) = readl_relaxed(DMA_TDTP_TPDR_RGOFFADDRESS(i)))

#define DMA_IER_TIE_MASK (unsigned long)(0x1)

#define DMA_IER_TIE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 0) & DMA_IER_TIE_MASK);\
} while (0)

#define DMA_IER_TXSE_MASK (unsigned long)(0x1)

#define DMA_IER_TXSE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 1) & DMA_IER_TXSE_MASK);\
} while (0)

#define DMA_IER_TBUE_MASK (unsigned long)(0x1)

#define DMA_IER_TBUE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 2) & DMA_IER_TBUE_MASK);\
} while (0)

#define DMA_IER_FBEE_MASK (unsigned long)(0x1)

#define DMA_IER_FBEE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 12) & DMA_IER_FBEE_MASK);\
} while (0)

#define DMA_IER_CDEE_MASK (unsigned long)(0x1)

#define DMA_IER_CDEE_UDFRD(i, data) do {\
		unsigned int data1;\
		DMA_IER_RGRD(i, data1);\
		data = ((data1 >> 13) & DMA_IER_CDEE_MASK);\
} while (0)

struct ethqos_tx_queue {
	struct stmmac_tx_queue *tx_q;
	unsigned int desc_cnt;
	struct dma_desc **tx_desc_ptrs;
	dma_addr_t *tx_desc_dma_addrs;

	void **ipa_tx_buff_pool_va_addrs_base;

	dma_addr_t *ipa_tx_buff_pool_pa_addrs_base;
	dma_addr_t ipa_tx_buff_pool_pa_addrs_base_dmahndl;

	dma_addr_t *skb_dma;		/* dma address of skb */
	struct sk_buff **skb;	/* virtual address of skb */
	unsigned short *len;	/* length of first skb */
	phys_addr_t *ipa_tx_buff_phy_addr; /* physical address of ipa TX buff */
};

struct ethqos_rx_queue {
	/* Rx descriptors */
	struct stmmac_rx_queue *rx_q;
	unsigned int desc_cnt;
	struct dma_desc **rx_desc_ptrs;
	dma_addr_t *rx_desc_dma_addrs;

	void **ipa_rx_buff_pool_va_addrs_base;

	dma_addr_t *ipa_rx_buff_pool_pa_addrs_base;
	dma_addr_t ipa_rx_buff_pool_pa_addrs_base_dmahndl;

	dma_addr_t *skb_dma;		/* dma address of skb */
	struct sk_buff **skb;	/* virtual address of skb */
	void **ipa_buff_va;	/* virtual address of ipa_buff */
	unsigned short *len;	/* length of received packet */
	phys_addr_t *ipa_rx_buff_phy_addr; /* physical address of ipa RX buff */
};

struct ethqos_ipa_stats {
	unsigned int ipa_rx_desc_ring_base;
	unsigned int ipa_rx_desc_ring_size;
	unsigned int ipa_rx_buff_ring_base;
	unsigned int ipa_rx_buff_ring_size;
	unsigned int ipa_rx_db_int_raised;
	unsigned int ipa_rx_cur_desc_ptr_indx;
	unsigned int ipa_rx_tail_ptr_indx;

	unsigned int ipa_rx_dma_status;
	unsigned int ipa_rx_dma_ch_status;
	unsigned int ipa_rx_dma_ch_underflow;
	unsigned int ipa_rx_dma_ch_stopped;
	unsigned int ipa_rx_dma_ch_complete;

	unsigned int ipa_rx_int_mask;
	unsigned long ipa_rx_transfer_complete_irq;
	unsigned long ipa_rx_transfer_stopped_irq;
	unsigned long ipa_rx_underflow_irq;
	unsigned long ipa_rx_early_trans_comp_irq;

	unsigned int ipa_tx_desc_ring_base;
	unsigned int ipa_tx_desc_ring_size;
	unsigned int ipa_tx_buff_ring_base;
	unsigned int ipa_tx_buff_ring_size;
	unsigned int ipa_tx_db_int_raised;
	unsigned long ipa_tx_curr_desc_ptr_indx;
	unsigned long ipa_tx_tail_ptr_indx;

	unsigned int ipa_tx_dma_status;
	unsigned int ipa_tx_dma_ch_status;
	unsigned int ipa_tx_dma_ch_underflow;
	unsigned int ipa_tx_dma_transfer_stopped;
	unsigned int ipa_tx_dma_transfer_complete;

	unsigned int ipa_tx_int_mask;
	unsigned long ipa_tx_transfer_complete_irq;
	unsigned long ipa_tx_transfer_stopped_irq;
	unsigned long ipa_tx_underflow_irq;
	unsigned long ipa_tx_early_trans_cmp_irq;
	unsigned long ipa_tx_fatal_err_irq;
	unsigned long ipa_tx_desc_err_irq;

	unsigned long long ipa_ul_exception;
};

struct ethqos_prv_ipa_data {
	struct ethqos_tx_queue *tx_queue;
	struct ethqos_rx_queue *rx_queue;

	phys_addr_t uc_db_rx_addr;
	phys_addr_t uc_db_tx_addr;
	u32 ipa_client_hndl;

	u32 ipa_dma_tx_desc_cnt;
	u32 ipa_dma_rx_desc_cnt;

	/* IPA state variables */
	/* State of EMAC HW initialization */
	bool emac_dev_ready;
	/* State of IPA readiness */
	bool ipa_ready;
	/* State of IPA and IPA UC readiness */
	bool ipa_uc_ready;
	/* State of IPA Offload intf registration with IPA driver */
	bool ipa_offload_init;
	/* State of IPA pipes connection */
	bool ipa_offload_conn;
	/* State of debugfs creation */
	bool ipa_debugfs_exists;
	/* State of IPA offload suspended by user */
	bool ipa_offload_susp;
	/* State of IPA offload enablement from PHY link event*/
	bool ipa_offload_link_down;

	/* Dev state */
	struct work_struct ntn_ipa_rdy_work;
	unsigned int ipa_ver;
	bool vlan_enable;
	unsigned short vlan_id;
	/* lock for ipa event handler*/
	struct mutex ipa_lock;

	struct dentry *debugfs_ipa_stats;
	struct dentry *debugfs_dma_stats;
	struct dentry *debugfs_suspend_ipa_offload;
	struct ethqos_ipa_stats ipa_stats;

	struct qcom_ethqos *ethqos;
};

#endif
