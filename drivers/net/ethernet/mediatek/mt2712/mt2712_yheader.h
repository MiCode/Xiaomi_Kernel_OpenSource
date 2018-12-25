/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT2712__YHEADER__
#define __MT2712__YHEADER__
/* OS Specific declarations and definitions */
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/cdev.h>

#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#include <linux/in.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/ptrace.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/bitops.h>
#include <linux/mii.h>
#include <asm/processor.h>
#include <asm/dma.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <net/checksum.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <asm-generic/errno.h>
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define ENABLE_VLAN_TAG
#include <linux/if_vlan.h>
#endif
/* for PTP */
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/clocksource.h>

#define Y_TRUE 1
#define Y_FALSE 0
#define Y_SUCCESS 0
#define Y_FAILURE 1

#define DEV_NAME "MT2712_ETH"

extern unsigned long dwc_eth_qos_platform_base_addr;

/* Error and status macros defined below */
#define E_DMA_SR_TPS 6
#define E_DMA_SR_TBU 7
#define E_DMA_SR_RBU 8
#define E_DMA_SR_RPS 9
#define S_DMA_SR_RWT 2
#define E_DMA_SR_FBE 10

#define TX_DESC_CNT 256
#define RX_DESC_CNT 256
#define MIN_RX_DESC_CNT 16
#define TX_BUF_SIZE 1536
#define RX_BUF_SIZE 1568

#define FIFO_SIZE_B(x) (x)
#define FIFO_SIZE_KB(x) ((x) * 1024)

/* for testing purpose: 4 KB Maximum data per buffer pointer(in Bytes) */
#define MAX_DATA_PER_TX_BUF ((int)BIT(12))
/* Maxmimum data per descriptor(in Bytes) */
#define MAX_DATA_PER_TXD (MAX_DATA_PER_TX_BUF * 2)

#define GET_TX_PKT_FEATURES_PTR (&pdata->tx_pkt_features)

#define MAX_TX_QUEUE_CNT 8
#define MAX_RX_QUEUE_CNT 8

#define MTK_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

/* Default MTL queue operation mode values */
#define Q_DISABLED	0x0
#define Q_AVB		0x1
#define Q_DCB		0x2
#define Q_GENERIC	0x3

/* MII/GMII register offset */
#define AUTO_NEGO_NP    0x0007
#define PHY_CTL     0x0010
#define PHY_STS     0x0011

struct s_RX_CONTEXT_DESC {
	unsigned int RDES0;
	unsigned int RDES1;
	unsigned int RDES2;
	unsigned int RDES3;
};

struct s_TX_CONTEXT_DESC {
	unsigned int TDES0;
	unsigned int TDES1;
	unsigned int TDES2;
	unsigned int TDES3;
};

struct s_RX_NORMAL_DESC {
	unsigned int RDES0;
	unsigned int RDES1;
	unsigned int RDES2;
	unsigned int RDES3;
};

struct s_TX_NORMAL_DESC {
	unsigned int TDES0;
	unsigned int TDES1;
	unsigned int TDES2;
	unsigned int TDES3;
};

struct s_rx_pkt_features {
	unsigned int pkt_attributes;
};

struct s_tx_pkt_features {
	unsigned int pkt_attributes;
	unsigned long hdr_len;
	unsigned long pay_len;
};

#define RDESC3_OWN	0x80000000
#define RDESC3_FD	0x20000000
#define RDESC3_LD	0x10000000
#define RDESC3_RS2V	0x08000000
#define RDESC3_RS1V	0x04000000
#define RDESC3_RS0V	0x02000000
#define RDESC3_LT	0x00070000
#define RDESC3_ES	0x00008000
#define RDESC3_PL	0x00007FFF

/* Maximum size of pkt that is copied to a new buffer on receive */
#define COPYBREAK_DEFAULT 256
#define SYSCLOCK	62000000 /* System clock is 62.5MHz */
#define SYSTIMEPERIOD	20 /* System time period is 16ns */

/* MAC Time stamp contorl reg bit fields */
#define MAC_TCR_TSENA         0x00000001 /* Enable timestamp */
#define MAC_TCR_TSCFUPDT      0x00000002 /* Enable Fine Timestamp Update */
#define MAC_TCR_TSENALL       0x00000100 /* Enable timestamping for all packets */
#define MAC_TCR_TSCTRLSSR     0x00000200 /* Enable Timestamp Digitla Contorl (1ns accuracy )*/
#define MAC_TCR_TSVER2ENA     0x00000400 /* Enable PTP packet processing for Version 2 Formate */
#define MAC_TCR_TSIPENA       0x00000800 /* Enable processing of PTP over Ethernet Packets */
#define MAC_TCR_TSIPV6ENA     0x00001000 /* Enable processing of PTP Packets sent over IPv6-UDP Packets */
#define MAC_TCR_TSIPV4ENA     0x00002000 /* Enable processing of PTP Packets sent over IPv4-UDP Packets */
#define MAC_TCR_TSEVENTENA    0x00004000 /* Enable Timestamp Snapshot for Event Messages */
#define MAC_TCR_TSMASTERENA   0x00008000 /* Enable snapshot for Message Relevant to Master */
#define MAC_TCR_SNAPTYPSEL_1  0x00010000 /* select PTP packets for taking snapshots */
#define MAC_TCR_SNAPTYPSEL_2  0x00020000
#define MAC_TCR_SNAPTYPSEL_3  0x00030000
#define MAC_TCR_AV8021ASMEN   0x10000000 /* Enable AV 802.1AS Mode */

/* Helper macro for handling coalesce parameters via ethtool */
/* Obtained by trial and error  */
#define OPTIMAL_DMA_RIWT_USEC  124
/* Max delay before RX interrupt after a pkt is received Max
 * delay in usecs is 1020 for 62.5MHz device clock
 */
#define MAX_DMA_RIWT  0xff
/* Max no of pkts to be received before an RX interrupt */
#define RX_MAX_FRAMES 16

#define TX_QUEUE_CNT (pdata->tx_queue_cnt)
#define RX_QUEUE_CNT (pdata->rx_queue_cnt)

/* Helper macros for TX descriptor handling */

#define GET_TX_QUEUE_PTR(q_inx) (&pdata->tx_queue[(q_inx)])

#define GET_TX_DESC_PTR(q_inx, d_inx) (pdata->tx_queue[(q_inx)].tx_desc_data.tx_desc_ptrs[(d_inx)])

#define GET_TX_DESC_DMA_ADDR(q_inx, d_inx) (pdata->tx_queue[(q_inx)].tx_desc_data.tx_desc_dma_addrs[(d_inx)])

#define GET_TX_WRAPPER_DESC(q_inx) (&pdata->tx_queue[(q_inx)].tx_desc_data)

#define GET_TX_BUF_PTR(q_inx, d_inx) (pdata->tx_queue[(q_inx)].tx_desc_data.tx_buf_ptrs[(d_inx)])

#define INCR_TX_DESC_INDEX(inx, offset) do {\
	(inx) += (offset);\
	if ((inx) >= TX_DESC_CNT)\
		(inx) = ((inx) - TX_DESC_CNT);\
} while (0)

#define DECR_TX_DESC_INDEX(inx) do {\
	(inx)--;\
	if ((inx) < 0)\
		(inx) = (TX_DESC_CNT + (inx));\
} while (0)

#define INCR_TX_LOCAL_INDEX(inx, offset)\
	(((inx) + (offset)) >= TX_DESC_CNT ?\
	((inx) + (offset) - TX_DESC_CNT) : ((inx) + (offset)))

#define GET_CURRENT_XFER_DESC_CNT(q_inx) (pdata->tx_queue[(q_inx)].tx_desc_data.packet_count)

#define GET_CURRENT_XFER_LAST_DESC_INDEX(q_inx, start_index, offset)\
	((GET_CURRENT_XFER_DESC_CNT((q_inx)) == 0) ? (TX_DESC_CNT - 1) :\
	((GET_CURRENT_XFER_DESC_CNT((q_inx)) == 1) ? (INCR_TX_LOCAL_INDEX((start_index), (offset))) :\
	INCR_TX_LOCAL_INDEX((start_index), (GET_CURRENT_XFER_DESC_CNT((q_inx)) + (offset) - 1))))

#define GET_TX_TOT_LEN(buffer, start_index, packet_count, total_len) do {\
	int i, pkt_idx = (start_index);\
	for (i = 0; i < (packet_count); i++) {\
		(total_len) += ((buffer)[pkt_idx].len + (buffer)[pkt_idx].len2);\
		pkt_idx = INCR_TX_LOCAL_INDEX(pkt_idx, 1);\
	} \
} while (0)

/* Helper macros for RX descriptor handling */

#define GET_RX_QUEUE_PTR(q_inx) (&pdata->rx_queue[(q_inx)])

#define GET_RX_DESC_PTR(q_inx, d_inx) (pdata->rx_queue[(q_inx)].rx_desc_data.rx_desc_ptrs[(d_inx)])

#define GET_RX_DESC_DMA_ADDR(q_inx, d_inx) (pdata->rx_queue[(q_inx)].rx_desc_data.rx_desc_dma_addrs[(d_inx)])

#define GET_RX_WRAPPER_DESC(q_inx) (&pdata->rx_queue[(q_inx)].rx_desc_data)

#define GET_RX_BUF_PTR(q_inx, d_inx) (pdata->rx_queue[(q_inx)].rx_desc_data.rx_buf_ptrs[(d_inx)])

#define INCR_RX_DESC_INDEX(inx, offset) do {\
	(inx) += (offset);\
	if ((inx) >= RX_DESC_CNT)\
		(inx) = ((inx) - RX_DESC_CNT);\
} while (0)

#define DECR_RX_DESC_INDEX(inx) do {\
	(inx)--;\
	if ((inx) < 0)\
		(inx) = (RX_DESC_CNT + (inx));\
} while (0)

#define INCR_RX_LOCAL_INDEX(inx, offset)\
	(((inx) + (offset)) >= RX_DESC_CNT ?\
	((inx) + (offset) - RX_DESC_CNT) : ((inx) + (offset)))

#define GET_CURRENT_RCVD_DESC_CNT(q_inx) (pdata->rx_queue[(q_inx)].rx_desc_data.pkt_received)

#define GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, offset) (RX_DESC_CNT - 1)

#define GET_TX_DESC_IDX(q_inx, desc) (((desc) - GET_TX_DESC_DMA_ADDR((q_inx), 0)) / (sizeof(struct s_TX_NORMAL_DESC)))

#define GET_RX_DESC_IDX(q_inx, desc) (((desc) - GET_RX_DESC_DMA_ADDR((q_inx), 0)) / (sizeof(struct s_RX_NORMAL_DESC)))

enum mtl_fifo_size {
	e_256 = 0x0,
	e_512 = 0x1,
	e_1k = 0x3,
	e_2k = 0x7,
	e_4k = 0xf,
	e_8k = 0x1f,
	e_16k = 0x3f,
	e_32k = 0x7f
};

/* Hash Table Reg count */
#define HTR_CNT (pdata->max_hash_table_size / 32)

/* For handling differnet PHY interfaces */
#define GMII_MII	0x0
#define RGMII	0x1
#define SGMII	0x2
#define TBI		0x3
#define RMII	0x4
#define RTBI	0x5
#define SMII	0x6
#define REVMII	0x7

/* do forward declaration of private data structure */
struct prv_data;
struct tx_wrapper_descriptor;

struct hw_if_struct {
	int (*init)(struct prv_data *pdata);
	int (*exit)(void);

	int (*tx_complete)(struct s_TX_NORMAL_DESC *txdesc);

	/* for handling multi-queue */
	int (*disable_rx_interrupt)(unsigned int);
	int (*enable_rx_interrupt)(unsigned int);

	int (*read_phy_regs)(int, int, int *phy_reg_data);
	int (*write_phy_regs)(int, int, int);
	int (*set_full_duplex)(void);
	int (*set_half_duplex)(void);
	int (*set_mii_speed_100)(void);
	int (*set_mii_speed_10)(void);
	int (*set_gmii_speed)(void);

	/* for FLOW ctrl */
	int (*enable_rx_flow_ctrl)(void);
	int (*disable_rx_flow_ctrl)(void);
	int (*enable_tx_flow_ctrl)(unsigned int);
	int (*disable_tx_flow_ctrl)(unsigned int);

	/* for hw time stamping */
	int (*config_hw_time_stamping)(unsigned int);
	int (*config_sub_second_increment)(unsigned long ptp_clock);
	int (*init_systime)(unsigned int, unsigned int);
	int (*config_addend)(unsigned int);
	int (*adjust_systime)(unsigned int, unsigned int, int, bool);
	unsigned long long (*get_systime)(void);
	unsigned int (*get_tx_tstamp_status)(struct s_TX_NORMAL_DESC *txdesc);
	unsigned long long (*get_tx_tstamp)(struct s_TX_NORMAL_DESC *txdesc);
	unsigned int (*get_tx_tstamp_status_via_reg)(void);
	unsigned long long (*get_tx_tstamp_via_reg)(void);
	unsigned int (*rx_tstamp_available)(struct s_RX_NORMAL_DESC *rxdesc);
	unsigned int (*get_rx_tstamp_status)(struct s_RX_CONTEXT_DESC *rxdesc);
	unsigned long long (*get_rx_tstamp)(struct s_RX_CONTEXT_DESC *rxdesc);
	int (*drop_tx_status_enabled)(void);

	int (*tx_aborted_error)(struct s_TX_NORMAL_DESC *txdesc);
	int (*tx_carrier_lost_error)(struct s_TX_NORMAL_DESC *txdesc);
	int (*tx_fifo_underrun)(struct s_TX_NORMAL_DESC *txdesc);

	void (*tx_desc_init)(struct prv_data *pdata, unsigned int q_inx);
	void (*rx_desc_init)(struct prv_data *pdata, unsigned int q_inx);
	void (*rx_desc_reset)(unsigned int, struct prv_data *pdata, unsigned int, unsigned int q_inx);
	int (*tx_desc_reset)(unsigned int, struct prv_data *pdata, unsigned int q_inx);

	/* last tx segmnet reports the tx status */
	int (*get_tx_desc_ls)(struct s_TX_NORMAL_DESC *txdesc);
	int (*get_tx_desc_ctxt)(struct s_TX_NORMAL_DESC *txdesc);
	void (*update_rx_tail_ptr)(unsigned int q_inx, unsigned int dma_addr);

	int (*start_dma_rx)(unsigned int);
	int (*stop_dma_rx)(unsigned int);
	int (*start_dma_tx)(unsigned int);
	int (*stop_dma_tx)(unsigned int);
	int (*start_mac_tx_rx)(void);
	int (*stop_mac_tx_rx)(void);

	int (*config_mac_pkt_filter_reg)(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);
	void (*pre_xmit)(struct prv_data *pdata, unsigned int q_inx);

	/* for RX watchdog timer */
	int (*config_rx_watchdog)(unsigned int, u32 riwt);

	int (*update_mac_addr32_127_low_high_reg)(int idx, unsigned char addr[]);
	int (*update_mac_addr1_31_low_high_reg)(int idx, unsigned char addr[]);
	int (*update_hash_table_reg)(int idx, unsigned int data);
};

/* wrapper buffer structure to hold transmit pkt details */
struct tx_buffer {
	dma_addr_t dma;		/* dma address of skb */
	struct sk_buff *skb;	/* virtual address of skb */
	unsigned short len;	/* length of first skb */
	unsigned char buf1_mapped_as_page;

	dma_addr_t dma2; /* dam address of second skb */
	unsigned short len2; /* length of second skb */
	unsigned char buf2_mapped_as_page;

};

struct tx_wrapper_descriptor {
	char *desc_name;	/* ID of descriptor */

	void *tx_desc_ptrs[TX_DESC_CNT];
	dma_addr_t tx_desc_dma_addrs[TX_DESC_CNT];

	struct tx_buffer *tx_buf_ptrs[TX_DESC_CNT];

	unsigned char contigous_mem;

	int cur_tx;	/* always gives index of desc which has to
			 * be used for current xfer
			 */
	int dirty_tx;	/* always gives index of desc which has to
			 * be checked for xfer complete
			 */
	unsigned int free_desc_cnt;	/* always gives total number of available
					 * free desc count for driver
					 */
	unsigned int tx_pkt_queued;	/* always gives total number of packets
					 * queued for transmission
					 */
	unsigned int queue_stopped;
	int packet_count;
};

struct tx_queue {
	/* Tx descriptors */
	struct tx_wrapper_descriptor tx_desc_data;
	int q_op_mode;
};

/* wrapper buffer structure to hold received pkt details */
struct rx_buffer {
	dma_addr_t dma;		/* dma address of skb */
	struct sk_buff *skb;	/* virtual address of skb */
	unsigned short len;	/* length of received packet */
	struct page *page;	/* page address */
	bool good_pkt;		/* set to 1 if it is good packet else
				 *set to 0
				 */
	unsigned int inte;	/* set to non-zero if INTE is set for
				 * corresponding desc
				 */

	dma_addr_t dma2;	/* dma address of second skb */
	struct page *page2;	/* page address of second buffer */
	unsigned short len2;	/* length of received packet-second buffer */

	unsigned short rx_hdr_size; /* header buff size in case of split header */
};

struct rx_wrapper_descriptor {
	char *desc_name;	/* ID of descriptor */

	void *rx_desc_ptrs[RX_DESC_CNT];
	dma_addr_t rx_desc_dma_addrs[RX_DESC_CNT];

	struct rx_buffer *rx_buf_ptrs[RX_DESC_CNT];

	unsigned char contigous_mem;

	int cur_rx;	/* always gives index of desc which needs to
			 * be checked for packet availabilty
			 */
	int dirty_rx;
	unsigned int pkt_received;	/* always gives total number of packets
					 * received from device in one RX interrupt
					 */
	unsigned int skb_realloc_idx;
	unsigned int skb_realloc_threshold;

	/* for rx coalesce schem */
	int use_riwt;	/* set to 1 if RX watchdog timer should be used
			 * for RX interrupt mitigation
			 */
	u32 rx_riwt;
	u32 rx_coal_frames;	/* Max no of pkts to be received before an RX interrupt */
};

struct rx_queue {
	/* Rx descriptors */
	struct rx_wrapper_descriptor rx_desc_data;
	struct napi_struct napi;
	struct prv_data *pdata;
};

struct desc_if_struct {
	int (*alloc_queue_struct)(struct prv_data *pdata);
	void (*free_queue_struct)(struct prv_data *pdata);
	int (*alloc_buff_and_desc)(struct prv_data *pdata);
	void (*realloc_skb)(struct prv_data *pdata, unsigned int q_inx);
	void (*unmap_rx_skb)(struct prv_data *pdata, struct rx_buffer *buffer);
	void (*unmap_tx_skb)(struct prv_data *pdata, struct tx_buffer *buffer);
	unsigned int (*map_tx_skb)(struct net_device *dev, struct sk_buff *skb);
	void (*tx_free_mem)(struct prv_data *pdata);
	void (*rx_free_mem)(struct prv_data *pdata);
	void (*wrapper_tx_desc_init)(struct prv_data *pdata);
	void (*wrapper_tx_desc_init_single_q)(struct prv_data *pdata, unsigned int q_inx);
	void (*wrapper_rx_desc_init)(struct prv_data *pdata);
	void (*wrapper_rx_desc_init_single_q)(struct prv_data *pdata, unsigned int q_inx);
	void (*rx_skb_free_mem)(struct prv_data *pdata, unsigned int rx_qcnt);
	void (*rx_skb_free_mem_single_q)(struct prv_data *pdata, unsigned int q_inx);
	void (*tx_skb_free_mem)(struct prv_data *pdata, unsigned int tx_qcnt);
	void (*tx_skb_free_mem_single_q)(struct prv_data *pdata, unsigned int q_inx);
	int (*handle_tso)(struct net_device *dev, struct sk_buff *skb);
};

struct hw_features {
	/* HW Feature Register0 */
	unsigned int mii_sel;	/* 10/100 Mbps support */
	unsigned int gmii_sel;	/* 1000 Mbps support */
	unsigned int hd_sel;	/* Half-duplex support */
	unsigned int pcs_sel;	/* PCS registers(TBI, SGMII or RTBI PHY interface) */
	unsigned int vlan_hash_en;	/* VLAN Hash filter selected */
	unsigned int sma_sel;	/* SMA(MDIO) Interface */
	unsigned int rwk_sel;	/* PMT remote wake-up packet */
	unsigned int mgk_sel;	/* PMT magic packet */
	unsigned int mmc_sel;	/* RMON module */
	unsigned int arp_offld_en;	/* ARP Offload features is selected */
	unsigned int ts_sel;	/* IEEE 1588-2008 Adavanced timestamp */
	unsigned int eee_sel;	/* Energy Efficient Ethernet is enabled */
	unsigned int tx_coe_sel;	/* Tx Checksum Offload is enabled */
	unsigned int rx_coe_sel;	/* Rx Checksum Offload is enabled */
	unsigned int mac_addr16_sel;	/* MAC Addresses 1-16 are selected */
	unsigned int mac_addr32_sel;	/* MAC Addresses 32-63 are selected */
	unsigned int mac_addr64_sel;	/* MAC Addresses 64-127 are selected */
	unsigned int tsstssel;	/* Timestamp System Time Source */
	unsigned int speed_sel;	/* Speed Select */
	unsigned int sa_vlan_ins;	/* Source Address or VLAN Insertion */
	unsigned int act_phy_sel;	/* Active PHY Selected */

	/* HW Feature Register1 */
	unsigned int rx_fifo_size;	/* MTL Receive FIFO Size */
	unsigned int tx_fifo_size;	/* MTL Transmit FIFO Size */
	unsigned int adv_ts_hword;	/* Advance timestamping High Word selected */
	unsigned int dcb_en;	/* DCB Feature Enable */
	unsigned int sph_en;	/* Split Header Feature Enable */
	unsigned int tso_en;	/* TCP Segmentation Offload Enable */
	unsigned int dma_debug_gen;	/* DMA debug registers are enabled */
	unsigned int av_sel;	/* AV Feature Enabled */
	unsigned int lp_mode_en;	/* Low Power Mode Enabled */
	unsigned int hash_tbl_sz;	/* Hash Table Size */
	unsigned int l3l4_filter_num;	/* Total number of L3-L4 Filters */

	/* HW Feature Register2 */
	unsigned int rx_q_cnt;	/* Number of MTL Receive Queues */
	unsigned int tx_q_cnt;	/* Number of MTL Transmit Queues */
	unsigned int rx_ch_cnt;	/* Number of DMA Receive Channels */
	unsigned int tx_ch_cnt;	/* Number of DMA Transmit Channels */
	unsigned int pps_out_num;	/* Number of PPS outputs */
	unsigned int aux_snap_num;	/* Number of Auxiliary snapshot inputs */
};

/* structure to hold MMC values */
struct mmc_counters {
	/* MMC TX counters */
	unsigned long mmc_tx_octetcount_gb;
	unsigned long mmc_tx_framecount_gb;
	unsigned long mmc_tx_broadcastframe_g;
	unsigned long mmc_tx_multicastframe_g;
	unsigned long mmc_tx_64_octets_gb;
	unsigned long mmc_tx_65_to_127_octets_gb;
	unsigned long mmc_tx_128_to_255_octets_gb;
	unsigned long mmc_tx_256_to_511_octets_gb;
	unsigned long mmc_tx_512_to_1023_octets_gb;
	unsigned long mmc_tx_1024_to_max_octets_gb;
	unsigned long mmc_tx_unicast_gb;
	unsigned long mmc_tx_multicast_gb;
	unsigned long mmc_tx_broadcast_gb;
	unsigned long mmc_tx_underflow_error;
	unsigned long mmc_tx_singlecol_g;
	unsigned long mmc_tx_multicol_g;
	unsigned long mmc_tx_deferred;
	unsigned long mmc_tx_latecol;
	unsigned long mmc_tx_exesscol;
	unsigned long mmc_tx_carrier_error;
	unsigned long mmc_tx_octetcount_g;
	unsigned long mmc_tx_framecount_g;
	unsigned long mmc_tx_excessdef;
	unsigned long mmc_tx_pause_frame;
	unsigned long mmc_tx_vlan_frame_g;
	unsigned long mmc_tx_osize_frame_g;

	/* MMC RX counters */
	unsigned long mmc_rx_framecount_gb;
	unsigned long mmc_rx_octetcount_gb;
	unsigned long mmc_rx_octetcount_g;
	unsigned long mmc_rx_broadcastframe_g;
	unsigned long mmc_rx_multicastframe_g;
	unsigned long mmc_rx_crc_errror;
	unsigned long mmc_rx_align_error;
	unsigned long mmc_rx_run_error;
	unsigned long mmc_rx_jabber_error;
	unsigned long mmc_rx_undersize_g;
	unsigned long mmc_rx_oversize_g;
	unsigned long mmc_rx_64_octets_gb;
	unsigned long mmc_rx_65_to_127_octets_gb;
	unsigned long mmc_rx_128_to_255_octets_gb;
	unsigned long mmc_rx_256_to_511_octets_gb;
	unsigned long mmc_rx_512_to_1023_octets_gb;
	unsigned long mmc_rx_1024_to_max_octets_gb;
	unsigned long mmc_rx_unicast_g;
	unsigned long mmc_rx_length_error;
	unsigned long mmc_rx_outofrangetype;
	unsigned long mmc_rx_pause_frames;
	unsigned long mmc_rx_fifo_overflow;
	unsigned long mmc_rx_vlan_frames_gb;
	unsigned long mmc_rx_watchdog_error;
	unsigned long mmc_rx_receive_error;
	unsigned long mmc_rx_ctrl_frames_g;

	/* IPC */
	unsigned long mmc_rx_ipc_intr_mask;
	unsigned long mmc_rx_ipc_intr;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd;
	unsigned long mmc_rx_ipv4_hderr;
	unsigned long mmc_rx_ipv4_nopay;
	unsigned long mmc_rx_ipv4_frag;
	unsigned long mmc_rx_ipv4_udsbl;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd_octets;
	unsigned long mmc_rx_ipv6_hderr_octets;
	unsigned long mmc_rx_ipv6_nopay_octets;

	/* Protocols */
	unsigned long mmc_rx_udp_gd;
	unsigned long mmc_rx_udp_err;
	unsigned long mmc_rx_tcp_gd;
	unsigned long mmc_rx_tcp_err;
	unsigned long mmc_rx_icmp_gd;
	unsigned long mmc_rx_icmp_err;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd_octets;
	unsigned long mmc_rx_ipv4_hderr_octets;
	unsigned long mmc_rx_ipv4_nopay_octets;
	unsigned long mmc_rx_ipv4_frag_octets;
	unsigned long mmc_rx_ipv4_udsbl_octets;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd;
	unsigned long mmc_rx_ipv6_hderr;
	unsigned long mmc_rx_ipv6_nopay;

	/* Protocols */
	unsigned long mmc_rx_udp_gd_octets;
	unsigned long mmc_rx_udp_err_octets;
	unsigned long mmc_rx_tcp_gd_octets;
	unsigned long mmc_rx_tcp_err_octets;
	unsigned long mmc_rx_icmp_gd_octets;
	unsigned long mmc_rx_icmp_err_octets;
};

struct extra_stats {
	unsigned long q_re_alloc_rx_buf_failed[8];

	/* Tx/Rx IRQ error info */
	unsigned long tx_process_stopped_irq_n[8];
	unsigned long rx_process_stopped_irq_n[8];
	unsigned long tx_buf_unavailable_irq_n[8];
	unsigned long rx_buf_unavailable_irq_n[8];
	unsigned long rx_watchdog_irq_n;
	unsigned long fatal_bus_error_irq_n;
	/* Tx/Rx IRQ Events */
	unsigned long tx_normal_irq_n[8];
	unsigned long rx_normal_irq_n[8];
	unsigned long napi_poll_n;
	unsigned long tx_clean_n[8];
	/* Tx/Rx frames */
	unsigned long tx_pkt_n;
	unsigned long rx_pkt_n;
	unsigned long tx_timestamp_captured_n;
	unsigned long rx_timestamp_captured_n;

	/* Tx/Rx frames per channels/queues */
	unsigned long q_tx_pkt_n[8];
	unsigned long q_rx_pkt_n[8];
};

struct prv_data {
	struct net_device *dev;
	struct platform_device *pdev;
	int mac_result;
	struct clk *peri_axi, *peri_apb, *ptp_clk, *ext_125m_clk, *ptp_parent_clk;

	spinlock_t lock;	/* rx lock */
	spinlock_t tx_lock;	/* tx lock */
	spinlock_t pmt_lock;
	int irq_number;
	struct hw_if_struct hw_if;
	struct desc_if_struct desc_if;

	struct s_rx_pkt_features rx_pkt_features;
	struct s_tx_pkt_features tx_pkt_features;

	/* TX Queue */
	struct tx_queue *tx_queue;
	unsigned char tx_queue_cnt;
	unsigned int tx_q_inx;

	/* RX Queue */
	struct rx_queue *rx_queue;
	unsigned char rx_queue_cnt;
	unsigned int rx_q_inx;

	struct mii_bus *mii;
	struct phy_device *phydev;
	int oldlink;
	int speed;
	int oldduplex;
	int phyaddr;
	int bus_id;
	u32 interface;

/* Helper macros for handling FLOW control in HW */
#define MTK_FLOW_CTRL_OFF 0
#define MTK_FLOW_CTRL_RX  1
#define MTK_FLOW_CTRL_TX  2
#define MTK_FLOW_CTRL_TX_RX (MTK_FLOW_CTRL_TX |\
					MTK_FLOW_CTRL_RX)

	unsigned int flow_ctrl;

	/* keeps track of previous programmed flow control options */
	unsigned int oldflow_ctrl;

	struct hw_features hw_feat;

	/* AXI parameters */
	unsigned int axi_pbl;
	unsigned int axi_worl;
	unsigned int axi_rorl;

	int (*clean_rx)(struct prv_data *pdata, int quota, unsigned int q_inx);
	int (*alloc_rx_buf)(struct prv_data *pdata,
			    struct rx_buffer *buffer, gfp_t gfp);
	unsigned int rx_buffer_len;

	struct mmc_counters mmc;
	struct extra_stats xstats;

	/* for hw time stamping */
	unsigned char hwts_tx_en;
	unsigned char hwts_rx_en;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	spinlock_t ptp_lock; /* protects registers */
	u64 default_addend;
	/* set to 1 if one nano second accuracy is enabled else set to zero */
	bool one_nsec_accuracy;

	/* for filtering */
	int max_hash_table_size;
	int max_addr_reg_cnt;

	unsigned char vlan_hash_filtering;
	unsigned int l2_filtering_mode; /* 0 - if perfect and 1 - if hash filtering */

	/* For handling PCS(TBI/RTBI/SGMII) and RGMII/SMII interface */
	unsigned int pcs_link;
	unsigned int pcs_duplex;
	unsigned int pcs_speed;
	unsigned int pause;
	unsigned int duplex;
	unsigned int lp_pause;
	unsigned int lp_duplex;
};

/* Function prototypes*/

void init_function_ptrs_dev(struct hw_if_struct *hw_if);
void init_function_ptrs_desc(struct desc_if_struct *desc_if);
const struct net_device_ops *get_netdev_ops(void);
const struct ethtool_ops *get_ethtool_ops(void);
int poll_mq(struct napi_struct *napi, int budget);
void get_pdata(struct prv_data *pdata);
int start_xmit(struct sk_buff *skb, struct net_device *dev);
int mdio_register(struct net_device *dev);
void mdio_unregister(struct net_device *dev);
int mdio_read_direct(struct prv_data *pdata,
		     int phyaddr, int phyreg, int *phydata);
int mdio_write_direct(struct prv_data *pdata,
		      int phyaddr, int phyreg, int phydata);
void dbgpr_regs(void);
void dump_phy_registers(struct prv_data *pdata);
void get_all_hw_features(struct prv_data *pdata);
void print_all_hw_features(struct prv_data *pdata);
void configure_flow_ctrl(struct prv_data *pdata);
u32 usec2riwt(u32 usec, struct prv_data *pdata);
void init_rx_coalesce(struct prv_data *pdata);
void enable_all_ch_rx_interrpt(struct prv_data *pdata);
void disable_all_ch_rx_interrpt(struct prv_data *pdata);
void update_rx_errors(struct net_device *dev, unsigned int rx_status);
unsigned char get_tx_queue_count(void);
void all_ch_napi_disable(struct prv_data *pdata);
void napi_enable_mq(struct prv_data *pdata);
void set_rx_mode(struct net_device *dev);
unsigned char get_rx_queue_count(void);
void mmc_read(struct mmc_counters *mmc);
unsigned int get_total_desc_cnt(struct prv_data *pdata, struct sk_buff *skb, unsigned int q_inx);
int ptp_init(struct prv_data *pdata);
void ptp_remove(struct prv_data *pdata);
phy_interface_t get_phy_interface(struct prv_data *pdata);

#endif
