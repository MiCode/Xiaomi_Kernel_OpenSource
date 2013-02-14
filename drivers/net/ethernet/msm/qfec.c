/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/module.h>

#include <linux/platform_device.h>

#include <linux/types.h>        /* size_t */
#include <linux/interrupt.h>    /* mark_bh */

#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/skbuff.h>

#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/mii.h>

#include <linux/ethtool.h>
#include <linux/net_tstamp.h>
#include <linux/phy.h>
#include <linux/inet.h>

#include "qfec.h"

#define QFEC_NAME       "qfec"
#define QFEC_DRV_VER    "Nov 29 2011"

#define ETH_BUF_SIZE    0x600
#define MAX_N_BD        50
#define MAC_ADDR_SIZE	6

#define RX_TX_BD_RATIO  8
#define TX_BD_NUM       256
#define RX_BD_NUM       256
#define TX_BD_TI_RATIO  4
#define MAX_MDIO_REG    32

#define H_DPLX     0
#define F_DPLX     1
/*
 * logging macros
 */
#define QFEC_LOG_PR     1
#define QFEC_LOG_DBG    2
#define QFEC_LOG_DBG2   4
#define QFEC_LOG_MDIO_W 8
#define QFEC_LOG_MDIO_R 16
#define QFEC_MII_EXP_MASK (EXPANSION_LCWP | EXPANSION_ENABLENPAGE \
							| EXPANSION_NPCAPABLE)

static int qfec_debug = QFEC_LOG_PR;

#ifdef QFEC_DEBUG
# define QFEC_LOG(flag, ...)                    \
	do {                                    \
		if (flag & qfec_debug)          \
			pr_info(__VA_ARGS__);  \
	} while (0)
#else
# define QFEC_LOG(flag, ...)
#endif

#define QFEC_LOG_ERR(...) pr_err(__VA_ARGS__)

/*
 * driver buffer-descriptor
 *   contains the 4 word HW descriptor plus an additional 4-words.
 *   (See the DSL bits in the BUS-Mode register).
 */
#define BD_FLAG_LAST_BD     1

struct buf_desc {
	struct qfec_buf_desc   *p_desc;
	struct sk_buff         *skb;
	void                   *buf_virt_addr;
	void                   *buf_phys_addr;
	uint32_t                last_bd_flag;
};

/*
 *inline functions accessing non-struct qfec_buf_desc elements
 */

/* skb */
static inline struct sk_buff *qfec_bd_skbuf_get(struct buf_desc *p_bd)
{
	return p_bd->skb;
};

static inline void qfec_bd_skbuf_set(struct buf_desc *p_bd, struct sk_buff *p)
{
	p_bd->skb   = p;
};

/* virtual addr  */
static inline void qfec_bd_virt_set(struct buf_desc *p_bd, void *addr)
{
	p_bd->buf_virt_addr = addr;
};

static inline void *qfec_bd_virt_get(struct buf_desc *p_bd)
{
	return p_bd->buf_virt_addr;
};

/* physical addr  */
static inline void qfec_bd_phys_set(struct buf_desc *p_bd, void *addr)
{
	p_bd->buf_phys_addr = addr;
};

static inline void *qfec_bd_phys_get(struct buf_desc *p_bd)
{
	return p_bd->buf_phys_addr;
};

/* last_bd_flag */
static inline uint32_t qfec_bd_last_bd(struct buf_desc *p_bd)
{
	return (p_bd->last_bd_flag != 0);
};

static inline void qfec_bd_last_bd_set(struct buf_desc *p_bd)
{
	p_bd->last_bd_flag = BD_FLAG_LAST_BD;
};

/*
 *inline functions accessing struct qfec_buf_desc elements
 */

/* ownership bit */
static inline uint32_t qfec_bd_own(struct buf_desc *p_bd)
{
	return p_bd->p_desc->status & BUF_OWN;
};

static inline void qfec_bd_own_set(struct buf_desc *p_bd)
{
	p_bd->p_desc->status |= BUF_OWN ;
};

static inline void qfec_bd_own_clr(struct buf_desc *p_bd)
{
	p_bd->p_desc->status &= ~(BUF_OWN);
};

static inline uint32_t qfec_bd_status_get(struct buf_desc *p_bd)
{
	return p_bd->p_desc->status;
};

static inline void qfec_bd_status_set(struct buf_desc *p_bd, uint32_t status)
{
	p_bd->p_desc->status = status;
};

static inline uint32_t qfec_bd_status_len(struct buf_desc *p_bd)
{
	return BUF_RX_FL_GET((*p_bd->p_desc));
};

/* control register */
static inline void qfec_bd_ctl_reset(struct buf_desc *p_bd)
{
	p_bd->p_desc->ctl  = 0;
};

static inline uint32_t qfec_bd_ctl_get(struct buf_desc *p_bd)
{
	return p_bd->p_desc->ctl;
};

static inline void qfec_bd_ctl_set(struct buf_desc *p_bd, uint32_t val)
{
	p_bd->p_desc->ctl |= val;
};

static inline void qfec_bd_ctl_wr(struct buf_desc *p_bd, uint32_t val)
{
	p_bd->p_desc->ctl = val;
};

/* pbuf register  */
static inline void *qfec_bd_pbuf_get(struct buf_desc *p_bd)
{
	return p_bd->p_desc->p_buf;
}

static inline void qfec_bd_pbuf_set(struct buf_desc *p_bd, void *p)
{
	p_bd->p_desc->p_buf = p;
}

/* next register */
static inline void *qfec_bd_next_get(struct buf_desc *p_bd)
{
	return p_bd->p_desc->next;
};

/*
 * initialize an RX BD w/ a new buf
 */
static int qfec_rbd_init(struct net_device *dev, struct buf_desc *p_bd)
{
	struct sk_buff     *skb;
	void               *p;
	void               *v;

	/* allocate and record ptrs for sk buff */
	skb   = dev_alloc_skb(ETH_BUF_SIZE);
	if (!skb)
		goto err;

	qfec_bd_skbuf_set(p_bd, skb);

	v = skb_put(skb, ETH_BUF_SIZE);
	qfec_bd_virt_set(p_bd, v);

	p = (void *) dma_map_single(&dev->dev,
		(void *)skb->data, ETH_BUF_SIZE, DMA_FROM_DEVICE);
	qfec_bd_pbuf_set(p_bd, p);
	qfec_bd_phys_set(p_bd, p);

	/* populate control register */
	/* mark the last BD and set end-of-ring bit */
	qfec_bd_ctl_wr(p_bd, ETH_BUF_SIZE |
		(qfec_bd_last_bd(p_bd) ? BUF_RX_RER : 0));

	qfec_bd_status_set(p_bd, BUF_OWN);

	if (!(qfec_debug & QFEC_LOG_DBG2))
		return 0;

	/* debug messages */
	QFEC_LOG(QFEC_LOG_DBG2, "%s: %p bd\n", __func__, p_bd);

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %p skb\n", __func__, skb);

	QFEC_LOG(QFEC_LOG_DBG2,
		"%s: %p p_bd, %p data, %p skb_put, %p virt, %p p_buf, %p p\n",
		__func__, (void *)p_bd,
		(void *)skb->data, v, /*(void *)skb_put(skb, ETH_BUF_SIZE), */
		(void *)qfec_bd_virt_get(p_bd), (void *)qfec_bd_pbuf_get(p_bd),
		(void *)p);

	return 0;

err:
	return -ENOMEM;
};

/*
 * ring structure used to maintain indices of buffer-descriptor (BD) usage
 *
 *   The RX BDs are normally all pre-allocated with buffers available to be
 *   DMA'd into with received frames.  The head indicates the first BD/buffer
 *   containing a received frame, and the tail indicates the oldest BD/buffer
 *   that needs to be restored for use.   Head and tail are both initialized
 *   to zero, and n_free is initialized to zero, since all BD are initialized.
 *
 *   The TX BDs are normally available for use, only being initialized as
 *   TX frames are requested for transmission.   The head indicates the
 *   first available BD, and the tail indicate the oldest BD that has
 *   not been acknowledged as transmitted.    Head and tail are both initialized
 *   to zero, and n_free is initialized to len, since all are available for use.
 */
struct ring {
	int     head;
	int     tail;
	int     n_free;
	int     len;
};

/* accessory in line functions for struct ring */
static inline void qfec_ring_init(struct ring *p_ring, int size, int free)
{
	p_ring->head  = p_ring->tail = 0;
	p_ring->len   = size;
	p_ring->n_free = free;
}

static inline int qfec_ring_full(struct ring *p_ring)
{
	return (p_ring->n_free == 0);
};

static inline int qfec_ring_empty(struct ring *p_ring)
{
	return (p_ring->n_free == p_ring->len);
}

static inline void qfec_ring_head_adv(struct ring *p_ring)
{
	if (++p_ring->head == p_ring->len)
		p_ring->head = 0;
	p_ring->n_free--;
};

static inline void qfec_ring_tail_adv(struct ring *p_ring)
{
	if (++p_ring->tail == p_ring->len)
		p_ring->tail = 0;
	p_ring->n_free++;
};

static inline int qfec_ring_head(struct ring *p_ring)
{

	return p_ring->head;
};

static inline int qfec_ring_tail(struct ring *p_ring)
{
	return p_ring->tail;
};

static inline int qfec_ring_room(struct ring *p_ring)
{
	return p_ring->n_free;
};

/*
 * counters track normal and abnormal driver events and activity
 */
enum cntr {
	isr                  =  0,
	fatal_bus,

	early_tx,
	tx_no_resource,
	tx_proc_stopped,
	tx_jabber_tmout,

	xmit,
	tx_int,
	tx_isr,
	tx_owned,
	tx_underflow,

	tx_replenish,
	tx_skb_null,
	tx_timeout,
	tx_too_large,

	gmac_isr,

	/* half */
	norm_int,
	abnorm_int,

	early_rx,
	rx_buf_unavail,
	rx_proc_stopped,
	rx_watchdog,

	netif_rx_cntr,
	rx_int,
	rx_isr,
	rx_owned,
	rx_overflow,

	rx_dropped,
	rx_skb_null,
	queue_start,
	queue_stop,

	rx_paddr_nok,
	ts_ioctl,
	ts_tx_en,
	ts_tx_rtn,

	ts_rec,
	cntr_last,
};

static char *cntr_name[]  = {
	"isr",
	"fatal_bus",

	"early_tx",
	"tx_no_resource",
	"tx_proc_stopped",
	"tx_jabber_tmout",

	"xmit",
	"tx_int",
	"tx_isr",
	"tx_owned",
	"tx_underflow",

	"tx_replenish",
	"tx_skb_null",
	"tx_timeout",
	"tx_too_large",

	"gmac_isr",

	/* half */
	"norm_int",
	"abnorm_int",

	"early_rx",
	"rx_buf_unavail",
	"rx_proc_stopped",
	"rx_watchdog",

	"netif_rx",
	"rx_int",
	"rx_isr",
	"rx_owned",
	"rx_overflow",

	"rx_dropped",
	"rx_skb_null",
	"queue_start",
	"queue_stop",

	"rx_paddr_nok",
	"ts_ioctl",
	"ts_tx_en",
	"ts_tx_rtn",

	"ts_rec",
	""
};

/*
 * private data
 */

static struct net_device  *qfec_dev;

enum qfec_state {
	timestamping  = 0x04,
};

struct qfec_priv {
	struct net_device      *net_dev;
	struct net_device_stats stats;            /* req statistics */

	struct device           dev;

	spinlock_t              xmit_lock;
	spinlock_t              mdio_lock;

	unsigned int            state;            /* driver state */

	unsigned int            bd_size;          /* buf-desc alloc size */
	struct qfec_buf_desc   *bd_base;          /* * qfec-buf-desc */
	dma_addr_t              tbd_dma;          /* dma/phy-addr buf-desc */
	dma_addr_t              rbd_dma;          /* dma/phy-addr buf-desc */

	struct resource        *mac_res;
	void                   *mac_base;         /* mac (virt) base address */

	struct resource        *clk_res;
	void                   *clk_base;         /* clk (virt) base address */

	struct resource        *fuse_res;
	void                   *fuse_base;        /* mac addr fuses */

	unsigned int            n_tbd;            /* # of TX buf-desc */
	struct ring             ring_tbd;         /* TX ring */
	struct buf_desc        *p_tbd;
	unsigned int            tx_ic_mod;        /* (%) val for setting IC */

	unsigned int            n_rbd;            /* # of RX buf-desc */
	struct ring             ring_rbd;         /* RX ring */
	struct buf_desc        *p_rbd;

	struct buf_desc        *p_latest_rbd;
	struct buf_desc        *p_ending_rbd;

	unsigned long           cntr[cntr_last];  /* activity counters */

	struct mii_if_info      mii;              /* used by mii lib */

	int                     mdio_clk;         /* phy mdio clock rate */
	int                     phy_id;           /* default PHY addr (0) */
	struct timer_list       phy_tmr;          /* monitor PHY state */
};

/*
 * cntrs display
 */

static int qfec_cntrs_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv        *priv = netdev_priv(to_net_dev(dev));
	int                      h    = (cntr_last + 1) / 2;
	int                      l;
	int                      n;
	int                      count = PAGE_SIZE;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	l = snprintf(&buf[0], count, "%s:\n", __func__);
	for (n = 0; n < h; n++)  {
		l += snprintf(&buf[l], count - l,
			"      %12lu  %-16s %12lu  %s\n",
			priv->cntr[n],   cntr_name[n],
			priv->cntr[n+h], cntr_name[n+h]);
	}

	return l;
}

# define CNTR_INC(priv, name)  (priv->cntr[name]++)

/*
 * functions that manage state
 */
static inline void qfec_queue_start(struct net_device *dev)
{
	struct qfec_priv  *priv = netdev_priv(dev);

	if (netif_queue_stopped(dev)) {
		netif_wake_queue(dev);
		CNTR_INC(priv, queue_start);
	}
};

static inline void qfec_queue_stop(struct net_device *dev)
{
	struct qfec_priv  *priv = netdev_priv(dev);

	netif_stop_queue(dev);
	CNTR_INC(priv, queue_stop);
};

/*
 * functions to access and initialize the MAC registers
 */
static inline uint32_t qfec_reg_read(struct qfec_priv *priv, uint32_t reg)
{
	return ioread32((void *) (priv->mac_base + reg));
}

static void qfec_reg_write(struct qfec_priv *priv, uint32_t reg, uint32_t val)
{
	uint32_t    addr = (uint32_t)priv->mac_base + reg;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %08x <- %08x\n", __func__, addr, val);
	iowrite32(val, (void *)addr);
}

/*
 * speed/duplex/pause  settings
 */
static int qfec_config_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv        *priv = netdev_priv(to_net_dev(dev));
	int                      cfg  = qfec_reg_read(priv, MAC_CONFIG_REG);
	int                      flow = qfec_reg_read(priv, FLOW_CONTROL_REG);
	int                      l    = 0;
	int                      count = PAGE_SIZE;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	l += snprintf(&buf[l], count, "%s:", __func__);

	l += snprintf(&buf[l], count - l, "  [0x%08x] %4dM %s %s", cfg,
		(cfg & MAC_CONFIG_REG_PS)
			? ((cfg & MAC_CONFIG_REG_FES) ? 100 : 10) : 1000,
		cfg & MAC_CONFIG_REG_DM ? "FD" : "HD",
		cfg & MAC_CONFIG_REG_IPC ? "IPC" : "NoIPC");

	flow &= FLOW_CONTROL_RFE | FLOW_CONTROL_TFE;
	l += snprintf(&buf[l], count - l, "  [0x%08x] %s", flow,
		(flow == (FLOW_CONTROL_RFE | FLOW_CONTROL_TFE)) ? "PAUSE"
			: ((flow == FLOW_CONTROL_RFE) ? "RX-PAUSE"
			: ((flow == FLOW_CONTROL_TFE) ? "TX-PAUSE" : "")));

	l += snprintf(&buf[l], count - l, " %s", QFEC_DRV_VER);
	l += snprintf(&buf[l], count - l, "\n");
	return l;
}


/*
 * table and functions to initialize controller registers
 */

struct reg_entry {
	unsigned int  rdonly;
	unsigned int  addr;
	char         *label;
	unsigned int  val;
};

static struct reg_entry  qfec_reg_tbl[] = {
	{ 0, BUS_MODE_REG,           "BUS_MODE_REG",     BUS_MODE_REG_DEFAULT },
	{ 0, AXI_BUS_MODE_REG,       "AXI_BUS_MODE_REG", AXI_BUS_MODE_DEFAULT },
	{ 0, AXI_STATUS_REG,         "AXI_STATUS_REG",     0 },

	{ 0, MAC_ADR_0_HIGH_REG,     "MAC_ADR_0_HIGH_REG", 0x00000302 },
	{ 0, MAC_ADR_0_LOW_REG,      "MAC_ADR_0_LOW_REG",  0x01350702 },

	{ 1, RX_DES_LST_ADR_REG,     "RX_DES_LST_ADR_REG", 0 },
	{ 1, TX_DES_LST_ADR_REG,     "TX_DES_LST_ADR_REG", 0 },
	{ 1, STATUS_REG,             "STATUS_REG",         0 },
	{ 1, DEBUG_REG,              "DEBUG_REG",          0 },

	{ 0, INTRP_EN_REG,           "INTRP_EN_REG",       QFEC_INTRP_SETUP},

	{ 1, CUR_HOST_TX_DES_REG,    "CUR_HOST_TX_DES_REG",    0 },
	{ 1, CUR_HOST_RX_DES_REG,    "CUR_HOST_RX_DES_REG",    0 },
	{ 1, CUR_HOST_TX_BU_ADR_REG, "CUR_HOST_TX_BU_ADR_REG", 0 },
	{ 1, CUR_HOST_RX_BU_ADR_REG, "CUR_HOST_RX_BU_ADR_REG", 0 },

	{ 1, MAC_FR_FILTER_REG,      "MAC_FR_FILTER_REG",      0 },

	{ 0, MAC_CONFIG_REG,         "MAC_CONFIG_REG",    MAC_CONFIG_REG_SPD_1G
							| MAC_CONFIG_REG_DM
							| MAC_CONFIG_REG_TE
							| MAC_CONFIG_REG_RE
							| MAC_CONFIG_REG_IPC },

	{ 1, INTRP_STATUS_REG,       "INTRP_STATUS_REG",   0 },
	{ 1, INTRP_MASK_REG,         "INTRP_MASK_REG",     0 },

	{ 0, OPER_MODE_REG,          "OPER_MODE_REG",  OPER_MODE_REG_DEFAULT },

	{ 1, GMII_ADR_REG,           "GMII_ADR_REG",           0 },
	{ 1, GMII_DATA_REG,          "GMII_DATA_REG",          0 },

	{ 0, MMC_INTR_MASK_RX_REG,   "MMC_INTR_MASK_RX_REG",   0xFFFFFFFF },
	{ 0, MMC_INTR_MASK_TX_REG,   "MMC_INTR_MASK_TX_REG",   0xFFFFFFFF },

	{ 1, TS_HIGH_REG,            "TS_HIGH_REG",            0 },
	{ 1, TS_LOW_REG,             "TS_LOW_REG",             0 },

	{ 1, TS_HI_UPDT_REG,         "TS_HI_UPDATE_REG",       0 },
	{ 1, TS_LO_UPDT_REG,         "TS_LO_UPDATE_REG",       0 },
	{ 0, TS_SUB_SEC_INCR_REG,    "TS_SUB_SEC_INCR_REG",    1 },
	{ 0, TS_CTL_REG,             "TS_CTL_REG",        TS_CTL_TSENALL
							| TS_CTL_TSCTRLSSR
							| TS_CTL_TSINIT
							| TS_CTL_TSENA },
};

static void qfec_reg_init(struct qfec_priv *priv)
{
	struct reg_entry *p = qfec_reg_tbl;
	int         n = ARRAY_SIZE(qfec_reg_tbl);

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	for  (; n--; p++) {
		if (!p->rdonly)
			qfec_reg_write(priv, p->addr, p->val);
	}
}

/*
 * display registers thru sysfs
 */
static int qfec_reg_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	struct reg_entry   *p = qfec_reg_tbl;
	int                 n = ARRAY_SIZE(qfec_reg_tbl);
	int                 l = 0;
	int                 count = PAGE_SIZE;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	for (; n--; p++) {
		l += snprintf(&buf[l], count - l, "    %8p   %04x %08x  %s\n",
			(void *)priv->mac_base + p->addr, p->addr,
			qfec_reg_read(priv, p->addr), p->label);
	}

	return  l;
}

/*
 * set the MAC-0 address
 */
static void qfec_set_adr_regs(struct qfec_priv *priv, uint8_t *addr)
{
	uint32_t        h = 0;
	uint32_t        l = 0;

	h = h << 8 | addr[5];
	h = h << 8 | addr[4];

	l = l << 8 | addr[3];
	l = l << 8 | addr[2];
	l = l << 8 | addr[1];
	l = l << 8 | addr[0];

	qfec_reg_write(priv, MAC_ADR_0_HIGH_REG, h);
	qfec_reg_write(priv, MAC_ADR_0_LOW_REG,  l);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %08x %08x\n", __func__, h, l);
}

/*
 * set up the RX filter
 */
static void qfec_set_rx_mode(struct net_device *dev)
{
	struct qfec_priv *priv = netdev_priv(dev);
	uint32_t filter_conf;
	int index;

	/* Clear address filter entries */
	for (index = 1; index < MAC_ADR_MAX; ++index) {
		qfec_reg_write(priv, MAC_ADR_HIGH_REG_N(index), 0);
		qfec_reg_write(priv, MAC_ADR_LOW_REG_N(index), 0);
	}

	if (dev->flags & IFF_PROMISC) {
		/* Receive all frames */
		filter_conf = MAC_FR_FILTER_RA;
	} else if ((dev->flags & IFF_MULTICAST) == 0) {
		/* Unicast filtering only */
		filter_conf = MAC_FR_FILTER_HPF;
	} else if ((netdev_mc_count(dev) > MAC_ADR_MAX - 1) ||
		   (dev->flags & IFF_ALLMULTI)) {
		/* Unicast filtering is enabled, Pass all multicast frames */
		filter_conf = MAC_FR_FILTER_HPF | MAC_FR_FILTER_PM;
	} else {
		struct netdev_hw_addr *ha;

		/* Both unicast and multicast filtering are enabled */
		filter_conf = MAC_FR_FILTER_HPF;

		index = 1;

		netdev_for_each_mc_addr(ha, dev) {
			uint32_t high, low;

			high = (1 << 31) | (ha->addr[5] << 8) | (ha->addr[4]);
			low = (ha->addr[3] << 24) | (ha->addr[2] << 16) |
				(ha->addr[1] << 8) | (ha->addr[0]);

			qfec_reg_write(priv, MAC_ADR_HIGH_REG_N(index), high);
			qfec_reg_write(priv, MAC_ADR_LOW_REG_N(index), low);

			index++;
		}
	}

	qfec_reg_write(priv, MAC_FR_FILTER_REG, filter_conf);
}

/*
 * reset the controller
 */

#define QFEC_RESET_TIMEOUT   10000
	/* reset should always clear but did not w/o test/delay
	 * in RgMii mode.  there is no spec'd max timeout
	 */

static int qfec_hw_reset(struct qfec_priv *priv)
{
	int             timeout = QFEC_RESET_TIMEOUT;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_write(priv, BUS_MODE_REG, BUS_MODE_SWR);

	while (qfec_reg_read(priv, BUS_MODE_REG) & BUS_MODE_SWR) {
		if (timeout-- == 0) {
			QFEC_LOG_ERR("%s: timeout\n", __func__);
			return -ETIME;
		}

		/* there were problems resetting the controller
		 * in RGMII mode when there wasn't sufficient
		 * delay between register reads
		 */
		usleep_range(100, 200);
	}

	return 0;
}

/*
 * initialize controller
 */
static int qfec_hw_init(struct qfec_priv *priv)
{
	int  res = 0;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	res = qfec_hw_reset(priv);
	if (res)
		return res;

	qfec_reg_init(priv);

	/* config buf-desc locations */
	qfec_reg_write(priv, TX_DES_LST_ADR_REG, priv->tbd_dma);
	qfec_reg_write(priv, RX_DES_LST_ADR_REG, priv->rbd_dma);

	/* clear interrupts */
	qfec_reg_write(priv, STATUS_REG, INTRP_EN_REG_NIE | INTRP_EN_REG_RIE
		| INTRP_EN_REG_TIE | INTRP_EN_REG_TUE | INTRP_EN_REG_ETE);

	if (priv->mii.supports_gmii) {
		/* Clear RGMII */
		qfec_reg_read(priv, SG_RG_SMII_STATUS_REG);
		/* Disable RGMII int */
		qfec_reg_write(priv, INTRP_MASK_REG, 1);
	}

	return res;
}

/*
 * en/disable controller
 */
static void qfec_hw_enable(struct qfec_priv *priv)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_write(priv, OPER_MODE_REG,
	qfec_reg_read(priv, OPER_MODE_REG)
		| OPER_MODE_REG_ST | OPER_MODE_REG_SR);
}

static void qfec_hw_disable(struct qfec_priv *priv)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_write(priv, OPER_MODE_REG,
	qfec_reg_read(priv, OPER_MODE_REG)
		& ~(OPER_MODE_REG_ST | OPER_MODE_REG_SR));
}

/*
 * interface selection
 */
struct intf_config  {
	uint32_t     intf_sel;
	uint32_t     emac_ns;
	uint32_t     eth_x_en_ns;
	uint32_t     clkmux_sel;
};

#define ETH_X_EN_NS_REVMII      (ETH_X_EN_NS_DEFAULT | ETH_TX_CLK_INV)
#define CLKMUX_REVMII           (EMAC_CLKMUX_SEL_0 | EMAC_CLKMUX_SEL_1)

static struct intf_config intf_config_tbl[] = {
	{ EMAC_PHY_INTF_SEL_MII,    EMAC_NS_DEFAULT, ETH_X_EN_NS_DEFAULT, 0 },
	{ EMAC_PHY_INTF_SEL_RGMII,  EMAC_NS_DEFAULT, ETH_X_EN_NS_DEFAULT, 0 },
	{ EMAC_PHY_INTF_SEL_REVMII, EMAC_NS_DEFAULT, ETH_X_EN_NS_REVMII,
								CLKMUX_REVMII }
};

/*
 * emac clk register read and write functions
 */
static inline uint32_t qfec_clkreg_read(struct qfec_priv *priv, uint32_t reg)
{
	return ioread32((void *) (priv->clk_base + reg));
}

static inline void qfec_clkreg_write(struct qfec_priv *priv,
	uint32_t reg, uint32_t val)
{
	uint32_t   addr = (uint32_t)priv->clk_base + reg;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %08x <- %08x\n", __func__, addr, val);
	iowrite32(val, (void *)addr);
}

/*
 * configure the PHY interface and clock routing and signal bits
 */
enum phy_intfc  {
	INTFC_MII     = 0,
	INTFC_RGMII   = 1,
	INTFC_REVMII  = 2,
};

static int qfec_intf_sel(struct qfec_priv *priv, unsigned int intfc)
{
	struct intf_config   *p;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %d\n", __func__, intfc);

	if (intfc > INTFC_REVMII)  {
		QFEC_LOG_ERR("%s: range\n", __func__);
		return -ENXIO;
	}

	p = &intf_config_tbl[intfc];

	qfec_clkreg_write(priv, EMAC_PHY_INTF_SEL_REG, p->intf_sel);
	qfec_clkreg_write(priv, EMAC_NS_REG,           p->emac_ns);
	qfec_clkreg_write(priv, ETH_X_EN_NS_REG,       p->eth_x_en_ns);
	qfec_clkreg_write(priv, EMAC_CLKMUX_SEL_REG,   p->clkmux_sel);

	return 0;
}

/*
 * display registers thru proc-fs
 */
static struct qfec_clk_reg {
	uint32_t        offset;
	char           *label;
} qfec_clk_regs[] = {
	{ ETH_MD_REG,                  "ETH_MD_REG"  },
	{ ETH_NS_REG,                  "ETH_NS_REG"  },
	{ ETH_X_EN_NS_REG,             "ETH_X_EN_NS_REG"  },
	{ EMAC_PTP_MD_REG,             "EMAC_PTP_MD_REG"  },
	{ EMAC_PTP_NS_REG,             "EMAC_PTP_NS_REG"  },
	{ EMAC_NS_REG,                 "EMAC_NS_REG"  },
	{ EMAC_TX_FS_REG,              "EMAC_TX_FS_REG"  },
	{ EMAC_RX_FS_REG,              "EMAC_RX_FS_REG"  },
	{ EMAC_PHY_INTF_SEL_REG,       "EMAC_PHY_INTF_SEL_REG"  },
	{ EMAC_PHY_ADDR_REG,           "EMAC_PHY_ADDR_REG"  },
	{ EMAC_REVMII_PHY_ADDR_REG,    "EMAC_REVMII_PHY_ADDR_REG"  },
	{ EMAC_CLKMUX_SEL_REG,         "EMAC_CLKMUX_SEL_REG"  },
};

static int qfec_clk_reg_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv        *priv = netdev_priv(to_net_dev(dev));
	struct qfec_clk_reg     *p = qfec_clk_regs;
	int                      n = ARRAY_SIZE(qfec_clk_regs);
	int                      l = 0;
	int                      count = PAGE_SIZE;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	for (; n--; p++) {
		l += snprintf(&buf[l], count - l, "    %8p  %8x  %08x  %s\n",
			(void *)priv->clk_base + p->offset, p->offset,
			qfec_clkreg_read(priv, p->offset), p->label);
	}

	return  l;
}

/*
 * speed selection
 */

struct qfec_pll_cfg {
	uint32_t    spd;
	uint32_t    eth_md;     /* M [31:16], NOT 2*D [15:0] */
	uint32_t    eth_ns;     /* NOT(M-N) [31:16], ctl bits [11:0]  */
};

static struct qfec_pll_cfg qfec_pll_cfg_tbl[] = {
	/* 2.5 MHz */
	{ MAC_CONFIG_REG_SPD_10,   ETH_MD_M(1)  | ETH_MD_2D_N(100),
						  ETH_NS_NM(100-1)
						| ETH_NS_MCNTR_EN
						| ETH_NS_MCNTR_MODE_DUAL
						| ETH_NS_PRE_DIV(0)
						| CLK_SRC_PLL_EMAC },
	/* 25 MHz */
	{ MAC_CONFIG_REG_SPD_100,  ETH_MD_M(1)  | ETH_MD_2D_N(10),
						  ETH_NS_NM(10-1)
						| ETH_NS_MCNTR_EN
						| ETH_NS_MCNTR_MODE_DUAL
						| ETH_NS_PRE_DIV(0)
						| CLK_SRC_PLL_EMAC },
	/* 125 MHz */
	{MAC_CONFIG_REG_SPD_1G,    0,             ETH_NS_PRE_DIV(1)
						| CLK_SRC_PLL_EMAC },
};

enum speed  {
	SPD_10   = 0,
	SPD_100  = 1,
	SPD_1000 = 2,
};

/*
 * configure the PHY interface and clock routing and signal bits
 */
static int qfec_speed_cfg(struct net_device *dev, unsigned int spd,
	unsigned int dplx)
{
	struct qfec_priv       *priv = netdev_priv(dev);
	struct qfec_pll_cfg    *p;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %d spd, %d dplx\n", __func__, spd, dplx);

	if (spd > SPD_1000)  {
		QFEC_LOG_ERR("%s: range\n", __func__);
		return -ENODEV;
	}

	p = &qfec_pll_cfg_tbl[spd];

	/* set the MAC speed bits */
	qfec_reg_write(priv, MAC_CONFIG_REG,
	(qfec_reg_read(priv, MAC_CONFIG_REG)
		& ~(MAC_CONFIG_REG_SPD | MAC_CONFIG_REG_DM))
			| p->spd | (dplx ? MAC_CONFIG_REG_DM : H_DPLX));

	qfec_clkreg_write(priv, ETH_MD_REG, p->eth_md);
	qfec_clkreg_write(priv, ETH_NS_REG, p->eth_ns);

	return 0;
}

/*
 * configure PTP divider for 25 MHz assuming EMAC PLL 250 MHz
 */

static struct qfec_pll_cfg qfec_pll_ptp = {
	/* 19.2 MHz  tcxo */
	0,      0,                                ETH_NS_PRE_DIV(0)
						| EMAC_PTP_NS_ROOT_EN
						| EMAC_PTP_NS_CLK_EN
						| CLK_SRC_TCXO
};

#define PLLTEST_PAD_CFG     0x01E0
#define PLLTEST_PLL_7       0x3700

#define CLKTEST_REG         0x01EC
#define CLKTEST_EMAC_RX     0x3fc07f7a

static int qfec_ptp_cfg(struct qfec_priv *priv)
{
	struct qfec_pll_cfg    *p    = &qfec_pll_ptp;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %08x md, %08x ns\n",
		__func__, p->eth_md, p->eth_ns);

	qfec_clkreg_write(priv, EMAC_PTP_MD_REG, p->eth_md);
	qfec_clkreg_write(priv, EMAC_PTP_NS_REG, p->eth_ns);

	/* configure HS/LS clk test ports to verify clks */
	qfec_clkreg_write(priv, CLKTEST_REG,     CLKTEST_EMAC_RX);
	qfec_clkreg_write(priv, PLLTEST_PAD_CFG, PLLTEST_PLL_7);

	return 0;
}

/*
 * MDIO operations
 */

/*
 * wait reasonable amount of time for MDIO operation to complete, not busy
 */
static int qfec_mdio_busy(struct net_device *dev)
{
	int     i;

	for (i = 100; i > 0; i--)  {
		if (!(qfec_reg_read(
			netdev_priv(dev), GMII_ADR_REG) & GMII_ADR_REG_GB))  {
			return 0;
		}
		udelay(1);
	}

	return -ETIME;
}

/*
 * initiate either a read or write MDIO operation
 */

static int qfec_mdio_oper(struct net_device *dev, int phy_id, int reg, int wr)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	int                 res = 0;

	/* insure phy not busy */
	res = qfec_mdio_busy(dev);
	if (res)  {
		QFEC_LOG_ERR("%s: busy\n", __func__);
		goto done;
	}

	/* initiate operation */
	qfec_reg_write(priv, GMII_ADR_REG,
		GMII_ADR_REG_ADR_SET(phy_id)
		| GMII_ADR_REG_REG_SET(reg)
		| GMII_ADR_REG_CSR_SET(priv->mdio_clk)
		| (wr ? GMII_ADR_REG_GW : 0)
		| GMII_ADR_REG_GB);

	/* wait for operation to complete */
	res = qfec_mdio_busy(dev);
	if (res)
		QFEC_LOG_ERR("%s: timeout\n", __func__);

done:
	return res;
}

/*
 * read MDIO register
 */
static int qfec_mdio_read(struct net_device *dev, int phy_id, int reg)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	int                 res = 0;
	unsigned long       flags;

	spin_lock_irqsave(&priv->mdio_lock, flags);

	res = qfec_mdio_oper(dev, phy_id, reg, 0);
	if (res)  {
		QFEC_LOG_ERR("%s: oper\n", __func__);
		goto done;
	}

	res = qfec_reg_read(priv, GMII_DATA_REG);
	QFEC_LOG(QFEC_LOG_MDIO_R, "%s: %2d reg, 0x%04x val\n",
		__func__, reg, res);

done:
	spin_unlock_irqrestore(&priv->mdio_lock, flags);
	return res;
}

/*
 * write MDIO register
 */
static void qfec_mdio_write(struct net_device *dev, int phy_id, int reg,
	int val)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	unsigned long       flags;

	spin_lock_irqsave(&priv->mdio_lock, flags);

	QFEC_LOG(QFEC_LOG_MDIO_W, "%s: %2d reg, %04x\n",
		__func__, reg, val);

	qfec_reg_write(priv, GMII_DATA_REG, val);

	if (qfec_mdio_oper(dev, phy_id, reg, 1))
		QFEC_LOG_ERR("%s: oper\n", __func__);

	spin_unlock_irqrestore(&priv->mdio_lock, flags);
}

/*
 * MDIO show
 */
static int qfec_mdio_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct qfec_priv        *priv = netdev_priv(to_net_dev(dev));
	int                      n;
	int                      l = 0;
	int                      count = PAGE_SIZE;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	for (n = 0; n < MAX_MDIO_REG; n++) {
		if (!(n % 8))
			l += snprintf(&buf[l], count - l, "\n   %02x: ", n);

		l += snprintf(&buf[l], count - l, " %04x",
			qfec_mdio_read(to_net_dev(dev), priv->phy_id, n));
	}
	l += snprintf(&buf[l], count - l, "\n");

	return  l;
}

/*
 * get auto-negotiation results
 */
#define QFEC_100        (LPA_100HALF | LPA_100FULL | LPA_100HALF)
#define QFEC_100_FD     (LPA_100FULL | LPA_100BASE4)
#define QFEC_10         (LPA_10HALF  | LPA_10FULL)
#define QFEC_10_FD       LPA_10FULL

static void qfec_get_an(struct net_device *dev, uint32_t *spd, uint32_t *dplx)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	uint32_t  advert   = qfec_mdio_read(dev, priv->phy_id, MII_ADVERTISE);
	uint32_t  lpa      = qfec_mdio_read(dev, priv->phy_id, MII_LPA);
	uint32_t  mastCtrl = qfec_mdio_read(dev, priv->phy_id, MII_CTRL1000);
	uint32_t  mastStat = qfec_mdio_read(dev, priv->phy_id, MII_STAT1000);
	uint32_t  anExp    = qfec_mdio_read(dev, priv->phy_id, MII_EXPANSION);
	uint32_t  status   = advert & lpa;
	uint32_t  flow;

	if (priv->mii.supports_gmii) {
		if (((anExp & QFEC_MII_EXP_MASK) == QFEC_MII_EXP_MASK)
			&& (mastCtrl & ADVERTISE_1000FULL)
			&& (mastStat & LPA_1000FULL)) {
			*spd  = SPD_1000;
			*dplx = F_DPLX;
			goto pause;
		}

		else if (((anExp & QFEC_MII_EXP_MASK) == QFEC_MII_EXP_MASK)
			&& (mastCtrl & ADVERTISE_1000HALF)
			&& (mastStat & LPA_1000HALF)) {
			*spd  = SPD_1000;
			*dplx = H_DPLX;
			goto pause;
		}
	}

	/* mii speeds */
	if (status & QFEC_100)  {
		*spd  = SPD_100;
		*dplx = status & QFEC_100_FD ? F_DPLX : H_DPLX;
	}

	else if (status & QFEC_10)  {
		*spd  = SPD_10;
		*dplx = status & QFEC_10_FD ? F_DPLX : H_DPLX;
	}

	/* check pause */
pause:
	flow  = qfec_reg_read(priv, FLOW_CONTROL_REG);
	flow &= ~(FLOW_CONTROL_TFE | FLOW_CONTROL_RFE);

	if (status & ADVERTISE_PAUSE_CAP)  {
		flow |= FLOW_CONTROL_RFE | FLOW_CONTROL_TFE;
	} else if (status & ADVERTISE_PAUSE_ASYM)  {
		if (lpa & ADVERTISE_PAUSE_CAP)
			flow |= FLOW_CONTROL_TFE;
		else if (advert & ADVERTISE_PAUSE_CAP)
			flow |= FLOW_CONTROL_RFE;
	}

	qfec_reg_write(priv, FLOW_CONTROL_REG, flow);
}

/*
 * monitor phy status, and process auto-neg results when changed
 */

static void qfec_phy_monitor(unsigned long data)
{
	struct net_device  *dev  = (struct net_device *) data;
	struct qfec_priv   *priv = netdev_priv(dev);
	unsigned int        spd  = H_DPLX;
	unsigned int        dplx = F_DPLX;

	mod_timer(&priv->phy_tmr, jiffies + HZ);

	if (mii_link_ok(&priv->mii) && !netif_carrier_ok(priv->net_dev))  {
		qfec_get_an(dev, &spd, &dplx);
		qfec_speed_cfg(dev, spd, dplx);
		QFEC_LOG(QFEC_LOG_DBG, "%s: link up, %d spd, %d dplx\n",
			__func__, spd, dplx);

		netif_carrier_on(dev);
	}

	else if (!mii_link_ok(&priv->mii) && netif_carrier_ok(priv->net_dev))  {
		QFEC_LOG(QFEC_LOG_DBG, "%s: link down\n", __func__);
		netif_carrier_off(dev);
	}
}

/*
 * dealloc buffer descriptor memory
 */

static void qfec_mem_dealloc(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);

	dma_free_coherent(&dev->dev,
		priv->bd_size, priv->bd_base, priv->tbd_dma);
	priv->bd_base = 0;
}

/*
 * allocate shared device memory for TX/RX buf-desc (and buffers)
 */

static int qfec_mem_alloc(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %p dev\n", __func__, dev);

	priv->bd_size =
		(priv->n_tbd + priv->n_rbd) * sizeof(struct qfec_buf_desc);

	priv->p_tbd = kcalloc(priv->n_tbd, sizeof(struct buf_desc), GFP_KERNEL);
	if (!priv->p_tbd)  {
		QFEC_LOG_ERR("%s: kcalloc failed p_tbd\n", __func__);
		return -ENOMEM;
	}

	priv->p_rbd = kcalloc(priv->n_rbd, sizeof(struct buf_desc), GFP_KERNEL);
	if (!priv->p_rbd)  {
		QFEC_LOG_ERR("%s: kcalloc failed p_rbd\n", __func__);
		return -ENOMEM;
	}

	/* alloc mem for buf-desc, if not already alloc'd */
	if (!priv->bd_base)  {
		priv->bd_base = dma_alloc_coherent(&dev->dev,
			priv->bd_size, &priv->tbd_dma,
			GFP_KERNEL | __GFP_DMA);
	}

	if (!priv->bd_base)  {
		QFEC_LOG_ERR("%s: dma_alloc_coherent failed\n", __func__);
		return -ENOMEM;
	}

	priv->rbd_dma   = priv->tbd_dma
			+ (priv->n_tbd * sizeof(struct qfec_buf_desc));

	QFEC_LOG(QFEC_LOG_DBG,
		" %s: 0x%08x size, %d n_tbd, %d n_rbd\n",
		__func__, priv->bd_size, priv->n_tbd, priv->n_rbd);

	return 0;
}

/*
 * display buffer descriptors
 */

static int qfec_bd_fmt(char *buf, int size, struct buf_desc *p_bd)
{
	return snprintf(buf, size,
		"%8p: %08x %08x %8p %8p  %8p %8p %8p %x",
		p_bd,                     qfec_bd_status_get(p_bd),
		qfec_bd_ctl_get(p_bd),    qfec_bd_pbuf_get(p_bd),
		qfec_bd_next_get(p_bd),   qfec_bd_skbuf_get(p_bd),
		qfec_bd_virt_get(p_bd),   qfec_bd_phys_get(p_bd),
		qfec_bd_last_bd(p_bd));
}

static int qfec_bd_show(char *buf, int count, struct buf_desc *p_bd, int n_bd,
	struct ring *p_ring, char *label)
{
	int     l = 0;
	int     n;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %s\n", __func__, label);

	l += snprintf(&buf[l], count, "%s: %s\n", __func__, label);
	if (!p_bd)
		return l;

	n_bd = n_bd > MAX_N_BD ? MAX_N_BD : n_bd;

	for (n = 0; n < n_bd; n++, p_bd++) {
		l += qfec_bd_fmt(&buf[l], count - l, p_bd);
		l += snprintf(&buf[l], count - l, "%s%s\n",
			(qfec_ring_head(p_ring) == n ? " < h" : ""),
			(qfec_ring_tail(p_ring) == n ? " < t" : ""));
	}

	return l;
}

/*
 * display TX BDs
 */
static int qfec_bd_tx_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;

	return qfec_bd_show(buf, count, priv->p_tbd, priv->n_tbd,
				&priv->ring_tbd, "TX");
}

/*
 * display RX BDs
 */
static int qfec_bd_rx_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;

	return  qfec_bd_show(buf, count, priv->p_rbd, priv->n_rbd,
				&priv->ring_rbd, "RX");
}

/*
 * process timestamp values
 *    The pbuf and next fields of the buffer descriptors are overwritten
 *    with the timestamp high and low register values.
 *
 *    The low register is incremented by the value in the subsec_increment
 *    register and overflows at 0x8000 0000 causing the high register to
 *    increment.
 *
 *    The subsec_increment register is recommended to be set to the number
 *    of nanosec corresponding to each clock tic, scaled by 2^31 / 10^9
 *    (e.g. 40 * 2^32 / 10^9 = 85.9, or 86 for 25 MHz).  However, the
 *    rounding error in this case will result in a 1 sec error / ~14 mins.
 *
 *    An alternate approach is used.  The subsec_increment is set to 1,
 *    and the concatenation of the 2 timestamp registers used to count
 *    clock tics.  The 63-bit result is manipulated to determine the number
 *    of sec and ns.
 */

/*
 * convert 19.2 MHz clock tics into sec/ns
 */
#define TS_LOW_REG_BITS    31

#define MILLION            1000000UL
#define BILLION            1000000000UL

#define F_CLK              19200000UL
#define F_CLK_PRE_SC       24
#define F_CLK_INV_Q        56
#define F_CLK_INV          (((unsigned long long)1 << F_CLK_INV_Q) / F_CLK)
#define F_CLK_TO_NS_Q      25
#define F_CLK_TO_NS \
	(((((unsigned long long)1<<F_CLK_TO_NS_Q)*BILLION)+(F_CLK-1))/F_CLK)
#define US_TO_F_CLK_Q      20
#define US_TO_F_CLK \
	(((((unsigned long long)1<<US_TO_F_CLK_Q)*F_CLK)+(MILLION-1))/MILLION)

static inline void qfec_get_sec(uint64_t *cnt,
			uint32_t  *sec, uint32_t  *ns)
{
	unsigned long long  t;
	unsigned long long  subsec;

	t       = *cnt >> F_CLK_PRE_SC;
	t      *= F_CLK_INV;
	t     >>= F_CLK_INV_Q - F_CLK_PRE_SC;
	*sec    = t;

	t       = *cnt - (t * F_CLK);
	subsec  = t;

	if (subsec >= F_CLK)  {
		subsec -= F_CLK;
		*sec   += 1;
	}

	subsec  *= F_CLK_TO_NS;
	subsec >>= F_CLK_TO_NS_Q;
	*ns      = subsec;
}

/*
 * read ethernet timestamp registers, pass up raw register values
 * and values converted to sec/ns
 */
static void qfec_read_timestamp(struct buf_desc *p_bd,
	struct skb_shared_hwtstamps *ts)
{
	unsigned long long  cnt;
	unsigned int        sec;
	unsigned int        subsec;

	cnt    = (unsigned long)qfec_bd_next_get(p_bd);
	cnt  <<= TS_LOW_REG_BITS;
	cnt   |= (unsigned long)qfec_bd_pbuf_get(p_bd);

	/* report raw counts as concatenated 63 bits */
	sec    = cnt >> 32;
	subsec = cnt & 0xffffffff;

	ts->hwtstamp  = ktime_set(sec, subsec);

	/* translate counts to sec and ns */
	qfec_get_sec(&cnt, &sec, &subsec);

	ts->syststamp = ktime_set(sec, subsec);
}

/*
 * capture the current system time in the timestamp registers
 */
static int qfec_cmd(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct qfec_priv  *priv = netdev_priv(to_net_dev(dev));
	struct timeval     tv;

	if (!strncmp(buf, "setTs", 5))  {
		unsigned long long  cnt;
		uint32_t            ts_hi;
		uint32_t            ts_lo;
		unsigned long long  subsec;

		do_gettimeofday(&tv);

		/* convert raw sec/usec to ns */
		subsec   = tv.tv_usec;
		subsec  *= US_TO_F_CLK;
		subsec >>= US_TO_F_CLK_Q;

		cnt     = tv.tv_sec;
		cnt    *= F_CLK;
		cnt    += subsec;

		ts_hi   = cnt >> 31;
		ts_lo   = cnt & 0x7FFFFFFF;

		qfec_reg_write(priv, TS_HI_UPDT_REG, ts_hi);
		qfec_reg_write(priv, TS_LO_UPDT_REG, ts_lo);

		qfec_reg_write(priv, TS_CTL_REG,
			qfec_reg_read(priv, TS_CTL_REG) | TS_CTL_TSINIT);
	} else
		pr_err("%s: unknown cmd, %s.\n", __func__, buf);

	return strnlen(buf, count);
}

/*
 * display ethernet tstamp and system time
 */
static int qfec_tstamp_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;
	int                 l;
	struct timeval      tv;
	unsigned long long  cnt;
	uint32_t            sec;
	uint32_t            ns;
	uint32_t            ts_hi;
	uint32_t            ts_lo;

	/* insure that ts_hi didn't increment during read */
	do {
		ts_hi = qfec_reg_read(priv, TS_HIGH_REG);
		ts_lo = qfec_reg_read(priv, TS_LOW_REG);
	} while (ts_hi != qfec_reg_read(priv, TS_HIGH_REG));

	cnt    = ts_hi;
	cnt  <<= TS_LOW_REG_BITS;
	cnt   |= ts_lo;

	do_gettimeofday(&tv);

	ts_hi  = cnt >> 32;
	ts_lo  = cnt & 0xffffffff;

	qfec_get_sec(&cnt, &sec, &ns);

	l = snprintf(buf, count,
		"%12u.%09u sec 0x%08x 0x%08x tstamp  %12u.%06u time-of-day\n",
		sec, ns, ts_hi, ts_lo, (int)tv.tv_sec, (int)tv.tv_usec);

	return l;
}

/*
 * free transmitted skbufs from buffer-descriptor no owned by HW
 */
static int qfec_tx_replenish(struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct ring        *p_ring = &priv->ring_tbd;
	struct buf_desc    *p_bd   = &priv->p_tbd[qfec_ring_tail(p_ring)];
	struct sk_buff     *skb;
	unsigned long      flags;

	CNTR_INC(priv, tx_replenish);

	spin_lock_irqsave(&priv->xmit_lock, flags);

	while (!qfec_ring_empty(p_ring))  {
		if (qfec_bd_own(p_bd))
			break;          /* done for now */

		skb = qfec_bd_skbuf_get(p_bd);
		if (unlikely(skb == NULL))  {
			QFEC_LOG_ERR("%s: null sk_buff\n", __func__);
			CNTR_INC(priv, tx_skb_null);
			break;
		}

		qfec_reg_write(priv, STATUS_REG,
			STATUS_REG_TU | STATUS_REG_TI);

		/* retrieve timestamp if requested */
		if (qfec_bd_status_get(p_bd) & BUF_TX_TTSS)  {
			CNTR_INC(priv, ts_tx_rtn);
			qfec_read_timestamp(p_bd, skb_hwtstamps(skb));
			skb_tstamp_tx(skb, skb_hwtstamps(skb));
		}

		/* update statistics before freeing skb */
		priv->stats.tx_packets++;
		priv->stats.tx_bytes  += skb->len;

		dma_unmap_single(&dev->dev, (dma_addr_t) qfec_bd_pbuf_get(p_bd),
				skb->len, DMA_TO_DEVICE);

		dev_kfree_skb_any(skb);
		qfec_bd_skbuf_set(p_bd, NULL);

		qfec_ring_tail_adv(p_ring);
		p_bd   = &priv->p_tbd[qfec_ring_tail(p_ring)];
	}

	spin_unlock_irqrestore(&priv->xmit_lock, flags);

	qfec_queue_start(dev);

	return 0;
}

/*
 * clear ownership bits of all TX buf-desc and release the sk-bufs
 */
static void qfec_tx_timeout(struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct buf_desc    *bd     = priv->p_tbd;
	int                 n;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);
	CNTR_INC(priv, tx_timeout);

	for (n = 0; n < priv->n_tbd; n++, bd++)
		qfec_bd_own_clr(bd);

	qfec_tx_replenish(dev);
}

/*
 * rx() - process a received frame
 */
static void qfec_rx_int(struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct ring        *p_ring = &priv->ring_rbd;
	struct buf_desc    *p_bd   = priv->p_latest_rbd;
	uint32_t desc_status;
	uint32_t mis_fr_reg;

	desc_status = qfec_bd_status_get(p_bd);
	mis_fr_reg = qfec_reg_read(priv, MIS_FR_REG);

	CNTR_INC(priv, rx_int);

	/* check that valid interrupt occurred */
	if (unlikely(desc_status & BUF_OWN))
		return;

	/* accumulate missed-frame count (reg reset when read) */
	priv->stats.rx_missed_errors += mis_fr_reg
					& MIS_FR_REG_MISS_CNT;

	/* process all unowned frames */
	while (!(desc_status & BUF_OWN) && (!qfec_ring_full(p_ring)))  {
		struct sk_buff     *skb;
		struct buf_desc    *p_bd_next;

		skb = qfec_bd_skbuf_get(p_bd);

		if (unlikely(skb == NULL))  {
			QFEC_LOG_ERR("%s: null sk_buff\n", __func__);
			CNTR_INC(priv, rx_skb_null);
			break;
		}

		/* cache coherency before skb->data is accessed */
		dma_unmap_single(&dev->dev,
			(dma_addr_t) qfec_bd_phys_get(p_bd),
			ETH_BUF_SIZE, DMA_FROM_DEVICE);
		prefetch(skb->data);

		if (unlikely(desc_status & BUF_RX_ES)) {
			priv->stats.rx_dropped++;
			CNTR_INC(priv, rx_dropped);
			dev_kfree_skb(skb);
		} else  {
			qfec_reg_write(priv, STATUS_REG, STATUS_REG_RI);

			skb->len = BUF_RX_FL_GET_FROM_STATUS(desc_status);

			if (priv->state & timestamping)  {
				CNTR_INC(priv, ts_rec);
				qfec_read_timestamp(p_bd, skb_hwtstamps(skb));
			}

			/* update statistics before freeing skb */
			priv->stats.rx_packets++;
			priv->stats.rx_bytes  += skb->len;

			skb->dev        = dev;
			skb->protocol   = eth_type_trans(skb, dev);
			skb->ip_summed  = CHECKSUM_UNNECESSARY;

			if (NET_RX_DROP == netif_rx(skb))  {
				priv->stats.rx_dropped++;
				CNTR_INC(priv, rx_dropped);
			}
			CNTR_INC(priv, netif_rx_cntr);
		}

		if (p_bd != priv->p_ending_rbd)
			p_bd_next = p_bd + 1;
		else
			p_bd_next = priv->p_rbd;
		desc_status = qfec_bd_status_get(p_bd_next);

		qfec_bd_skbuf_set(p_bd, NULL);

		qfec_ring_head_adv(p_ring);
		p_bd = p_bd_next;
	}

	priv->p_latest_rbd = p_bd;

	/* replenish bufs */
	while (!qfec_ring_empty(p_ring))  {
		if (qfec_rbd_init(dev, &priv->p_rbd[qfec_ring_tail(p_ring)]))
			break;
		qfec_ring_tail_adv(p_ring);
	}

	qfec_reg_write(priv, STATUS_REG, STATUS_REG_RI);
}

/*
 * isr() - interrupt service routine
 *          determine cause of interrupt and invoke/schedule appropriate
 *          processing or error handling
 */
#define ISR_ERR_CHK(priv, status, interrupt, cntr) \
	if (status & interrupt) \
		CNTR_INC(priv, cntr)

static irqreturn_t qfec_int(int irq, void *dev_id)
{
	struct net_device  *dev      = dev_id;
	struct qfec_priv   *priv     = netdev_priv(dev);
	uint32_t            status   = qfec_reg_read(priv, STATUS_REG);
	uint32_t            int_bits = STATUS_REG_NIS | STATUS_REG_AIS;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %s\n", __func__, dev->name);

	/* abnormal interrupt */
	if (status & STATUS_REG_AIS)  {
		QFEC_LOG(QFEC_LOG_DBG, "%s: abnormal status 0x%08x\n",
			__func__, status);

		ISR_ERR_CHK(priv, status, STATUS_REG_RU,  rx_buf_unavail);
		ISR_ERR_CHK(priv, status, STATUS_REG_FBI, fatal_bus);

		ISR_ERR_CHK(priv, status, STATUS_REG_RWT, rx_watchdog);
		ISR_ERR_CHK(priv, status, STATUS_REG_RPS, rx_proc_stopped);
		ISR_ERR_CHK(priv, status, STATUS_REG_UNF, tx_underflow);

		ISR_ERR_CHK(priv, status, STATUS_REG_OVF, rx_overflow);
		ISR_ERR_CHK(priv, status, STATUS_REG_TJT, tx_jabber_tmout);
		ISR_ERR_CHK(priv, status, STATUS_REG_TPS, tx_proc_stopped);

		int_bits |= STATUS_REG_AIS_BITS;
		CNTR_INC(priv, abnorm_int);
	}

	if (status & STATUS_REG_NIS)
		CNTR_INC(priv, norm_int);

	/* receive interrupt */
	if (status & STATUS_REG_RI) {
		CNTR_INC(priv, rx_isr);
		qfec_rx_int(dev);
	}

	/* transmit interrupt */
	if (status & STATUS_REG_TI)  {
		CNTR_INC(priv, tx_isr);
		qfec_tx_replenish(dev);
	}

	/* gmac interrupt */
	if (status & (STATUS_REG_GPI | STATUS_REG_GMI | STATUS_REG_GLI))  {
		status &= ~(STATUS_REG_GPI | STATUS_REG_GMI | STATUS_REG_GLI);
		CNTR_INC(priv, gmac_isr);
		int_bits |= STATUS_REG_GPI | STATUS_REG_GMI | STATUS_REG_GLI;
		qfec_reg_read(priv, SG_RG_SMII_STATUS_REG);
	}

	/* clear interrupts */
	qfec_reg_write(priv, STATUS_REG, int_bits);
	CNTR_INC(priv, isr);

	return IRQ_HANDLED;
}

/*
 * open () - register system resources (IRQ, DMA, ...)
 *   turn on HW, perform device setup.
 */
static int qfec_open(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct buf_desc    *p_bd;
	struct ring        *p_ring;
	struct qfec_buf_desc *p_desc;
	int                 n;
	int                 res = 0;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %p dev\n", __func__, dev);

	if (!dev)  {
		res = -EINVAL;
		goto err;
	}

	/* allocate TX/RX buffer-descriptors and buffers */

	res = qfec_mem_alloc(dev);
	if (res)
		goto err;

	/* initialize TX */
	p_desc = priv->bd_base;

	for (n = 0, p_bd = priv->p_tbd; n < priv->n_tbd; n++, p_bd++) {
		p_bd->p_desc = p_desc++;

		if (n == (priv->n_tbd - 1))
			qfec_bd_last_bd_set(p_bd);

		qfec_bd_own_clr(p_bd);      /* clear ownership */
	}

	qfec_ring_init(&priv->ring_tbd, priv->n_tbd, priv->n_tbd);

	priv->tx_ic_mod = priv->n_tbd / TX_BD_TI_RATIO;
	if (priv->tx_ic_mod == 0)
		priv->tx_ic_mod = 1;

	/* initialize RX buffer descriptors and allocate sk_bufs */
	p_ring = &priv->ring_rbd;
	qfec_ring_init(p_ring, priv->n_rbd, 0);
	qfec_bd_last_bd_set(&priv->p_rbd[priv->n_rbd - 1]);

	for (n = 0, p_bd = priv->p_rbd; n < priv->n_rbd; n++, p_bd++) {
		p_bd->p_desc = p_desc++;

		if (qfec_rbd_init(dev, p_bd))
			break;
		qfec_ring_tail_adv(p_ring);
	}

	priv->p_latest_rbd = priv->p_rbd;
	priv->p_ending_rbd = priv->p_rbd + priv->n_rbd - 1;

	/* config ptp clock */
	qfec_ptp_cfg(priv);

	/* configure PHY - must be set before reset/hw_init */
	priv->mii.supports_gmii = mii_check_gmii_support(&priv->mii);
	if (priv->mii.supports_gmii) {
		QFEC_LOG_ERR("%s: RGMII\n", __func__);
		qfec_intf_sel(priv, INTFC_RGMII);
	} else {
		QFEC_LOG_ERR("%s: MII\n", __func__);
		qfec_intf_sel(priv, INTFC_MII);
	}

	/* initialize controller after BDs allocated */
	res = qfec_hw_init(priv);
	if (res)
		goto err1;

	/* get/set (primary) MAC address */
	qfec_set_adr_regs(priv, dev->dev_addr);
	qfec_set_rx_mode(dev);

	/* start phy monitor */
	QFEC_LOG(QFEC_LOG_DBG, " %s: start timer\n", __func__);
	netif_carrier_off(priv->net_dev);
	setup_timer(&priv->phy_tmr, qfec_phy_monitor, (unsigned long)dev);
	mod_timer(&priv->phy_tmr, jiffies + HZ);

	/* driver supports AN capable PHY only */
	qfec_mdio_write(dev, priv->phy_id, MII_BMCR, BMCR_RESET);
	res = (BMCR_ANENABLE|BMCR_ANRESTART);
	qfec_mdio_write(dev, priv->phy_id, MII_BMCR, res);

	/* initialize interrupts */
	QFEC_LOG(QFEC_LOG_DBG, " %s: request irq %d\n", __func__, dev->irq);
	res = request_irq(dev->irq, qfec_int, 0, dev->name, dev);
	if (res)
		goto err1;

	/* enable controller */
	qfec_hw_enable(priv);
	netif_start_queue(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %08x link, %08x carrier\n", __func__,
		mii_link_ok(&priv->mii), netif_carrier_ok(priv->net_dev));

	QFEC_LOG(QFEC_LOG_DBG, " %s: done\n", __func__);
	return 0;

err1:
	qfec_mem_dealloc(dev);
err:
	QFEC_LOG_ERR("%s: error - %d\n", __func__, res);
	return res;
}

/*
 * stop() - "reverse operations performed at open time"
 */
static int qfec_stop(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct buf_desc    *p_bd;
	struct sk_buff     *skb;
	int                 n;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	del_timer_sync(&priv->phy_tmr);

	qfec_hw_disable(priv);
	qfec_queue_stop(dev);
	free_irq(dev->irq, dev);

	/* free all pending sk_bufs */
	for (n = priv->n_rbd, p_bd = priv->p_rbd; n > 0; n--, p_bd++) {
		skb = qfec_bd_skbuf_get(p_bd);
		if (skb)
			dev_kfree_skb(skb);
	}

	for (n = priv->n_tbd, p_bd = priv->p_tbd; n > 0; n--, p_bd++) {
		skb = qfec_bd_skbuf_get(p_bd);
		if (skb)
			dev_kfree_skb(skb);
	}

	qfec_mem_dealloc(dev);

	QFEC_LOG(QFEC_LOG_DBG, " %s: done\n", __func__);

	return 0;
}

static int qfec_set_config(struct net_device *dev, struct ifmap *map)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);
	return 0;
}

/*
 * pass data from skbuf to buf-desc
 */
static int qfec_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct ring        *p_ring = &priv->ring_tbd;
	struct buf_desc    *p_bd;
	uint32_t            ctrl   = 0;
	int                 ret    = NETDEV_TX_OK;
	unsigned long       flags;

	CNTR_INC(priv, xmit);

	spin_lock_irqsave(&priv->xmit_lock, flags);

	/* If there is no room, on the ring try to free some up */
	if (qfec_ring_room(p_ring) == 0)
		qfec_tx_replenish(dev);

	/* stop queuing if no resources available */
	if (qfec_ring_room(p_ring) == 0)  {
		qfec_queue_stop(dev);
		CNTR_INC(priv, tx_no_resource);

		ret = NETDEV_TX_BUSY;
		goto done;
	}

	/* locate and save *sk_buff */
	p_bd = &priv->p_tbd[qfec_ring_head(p_ring)];
	qfec_bd_skbuf_set(p_bd, skb);

	/* set DMA ptr to sk_buff data and write cache to memory */
	qfec_bd_pbuf_set(p_bd, (void *)
	dma_map_single(&dev->dev,
		(void *)skb->data, skb->len, DMA_TO_DEVICE));

	ctrl  = skb->len;
	if (!(qfec_ring_head(p_ring) % priv->tx_ic_mod))
		ctrl |= BUF_TX_IC; /* interrupt on complete */

	/* check if timestamping enabled and requested */
	if (priv->state & timestamping)  {
		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
			CNTR_INC(priv, ts_tx_en);
			ctrl |= BUF_TX_IC;	/* interrupt on complete */
			ctrl |= BUF_TX_TTSE;	/* enable timestamp */
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		}
	}

	if (qfec_bd_last_bd(p_bd))
		ctrl |= BUF_RX_RER;

	/* no gather, no multi buf frames */
	ctrl |= BUF_TX_FS | BUF_TX_LS;  /* 1st and last segment */

	qfec_bd_ctl_wr(p_bd, ctrl);
	qfec_bd_status_set(p_bd, BUF_OWN);

	qfec_ring_head_adv(p_ring);
	qfec_reg_write(priv, TX_POLL_DEM_REG, 1);      /* poll */

done:
	spin_unlock_irqrestore(&priv->xmit_lock, flags);

	return ret;
}

static int qfec_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct qfec_priv        *priv = netdev_priv(dev);
	struct hwtstamp_config  *cfg  = (struct hwtstamp_config *) ifr;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	if (cmd == SIOCSHWTSTAMP) {
		CNTR_INC(priv, ts_ioctl);
		QFEC_LOG(QFEC_LOG_DBG,
			"%s: SIOCSHWTSTAMP - %x flags  %x tx  %x rx\n",
			__func__, cfg->flags, cfg->tx_type, cfg->rx_filter);

		cfg->flags      = 0;
		cfg->tx_type    = HWTSTAMP_TX_ON;
		cfg->rx_filter  = HWTSTAMP_FILTER_ALL;

		priv->state |= timestamping;
		qfec_reg_write(priv, TS_CTL_REG,
			qfec_reg_read(priv, TS_CTL_REG) | TS_CTL_TSENALL);

		return 0;
	}

	return generic_mii_ioctl(&priv->mii, if_mii(ifr), cmd, NULL);
}

static struct net_device_stats *qfec_get_stats(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG2, "qfec_stats:\n");

	priv->stats.multicast = qfec_reg_read(priv, NUM_MULTCST_FRM_RCVD_G);

	return &priv->stats;
}

/*
 * accept new mac address
 */
static int qfec_set_mac_address(struct net_device *dev, void *p)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct sockaddr    *addr = p;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	qfec_set_adr_regs(priv, dev->dev_addr);

	return 0;
}

/*
 *  read discontinuous MAC address from corrected fuse memory region
 */

static int qfec_get_mac_address(char *buf, char *mac_base, int nBytes)
{
	static int  offset[] = { 0, 1, 2, 3, 4, 8 };
	int         n;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	for (n = 0; n < nBytes; n++)
		buf[n] = ioread8(mac_base + offset[n]);

	/* check that MAC programmed  */
	if ((buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5]) == 0)  {
		QFEC_LOG_ERR("%s: null MAC address\n", __func__);
		return -ENODATA;
	}

	return 0;
}

/*
 * static definition of driver functions
 */
static const struct net_device_ops qfec_netdev_ops = {
	.ndo_open               = qfec_open,
	.ndo_stop               = qfec_stop,
	.ndo_start_xmit         = qfec_xmit,

	.ndo_do_ioctl           = qfec_do_ioctl,
	.ndo_tx_timeout         = qfec_tx_timeout,
	.ndo_set_mac_address    = qfec_set_mac_address,
	.ndo_set_rx_mode        = qfec_set_rx_mode,

	.ndo_change_mtu         = eth_change_mtu,
	.ndo_validate_addr      = eth_validate_addr,

	.ndo_get_stats          = qfec_get_stats,
	.ndo_set_config         = qfec_set_config,
};

/*
 * ethtool functions
 */

static int qfec_nway_reset(struct net_device *dev)
{
	struct qfec_priv  *priv = netdev_priv(dev);
	return mii_nway_restart(&priv->mii);
}

/*
 * speed, duplex, auto-neg settings
 */
static void qfec_ethtool_getpauseparam(struct net_device *dev,
			struct ethtool_pauseparam *pp)
{
	struct qfec_priv  *priv = netdev_priv(dev);
	u32                flow = qfec_reg_read(priv, FLOW_CONTROL_REG);
	u32                advert;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	/* report current settings */
	pp->tx_pause = (flow & FLOW_CONTROL_TFE) != 0;
	pp->rx_pause = (flow & FLOW_CONTROL_RFE) != 0;

	/* report if pause is being advertised */
	advert = qfec_mdio_read(dev, priv->phy_id, MII_ADVERTISE);
	pp->autoneg =
		(advert & (ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM)) != 0;
}

static int qfec_ethtool_setpauseparam(struct net_device *dev,
			struct ethtool_pauseparam *pp)
{
	struct qfec_priv  *priv = netdev_priv(dev);
	u32                advert;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %d aneg, %d rx, %d tx\n", __func__,
		pp->autoneg, pp->rx_pause, pp->tx_pause);

	advert  =  qfec_mdio_read(dev, priv->phy_id, MII_ADVERTISE);
	advert &= ~(ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

	/* If pause autonegotiation is enabled, but both rx and tx are not
	 * because neither was specified in the ethtool cmd,
	 * enable both symetrical and asymetrical pause.
	 * otherwise, only enable the pause mode indicated by rx/tx.
	 */
	if (pp->autoneg)  {
		if (pp->rx_pause)
			advert |= ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP;
		else if (pp->tx_pause)
			advert |= ADVERTISE_PAUSE_ASYM;
		else
			advert |= ADVERTISE_PAUSE_CAP;
	}

	qfec_mdio_write(dev, priv->phy_id, MII_ADVERTISE, advert);

	return 0;
}

/*
 * ethtool ring parameter (-g/G) support
 */

/*
 * setringparamam - change the tx/rx ring lengths
 */
#define MIN_RING_SIZE	3
#define MAX_RING_SIZE	1000
static int qfec_ethtool_setringparam(struct net_device *dev,
	struct ethtool_ringparam *ring)
{
	struct qfec_priv  *priv    = netdev_priv(dev);
	u32                timeout = 20;

	/* notify stack the link is down */
	netif_carrier_off(dev);

	/* allow tx to complete & free skbufs on the tx ring */
	do {
		usleep_range(10000, 100000);
		qfec_tx_replenish(dev);

		if (timeout-- == 0)  {
			QFEC_LOG_ERR("%s: timeout\n", __func__);
			return -ETIME;
		}
	} while (!qfec_ring_empty(&priv->ring_tbd));


	qfec_stop(dev);

	/* set tx ring size */
	if (ring->tx_pending < MIN_RING_SIZE)
		ring->tx_pending = MIN_RING_SIZE;
	else if (ring->tx_pending > MAX_RING_SIZE)
		ring->tx_pending = MAX_RING_SIZE;
	priv->n_tbd = ring->tx_pending;

	/* set rx ring size */
	if (ring->rx_pending < MIN_RING_SIZE)
		ring->rx_pending = MIN_RING_SIZE;
	else if (ring->rx_pending > MAX_RING_SIZE)
		ring->rx_pending = MAX_RING_SIZE;
	priv->n_rbd = ring->rx_pending;


	qfec_open(dev);

	return 0;
}

/*
 * getringparamam - returns local values
 */
static void qfec_ethtool_getringparam(struct net_device *dev,
	struct ethtool_ringparam *ring)
{
	struct qfec_priv  *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	ring->rx_max_pending       = MAX_RING_SIZE;
	ring->rx_mini_max_pending  = 0;
	ring->rx_jumbo_max_pending = 0;
	ring->tx_max_pending       = MAX_RING_SIZE;

	ring->rx_pending           = priv->n_rbd;
	ring->rx_mini_pending      = 0;
	ring->rx_jumbo_pending     = 0;
	ring->tx_pending           = priv->n_tbd;
}

/*
 * speed, duplex, auto-neg settings
 */
static int
qfec_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct qfec_priv  *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	cmd->maxrxpkt = priv->n_rbd;
	cmd->maxtxpkt = priv->n_tbd;

	return mii_ethtool_gset(&priv->mii, cmd);
}

static int
qfec_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct qfec_priv  *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	return mii_ethtool_sset(&priv->mii, cmd);
}

/*
 * msg/debug level
 */
static u32 qfec_ethtool_getmsglevel(struct net_device *dev)
{
	return qfec_debug;
}

static void qfec_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	qfec_debug ^= level;	/* toggle on/off */
}

/*
 * register dump
 */
#define DMA_DMP_OFFSET  0x0000
#define DMA_REG_OFFSET  0x1000
#define DMA_REG_LEN     23

#define MAC_DMP_OFFSET  0x0080
#define MAC_REG_OFFSET  0x0000
#define MAC_REG_LEN     55

#define TS_DMP_OFFSET   0x0180
#define TS_REG_OFFSET   0x0700
#define TS_REG_LEN      15

#define MDIO_DMP_OFFSET 0x0200
#define MDIO_REG_LEN    16

#define REG_SIZE    (MDIO_DMP_OFFSET + (MDIO_REG_LEN * sizeof(short)))

static int qfec_ethtool_getregs_len(struct net_device *dev)
{
	return REG_SIZE;
}

static void
qfec_ethtool_getregs(struct net_device *dev, struct ethtool_regs *regs,
			 void *buf)
{
	struct qfec_priv  *priv   = netdev_priv(dev);
	u32               *data   = buf;
	u16               *data16;
	unsigned int       i;
	unsigned int       j;
	unsigned int       n;

	memset(buf, 0, REG_SIZE);

	j = DMA_DMP_OFFSET / sizeof(u32);
	for (i = DMA_REG_OFFSET, n = DMA_REG_LEN; n--; i += sizeof(u32))
		data[j++] = htonl(qfec_reg_read(priv, i));

	j = MAC_DMP_OFFSET / sizeof(u32);
	for (i = MAC_REG_OFFSET, n = MAC_REG_LEN; n--; i += sizeof(u32))
		data[j++] = htonl(qfec_reg_read(priv, i));

	j = TS_DMP_OFFSET / sizeof(u32);
	for (i = TS_REG_OFFSET, n = TS_REG_LEN; n--; i += sizeof(u32))
		data[j++] = htonl(qfec_reg_read(priv, i));

	data16 = (u16 *)&data[MDIO_DMP_OFFSET / sizeof(u32)];
	for (i = 0, n = 0; i < MDIO_REG_LEN; i++)
		data16[n++] = htons(qfec_mdio_read(dev, 0, i));

	regs->len     = REG_SIZE;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %d bytes\n", __func__, regs->len);
}

/*
 * statistics
 *   return counts of various ethernet activity.
 *   many of these are same as in struct net_device_stats
 *
 *   missed-frames indicates the number of attempts made by the ethernet
 *      controller to write to a buffer-descriptor when the BD ownership
 *      bit was not set.   The rxfifooverflow counter (0x1D4) is not
 *      available.  The Missed Frame and Buffer Overflow Counter register
 *      (0x1020) is used, but has only 16-bits and is reset when read.
 *      It is read and updates the value in priv->stats.rx_missed_errors
 *      in qfec_rx_int().
 */
static char qfec_stats_strings[][ETH_GSTRING_LEN] = {
	"TX good/bad Bytes         ",
	"TX Bytes                  ",
	"TX good/bad Frames        ",
	"TX Bcast Frames           ",
	"TX Mcast Frames           ",
	"TX Unicast Frames         ",
	"TX Pause Frames           ",
	"TX Vlan Frames            ",
	"TX Frames 64              ",
	"TX Frames 65-127          ",
	"TX Frames 128-255         ",
	"TX Frames 256-511         ",
	"TX Frames 512-1023        ",
	"TX Frames 1024+           ",
	"TX Pause Frames           ",
	"TX Collisions             ",
	"TX Late Collisions        ",
	"TX Excessive Collisions   ",

	"RX good/bad Bytes         ",
	"RX Bytes                  ",
	"RX good/bad Frames        ",
	"RX Bcast Frames           ",
	"RX Mcast Frames           ",
	"RX Unicast Frames         ",
	"RX Pause Frames           ",
	"RX Vlan Frames            ",
	"RX Frames 64              ",
	"RX Frames 65-127          ",
	"RX Frames 128-255         ",
	"RX Frames 256-511         ",
	"RX Frames 512-1023        ",
	"RX Frames 1024+           ",
	"RX Pause Frames           ",
	"RX Crc error Frames       ",
	"RX Length error Frames    ",
	"RX Alignment error Frames ",
	"RX Runt Frames            ",
	"RX Oversize Frames        ",
	"RX Missed Frames          ",

};

static u32 qfec_stats_regs[] =  {

	     69,     89,     70,     71,     72,     90,     92,     93,
	     73,     74,     75,     76,     77,     78,     92,     84,
	     86,     87,

	     97,     98,     96,     99,    100,    113,    116,    118,
	    107,    108,    109,    110,    111,    112,    116,    101,
	    114,    102,    103,    106
};

static int qfec_stats_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct qfec_priv  *priv = netdev_priv(to_net_dev(dev));
	int                count = PAGE_SIZE;
	int                l     = 0;
	int                n;

	QFEC_LOG(QFEC_LOG_DBG2, "%s:\n", __func__);

	for (n = 0; n < ARRAY_SIZE(qfec_stats_regs); n++)  {
		l += snprintf(&buf[l], count - l, "      %12u  %s\n",
			qfec_reg_read(priv,
				qfec_stats_regs[n] * sizeof(uint32_t)),
			qfec_stats_strings[n]);
	}

	return l;
}

static int qfec_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(qfec_stats_regs) + 1;	/* missed frames */

	default:
		return -EOPNOTSUPP;
	}
}

static void qfec_ethtool_getstrings(struct net_device *dev, u32 stringset,
		u8 *buf)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s: %d bytes\n", __func__,
		sizeof(qfec_stats_strings));

	memcpy(buf, qfec_stats_strings, sizeof(qfec_stats_strings));
}

static void qfec_ethtool_getstats(struct net_device *dev,
		struct ethtool_stats *stats, uint64_t *data)
{
	struct qfec_priv        *priv = netdev_priv(dev);
	int                      j = 0;
	int                      n;

	for (n = 0; n < ARRAY_SIZE(qfec_stats_regs); n++)
		data[j++] = qfec_reg_read(priv,
				qfec_stats_regs[n] * sizeof(uint32_t));

	data[j++] = priv->stats.rx_missed_errors;

	stats->n_stats = j;
}

static void qfec_ethtool_getdrvinfo(struct net_device *dev,
					struct ethtool_drvinfo *info)
{
	strlcpy(info->driver,  QFEC_NAME,    sizeof(info->driver));
	strlcpy(info->version, QFEC_DRV_VER, sizeof(info->version));
	strlcpy(info->bus_info, dev_name(dev->dev.parent),
		sizeof(info->bus_info));

	info->eedump_len  = 0;
	info->regdump_len = qfec_ethtool_getregs_len(dev);
}

/*
 * ethtool ops table
 */
static const struct ethtool_ops qfec_ethtool_ops = {
	.nway_reset         = qfec_nway_reset,

	.get_settings       = qfec_ethtool_getsettings,
	.set_settings       = qfec_ethtool_setsettings,
	.get_link           = ethtool_op_get_link,
	.get_drvinfo        = qfec_ethtool_getdrvinfo,
	.get_msglevel       = qfec_ethtool_getmsglevel,
	.set_msglevel       = qfec_ethtool_setmsglevel,
	.get_regs_len       = qfec_ethtool_getregs_len,
	.get_regs           = qfec_ethtool_getregs,

	.get_ringparam      = qfec_ethtool_getringparam,
	.set_ringparam      = qfec_ethtool_setringparam,

	.get_pauseparam     = qfec_ethtool_getpauseparam,
	.set_pauseparam     = qfec_ethtool_setpauseparam,

	.get_sset_count     = qfec_get_sset_count,
	.get_strings        = qfec_ethtool_getstrings,
	.get_ethtool_stats  = qfec_ethtool_getstats,
};

/*
 *  create sysfs entries
 */
static DEVICE_ATTR(bd_tx,   0444, qfec_bd_tx_show,   NULL);
static DEVICE_ATTR(bd_rx,   0444, qfec_bd_rx_show,   NULL);
static DEVICE_ATTR(cfg,     0444, qfec_config_show,  NULL);
static DEVICE_ATTR(clk_reg, 0444, qfec_clk_reg_show, NULL);
static DEVICE_ATTR(cmd,     0222, NULL,              qfec_cmd);
static DEVICE_ATTR(cntrs,   0444, qfec_cntrs_show,   NULL);
static DEVICE_ATTR(reg,     0444, qfec_reg_show,     NULL);
static DEVICE_ATTR(mdio,    0444, qfec_mdio_show,    NULL);
static DEVICE_ATTR(stats,   0444, qfec_stats_show,   NULL);
static DEVICE_ATTR(tstamp,  0444, qfec_tstamp_show,  NULL);

static void qfec_sysfs_create(struct net_device *dev)
{
	if (device_create_file(&(dev->dev), &dev_attr_bd_tx) ||
		device_create_file(&(dev->dev), &dev_attr_bd_rx) ||
		device_create_file(&(dev->dev), &dev_attr_cfg) ||
		device_create_file(&(dev->dev), &dev_attr_clk_reg) ||
		device_create_file(&(dev->dev), &dev_attr_cmd) ||
		device_create_file(&(dev->dev), &dev_attr_cntrs) ||
		device_create_file(&(dev->dev), &dev_attr_mdio) ||
		device_create_file(&(dev->dev), &dev_attr_reg) ||
		device_create_file(&(dev->dev), &dev_attr_stats) ||
		device_create_file(&(dev->dev), &dev_attr_tstamp))
		pr_err("qfec_sysfs_create failed to create sysfs files\n");
}

/*
 * map a specified resource
 */
static int qfec_map_resource(struct platform_device *plat, int resource,
	struct resource **priv_res,
	void                   **addr)
{
	struct resource         *res;

	QFEC_LOG(QFEC_LOG_DBG, "%s: 0x%x resource\n", __func__, resource);

	/* allocate region to access controller registers */
	*priv_res = res = platform_get_resource(plat, resource, 0);
	if (!res) {
		QFEC_LOG_ERR("%s: platform_get_resource failed\n", __func__);
		return -ENODEV;
	}

	res = request_mem_region(res->start, res->end - res->start, QFEC_NAME);
	if (!res) {
		QFEC_LOG_ERR("%s: request_mem_region failed, %08x %08x\n",
			__func__, res->start, res->end - res->start);
		return -EBUSY;
	}

	*addr = ioremap(res->start, res->end - res->start);
	if (!*addr)
		return -ENOMEM;

	QFEC_LOG(QFEC_LOG_DBG, " %s: io mapped from %p to %p\n",
		__func__, (void *)res->start, *addr);

	return 0;
};

/*
 * free allocated io regions
 */
static void qfec_free_res(struct resource *res, void *base)
{

	if (res)  {
		if (base)
			iounmap((void __iomem *)base);

		release_mem_region(res->start, res->end - res->start);
	}
};

/*
 * probe function that obtain configuration info and allocate net_device
 */
static int __devinit qfec_probe(struct platform_device *plat)
{
	struct net_device  *dev;
	struct qfec_priv   *priv;
	int                 ret = 0;

	/* allocate device */
	dev = alloc_etherdev(sizeof(struct qfec_priv));
	if (!dev) {
		QFEC_LOG_ERR("%s: alloc_etherdev failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	QFEC_LOG(QFEC_LOG_DBG, "%s: %08x dev\n",      __func__, (int)dev);

	qfec_dev = dev;
	SET_NETDEV_DEV(dev, &plat->dev);

	dev->netdev_ops      = &qfec_netdev_ops;
	dev->ethtool_ops     = &qfec_ethtool_ops;
	dev->watchdog_timeo  = 2 * HZ;
	dev->irq             = platform_get_irq(plat, 0);

	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	/* initialize private data */
	priv = (struct qfec_priv *)netdev_priv(dev);
	memset((void *)priv, 0, sizeof(priv));

	priv->net_dev   = dev;
	platform_set_drvdata(plat, dev);

	priv->n_tbd     = TX_BD_NUM;
	priv->n_rbd     = RX_BD_NUM;

	/* initialize phy structure */
	priv->mii.phy_id_mask   = 0x1F;
	priv->mii.reg_num_mask  = 0x1F;
	priv->mii.dev           = dev;
	priv->mii.mdio_read     = qfec_mdio_read;
	priv->mii.mdio_write    = qfec_mdio_write;

	/* map register regions */
	ret = qfec_map_resource(
		plat, IORESOURCE_MEM, &priv->mac_res, &priv->mac_base);
	if (ret)  {
		QFEC_LOG_ERR("%s: IORESOURCE_MEM mac failed\n", __func__);
		goto err1;
	}

	ret = qfec_map_resource(
		plat, IORESOURCE_IO, &priv->clk_res, &priv->clk_base);
	if (ret)  {
		QFEC_LOG_ERR("%s: IORESOURCE_IO clk failed\n", __func__);
		goto err2;
	}

	ret = qfec_map_resource(
		plat, IORESOURCE_DMA, &priv->fuse_res, &priv->fuse_base);
	if (ret)  {
		QFEC_LOG_ERR("%s: IORESOURCE_DMA fuse failed\n", __func__);
		goto err3;
	}

	/* initialize MAC addr */
	ret = qfec_get_mac_address(dev->dev_addr, priv->fuse_base,
		MAC_ADDR_SIZE);
	if (ret)
		goto err4;

	QFEC_LOG(QFEC_LOG_DBG, "%s: mac  %02x:%02x:%02x:%02x:%02x:%02x\n",
		__func__,
		dev->dev_addr[0], dev->dev_addr[1],
		dev->dev_addr[2], dev->dev_addr[3],
		dev->dev_addr[4], dev->dev_addr[5]);

	ret = register_netdev(dev);
	if (ret)  {
		QFEC_LOG_ERR("%s: register_netdev failed\n", __func__);
		goto err4;
	}

	spin_lock_init(&priv->mdio_lock);
	spin_lock_init(&priv->xmit_lock);
	qfec_sysfs_create(dev);

	return 0;

	/* error handling */
err4:
	qfec_free_res(priv->fuse_res, priv->fuse_base);
err3:
	qfec_free_res(priv->clk_res, priv->clk_base);
err2:
	qfec_free_res(priv->mac_res, priv->mac_base);
err1:
	free_netdev(dev);
err:
	QFEC_LOG_ERR("%s: err\n", __func__);
	return ret;
}

/*
 * module remove
 */
static int __devexit qfec_remove(struct platform_device *plat)
{
	struct net_device  *dev  = platform_get_drvdata(plat);
	struct qfec_priv   *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	platform_set_drvdata(plat, NULL);

	qfec_free_res(priv->fuse_res, priv->fuse_base);
	qfec_free_res(priv->clk_res, priv->clk_base);
	qfec_free_res(priv->mac_res, priv->mac_base);

	unregister_netdev(dev);
	free_netdev(dev);

	return 0;
}

/*
 * module support
 *     the FSM9xxx is not a mobile device does not support power management
 */

static struct platform_driver qfec_driver = {
	.probe  = qfec_probe,
	.remove = __devexit_p(qfec_remove),
	.driver = {
		.name   = QFEC_NAME,
		.owner  = THIS_MODULE,
	},
};

/*
 * module init
 */
static int __init qfec_init_module(void)
{
	int  res;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %s\n", __func__, qfec_driver.driver.name);

	res = platform_driver_register(&qfec_driver);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %d - platform_driver_register\n",
		__func__, res);

	return  res;
}

/*
 * module exit
 */
static void __exit qfec_exit_module(void)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	platform_driver_unregister(&qfec_driver);
}

MODULE_DESCRIPTION("FSM Network Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rohit Vaswani <rvaswani@codeaurora.org>");
MODULE_VERSION("1.0");

module_init(qfec_init_module);
module_exit(qfec_exit_module);
