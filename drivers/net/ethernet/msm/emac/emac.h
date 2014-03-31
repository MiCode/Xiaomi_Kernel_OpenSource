/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_EMAC_H_
#define _MSM_EMAC_H_

#include <asm/byteorder.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/clk.h>

/* Device IDs */
#define EMAC_DEV_ID                0x0040

/* DMA address */
#define DMA_ADDR_HI_MASK           0xffffffff00000000ULL
#define DMA_ADDR_LO_MASK           0x00000000ffffffffULL

#define EMAC_DMA_ADDR_HI(_addr) \
		((u32)(((u64)(_addr) & DMA_ADDR_HI_MASK) >> 32))
#define EMAC_DMA_ADDR_LO(_addr) \
		((u32)((u64)(_addr) & DMA_ADDR_LO_MASK))

/* 4 emac core irq, 1 phy irq, 1 wol irq */
#define EMAC_NUM_CORE_IRQ     4
#define EMAC_WOL_IRQ          4
#define EMAC_SGMII_PHY_IRQ    5
#define EMAC_NUM_IRQ          6

/* emac clocks */
#define EMAC_AXI_CLK          0
#define EMAC_CFG_AHB_CLK      1
#define EMAC_125M_CLK         2
#define EMAC_SYS_25M_CLK      3
#define EMAC_TX_CLK           4
#define EMAC_RX_CLK           5
#define EMAC_SYS_CLK          6
#define EMAC_NUM_CLK          7

/* mdio/mdc gpios */
#define EMAC_NUM_GPIO         2

#define EMAC_LINK_SPEED_UNKNOWN         0x0
#define EMAC_LINK_SPEED_10_HALF         0x0001
#define EMAC_LINK_SPEED_10_FULL         0x0002
#define EMAC_LINK_SPEED_100_HALF        0x0004
#define EMAC_LINK_SPEED_100_FULL        0x0008
#define EMAC_LINK_SPEED_1GB_FULL        0x0020

#define EMAC_LINK_SPEED_DEFAULT (\
		EMAC_LINK_SPEED_10_HALF  |\
		EMAC_LINK_SPEED_10_FULL  |\
		EMAC_LINK_SPEED_100_HALF |\
		EMAC_LINK_SPEED_100_FULL |\
		EMAC_LINK_SPEED_1GB_FULL)

#define EMAC_MAX_SETUP_LNK_CYCLE        100

/* Wake On Lan */
#define EMAC_WOL_PHY                     0x00000001 /* PHY Status Change */
#define EMAC_WOL_MAGIC                   0x00000002 /* Magic Packet */

enum emac_reg_bases {
	EMAC,
	EMAC_CSR,
	EMAC_1588,
	EMAC_QSERDES,
	EMAC_SGMII_PHY,
	NUM_EMAC_REG_BASES
};

/* DMA Order Settings */
enum emac_dma_order {
	emac_dma_ord_in = 1,
	emac_dma_ord_enh = 2,
	emac_dma_ord_out = 4
};

enum emac_mac_speed {
	emac_mac_speed_0 = 0,
	emac_mac_speed_10_100 = 1,
	emac_mac_speed_1000 = 2
};

enum emac_dma_req_block {
	emac_dma_req_128 = 0,
	emac_dma_req_256 = 1,
	emac_dma_req_512 = 2,
	emac_dma_req_1024 = 3,
	emac_dma_req_2048 = 4,
	emac_dma_req_4096 = 5
};

/* Flow Control Settings */
enum emac_fc_mode {
	emac_fc_none = 0,
	emac_fc_rx_pause,
	emac_fc_tx_pause,
	emac_fc_full,
	emac_fc_default
};

/* IEEE1588 */
enum emac_ptp_clk_mode {
	emac_ptp_clk_mode_oc_two_step,
	emac_ptp_clk_mode_oc_one_step
};

enum emac_ptp_mode {
	emac_ptp_mode_slave,
	emac_ptp_mode_master
};

struct emac_hw_stats {
	/* rx */
	u64 rx_ok;              /* good packets */
	u64 rx_bcast;           /* good broadcast packets */
	u64 rx_mcast;           /* good multicast packets */
	u64 rx_pause;           /* pause packet */
	u64 rx_ctrl;            /* control packets other than pause frame. */
	u64 rx_fcs_err;         /* packets with bad FCS. */
	u64 rx_len_err;         /* packets with length mismatch */
	u64 rx_byte_cnt;        /* good bytes count (without FCS) */
	u64 rx_runt;            /* runt packets */
	u64 rx_frag;            /* fragment count */
	u64 rx_sz_64;	        /* packets that are 64 bytes */
	u64 rx_sz_65_127;       /* packets that are 65-127 bytes */
	u64 rx_sz_128_255;      /* packets that are 128-255 bytes */
	u64 rx_sz_256_511;      /* packets that are 256-511 bytes */
	u64 rx_sz_512_1023;     /* packets that are 512-1023 bytes */
	u64 rx_sz_1024_1518;    /* packets that are 1024-1518 bytes */
	u64 rx_sz_1519_max;     /* packets that are 1519-MTU bytes*/
	u64 rx_sz_ov;           /* packets that are >MTU bytes (truncated) */
	u64 rx_rxf_ov;          /* packets dropped due to RX FIFO overflow */
	u64 rx_align_err;       /* alignment errors */
	u64 rx_bcast_byte_cnt;  /* broadcast packets byte count (without FCS) */
	u64 rx_mcast_byte_cnt;  /* multicast packets byte count (without FCS) */
	u64 rx_err_addr;        /* packets dropped due to address filtering */
	u64 rx_crc_allign;      /* CRC align errors */
	u64 rx_jubbers;         /* jubbers */

	/* tx */
	u64 tx_ok;              /* good packets */
	u64 tx_bcast;           /* good broadcast packets */
	u64 tx_mcast;           /* good multicast packets */
	u64 tx_pause;           /* pause packets */
	u64 tx_exc_defer;       /* packets with excessive deferral */
	u64 tx_ctrl;            /* control packets other than pause frame */
	u64 tx_defer;           /* packets that are deferred. */
	u64 tx_byte_cnt;        /* good bytes count (without FCS) */
	u64 tx_sz_64;           /* packets that are 64 bytes */
	u64 tx_sz_65_127;       /* packets that are 65-127 bytes */
	u64 tx_sz_128_255;      /* packets that are 128-255 bytes */
	u64 tx_sz_256_511;      /* packets that are 256-511 bytes */
	u64 tx_sz_512_1023;     /* packets that are 512-1023 bytes */
	u64 tx_sz_1024_1518;    /* packets that are 1024-1518 bytes */
	u64 tx_sz_1519_max;     /* packets that are 1519-MTU bytes */
	u64 tx_1_col;           /* packets single prior collision */
	u64 tx_2_col;           /* packets with multiple prior collisions */
	u64 tx_late_col;        /* packets with late collisions */
	u64 tx_abort_col;       /* packets aborted due to excess collisions */
	u64 tx_underrun;        /* packets aborted due to FIFO underrun */
	u64 tx_rd_eop;          /* count of reads beyond EOP */
	u64 tx_len_err;         /* packets with length mismatch */
	u64 tx_trunc;           /* packets truncated due to size >MTU */
	u64 tx_bcast_byte;      /* broadcast packets byte count (without FCS) */
	u64 tx_mcast_byte;      /* multicast packets byte count (without FCS) */
	u64 tx_col;             /* collisions */
};

struct emac_hw {
	void __iomem *reg_addr[NUM_EMAC_REG_BASES];

	struct emac_adapter *adpt;

	u16     devid;
	u16     revid;

	/* ring parameter */
	u8      tpd_burst;
	u8      rfd_burst;
	u8      dmaw_dly_cnt;
	u8      dmar_dly_cnt;
	enum emac_dma_req_block   dmar_block;
	enum emac_dma_req_block   dmaw_block;
	enum emac_dma_order       dma_order;

	/* PHY parameter */
	u32             phy_addr;
	u16             phy_id[2];
	bool            autoneg;
	u32             autoneg_advertised;
	u32             link_speed;
	bool            link_up;
	spinlock_t      mdio_lock;

	/* MAC parameter */
	u8      mac_addr[ETH_ALEN];
	u8      mac_perm_addr[ETH_ALEN];
	u32     mtu;

	/* flow control parameter */
	enum emac_fc_mode   cur_fc_mode; /* FC mode in effect */
	enum emac_fc_mode   req_fc_mode; /* FC mode requested by caller */
	bool                disable_fc_autoneg; /* Do not autonegotiate FC */

	/* RSS parameter */
	u8      rss_hstype;
	u8      rss_base_cpu;
	u16     rss_idt_size;
	u32     rss_idt[32];
	u8      rss_key[40];
	bool    rss_initialized;

	/* 1588 parameter */
	enum emac_ptp_clk_mode  ptp_clk_mode;
	u32                     rtc_ref_clkrate;
	spinlock_t              ptp_lock;

	u32                 irq_mod;
	u32                 preamble;
	unsigned long       flags;
};

#define EMAC_HW_FLAG_PROMISC_EN          0
#define EMAC_HW_FLAG_VLANSTRIP_EN        1
#define EMAC_HW_FLAG_MULTIALL_EN         2
#define EMAC_HW_FLAG_LOOPBACK_EN         3

#define EMAC_HW_FLAG_PTP_CAP             4
#define EMAC_HW_FLAG_PTP_EN              5
#define EMAC_HW_FLAG_TS_RX_EN            6
#define EMAC_HW_FLAG_TS_TX_EN            7

#define CHK_HW_FLAG(_flag)              CHK_FLAG(hw, HW, _flag)
#define SET_HW_FLAG(_flag)              SET_FLAG(hw, HW, _flag)
#define CLI_HW_FLAG(_flag)              CLI_FLAG(hw, HW, _flag)

/* RSS hstype Definitions */
#define EMAC_RSS_HSTYP_IPV4_EN           0x00000001
#define EMAC_RSS_HSTYP_TCP4_EN           0x00000002
#define EMAC_RSS_HSTYP_IPV6_EN           0x00000004
#define EMAC_RSS_HSTYP_TCP6_EN           0x00000008
#define EMAC_RSS_HSTYP_ALL_EN (\
		EMAC_RSS_HSTYP_IPV4_EN   |\
		EMAC_RSS_HSTYP_TCP4_EN   |\
		EMAC_RSS_HSTYP_IPV6_EN   |\
		EMAC_RSS_HSTYP_TCP6_EN)

/******************************************************************************/
/* Logging functions and macros */
#define emac_err(_adpt, _format, ...) \
	netdev_err(_adpt->netdev, _format, ##__VA_ARGS__)

#define emac_info(_adpt, _mlevel, _format, ...) \
	netif_info(_adpt, _mlevel, _adpt->netdev, _format, ##__VA_ARGS__)

#define emac_warn(_adpt, _mlevel, _format, ...) \
	netif_warn(_adpt, _mlevel, _adpt->netdev, _format, ##__VA_ARGS__)

#define emac_dbg(_adpt, _mlevel, _format, ...) \
	netif_dbg(_adpt, _mlevel, _adpt->netdev, _format, ##__VA_ARGS__)


#define EMAC_VLAN_TO_TAG(_vlan, _tag) \
		_tag =  ((((_vlan) >> 8) & 0xFF) | (((_vlan) & 0xFF) << 8));

#define EMAC_TAG_TO_VLAN(_tag, _vlan) \
		_vlan = ((((_tag) >> 8) & 0xFF) | (((_tag) & 0xFF) << 8));


#define EMAC_MAX_HANDLED_INTRS          5

#define EMAC_DEF_RX_BUF_SIZE            1536
#define EMAC_MAX_JUMBO_PKT_SIZE         (9*1024)
#define EMAC_MAX_TX_OFFLOAD_THRESH      (9*1024)

#define EMAC_MAX_ETH_FRAME_SIZE         EMAC_MAX_JUMBO_PKT_SIZE
#define EMAC_MIN_ETH_FRAME_SIZE         68

#define EMAC_MAX_TX_QUEUES      4
#define EMAC_DEF_TX_QUEUES      1
#define EMAC_ACTIVE_TXQ         0

#define EMAC_MAX_RX_QUEUES      4
#define EMAC_DEF_RX_QUEUES      1

#define EMAC_MIN_TX_DESCS       128
#define EMAC_MIN_RX_DESCS       128

#define EMAC_MAX_TX_DESCS       16383
#define EMAC_MAX_RX_DESCS       2047

#define EMAC_DEF_TX_DESCS       512
#define EMAC_DEF_RX_DESCS       256

#define EMAC_DEF_RX_IRQ_MOD     250
#define EMAC_DEF_TX_IRQ_MOD     250

#define EMAC_WATCHDOG_TIME      (5 * HZ)

/* RRD */
/* general parameter format of rrd */
struct emac_sw_rrdes_general {
	/* dword 0 */
	u32  xsum:16;
	u32  nor:4;       /* number of RFD */
	u32  si:12;       /* start index of rfd-ring */
	/* dword 1 */
	u32  hash;
	/* dword 2 */
	u32  cvlan_tag:16; /* vlan-tag */
	u32  reserved:8;
	u32  ptp_timestamp:1;
	u32  rss_cpu:3;   /* CPU number used by RSS */
	u32  rss_flag:4;  /* rss_flag 0, TCP(IPv6) flag for RSS hash algrithm
			   * rss_flag 1, IPv6 flag for RSS hash algrithm
			   * rss_flag 2, TCP(IPv4) flag for RSS hash algrithm
			   * rss_flag 3, IPv4 flag for RSS hash algrithm
			   */
	/* dword 3 */
	u32  pkt_len:14;  /* length of the packet */
	u32  l4f:1;       /* L4(TCP/UDP) checksum failed */
	u32  ipf:1;       /* IP checksum failed */
	u32  cvlan_flag:1; /* vlan tagged */
	u32  pid:3;
	u32  res:1;       /* received error summary */
	u32  crc:1;       /* crc error */
	u32  fae:1;       /* frame alignment error */
	u32  trunc:1;     /* truncated packet, larger than MTU */
	u32  runt:1;      /* runt packet */
	u32  icmp:1;      /* incomplete packet due to insufficient rx-desc*/
	u32  bar:1;       /* broadcast address received */
	u32  mar:1;       /* multicast address received */
	u32  type:1;      /* ethernet type */
	u32  fov:1;       /* fifo overflow */
	u32  lene:1;      /* length error */
	u32  update:1;    /* update */

	/* dword 4 */
	u32 ts_low:30;
	u32 __unused__:2;
	/* dword 5 */
	u32 ts_high;
};

union emac_sw_rrdesc {
	struct emac_sw_rrdes_general genr;

	/* dword flat format */
	struct {
		u32 dw[6];
	} dfmt;
};

/* RFD */
/* general parameter format of rfd */
struct emac_sw_rfdes_general {
	u64   addr;
};

union emac_sw_rfdesc {
	struct emac_sw_rfdes_general genr;

	/* dword flat format */
	struct {
		u32 dw[2];
	} dfmt;
};

/* TPD */
/* general parameter format of tpd */
struct emac_sw_tpdes_general {
	/* dword 0 */
	u32  buffer_len:16; /* include 4-byte CRC */
	u32  svlan_tag:16;
	/* dword 1 */
	u32  l4hdr_offset:8; /* l4 header offset to the 1st byte of packet */
	u32  c_csum:1;
	u32  ip_csum:1;
	u32  tcp_csum:1;
	u32  udp_csum:1;
	u32  lso:1;
	u32  lso_v2:1;
	u32  svtagged:1;   /* vlan-id tagged already */
	u32  ins_svtag:1;  /* insert vlan tag */
	u32  ipv4:1;       /* ipv4 packet */
	u32  type:1;       /* type of packet (ethernet_ii(0) or snap(1)) */
	u32  reserve:12;
	u32  epad:1;       /* even byte padding when this packet */
	u32  last_frag:1;  /* last fragment(buffer) of the packet */
	/* dword 2 */
	u32  addr_lo;
	/* dword 3 */
	u32  cvlan_tag:16;
	u32  cvtagged:1;
	u32  ins_cvtag:1;
	u32  addr_hi:13;
	u32  tstmp_sav:1;
};

/* custom checksum parameter format of tpd */
struct emac_sw_tpdes_checksum {
	/* dword 0 */
	u32  buffer_len:16;
	u32  svlan_tag:16;
	/* dword 1 */
	u32  payld_offset:8; /* payload offset to the 1st byte of packet */
	u32  c_csum:1;       /* do custom checksum offload */
	u32  ip_csum:1;      /* do ip(v4) header checksum offload */
	u32  tcp_csum:1;     /* do tcp checksum offload, both ipv4 and ipv6 */
	u32  udp_csum:1;     /* do udp checksum offlaod, both ipv4 and ipv6 */
	u32  lso:1;
	u32  lso_v2:1;
	u32  svtagged:1;     /* vlan-id tagged already */
	u32  ins_svtag:1;    /* insert vlan tag */
	u32  ipv4:1;         /* ipv4 packet */
	u32  type:1;         /* type of packet (ethernet_ii(0) or snap(1)) */
	u32  cxsum_offset:8; /* checksum offset to the 1st byte of packet */
	u32  reserve:4;
	u32  epad:1;         /* even byte padding when this packet */
	u32  last_frag:1;    /* last fragment(buffer) of the packet */
	/* dword 2 */
	u32  addr_lo;
	/* dword 3 */
	u32  cvlan_tag:16;
	u32  cvtagged:1;
	u32  ins_cvtag:1;
	u32  addr_hi:14;
};

/* tcp large send format (v1/v2) of tpd */
struct emac_sw_tpdes_tso {
	/* dword 0 */
	u32  buffer_len:16; /* include 4-byte CRC */
	u32  svlan_tag:16;
	/* dword 1 */
	u32  tcphdr_offset:8; /* tcp hdr offset to the 1st byte of packet */
	u32  c_csum:1;
	u32  ip_csum:1;
	u32  tcp_csum:1;
	u32  udp_csum:1;
	u32  lso:1;        /* do tcp large send (ipv4 only) */
	u32  lso_v2:1;     /* must be 0 in this format */
	u32  svtagged:1;   /* vlan-id tagged already */
	u32  ins_svtag:1;  /* insert vlan tag */
	u32  ipv4:1;       /* ipv4 packet */
	u32  type:1;       /* type of packet (ethernet_ii(1) or snap(0)) */
	u32  mss:13;       /* mss if do tcp large send */
	u32  last_frag:1;  /* last fragment(buffer) of the packet */
	/* dword 2 & 3 */
	u64  pkt_len:32;   /* packet length in ext tpd */
	u64  reserve:32;
};

union emac_sw_tpdesc {
	struct emac_sw_tpdes_general   genr;
	struct emac_sw_tpdes_checksum  csum;
	struct emac_sw_tpdes_tso       tso;

	/* dword flat format */
	struct {
		u32 dw[4];
	} dfmt;
};

#define EMAC_RRD(_que, _size, _i)			\
		((_que)->rrd.rrdesc + (_size * (_i)))
#define EMAC_RFD(_que, _size, _i)			\
		((_que)->rfd.rfdesc + (_size * (_i)))
#define EMAC_TPD(_que, _size, _i)			\
		((_que)->tpd.tpdesc + (_size * (_i)))

#define EMAC_TPD_LAST_FRAGMENT  0x80000000
#define EMAC_TPD_TSTAMP_SAVE    0x80000000

struct emac_irq_info {
	unsigned int irq;
	char *name;
	irq_handler_t handler;

	u32 status_reg;
	u32 mask_reg;
	u32 mask;

	struct emac_rx_queue *rxque;
	struct emac_adapter  *adpt;
};

struct emac_gpio_info {
	unsigned int gpio;
	char *name;
};

struct emac_clk_info {
	struct clk           *clk;
	char                 *name;
	bool                  enabled;
	struct emac_adapter  *adpt;
};

/* emac_ring_header represents a single, contiguous block of DMA space
 * mapped for the three descriptor rings (tpd, rfd, rrd)
 */
struct emac_ring_header {
	void           *desc;  /* virtual address */
	dma_addr_t      dma;    /* physical address */
	unsigned int    size;   /* length in bytes */
	unsigned int    used;
};

/* emac_buffer is wrapper around a pointer to a socket buffer
 * so a DMA handle can be stored along with the skb
 */
struct emac_buffer {
	struct sk_buff *skb;      /* socket buffer */
	u16             length;   /* rx buffer length */
	dma_addr_t      dma;
};

/* receive free descriptor (rfd) ring */
struct emac_rfd_ring {
	struct emac_buffer      *rfbuff;
	u32 __iomem             *rfdesc;  /* virtual address */
	dma_addr_t               rfdma;   /* physical address */
	u64 size;          /* length in bytes */
	u32 count;         /* number of descriptors in the ring */
	u32 produce_idx;
	u32 process_idx;
	u32 consume_idx;   /* unused */
};

/* receive return desciptor (rrd) ring */
struct emac_rrd_ring {
	u32 __iomem         *rrdesc;    /* virtual address */
	dma_addr_t           rrdma;     /* physical address */
	u64 size;          /* length in bytes */
	u32 count;         /* number of descriptors in the ring */
	u32 produce_idx;   /* unused */
	u32 consume_idx;
};

/* rx queue */
struct emac_rx_queue {
	struct device          *dev;      /* device for dma mapping */
	struct net_device      *netdev;   /* netdev ring belongs to */
	struct emac_rrd_ring    rrd;
	struct emac_rfd_ring    rfd;
	struct napi_struct      napi;

	u16 que_idx;       /* index in multi rx queues*/
	u16 produce_reg;
	u32 produce_mask;
	u8 produce_shft;

	u16 process_reg;
	u32 process_mask;
	u8 process_shft;

	u16 consume_reg;
	u32 consume_mask;
	u8 consume_shft;

	u32 intr;
	struct emac_irq_info *irq_info;
};

#define GET_RFD_BUFFER(_rque, _i)    (&((_rque)->rfd.rfbuff[(_i)]))

/* transimit packet descriptor (tpd) ring */
struct emac_tpd_ring {
	struct emac_buffer *tpbuff;
	u32 __iomem        *tpdesc;   /* virtual address */
	dma_addr_t          tpdma;    /* physical address */

	u64 size;    /* length in bytes */
	u32 count;   /* number of descriptors in the ring */
	u32 produce_idx;
	u32 consume_idx;
	u32 last_produce_idx;
};

/* tx queue */
struct emac_tx_queue {
	struct device         *dev;     /* device for dma mapping */
	struct net_device     *netdev;  /* netdev ring belongs to */
	struct emac_tpd_ring   tpd;

	u16 que_idx;       /* needed for multiqueue queue management */
	u16 max_packets;   /* max packets per interrupt */
	u16 produce_reg;
	u32 produce_mask;
	u8 produce_shft;

	u16 consume_reg;
	u32 consume_mask;
	u8 consume_shft;
};
#define GET_TPD_BUFFER(_tque, _i)    (&((_tque)->tpd.tpbuff[(_i)]))

/* driver private data structure */
struct emac_adapter {
	struct net_device *netdev;

	struct emac_irq_info  irq_info[EMAC_NUM_IRQ];
	struct emac_gpio_info gpio_info[EMAC_NUM_GPIO];
	struct emac_clk_info  clk_info[EMAC_NUM_CLK];

	/* dma parameters */
	u64                             dma_mask;
	struct device_dma_parameters    dma_parms;

	/* All Descriptor memory */
	struct emac_ring_header ring_header;
	struct emac_tx_queue tx_queue[EMAC_MAX_TX_QUEUES];
	struct emac_rx_queue rx_queue[EMAC_MAX_RX_QUEUES];
	u16 num_txques;
	u16 num_rxques;

	u32 num_txdescs;
	u32 num_rxdescs;
	u8 rrdesc_size; /* in quad words */
	u8 rfdesc_size; /* in quad words */
	u8 tpdesc_size; /* in quad words */

	u32 rxbuf_size;

	struct emac_hw hw;
	struct emac_hw_stats hw_stats;

	struct work_struct emac_task;
	struct timer_list  emac_timer;
	unsigned long	link_jiffies;

	bool            tstamp_en;
	int             phy_mode;
	bool            no_ephy;
	bool            no_mdio_gpio;
	u32             wol;
	u16             msg_enable;
	unsigned long   flags;
};

#define EMAC_ADPT_FLAG_STATE_RESETTING          16
#define EMAC_ADPT_FLAG_STATE_DOWN               17
#define EMAC_ADPT_FLAG_STATE_WATCH_DOG          18

#define EMAC_ADPT_FLAG_TASK_REINIT_REQ          19
#define EMAC_ADPT_FLAG_TASK_LSC_REQ             20
#define EMAC_ADPT_FLAG_TASK_CHK_SGMII_REQ       21

#define CHK_ADPT_FLAG(_flag)           CHK_FLAG(adpt, ADPT, _flag)
#define SET_ADPT_FLAG(_flag)           SET_FLAG(adpt, ADPT, _flag)
#define CLI_ADPT_FLAG(_flag)           CLI_FLAG(adpt, ADPT, _flag)
#define CHK_AND_SET_ADPT_FLAG(_flag)   CHK_AND_SET_FLAG(adpt, ADPT, _flag)

/* definitions for flags */
#define CHK_FLAG(_st, _type, _flag) \
		test_bit((EMAC_##_type##_FLAG_##_flag), &((_st)->flags))
#define SET_FLAG(_st, _type, _flag) \
		set_bit((EMAC_##_type##_FLAG_##_flag), &((_st)->flags))
#define CLI_FLAG(_st, _type, _flag) \
		clear_bit((EMAC_##_type##_FLAG_##_flag), &((_st)->flags))
#define CHK_AND_SET_FLAG(_st, _type, _flag) \
		test_and_set_bit((EMAC_##_type##_FLAG_##_flag), &((_st)->flags))

/* default to trying for four seconds */
#define EMAC_TRY_LINK_TIMEOUT     (4 * HZ)

#define EMAC_HW_CTRL_RESET_MAC         0x00000001

extern char emac_drv_name[];
extern const char emac_drv_version[];
extern void emac_set_ethtool_ops(struct net_device *netdev);
extern void emac_reinit_locked(struct emac_adapter *adpt);
extern void emac_update_hw_stats(struct emac_adapter *adpt);
extern int emac_resize_rings(struct net_device *netdev);
extern int emac_up(struct emac_adapter *adpt);
extern void emac_down(struct emac_adapter *adpt, u32 ctrl);

#endif /* _MSM_EMAC_H_ */
