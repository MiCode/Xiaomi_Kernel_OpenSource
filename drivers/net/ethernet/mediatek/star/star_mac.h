/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
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

#ifndef _STAR_MAC_H_
#define _STAR_MAC_H_

#include <linux/netdevice.h>
#include <linux/delay.h>

#define desc_tx_dma(desc) ((((desc)->ctrl_len) & TX_COWN) ? 0 : 1)
#define desc_rx_dma(desc) ((((desc)->ctrl_len) & RX_COWN) ? 0 : 1)
#define desc_tx_last(desc) ((((desc)->ctrl_len) & TX_EOR) ? 1 : 0)
#define desc_rx_last(desc) ((((desc)->ctrl_len) & RX_EOR) ? 1 : 0)

#ifndef STAR_POLLING_TIMEOUT
#define STAR_TIMEOUT_COUNT 3000
#define STAR_POLLING_TIMEOUT(cond) \
do {\
	u32 timeout = STAR_TIMEOUT_COUNT; \
	while (!cond) { \
		if (--timeout == 0) \
			break; \
	}   \
	if (timeout == 0) { \
		STAR_PR_INFO("polling timeout in %s\n",  __func__); \
	} \
} while (0)
#endif

/* Star Ethernet Controller registers */
/* =============================== */
#define STAR_PHY_CTRL0(base) (base + 0x0000)
#define STAR_PHY_CTRL0_RWDATA_MASK (0xffff)
#define STAR_PHY_CTRL0_RWDATA_OFFSET (16)
#define STAR_PHY_CTRL0_RWOK BIT(15)
#define STAR_PHY_CTRL0_RDCMD BIT(14)
#define STAR_PHY_CTRL0_WTCMD  BIT(13)
#define STAR_PHY_CTRL0_PREG_MASK (0x1f)
#define STAR_PHY_CTRL0_PREG_OFFSET (8)
#define STAR_PHY_CTRL0_PA_MASK (0x1f)
#define STAR_PHY_CTRL0_PA_OFFSET (0)

#define STAR_PHY_CTRL1(base) (base + 0x0004)
#define STAR_PHY_CTRL1_APDIS BIT(31)
#define STAR_PHY_CTRL1_APEN (0 << 31)
#define STAR_PHY_CTRL1_phy_addr_MASK (0x1f)
#define STAR_PHY_CTRL1_phy_addr_OFFSET (24)
#define STAR_PHY_CTRL1_RGMII BIT(17)
#define STAR_PHY_CTRL1_REVMII BIT(16)
#define STAR_PHY_CTRL1_TXCLK_CKEN BIT(14)
#define STAR_PHY_CTRL1_FORCETXFC BIT(13)
#define STAR_PHY_CTRL1_FORCERXFC BIT(12)
#define STAR_PHY_CTRL1_FORCEFULL BIT(11)
#define STAR_PHY_CTRL1_FORCESPD_MASK (0x3)
#define STAR_PHY_CTRL1_FORCESPD_OFFSET (9)
#define STAR_PHY_CTRL1_FORCESPD_10M (0 << STAR_PHY_CTRL1_FORCESPD_OFFSET)
#define STAR_PHY_CTRL1_FORCESPD_100M BIT(9)
#define STAR_PHY_CTRL1_FORCESPD_1G (2 << STAR_PHY_CTRL1_FORCESPD_OFFSET)
#define STAR_PHY_CTRL1_FORCESPD_RESV (3 << STAR_PHY_CTRL1_FORCESPD_OFFSET)
#define STAR_PHY_CTRL1_ANEN BIT(8)
#define STAR_PHY_CTRL1_MIDIS BIT(7)
#define STAR_PHY_CTRL1_STA_TXFC BIT(6)
#define STAR_PHY_CTRL1_STA_RXFC BIT(5)
#define STAR_PHY_CTRL1_STA_FULL BIT(4)
#define STAR_PHY_CTRL1_STA_SPD_MASK (0x3)
#define STAR_PHY_CTRL1_STA_DPX_MASK (0x1)
#define STAR_PHY_CTRL1_STA_SPD_DPX_MASK (0x7)
#define STAR_PHY_CTRL1_STA_SPD_OFFSET (2)
#define STAR_PHY_CTRL1_STA_SPD_10M (0 << STAR_PHY_CTRL1_STA_SPD_OFFSET)
#define STAR_PHY_CTRL1_STA_SPD_100M  BIT(2)
#define STAR_PHY_CTRL1_STA_SPD_1G (2 << STAR_PHY_CTRL1_STA_SPD_OFFSET)
#define STAR_PHY_CTRL1_STA_SPD_RESV (3 << STAR_PHY_CTRL1_STA_SPD_OFFSET)
#define STAR_PHY_CTRL1_STA_TXCLK BIT(1)
#define STAR_PHY_CTRL1_STA_LINK BIT(0)
#define STAR_PHY_CTRL1_STA_10M_HALF (0x0)
#define STAR_PHY_CTRL1_STA_100M_HALF (0x1)
#define STAR_PHY_CTRL1_STA_10M_FULL (0x4)
#define STAR_PHY_CTRL1_STA_100M_FULL (0x5)

#define STAR_MAC_CFG(base) (base + 0x0008)
#define STAR_MAC_CFG_NICPD BIT(31)
#define STAR_MAC_CFG_WOLEN BIT(30)
#define STAR_MAC_CFG_NICPDRDY BIT(29)
#define STAR_MAC_CFG_TXCKSEN BIT(26)
#define STAR_MAC_CFG_RXCKSEN BIT(25)
#define STAR_MAC_CFG_ACPTCKSERR BIT(24)
#define STAR_MAC_CFG_ISTEN BIT(23)
#define STAR_MAC_CFG_VLANSTRIP BIT(22)
#define STAR_MAC_CFG_ACPTCRCERR BIT(21)
#define STAR_MAC_CFG_CRCSTRIP BIT(20)
#define STAR_MAC_CFG_TXAUTOPAD BIT(19)
#define STAR_MAC_CFG_ACPTLONGPKT BIT(18)
#define STAR_MAC_CFG_MAXLEN_MASK (0x3)
#define STAR_MAC_CFG_MAXLEN_OFFSET (16)
#define STAR_MAC_CFG_MAXLEN_1518 (0 << STAR_MAC_CFG_MAXLEN_OFFSET)
#define STAR_MAC_CFG_MAXLEN_1522  BIT(16)
#define STAR_MAC_CFG_MAXLEN_1536 (2 << STAR_MAC_CFG_MAXLEN_OFFSET)
#define STAR_MAC_CFG_MAXLEN_RESV (3 << STAR_MAC_CFG_MAXLEN_OFFSET)
#define STAR_MAC_CFG_IPG_MASK (0x1f)
#define STAR_MAC_CFG_IPG_OFFSET (10)
#define STAR_MAC_CFG_NSKP16COL BIT(9)
#define STAR_MAC_CFG_FASTBACKOFF BIT(8)
#define STAR_MAC_CFG_TXVLAN_ATPARSE BIT(0)

#define STAR_FC_CFG(base) (base + 0x000c)
#define STAR_FC_CFG_SENDPAUSETH_MASK (0xfff)
#define STAR_FC_CFG_SENDPAUSETH_OFFSET (16)
#define STAR_FC_CFG_COLCNT_CLR_MODE BIT(9)
#define STAR_FC_CFG_UCPAUSEDIS BIT(8)
#define STAR_FC_CFG_BPEN BIT(7)
#define STAR_FC_CFG_CRS_BP_MODE BIT(6)
#define STAR_FC_CFG_MAXBPCOLEN BIT(5)
#define STAR_FC_CFG_MAXBPCOLCNT_MASK (0x1f)
#define STAR_FC_CFG_MAXBPCOLCNT_OFFSET (0)
/* default value for SEND_PAUSE_TH */
#define STAR_FC_CFG_SEND_PAUSE_TH_DEF ((STAR_FC_CFG_SEND_PAUSE_TH_2K & \
				       STAR_FC_CFG_SENDPAUSETH_MASK) \
				       << STAR_FC_CFG_SENDPAUSETH_OFFSET)
#define STAR_FC_CFG_SEND_PAUSE_TH_2K (0x800)

#define STAR_ARL_CFG(base) (base + 0x0010)
#define STAR_ARL_CFG_FILTER_PRI_TAG BIT(6)
#define STAR_ARL_CFG_FILTER_VLAN_UNTAG BIT(5)
#define STAR_ARL_CFG_MISCMODE BIT(4)
#define STAR_ARL_CFG_MYMACONLY BIT(3)
#define STAR_ARL_CFG_CPULEARNDIS BIT(2)
#define STAR_ARL_CFG_RESVMCFILTER BIT(1)
#define STAR_ARL_CFG_HASHALG_CRCDA BIT(0)

#define star_my_mac_h(base) (base + 0x0014)
#define star_my_mac_l(base) (base + 0x0018)

#define star_hash_ctrl(base) (base + 0x001c)
#define STAR_HASH_CTRL_HASHEN BIT(31)
#define STAR_HASH_CTRL_HTBISTDONE BIT(17)
#define STAR_HASH_CTRL_HTBISTOK BIT(16)
#define STAR_HASH_CTRL_START BIT(14)
#define STAR_HASH_CTRL_ACCESSWT BIT(13)
#define STAR_HASH_CTRL_ACCESSRD (0 << 13)
#define STAR_HASH_CTRL_HBITDATA BIT(12)
#define STAR_HASH_CTRL_HBITADDR_MASK (0x1ff)
#define STAR_HASH_CTRL_HBITADDR_OFFSET (0)

#define star_vlan_ctrl(base) (base + 0x0020)
#define STAR_VLAN_ID_0_1(base) (base + 0x0024)
#define STAR_VLAN_ID_2_3(base) (base + 0x0028)

#define star_dummy(base) (base + 0x002C)
#define STAR_DUMMY_FPGA_MODE BIT(31)
#define STAR_DUMMY_E2_ECO BIT(7)
#define STAR_DUMMY_TXRXRDY BIT(1)
#define STAR_DUMMY_MDCMDIODONE BIT(0)

#define star_dma_cfg(base) (base + 0x0030)
#define STAR_DMA_CFG_RX2BOFSTDIS BIT(16)
#define STAR_DMA_CFG_TXPOLLPERIOD_MASK (0x3)
#define STAR_DMA_CFG_TXPOLLPERIOD_OFFSET (6)
#define STAR_DMA_CFG_TXPOLLPERIOD_1US (0 << STAR_DMA_CFG_TXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_TXPOLLPERIOD_10US BIT(6)
#define STAR_DMA_CFG_TXPOLLPERIOD_100US (2 << STAR_DMA_CFG_TXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_TXPOLLPERIOD_1000US (3 << STAR_DMA_CFG_TXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_TXPOLLEN BIT(5)
#define STAR_DMA_CFG_TXSUSPEND BIT(4)
#define STAR_DMA_CFG_RXPOLLPERIOD_MASK (0x3)
#define STAR_DMA_CFG_RXPOLLPERIOD_OFFSET (2)
#define STAR_DMA_CFG_RXPOLLPERIOD_1US (0 << STAR_DMA_CFG_RXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_RXPOLLPERIOD_10US BIT(2)
#define STAR_DMA_CFG_RXPOLLPERIOD_100US (2 << STAR_DMA_CFG_RXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_RXPOLLPERIOD_1000US (3 << STAR_DMA_CFG_RXPOLLPERIOD_OFFSET)
#define STAR_DMA_CFG_RXPOLLEN BIT(1)
#define STAR_DMA_CFG_RXSUSPEND BIT(0)

#define star_tx_dma_ctrl(base) (base + 0x0034)
#define TX_RESUME ((u32)0x01 << 2)
#define TX_STOP ((u32)0x01 << 1)
#define TX_START ((u32)0x01 << 0)

#define star_rx_dma_ctrl(base) (base + 0x0038)
#define STAR_TX_DPTR(base) (base + 0x003c)
#define STAR_RX_DPTR(base) (base + 0x0040)
#define STAR_TX_BASE_ADDR(base) (base + 0x0044)
#define STAR_RX_BASE_ADDR(base) (base + 0x0048)

#define star_int_sta(base) (base + 0x0050)
#define STAR_INT_STA_RX_PCODE BIT(10)
#define STAR_INT_STA_TX_SKIP BIT(9)
#define STAR_INT_STA_TXC BIT(8)
#define STAR_INT_STA_TXQE BIT(7)
#define STAR_INT_STA_RXC BIT(6)
#define STAR_INT_STA_RXQF BIT(5)
#define STAR_INT_STA_MAGICPKT BIT(4)
#define STAR_INT_STA_MIBCNTHALF BIT(3)
#define STAR_INT_STA_PORTCHANGE BIT(2)
#define STAR_INT_STA_RXFIFOFULL BIT(1)

#define star_int_mask(base) (base + 0x0054)
#define star_test0(base) (base + 0x0058)

#define star_test1(base) (base + 0x005c)
#define STAR_TEST1_RST_HASH_BIST BIT(31)
#define STAR_TEST1_EXTEND_RETRY BIT(20)

#define star_extend_cfg(base) (base + 0x0060)
#define STAR_EXTEND_CFG_SDPAUSEOFFTH_MASK (0xfff)
#define STAR_EXTEND_CFG_SDPAUSEOFFTH_OFFSET (16)
/* default value for SEND_PAUSE_RLS */
#define STAR_EXTEND_CFG_SEND_PAUSE_RLS_DEF \
		((STAR_EXTEND_CFG_SEND_PAUSE_RLS_1K & \
		STAR_EXTEND_CFG_SDPAUSEOFFTH_MASK) \
		<< STAR_EXTEND_CFG_SDPAUSEOFFTH_OFFSET)
#define STAR_EXTEND_CFG_SEND_PAUSE_RLS_1K (0x400)

#define ETHSYS_CONFIG(base)	(base + 0x94)
#define INT_PHY_SEL BIT(3)
#define SWC_MII_MODE BIT(2)
#define EXT_MDC_MODE BIT(1)
#define MII_PAD_OE BIT(0)

#define MAC_MODE_CONFIG(base) (base + 0x98)
#define BIG_ENDIAN BIT(0)

#define MAC_CLOCK_CONFIG(base)  (base + 0xac)
#define TXCLK_OUT_INV BIT(19)
#define RXCLK_OUT_INV BIT(18)
#define TXCLK_IN_INV BIT(17)
#define RXCLK_IN_INV BIT(16)
#define MDC_INV BIT(12)
#define MDC_NEG_LAT BIT(8)
#define MDC_DIV ((u32)0xFF << 0)
#define MDC_CLK_DIV_10 ((u32)0x0A << 0)

/* MIB Counter register */
#define STAR_MIB_RXOKPKT(base) (base + 0x0100)
#define STAR_MIB_RXOKBYTE(base) (base + 0x0104)
#define STAR_MIB_RXRUNT(base) (base + 0x0108)
#define STAR_MIB_RXOVERSIZE(base) (base + 0x010c)
#define STAR_MIB_RXNOBUFDROP(base) (base + 0x0110)
#define STAR_MIB_RXCRCERR(base) (base + 0x0114)
#define STAR_MIB_RXARLDROP(base) (base + 0x0118)
#define STAR_MIB_RXVLANDROP(base) (base + 0x011c)
#define STAR_MIB_RXCKSERR(base) (base + 0x0120)
#define STAR_MIB_RXPAUSE(base) (base + 0x0124)
#define STAR_MIB_TXOKPKT(base) (base + 0x0128)
#define STAR_MIB_TXOKBYTE(base) (base + 0x012c)
#define STAR_MIB_TXPAUSECOL(base) (base + 0x0130)

/**
 * @brief structure for Tx descriptor Ring
 */
struct tx_desc_s {
	/* Tx control and length */
	u32 ctrl_len;
/* Tx descriptor Own bit; 1: CPU own */
#define TX_COWN  BIT(31)
/* End of Tx descriptor ring */
#define TX_EOR BIT(30)
/* First Segment descriptor */
#define TX_FS BIT(29)
/* Last Segment descriptor */
#define TX_LS BIT(28)
/* Tx complete interrupt enable (when set, DMA generate
 * interrupt after tx sending out pkt)
 */
#define TX_INT BIT(27)
/* Insert VLAN Tag in the following word (in tdes2) */
#define TX_INSV BIT(26)
/* Enable IP checksum generation offload */
#define TX_ICO BIT(25)
/* Enable UDP checksum generation offload */
#define TX_UCO BIT(24)
/* Enable TCP checksum generation offload */
#define TX_TCO BIT(23)
/* Tx Segment Data length */
#define TX_LEN_MASK (0xffff)
#define TX_LEN_OFFSET (0)
	/* Tx segment data pointer */
	u32 buffer;
	u32 vtag;
/* VLAN Tag EPID */
#define TX_EPID_MASK (0xffff)
#define TX_EPID_OFFSET (16)
/* VLNA Tag Priority */
#define TX_PRI_MASK (0x7)
#define TX_PRI_OFFSET (13)
/* VLAN Tag CFI (Canonical Format Indicator) */
#define TX_CFI BIT(12)
/* VLAN Tag VID */
#define TX_VID_MASK (0xfff)
#define TX_VID_OFFSET (0)
	/* Tx pointer for external management usage */
	u32 reserve;
} tx_desc;

/* Rx Ring */
struct rx_desc_s {
	/* Rx control and length */
	u32 ctrl_len;
/* RX descriptor Own bit; 1: CPU own */
#define RX_COWN BIT(31)
/* End of Rx descriptor ring */
#define RX_EOR BIT(30)
/* First Segment descriptor */
#define RX_FS BIT(29)
/* Last Segment descriptor */
#define RX_LS BIT(28)
/* Rx packet is oversize */
#define RX_OSIZE BIT(25)
/* Rx packet is CRC Error */
#define RX_CRCERR BIT(24)
/* Rx packet DMAC is Reserved Multicast Address */
#define RX_RMC BIT(23)
/* Rx packet DMAC is hit in hash table */
#define RX_HHIT BIT(22)
/* Rx packet DMAC is My_MAC */
#define RX_MYMAC BIT(21)
/* VLAN Tagged int the following word */
#define RX_VTAG BIT(20)
#define RX_PROT_MASK (0x3)
#define RX_PROT_OFFSET (18)
/* Protocol: IPV4 */
#define RX_PROT_IP (0x0)
/* Protocol: UDP */
#define RX_PROT_UDP (0x1)
/* Protocol: TCP */
#define RX_PROT_TCP (0x2)
/* Protocol: TCP */
#define RX_PROT_OTHERS (0x3)
/* IP checksum fail (meaningful when PROT is IPV4) */
#define RX_IPF BIT(17)
/* Layer-4 checksum fail (meaningful when PROT is UDP or TCP) */
#define RX_L4F BIT(16)
/* Segment Data length(FS=0) / Whole Packet Length(FS=1) */
#define RX_LEN_MASK (0xffff)
#define RX_LEN_OFFSET			(0)
	/* RX segment data pointer */
	u32 buffer;

	u32 vtag;
#define RX_EPID_MASK			(0xffff)	/* VLAN Tag EPID */
#define RX_EPID_OFFSET			(16)
#define RX_PRI_MASK (0x7)		/* VLAN Tag Priority */
#define RX_PRI_OFFSET			(13)
#define RX_CFI BIT(12)
#define RX_VID_MASK (0xfff)		/* VLAN Tag VID */
#define RX_VID_OFFSET			(0)
	u32 reserve;	/* Rx pointer for external management usage */
} rx_desc;

struct star_dev_s {
	void __iomem *base;               /* Base register of Star Ethernet */
	void __iomem *pericfg_base;            /* Base register of PERICFG */
	tx_desc *tx_desc;         /* Base Address of Tx descriptor Ring */
	rx_desc *rx_desc;         /* Base Address of Rx descriptor Ring */
	u32 tx_ring_size;
	u32 rx_ring_size;
	u32 tx_head;             /* Head of Tx descriptor (least sent) */
	u32 tx_tail;             /* Tail of Tx descriptor (least be free) */
	u32 rx_head;             /* Head of Rx descriptor (least sent) */
	u32 rx_tail;             /* Tail of Rx descriptor (least be free) */
	u32 tx_num;
	u32 rx_num;
	u32 link_up;             /*link status */
	void *star_prv;
	struct net_device_stats stats;
	struct eth_phy_ops *phy_ops;
	struct device *dev;
} star_dev;

int star_hw_init(star_dev *dev);

u16 star_mdc_mdio_read(star_dev *dev, u32 phy_addr, u32 phy_reg);
void star_mdc_mdio_write(star_dev *dev, u32 phy_addr, u32 phy_reg, u16 value);

int star_dma_init(star_dev *dev, uintptr_t desc_viraddr,
		  dma_addr_t desc_dmaaddr);
int star_dma_tx_set(star_dev *dev, u32 buffer,
		    u32 length, uintptr_t extBuf);
int star_dma_tx_get(star_dev *dev, u32 *buffer,
		    u32 *ctrl_len, uintptr_t *extBuf);
int star_dma_rx_set(star_dev *dev, u32 buffer,
		    u32 length, uintptr_t extBuf);
int star_dma_rx_get(star_dev *dev, u32 *buffer,
		    u32 *ctrl_len, uintptr_t *extBuf);
void star_dma_tx_stop(star_dev *dev);
void star_dma_rx_stop(star_dev *dev);

int star_mac_init(star_dev *dev, u8 mac_addr[6]);

int star_mib_init(star_dev *dev);
int star_phyctrl_init(star_dev *dev, u32 enable, u32 phy_addr);
void star_set_hashbit(star_dev *dev, u32 addr, u32 value);

void star_link_status_change(star_dev *dev);
void star_nic_pdset(star_dev *dev, bool flag);

void star_config_wol(star_dev *star_dev, bool enable);
void enable_eth_wol(star_dev *star_dev);
void disable_eth_wol(star_dev *star_dev);
void star_switch_to_rmii_mode(star_dev *star_dev);
u32 desc_tx_empty(tx_desc *tx_desc);
u32 desc_rx_empty(rx_desc *rx_desc);
#endif
