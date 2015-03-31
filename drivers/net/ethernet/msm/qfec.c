/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include <asm/div64.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/of_device.h>
#include <linux/of_net.h>
#include <linux/clk.h>

#include "qfec.h"

#define QFEC_NAME       "qfec"
#define QFEC_DRV_VER    "Apr 09 2013"

#define ETH_BUF_SIZE    0x600
#define MAX_N_BD        50
#define MAC_ADDR_SIZE	6

/* Delay that produced best results while testing for IPSec ingress
 * across packet sizes
 */
#define RX_POLL_INT_NS	(150*1000)
#define PKTS_PER_POLL	1

#define RX_TX_BD_RATIO  8
#define TX_BD_NUM       256
#define RX_BD_NUM       256
#define TX_BD_TI_RATIO  4
#define MAX_MDIO_REG    32

#define H_DPLX     0
#define F_DPLX     1

#define DEFAULT_IFG 12
#define GMAC_CTL_IFG_DEFAULT ((DEFAULT_IFG << GMAC_IFG_LIMIT_SHFT) | \
			       DEFAULT_IFG)

/* logging macros */
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

/* driver buffer-descriptor
 *   contains the 4 word HW descriptor plus an additional 4-words.
 *   (See the DSL bits in the BUS-Mode register).
 */
#define BD_FLAG_LAST_BD     1
#define BD_FLAG_ENHDES      2
#define BD_FLAG_ATDS        4

struct buf_desc {
	void           *p_desc;
	struct sk_buff *skb;
	void           *buf_virt_addr;
	void           *buf_phys_addr;
	uint32_t        bd_flag;
};

/* inline functions accessing non-struct qfec_buf_desc elements */

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

/* bd_flag */
static inline uint32_t qfec_bd_last_bd(struct buf_desc *p_bd)
{
	return p_bd->bd_flag & BD_FLAG_LAST_BD;
};

static inline uint32_t qfec_bd_enhdes(struct buf_desc *p_bd)
{
	return p_bd->bd_flag & BD_FLAG_ENHDES;
};

static inline void qfec_bd_flag_set(struct buf_desc *p_bd, uint32_t flag)
{
	p_bd->bd_flag = flag;
};

/* inline functions accessing struct qfec_buf_desc elements */
static inline struct qfec_buf_desc *qfec_bd_buf_desc(struct buf_desc *p_bd)
{
	return (struct qfec_buf_desc *)(p_bd->p_desc);
};

static inline struct qfec_enh_buf_desc *qfec_bd_enh_buf_desc(
	struct buf_desc *p_bd)
{
	return (struct qfec_enh_buf_desc *)(p_bd->p_desc);
};

/* ownership bit */
static inline uint32_t qfec_bd_own(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);

	return p_desc->status & BUF_OWN;
};

static inline void qfec_bd_own_set(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->status |= BUF_OWN;
};

static inline void qfec_bd_own_clr(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->status &= ~(BUF_OWN);
};

static inline uint32_t qfec_bd_status_get(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	return p_desc->status;
};

static inline void qfec_bd_status_set(struct buf_desc *p_bd, uint32_t status)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->status |= status;
};

static inline void qfec_bd_status_wr(struct buf_desc *p_bd, uint32_t status)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->status = status;
};

static inline uint32_t qfec_bd_status_len(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	return BUF_RX_FL_GET((*p_desc));
};

static inline uint32_t qfec_tbd_status_tstamp(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);

	return (qfec_bd_enhdes(p_bd)) ? (p_desc->status & ENH_BUF_TX_TTSE) :
		(p_desc->status & BUF_TX_TTSE);
}

/* control register */
static inline void qfec_bd_ctl_reset(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->ctl  = 0;
};

static inline uint32_t qfec_bd_ctl_get(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	return p_desc->ctl;
};

static inline void qfec_bd_ctl_set(struct buf_desc *p_bd, uint32_t val)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->ctl |= val;
};

static inline void qfec_bd_ctl_wr(struct buf_desc *p_bd, uint32_t val)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->ctl = val;
};

/* pbuf register  */
static inline void *qfec_bd_pbuf_get(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	return p_desc->p_buf;
}

static inline void qfec_bd_pbuf_set(struct buf_desc *p_bd, void *p)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	p_desc->p_buf = p;
}

/* next register */
static inline void *qfec_bd_next_get(struct buf_desc *p_bd)
{
	struct qfec_buf_desc *p_desc = qfec_bd_buf_desc(p_bd);
	return p_desc->next;
};

/* initialize an RX BD w/ a new buf */
static int qfec_rbd_init(struct net_device *dev, struct buf_desc *p_bd)
{
	struct sk_buff     *skb;
	void               *p;
	void               *v;
	uint32_t            ctl = ETH_BUF_SIZE;

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
	if (qfec_bd_last_bd(p_bd))
		ctl |= (qfec_bd_enhdes(p_bd) ? ENH_BUF_RX_RER : BUF_RX_RER);
	qfec_bd_ctl_wr(p_bd, ctl);

	qfec_bd_status_wr(p_bd, BUF_OWN);

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

/* ring structure used to maintain indices of buffer-descriptor (BD) usage
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
	return (p_ring->n_free == 0) ? 1 : 0;
};

static inline int qfec_ring_empty(struct ring *p_ring)
{
	return (p_ring->n_free == p_ring->len) ? 1 : 0;
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

/* counters track normal and abnormal driver events and activity */
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

/* private data */
enum qfec_state {
	timestamping  = 0x04,
};

#define CLK_RATE_125M       125000000UL
#define CLK_RATE_25M        25000000UL
#define CLK_RATE_2P5M       2500000UL

enum qfec_clk {
	QFEC_CORE_CLK,
	QFEC_INTF_CLK,
	QFEC_TX_CLK,
	QFEC_RX_CLK,
	QFEC_PTP_CLK,
	NUM_QFEC_CLK
};

static const char *qfec_clk_name[NUM_QFEC_CLK] = {
	"core_clk",
	"intf_clk",
	"tx_clk",
	"rx_clk",
	"ptp_clk",
};

enum qfec_pinctrl {
	PINCTRL_MDIO_ACTIVE,
	PINCTRL_MDIO_DEACTIVE,
	NUM_PINCTRL
};

static const char *qfec_pinctrl_name[NUM_PINCTRL] = {
	"active",
	"deactive",
};

struct qfec_priv;

struct qfec_ops {
	int (*probe)(struct platform_device *plat);
	void (*remove)(struct platform_device *plat);
	int (*clk_enable)(struct qfec_priv *priv, uint8_t clk);
	int (*clk_disable)(struct qfec_priv *priv, uint8_t clk);
	int (*clk_set_rate)(struct qfec_priv *priv, uint8_t clk,
			    unsigned long rate);
	int (*init)(struct qfec_priv *priv);
	int (*reset)(struct qfec_priv *priv);
	int (*link_cfg)(struct qfec_priv *priv, unsigned int spd,
			unsigned int dplx);
};

struct qfec_priv {
	struct net_device      *net_dev;
	struct net_device_stats stats;            /* req statistics */
	struct hrtimer          rx_timer;

	struct device           dev;
	int                     idx;              /* controller index */

	const struct qfec_ops   *ops;             /* ops */
	void                   *pdata;            /* wrapper private data */

	struct clk             *clk[NUM_QFEC_CLK];

	struct pinctrl         *pinctrl;

	spinlock_t              xmit_lock;
	spinlock_t              mdio_lock;
	spinlock_t              rx_lock;

	unsigned int            hw_feature;
	unsigned int            state;            /* driver state */

	unsigned int            bd_size;          /* buf-desc alloc size */
	struct qfec_buf_desc   *bd_base;          /* * qfec-buf-desc */
	dma_addr_t              tbd_dma;          /* dma/phy-addr buf-desc */
	dma_addr_t              rbd_dma;          /* dma/phy-addr buf-desc */

	struct resource        *mac_res;
	void                   *mac_base;         /* mac (virt) base address */

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

	int                     rx_int_status;
	int                     intf;
};

static inline int qfec_ops_clk_enable(struct qfec_priv *priv, uint8_t clk)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->clk_enable) ? ops->clk_enable(priv, clk) : 0;
}

static inline int qfec_ops_clk_disable(struct qfec_priv *priv, uint8_t clk)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->clk_disable) ? ops->clk_disable(priv, clk) : 0;
}

static inline int qfec_ops_clk_set_rate(struct qfec_priv *priv, uint8_t clk,
					unsigned long rate)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->clk_set_rate) ? ops->clk_set_rate(priv, clk, rate) : 0;
}

static inline int qfec_ops_init(struct qfec_priv *priv)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->init) ? ops->init(priv) : 0;
}

static inline int qfec_ops_reset(struct qfec_priv *priv)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->reset) ? ops->reset(priv) : 0;
}

static inline int qfec_ops_link_cfg(struct qfec_priv *priv, unsigned int spd,
			unsigned int dplx)
{
	const struct qfec_ops *ops = priv->ops;
	return (ops->link_cfg) ? ops->link_cfg(priv, spd, dplx) : 0;
}

static inline bool qfec_hw_feature_enhanced_des(struct qfec_priv *priv)
{
	return (priv->hw_feature & HW_FEATURE_ENHDESSEL) ? true : false;
}

static inline bool qfec_hw_feature_advanced_tstamp(struct qfec_priv *priv)
{
	return (priv->hw_feature & HW_FEATURE_TSVER2SEL) ? true : false;
}

/* cntrs display */
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

/* functions that manage state */
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

/* functions to access and initialize the MAC registers */
static inline uint32_t qfec_reg_read(void *base, uint32_t reg)
{
	return ioread32((void *) (base + reg));
}

static void qfec_reg_write(void *base, uint32_t reg, uint32_t val)
{
	uint32_t    addr = (uint32_t)base + reg;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %08x <- %08x\n", __func__, addr, val);
	iowrite32(val, (void *)addr);
}

static inline void qfec_reg_update(void *base, uint32_t reg,
				   uint32_t mask, uint32_t val)
{
	uint32_t v = qfec_reg_read(base, reg);
	qfec_reg_write(base, reg, ((v & ~mask) | val));
}

/* speed/duplex/pause  settings */
static int qfec_config_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv *priv = netdev_priv(to_net_dev(dev));
	int               cfg  = qfec_reg_read(priv->mac_base, MAC_CONFIG_REG);
	int               flow = qfec_reg_read(priv->mac_base,
					       FLOW_CONTROL_REG);
	int               l    = 0;
	int               count = PAGE_SIZE;

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


/* table and functions to initialize controller registers */
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
	{ 0, MMC_IPC_INTR_MASK_RX_REG, "MMC_IPC_INTR_MASK_RX_REG", 0xFFFFFFFF },

	{ 1, TS_HIGH_REG,            "TS_HIGH_REG",            0 },
	{ 1, TS_LOW_REG,             "TS_LOW_REG",             0 },

	{ 1, TS_HI_UPDT_REG,         "TS_HI_UPDATE_REG",       0 },
	{ 1, TS_LO_UPDT_REG,         "TS_LO_UPDATE_REG",       0 },
	{ 0, TS_SUB_SEC_INCR_REG,    "TS_SUB_SEC_INCR_REG",    40 },
	{ 0, TS_CTL_REG,             "TS_CTL_REG",        TS_CTL_TSENALL
							| TS_CTL_TSCTRLSSR
							| TS_CTL_TSINIT
							| TS_CTL_TSENA },
};

static void qfec_reg_init(struct qfec_priv *priv)
{
	struct reg_entry *p = qfec_reg_tbl;
	int           n = ARRAY_SIZE(qfec_reg_tbl);
	unsigned long flags;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	for  (; n--; p++) {
		if (!p->rdonly) {
			if (p->addr == INTRP_EN_REG) {
				spin_lock_irqsave(&priv->rx_lock, flags);
				if (QFEC_INTRP_SETUP & INTRP_EN_REG_RIE)
					priv->rx_int_status = 1;
				else
					priv->rx_int_status = 0;
				qfec_reg_write(priv->mac_base, p->addr, p->val);
				spin_unlock_irqrestore(&priv->rx_lock, flags);
			} else
				qfec_reg_write(priv->mac_base, p->addr, p->val);
		}
	}
}

/* display registers thru sysfs */
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

/* set the MAC-0 address */
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

	qfec_reg_write(priv->mac_base, MAC_ADR_0_HIGH_REG, h);
	qfec_reg_write(priv->mac_base, MAC_ADR_0_LOW_REG,  l);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %08x %08x\n", __func__, h, l);
}

/* set up the RX filter */
static void qfec_set_rx_mode(struct net_device *dev)
{
	struct qfec_priv *priv = netdev_priv(dev);
	uint32_t filter_conf;
	int index;

	/* Clear address filter entries */
	for (index = 1; index < MAC_ADR_MAX; ++index) {
		qfec_reg_write(priv->mac_base, MAC_ADR_HIGH_REG_N(index), 0);
		qfec_reg_write(priv->mac_base, MAC_ADR_LOW_REG_N(index), 0);
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

			qfec_reg_write(priv->mac_base,
				       MAC_ADR_HIGH_REG_N(index), high);
			qfec_reg_write(priv->mac_base,
				       MAC_ADR_LOW_REG_N(index), low);

			index++;
		}
	}

	qfec_reg_write(priv->mac_base, MAC_FR_FILTER_REG, filter_conf);
}

/* reset the controller */
#define QFEC_RESET_TIMEOUT   10000
	/* reset should always clear but did not w/o test/delay
	 * in RgMii mode.  there is no spec'd max timeout
	 */

static int qfec_hw_reset(struct qfec_priv *priv)
{
	int             timeout = QFEC_RESET_TIMEOUT;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_write(priv->mac_base, BUS_MODE_REG, BUS_MODE_SWR);

	while (qfec_reg_read(priv->mac_base, BUS_MODE_REG) & BUS_MODE_SWR) {
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

/* initialize controller */
static int qfec_hw_init(struct qfec_priv *priv)
{
	int  res = 0;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	res = qfec_hw_reset(priv);
	if (res)
		return res;

	qfec_reg_init(priv);

	/* turn on the ATDS bit if use 8 word alternate descriptor */
	if (qfec_hw_feature_enhanced_des(priv) &&
	    qfec_hw_feature_advanced_tstamp(priv))
		qfec_reg_update(priv->mac_base, BUS_MODE_REG,
				BUS_MODE_ATDS, BUS_MODE_ATDS);
	else
		qfec_reg_update(priv->mac_base, BUS_MODE_REG,
				BUS_MODE_ATDS, 0);

	/* config buf-desc locations */
	qfec_reg_write(priv->mac_base, TX_DES_LST_ADR_REG, priv->tbd_dma);
	qfec_reg_write(priv->mac_base, RX_DES_LST_ADR_REG, priv->rbd_dma);

	/* clear interrupts */
	qfec_reg_write(priv->mac_base, STATUS_REG,
		       INTRP_EN_REG_NIE | INTRP_EN_REG_RIE |
		       INTRP_EN_REG_TIE | INTRP_EN_REG_TUE | INTRP_EN_REG_ETE);

	if (priv->mii.supports_gmii) {
		/* Clear RGMII */
		qfec_reg_read(priv->mac_base, SG_RG_SMII_STATUS_REG);
		/* Disable RGMII int */
		qfec_reg_write(priv->mac_base, INTRP_MASK_REG, 1);
	}

	return res;
}

/* en/disable controller */
static void qfec_hw_enable(struct qfec_priv *priv)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_update(priv->mac_base, OPER_MODE_REG,
			(OPER_MODE_REG_ST | OPER_MODE_REG_SR),
			(OPER_MODE_REG_ST | OPER_MODE_REG_SR));
}

static void qfec_hw_disable(struct qfec_priv *priv)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	qfec_reg_update(priv->mac_base, OPER_MODE_REG,
			(OPER_MODE_REG_ST | OPER_MODE_REG_SR), 0);
}

/* en/disable Rx interrupt */
static void qfec_rx_int_ctrl(struct qfec_priv *priv, bool enable)
{
	qfec_reg_update(priv->mac_base, INTRP_EN_REG, INTRP_EN_REG_RIE,
			(enable) ? INTRP_EN_REG_RIE : 0);
}

/* configure the PHY interface and clock routing and signal bits */
enum phy_intfc  {
	INTFC_MII     = 0,
	INTFC_RGMII   = 1,
	INTFC_SGMII   = 2,
	INTFC_TBI     = 3,
	INTFC_RMII    = 4,
	INTFC_RTBI    = 5,
	INTFC_SMII    = 6,
	INTFC_REVMII  = 7,
	INTFC_LAST    = INTFC_REVMII,
	NUM_INTFC
};

static const char *phy_intfc_name[NUM_INTFC] = {
	"MII/GMII",
	"RGMII",
	"SGMII",
	"TBI",
	"RMII",
	"RTBI",
	"SMII",
	"REVMII",
};

static inline int phy_mode_to_intfc(int phy_mode)
{
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		return INTFC_MII;
	case PHY_INTERFACE_MODE_RGMII:
		return INTFC_RGMII;
	case PHY_INTERFACE_MODE_RMII:
		return INTFC_RMII;
	case PHY_INTERFACE_MODE_RTBI:
		return INTFC_RTBI;
	case PHY_INTERFACE_MODE_SGMII:
		return INTFC_SGMII;
	case PHY_INTERFACE_MODE_SMII:
		return INTFC_SMII;
	case PHY_INTERFACE_MODE_TBI:
		return INTFC_TBI;
	default:
		return -EINVAL;
	}
}

/* speed selection */
enum speed  {
	SPD_10   = 0,
	SPD_100  = 1,
	SPD_1000 = 2,
};

/* configure the PHY interface and clock routing and signal bits */
static int qfec_speed_cfg(struct net_device *dev, unsigned int spd,
	unsigned int dplx)
{
	struct qfec_priv *priv = netdev_priv(dev);
	int               res = 0;
	uint32_t          val = (dplx) ? MAC_CONFIG_REG_DM : H_DPLX;
	unsigned long     clk_rate;

	QFEC_LOG(QFEC_LOG_DBG2, "%s: %d spd, %d dplx\n", __func__, spd, dplx);

	switch (spd) {
	case SPD_10:
		val |= MAC_CONFIG_REG_SPD_10;
		clk_rate = CLK_RATE_2P5M;
		break;
	case SPD_100:
		val |= MAC_CONFIG_REG_SPD_100;
		clk_rate = CLK_RATE_25M;
		break;
	case SPD_1000:
		val |= MAC_CONFIG_REG_SPD_1G;
		clk_rate = CLK_RATE_125M;
		break;
	default:
		return -EINVAL;
	}

	res = qfec_ops_clk_disable(priv, QFEC_TX_CLK);
	if (res) {
		QFEC_LOG_ERR("%s: failed to disable tx_clk\n", __func__);
		goto done;
	}
	res = qfec_ops_clk_disable(priv, QFEC_RX_CLK);
	if (res) {
		QFEC_LOG_ERR("%s: failed to disable rx_clk\n", __func__);
		goto done;
	}

	/* set the MAC speed bits */
	qfec_reg_update(priv->mac_base, MAC_CONFIG_REG,
			(MAC_CONFIG_REG_SPD | MAC_CONFIG_REG_DM), val);

	res = qfec_ops_link_cfg(priv, spd, dplx);
	if (res)
		goto done;

	res = qfec_ops_clk_set_rate(priv, QFEC_TX_CLK, clk_rate);
	if (res) {
		QFEC_LOG_ERR("%s: failed to set tx_clk rate\n", __func__);
		goto done;
	}
	res = qfec_ops_clk_set_rate(priv, QFEC_RX_CLK, clk_rate);
	if (res) {
		QFEC_LOG_ERR("%s: failed to set rx_clk rate\n", __func__);
		goto done;
	}

	res = qfec_ops_clk_enable(priv, QFEC_TX_CLK);
	if (res) {
		QFEC_LOG_ERR("%s: failed to enable tx_clk\n", __func__);
		goto done;
	}
	res = qfec_ops_clk_enable(priv, QFEC_RX_CLK);
	if (res)
		QFEC_LOG_ERR("%s: failed to enable rx_clk\n", __func__);

done:
	return res;
}

static int qfec_ptp_cfg(struct qfec_priv *priv)
{
	int ret;

	ret = qfec_ops_clk_set_rate(priv, QFEC_PTP_CLK, CLK_RATE_25M);
	if (ret)
		return ret;
	ret = qfec_ops_clk_enable(priv, QFEC_PTP_CLK);

	return ret;
}

/* MDIO operations */

/* wait reasonable amount of time for MDIO operation to complete, not busy */
static int qfec_mdio_busy(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	int                 i;

	for (i = 100; i > 0; i--)  {
		if (!(qfec_reg_read(priv->mac_base,
				    GMII_ADR_REG) & GMII_ADR_REG_GB))
			return 0;
		udelay(1);
	}

	return -ETIME;
}

/* initiate either a read or write MDIO operation */
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
	qfec_reg_write(priv->mac_base, GMII_ADR_REG,
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

/* read MDIO register */
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

	res = qfec_reg_read(priv->mac_base, GMII_DATA_REG);
	QFEC_LOG(QFEC_LOG_MDIO_R, "%s: phy %d, %2d reg, 0x%04x val\n",
		__func__, phy_id, reg, res);

done:
	spin_unlock_irqrestore(&priv->mdio_lock, flags);
	return res;
}

/* write MDIO register */
static void qfec_mdio_write(struct net_device *dev, int phy_id, int reg,
	int val)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	unsigned long       flags;

	spin_lock_irqsave(&priv->mdio_lock, flags);

	QFEC_LOG(QFEC_LOG_MDIO_W, "%s: %2d reg, %04x\n",
		__func__, reg, val);

	qfec_reg_write(priv->mac_base, GMII_DATA_REG, val);

	if (qfec_mdio_oper(dev, phy_id, reg, 1))
		QFEC_LOG_ERR("%s: oper\n", __func__);

	spin_unlock_irqrestore(&priv->mdio_lock, flags);
}

/* MDIO show */
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

/* get auto-negotiation results */
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
	flow  = qfec_reg_read(priv->mac_base, FLOW_CONTROL_REG);
	flow &= ~(FLOW_CONTROL_TFE | FLOW_CONTROL_RFE);

	if (status & ADVERTISE_PAUSE_CAP)  {
		flow |= FLOW_CONTROL_RFE | FLOW_CONTROL_TFE;
	} else if (status & ADVERTISE_PAUSE_ASYM)  {
		if (lpa & ADVERTISE_PAUSE_CAP)
			flow |= FLOW_CONTROL_TFE;
		else if (advert & ADVERTISE_PAUSE_CAP)
			flow |= FLOW_CONTROL_RFE;
	}

	qfec_reg_write(priv->mac_base, FLOW_CONTROL_REG, flow);
}

/* monitor phy status, and process auto-neg results when changed */
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

/* dealloc buffer descriptor memory */
static void qfec_mem_dealloc(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);

	dma_free_coherent(&dev->dev,
		priv->bd_size, priv->bd_base, priv->tbd_dma);
	priv->bd_base = 0;
}

/* allocate shared device memory for TX/RX buf-desc (and buffers) */
static int qfec_mem_alloc(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	int bd_size;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %p dev\n", __func__, dev);

	if (qfec_hw_feature_enhanced_des(priv) &&
	    qfec_hw_feature_advanced_tstamp(priv))
		bd_size = sizeof(struct qfec_enh_buf_desc);
	else
		bd_size = sizeof(struct qfec_buf_desc);

	priv->bd_size = (priv->n_tbd + priv->n_rbd) * bd_size;

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

	priv->rbd_dma   = priv->tbd_dma + (priv->n_tbd * bd_size);

	QFEC_LOG(QFEC_LOG_DBG,
		" %s: 0x%08x size, %d n_tbd, %d n_rbd\n",
		__func__, priv->bd_size, priv->n_tbd, priv->n_rbd);

	return 0;
}

/* display buffer descriptors */
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

/* display TX BDs */
static int qfec_bd_tx_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;

	return qfec_bd_show(buf, count, priv->p_tbd, priv->n_tbd,
				&priv->ring_tbd, "TX");
}

/* display RX BDs */
static int qfec_bd_rx_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;

	return  qfec_bd_show(buf, count, priv->p_rbd, priv->n_rbd,
				&priv->ring_rbd, "RX");
}

/* The Ethernet core includes IEEE-1588 support. This includes
 * TS_HIGH_REG and TS_LOW_REG registers driven by an external clock.
 * Each external clock cycle causes the TS_LOW_REG register to
 * increment by the value in TS_SUB_SEC_INCR_REG (e.g. set to 40 using
 * a 25 MHz clock).  Unfortunately, TS_HIGH_REG increments when
 * TS_LOW_REG overflows at 2^31 instead of 10^9.
 *
 * Conversion requires scaling (dividing) the 63-bit concatenated
 * timestamp register value by 10^9 to determine seconds, and taking
 * the remainder to determine nsec.  Since division is to be avoided,
 * a combination of multiplication and shift (>>) minimizes the number
 * of operations.
 *
 * To avoid loss of data, the timestamp value is multipled by 2<<30 /
 * 10^9, and the result scaled by 2<<30 (i.e. >> 30). The shift value
 * of 30 is determining the log-2 value of the denominator (10^9),
 * 29.9, and rounding up, 30.
 */
/* ------------------------------------------------
 * conversion factors
 */
#define TS_LOW_REG_BITS    31
#define TS_LOW_REG_MASK    (((uint64_t)1 << TS_LOW_REG_BITS) - 1)

#define MILLION            1000000UL
#define BILLION            1000000000UL

#define F_CLK              BILLION
#define F_CLK_PRE_SC       30
#define F_CLK_INV_Q        60
#define F_CLK_INV          (((uint64_t)1 << F_CLK_INV_Q) / F_CLK)

#define F_CLK_TO_NS_Q      25
#define F_CLK_TO_NS \
	(((((uint64_t)1<<F_CLK_TO_NS_Q)*BILLION)+(F_CLK/2))/F_CLK)

#define NS_TO_F_CLK_Q      30
#define NS_TO_F_CLK \
	(((((uint64_t)1<<NS_TO_F_CLK_Q)*F_CLK)+(BILLION/2))/BILLION)

/* qfec_hilo_collapse - The ptp timestamp low register is a 31 bit
 * quantity.  Its high order bit is a control bit, thus unused in time
 * representation.  This routine combines the high and low registers,
 * collapsing out said bit (ie. making a 63 bit quantity), and then
 * separates the new 63 bit value into high and low 32 bit values...
 */
static inline void qfec_hilo_collapse(
	uint32_t tsRegHi,
	uint32_t tsRegLo,
	uint32_t *tsRegHiPtr,
	uint32_t *tsRegLoPtr)
{
	uint64_t cnt;

	cnt   = tsRegHi;
	cnt <<= TS_LOW_REG_BITS;
	cnt  |= tsRegLo;

	*tsRegHiPtr = cnt >> 32;
	*tsRegLoPtr = cnt & 0xffffffff;
}

/* qfec_hilo_2secnsec - converts Etherent timestamp register values to
 * sec and nsec
 */
static inline void qfec_hilo_2secnsec(
	uint32_t  tsRegHi,
	uint32_t  tsRegLo,
	uint32_t *secPtr,
	uint32_t *nsecPtr)
{
	uint64_t cnt;
	uint64_t hi;
	uint64_t subsec = 0;

	cnt      = tsRegHi;
	cnt    <<= TS_LOW_REG_BITS;
	cnt     += tsRegLo;

	hi       = cnt >> F_CLK_PRE_SC;
	hi      *= F_CLK_INV;
	hi     >>= F_CLK_INV_Q - F_CLK_PRE_SC;

	*secPtr  = hi;
	subsec   = cnt - (hi * F_CLK);

	while (subsec > F_CLK) {
		subsec  -= F_CLK;
		*secPtr += 1;
	}

	*nsecPtr = subsec;
}

/* qfec_secnsec_2hilo - converts sec and nsec to Etherent timestamp
 * register values
 */
static inline void qfec_secnsec_2hilo(
	uint32_t  sec,
	uint32_t  nsec,
	uint32_t *tsRegHiPtr,
	uint32_t *tsRegLoPtr)
{
	uint64_t cnt;
	uint64_t subsec;

	subsec = nsec;

	cnt    = F_CLK;
	cnt   *= sec;
	cnt   += subsec;

	*tsRegHiPtr = cnt >> TS_LOW_REG_BITS;
	*tsRegLoPtr = cnt  & TS_LOW_REG_MASK;
}

/* qfec_reg_and_time --
 *
 * This function does two things:
 *
 * 1) Retrieves and returns the high and low time registers, and
 *
 * 2) Converts then returns those high and low register values as
 *    their seconds and nanoseconds equivalents.
 */
static inline void qfec_reg_and_time(
	struct qfec_priv *privPtr,
	uint32_t         *tsHiPtr,
	uint32_t         *tsLoPtr,
	uint32_t         *secPtr,
	uint32_t         *nsecPtr)
{
	/* Read/capture the high and low timestamp registers values.
	 *
	 * Insure that the high register's value doesn't increment during read.
	 */
	do {
		*tsHiPtr = qfec_reg_read(privPtr->mac_base, TS_HIGH_REG);
		*tsLoPtr = qfec_reg_read(privPtr->mac_base, TS_LOW_REG);
	} while (*tsHiPtr != qfec_reg_read(privPtr->mac_base, TS_HIGH_REG));

	/* Convert high and low time registers to secs and nsecs... */
	qfec_hilo_2secnsec(*tsHiPtr, *tsLoPtr, secPtr, nsecPtr);
}

/* read ethernet timestamp registers, pass up raw register values
 * and values converted to sec/ns
 */
static void qfec_read_timestamp(
	struct buf_desc *p_bd,
	struct skb_shared_hwtstamps *ts)
{
	uint32_t ts_hi;
	uint32_t ts_lo;
	uint32_t ts_hi63;
	uint32_t ts_lo63;
	uint32_t sec;
	uint32_t nsec;

	if (qfec_bd_enhdes(p_bd)) {
		struct qfec_enh_buf_desc *p_desc = qfec_bd_enh_buf_desc(p_bd);

		ts_hi = p_desc->tstamp_hi;
		ts_lo = p_desc->tstamp_lo;
	} else {
		ts_hi = (uint32_t)qfec_bd_next_get(p_bd);
		ts_lo = (uint32_t) qfec_bd_pbuf_get(p_bd);
	}

	/* Combine (then separate) raw registers into 63 (then 32) bit... */
	qfec_hilo_collapse(ts_hi, ts_lo, &ts_hi63, &ts_lo63);

	ts->hwtstamp = ktime_set(ts_hi63, ts_lo63);

	/* Translate raw registers to sec and ns */
	qfec_hilo_2secnsec(ts_hi, ts_lo, &sec, &nsec);

	ts->syststamp = ktime_set(sec, nsec);
}

/* capture the current system time in the timestamp registers */
static int qfec_cmd(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct qfec_priv  *priv = netdev_priv(to_net_dev(dev));
	struct timeval     tv;

	if (!strcmp(buf, "setTs")) {
		uint32_t ts_hi;
		uint32_t ts_lo;
		uint32_t cr;

		do_gettimeofday(&tv);

		/* convert raw sec/usec to hi/low registers */
		qfec_secnsec_2hilo(tv.tv_sec, tv.tv_usec * 1000,
			&ts_hi, &ts_lo);

		qfec_reg_write(priv->mac_base, TS_HI_UPDT_REG, ts_hi);
		qfec_reg_write(priv->mac_base, TS_LO_UPDT_REG, ts_lo);

		/* TS_CTL_TSINIT bit cannot be written until it is 0, hence the
		 * following while loop will run until the bit transitions to 0
		 */
		do {
			cr = qfec_reg_read(priv->mac_base, TS_CTL_REG);
		} while (cr & TS_CTL_TSINIT);

		qfec_reg_write(priv->mac_base, TS_CTL_REG, cr | TS_CTL_TSINIT);
	} else
		pr_err("%s: unknown cmd, %s.\n", __func__, buf);

	return strnlen(buf, count);
}

/* Do a "slam" of a very particular time into the time registers... */
static int qfec_slam(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct qfec_priv *priv   = netdev_priv(to_net_dev(dev));
	uint32_t          sec = 0;
	uint32_t          nsec = 0;

	if (sscanf(buf, "%u %u", &sec, &nsec) == 2) {
		uint32_t ts_hi;
		uint32_t ts_lo;
		uint32_t cr;

		qfec_secnsec_2hilo(sec, nsec, &ts_hi, &ts_lo);

		qfec_reg_write(priv->mac_base, TS_HI_UPDT_REG, ts_hi);
		qfec_reg_write(priv->mac_base, TS_LO_UPDT_REG, ts_lo);

		/* TS_CTL_TSINIT bit cannot be written until it is 0, hence the
		 * following while loop will run until the bit transitions to 0
		 */
		do {
			cr = qfec_reg_read(priv->mac_base, TS_CTL_REG);
		} while (cr & TS_CTL_TSINIT);

		qfec_reg_write(priv->mac_base, TS_CTL_REG, cr | TS_CTL_TSINIT);
	} else
		pr_err("%s: bad offset value, %s.\n", __func__, buf);

	return strnlen(buf, count);
}

/* Do a coarse time ajustment (ie. coarsely adjust (+/-) the time
 * registers by the passed offset)
 */
static int qfec_cadj(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct qfec_priv *priv   = netdev_priv(to_net_dev(dev));
	int64_t           offset = 0;

	if (sscanf(buf, "%lld", &offset) == 1) {
		uint64_t newOffset;
		uint32_t sec;
		uint32_t nsec;
		uint32_t ts_hi;
		uint32_t ts_lo;
		uint32_t cr;

		qfec_reg_and_time(priv, &ts_hi, &ts_lo, &sec, &nsec);

		newOffset = (((uint64_t) sec * BILLION) + (uint64_t) nsec)
			+ offset;

		nsec = do_div(newOffset, BILLION);
		sec  = newOffset;

		qfec_secnsec_2hilo(sec, nsec, &ts_hi, &ts_lo);

		qfec_reg_write(priv->mac_base, TS_HI_UPDT_REG, ts_hi);
		qfec_reg_write(priv->mac_base, TS_LO_UPDT_REG, ts_lo);

		/* The TS_CTL_TSINIT bit cannot be written until it is 0,
		 * hence the following while loop will run until the bit
		 * transitions to 0
		 */
		do {
			cr = qfec_reg_read(priv->mac_base, TS_CTL_REG);
		} while (cr & TS_CTL_TSINIT);

		qfec_reg_write(priv->mac_base, TS_CTL_REG, cr | TS_CTL_TSINIT);
	} else
		pr_err("%s: bad offset value, %s.\n", __func__, buf);

	return strnlen(buf, count);
}

/* Do a fine time ajustment (ie. have the timestamp registers adjust
 * themselves by the passed amount).
 */
static int qfec_fadj(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct qfec_priv *priv = netdev_priv(to_net_dev(dev));
	int64_t           offset = 0;

	if (sscanf(buf, "%lld", &offset) == 1) {
		uint32_t direction = 0;
		uint32_t cr;
		uint32_t sec, nsec;
		uint32_t ts_hi, ts_lo;

		if (offset < 0) {
			direction = 1 << TS_LOW_REG_BITS;
			offset   *= -1;
		}

		nsec = do_div(offset, BILLION);
		sec  = offset;

		qfec_secnsec_2hilo(sec, nsec, &ts_hi, &ts_lo);

		qfec_reg_write(priv->mac_base, TS_HI_UPDT_REG, ts_hi);
		qfec_reg_write(priv->mac_base, TS_LO_UPDT_REG,
			       ts_lo | direction);

		/* As per the hardware documentation, the TS_CTL_TSUPDT bit
		 * cannot be written until it is 0, hence the following while
		 * loop will run until the bit transitions to 0...
		 */
		do {
			cr = qfec_reg_read(priv->mac_base, TS_CTL_REG);
		} while (cr & TS_CTL_TSUPDT);

		qfec_reg_write(priv->mac_base, TS_CTL_REG, cr | TS_CTL_TSUPDT);
	} else
		pr_err("%s: bad offset value, %s.\n", __func__, buf);

	return strnlen(buf, count);
}

/* display ethernet tstamp and system time */
static int qfec_tstamp_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct qfec_priv   *priv = netdev_priv(to_net_dev(dev));
	int                 count = PAGE_SIZE;
	int                 l;
	struct timeval      tv;
	uint32_t            sec;
	uint32_t            nsec;
	uint32_t            ts_hi;
	uint32_t            ts_lo;

	qfec_reg_and_time(priv, &ts_hi, &ts_lo, &sec, &nsec);

	qfec_hilo_collapse(ts_hi, ts_lo, &ts_hi, &ts_lo);

	do_gettimeofday(&tv);

	l = snprintf(buf, count,
		"%12u.%09u sec 0x%08x 0x%08x tstamp  %12u.%06u time-of-day\n",
		sec, nsec, ts_hi, ts_lo, (int)tv.tv_sec, (int)tv.tv_usec);

	return l;
}

/* display ethernet mac time as well as the time of the next mac pps
 * pulse...
 */
static int qfec_mtnp_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct qfec_priv *priv  = netdev_priv(to_net_dev(dev));
	int               count = PAGE_SIZE;
	int               l;
	uint32_t          ts_hi;
	uint32_t          ts_lo;
	uint32_t          sec;
	uint32_t          nsec;
	uint32_t          ppsSec;
	uint32_t          ppsNsec;

	qfec_reg_and_time(priv, &ts_hi, &ts_lo, &sec, &nsec);

	/* Convert high and low to time of next rollover (ie. PPS
	 * pulse)...
	 */
	qfec_hilo_2secnsec(ts_hi + 1, 0, &ppsSec, &ppsNsec);

	l = snprintf(buf, count,
		"%u %u %u %u\n",
		sec, nsec, ppsSec, ppsNsec);

	return l;
}

/* free transmitted skbufs from buffer-descriptor no owned by HW */
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

		qfec_reg_write(priv->mac_base, STATUS_REG,
			STATUS_REG_TU | STATUS_REG_TI);

		/* retrieve timestamp if requested */
		if (qfec_tbd_status_tstamp(p_bd)) {
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

/* clear ownership bits of all TX buf-desc and release the sk-bufs */
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

/* rx() - process a received frame */
static int qfec_rx_int(struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct ring        *p_ring = &priv->ring_rbd;
	struct buf_desc    *p_bd   = priv->p_latest_rbd;
	uint32_t desc_status;
	uint32_t mis_fr_reg;
	int      pkt_recvd = 0;

	desc_status = qfec_bd_status_get(p_bd);
	mis_fr_reg = qfec_reg_read(priv->mac_base, MIS_FR_REG);

	CNTR_INC(priv, rx_int);

	/* check that valid interrupt occurred */
	if (unlikely(desc_status & BUF_OWN))
		return 0;

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
			pkt_recvd++;


			qfec_reg_write(priv->mac_base,
				       STATUS_REG, STATUS_REG_RI);

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

	return pkt_recvd;
}

/* isr() - interrupt service routine
 *          determine cause of interrupt and invoke/schedule appropriate
 *          processing or error handling
 */
#define ISR_ERR_CHK(priv, status, interrupt, cntr) { \
	if (status & interrupt) \
		CNTR_INC(priv, cntr); \
	}

static irqreturn_t qfec_int(int irq, void *dev_id)
{
	struct net_device  *dev      = dev_id;
	struct qfec_priv   *priv     = netdev_priv(dev);
	uint32_t            status   = qfec_reg_read(priv->mac_base,
						     STATUS_REG);
	uint32_t            int_bits = STATUS_REG_NIS | STATUS_REG_AIS;
	unsigned long       flags;

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
	spin_lock_irqsave(&priv->rx_lock, flags);
	if (status & STATUS_REG_RI) {
		CNTR_INC(priv, rx_isr);
		/* Disable RX interrupt & clear the cause */
		qfec_rx_int_ctrl(priv, false);
		qfec_reg_write(priv->mac_base, STATUS_REG, STATUS_REG_RI);
		/* While testing it was observed that rarely a Rx INT would
		 * appear even if it was disabled (the bit INTRP_EN_REG_RIE
		 * was observed to be cleared).
		 */
		if (priv->rx_int_status) {
			priv->rx_int_status = 0;
			qfec_rx_int(dev);
			hrtimer_start(&priv->rx_timer,
					ns_to_ktime(RX_POLL_INT_NS),
					HRTIMER_MODE_REL);
		}
	}
	spin_unlock_irqrestore(&priv->rx_lock, flags);

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
		qfec_reg_read(priv->mac_base, SG_RG_SMII_STATUS_REG);
	}

	/* clear interrupts */
	qfec_reg_write(priv->mac_base, STATUS_REG, int_bits);
	CNTR_INC(priv, isr);

	return IRQ_HANDLED;
}

/* Polling for rx packets */
enum hrtimer_restart qfec_rx_poll(struct hrtimer *timer)
{
	struct qfec_priv *priv = container_of(timer, struct qfec_priv,
						rx_timer);
	int pkts;
	unsigned long flags;

	spin_lock_irqsave(&priv->rx_lock, flags);
	pkts = qfec_rx_int(priv->net_dev);

	if (pkts < PKTS_PER_POLL) {
		priv->rx_int_status = 1;
		qfec_rx_int_ctrl(priv, true);
		spin_unlock_irqrestore(&priv->rx_lock, flags);
		return HRTIMER_NORESTART;
	} else {
		hrtimer_forward_now(timer, ns_to_ktime(RX_POLL_INT_NS));
		spin_unlock_irqrestore(&priv->rx_lock, flags);
		return HRTIMER_RESTART;
	}
}

/* open () - register system resources (IRQ, DMA, ...)
 *   turn on HW, perform device setup.
 */
static int qfec_open(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct buf_desc    *p_bd;
	struct ring        *p_ring;
	struct qfec_buf_desc *p_desc;
	struct qfec_enh_buf_desc *p_enhdesc;
	struct pinctrl_state *pin_state;
	int                 n;
	int                 res = 0;
	uint32_t            bd_flag = 0;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %p dev\n", __func__, dev);

	if (!dev)  {
		res = -EINVAL;
		goto err;
	}

	if (priv->pinctrl) {
		pin_state = pinctrl_lookup_state(priv->pinctrl,
				qfec_pinctrl_name[PINCTRL_MDIO_ACTIVE]);
		if (!IS_ERR(pin_state))
			pinctrl_select_state(priv->pinctrl, pin_state);
	}

	/* initialize hrtimer for Rx buffer polling */
	hrtimer_init(&priv->rx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	priv->rx_timer.function = qfec_rx_poll;

	/* allocate TX/RX buffer-descriptors and buffers */

	res = qfec_mem_alloc(dev);
	if (res)
		goto err;

	/* initialize TX */
	p_desc = priv->bd_base;
	p_enhdesc = (struct qfec_enh_buf_desc *)priv->bd_base;

	if (qfec_hw_feature_enhanced_des(priv))
		bd_flag = (qfec_hw_feature_advanced_tstamp(priv)) ?
			  (BD_FLAG_ENHDES | BD_FLAG_ATDS) :
			   BD_FLAG_ENHDES;

	for (n = 0, p_bd = priv->p_tbd; n < priv->n_tbd;
	      n++, p_bd++, p_desc++, p_enhdesc++) {
		if (bd_flag & BD_FLAG_ATDS)
			p_bd->p_desc = p_enhdesc;
		else
			p_bd->p_desc = p_desc;

		qfec_bd_flag_set(p_bd,
				 (n == (priv->n_tbd - 1)) ?
				 bd_flag | BD_FLAG_LAST_BD : bd_flag);

		qfec_bd_own_clr(p_bd);      /* clear ownership */
	}

	qfec_ring_init(&priv->ring_tbd, priv->n_tbd, priv->n_tbd);

	priv->tx_ic_mod = priv->n_tbd / TX_BD_TI_RATIO;
	if (priv->tx_ic_mod == 0)
		priv->tx_ic_mod = 1;

	/* initialize RX buffer descriptors and allocate sk_bufs */
	p_ring = &priv->ring_rbd;
	qfec_ring_init(p_ring, priv->n_rbd, 0);

	for (n = 0, p_bd = priv->p_rbd; n < priv->n_rbd;
	     n++, p_bd++, p_desc++, p_enhdesc++) {
		if (bd_flag & BD_FLAG_ATDS)
			p_bd->p_desc = p_enhdesc;
		else
			p_bd->p_desc = p_desc;

		qfec_bd_flag_set(p_bd,
				 (n == (priv->n_rbd - 1)) ?
				 bd_flag | BD_FLAG_LAST_BD : bd_flag);

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

	/* initialize controller after BDs allocated */
	res = qfec_ops_init(priv);
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
	hrtimer_cancel(&priv->rx_timer);
	QFEC_LOG_ERR("%s: error - %d\n", __func__, res);
	return res;
}

/* stop() - "reverse operations performed at open time" */
static int qfec_stop(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct buf_desc    *p_bd;
	struct sk_buff     *skb;
	struct pinctrl_state *pin_state;
	int                 n;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	del_timer_sync(&priv->phy_tmr);
	hrtimer_cancel(&priv->rx_timer);

	qfec_hw_disable(priv);
	qfec_queue_stop(dev);
	free_irq(dev->irq, dev);

	if (priv->pinctrl) {
		pin_state = pinctrl_lookup_state(priv->pinctrl,
				qfec_pinctrl_name[PINCTRL_MDIO_DEACTIVE]);
		if (!IS_ERR(pin_state))
			pinctrl_select_state(priv->pinctrl, pin_state);
	}

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

/* pass data from skbuf to buf-desc */
static int qfec_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct qfec_priv   *priv   = netdev_priv(dev);
	struct ring        *p_ring = &priv->ring_tbd;
	struct buf_desc    *p_bd;
	uint32_t            ctrl   = 0;
	uint32_t            status = BUF_OWN;
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

	ctrl = skb->len;
	if (!(qfec_ring_head(p_ring) % priv->tx_ic_mod)) {
		if (qfec_bd_enh_buf_desc(p_bd))
			status |= ENH_BUF_TX_IC;
		else
			ctrl |= BUF_TX_IC;
	}

	/* check if timestamping enabled and requested */
	if (priv->state & timestamping)  {
		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
			CNTR_INC(priv, ts_tx_en);
			if (qfec_bd_enh_buf_desc(p_bd))
				status |= ENH_BUF_TX_TTSE | ENH_BUF_TX_IC;
			else
				ctrl |= BUF_TX_IC | BUF_TX_TTSE;
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		}
	}

	if (qfec_bd_last_bd(p_bd)) {
		if (qfec_bd_enh_buf_desc(p_bd))
			status |= ENH_BUF_TX_TER;
		else
			ctrl |= BUF_TX_TER;
	}

	/* no gather, no multi buf frames */
	if (qfec_bd_enh_buf_desc(p_bd))
		status |= ENH_BUF_TX_FS | ENH_BUF_TX_LS;
	else
		ctrl |= BUF_TX_FS | BUF_TX_LS;

	qfec_bd_ctl_wr(p_bd, ctrl);
	qfec_bd_status_wr(p_bd, status);

	qfec_ring_head_adv(p_ring);
	qfec_reg_write(priv->mac_base, TX_POLL_DEM_REG, 1);      /* poll */

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
		qfec_reg_update(priv->mac_base, TS_CTL_REG,
				TS_CTL_TSENALL, TS_CTL_TSENALL);

		return 0;
	}

	return generic_mii_ioctl(&priv->mii, if_mii(ifr), cmd, NULL);
}

static struct net_device_stats *qfec_get_stats(struct net_device *dev)
{
	struct qfec_priv   *priv = netdev_priv(dev);

	QFEC_LOG(QFEC_LOG_DBG2, "qfec_stats:\n");

	priv->stats.multicast = qfec_reg_read(priv->mac_base,
					      NUM_MULTCST_FRM_RCVD_G);

	return &priv->stats;
}

/* accept new mac address */
static int qfec_set_mac_address(struct net_device *dev, void *p)
{
	struct qfec_priv   *priv = netdev_priv(dev);
	struct sockaddr    *addr = p;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	qfec_set_adr_regs(priv, dev->dev_addr);

	return 0;
}

/* static definition of driver functions */
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

/* ethtool functions */
static int qfec_nway_reset(struct net_device *dev)
{
	struct qfec_priv  *priv = netdev_priv(dev);
	return mii_nway_restart(&priv->mii);
}

/* speed, duplex, auto-neg settings */
static void qfec_ethtool_getpauseparam(struct net_device *dev,
			struct ethtool_pauseparam *pp)
{
	struct qfec_priv  *priv = netdev_priv(dev);
	u32                flow = qfec_reg_read(priv->mac_base,
						FLOW_CONTROL_REG);
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

/* ethtool ring parameter (-g/G) support */
/* setringparamam - change the tx/rx ring lengths */
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

/* getringparamam - returns local values */
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

/* speed, duplex, auto-neg settings */
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

/* msg/debug level */
static u32 qfec_ethtool_getmsglevel(struct net_device *dev)
{
	return qfec_debug;
}

static void qfec_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	qfec_debug ^= level;	/* toggle on/off */
}

/* register dump */
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
		data[j++] = htonl(qfec_reg_read(priv->mac_base, i));

	j = MAC_DMP_OFFSET / sizeof(u32);
	for (i = MAC_REG_OFFSET, n = MAC_REG_LEN; n--; i += sizeof(u32))
		data[j++] = htonl(qfec_reg_read(priv->mac_base, i));

	j = TS_DMP_OFFSET / sizeof(u32);
	for (i = TS_REG_OFFSET, n = TS_REG_LEN; n--; i += sizeof(u32))
		data[j++] = htonl(qfec_reg_read(priv->mac_base, i));

	data16 = (u16 *)&data[MDIO_DMP_OFFSET / sizeof(u32)];
	for (i = 0, n = 0; i < MDIO_REG_LEN; i++)
		data16[n++] = htons(qfec_mdio_read(dev, 0, i));

	regs->len     = REG_SIZE;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %d bytes\n", __func__, regs->len);
}

/* statistics
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
			qfec_reg_read(priv->mac_base,
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
		data[j++] = qfec_reg_read(priv->mac_base,
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

/* ethtool ops table */
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

/* create sysfs entries */
static DEVICE_ATTR(bd_tx,   0444, qfec_bd_tx_show,   NULL);
static DEVICE_ATTR(bd_rx,   0444, qfec_bd_rx_show,   NULL);
static DEVICE_ATTR(cfg,     0444, qfec_config_show,  NULL);
static DEVICE_ATTR(cmd,     0222, NULL,              qfec_cmd);
static DEVICE_ATTR(cntrs,   0444, qfec_cntrs_show,   NULL);
static DEVICE_ATTR(reg,     0444, qfec_reg_show,     NULL);
static DEVICE_ATTR(mdio,    0444, qfec_mdio_show,    NULL);
static DEVICE_ATTR(stats,   0444, qfec_stats_show,   NULL);
static DEVICE_ATTR(tstamp,  0444, qfec_tstamp_show,  NULL);
static DEVICE_ATTR(slam,    0222, NULL,              qfec_slam);
static DEVICE_ATTR(cadj,    0222, NULL,              qfec_cadj);
static DEVICE_ATTR(fadj,    0222, NULL,              qfec_fadj);
static DEVICE_ATTR(mtnp,    0444, qfec_mtnp_show,    NULL);

static void qfec_sysfs_create(struct net_device *dev)
{
	if (device_create_file(&(dev->dev), &dev_attr_bd_tx) ||
		device_create_file(&(dev->dev), &dev_attr_bd_rx) ||
		device_create_file(&(dev->dev), &dev_attr_cfg) ||
		device_create_file(&(dev->dev), &dev_attr_cmd) ||
		device_create_file(&(dev->dev), &dev_attr_cntrs) ||
		device_create_file(&(dev->dev), &dev_attr_mdio) ||
		device_create_file(&(dev->dev), &dev_attr_reg) ||
		device_create_file(&(dev->dev), &dev_attr_stats) ||
		device_create_file(&(dev->dev), &dev_attr_tstamp) ||
		device_create_file(&(dev->dev), &dev_attr_slam) ||
		device_create_file(&(dev->dev), &dev_attr_cadj) ||
		device_create_file(&(dev->dev), &dev_attr_fadj) ||
		device_create_file(&(dev->dev), &dev_attr_mtnp))
		pr_err("qfec_sysfs_create failed to create sysfs files\n");
}

/* map a specified resource */
static int qfec_map_resource(struct platform_device *plat,
	const char *name,
	struct resource **priv_res,
	void                   **addr)
{
	struct resource         *res;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %s resource\n", __func__, name);

	/* allocate region to access controller registers */
	res = platform_get_resource_byname(plat, IORESOURCE_MEM, name);
	if (!res)
		return -ENODEV;

	*addr = ioremap(res->start, resource_size(res));
	if (!*addr)
		return -ENOMEM;

	QFEC_LOG(QFEC_LOG_DBG, " %s: io mapped from %p to %p\n",
		__func__, (void *)res->start, *addr);

	*priv_res = res;
	return 0;
};

/* free allocated io regions */
static void qfec_free_res(struct resource *res, void *base)
{

	if (res)  {
		if (base)
			iounmap((void __iomem *)base);
	}
};

/* general qfec ops */
/* clock */
static int qfec_clk_enable(struct qfec_priv *priv, uint8_t clk)
{
	int res;

	res = clk_prepare_enable(priv->clk[clk]);
	if (res)
		QFEC_LOG_ERR("failed to enable clk(%s)\n", qfec_clk_name[clk]);

	return res;
}

static int qfec_clk_disable(struct qfec_priv *priv, uint8_t clk)
{
	clk_disable_unprepare(priv->clk[clk]);
	return 0;
}

static int qfec_clk_set_rate(struct qfec_priv *priv, uint8_t clk,
			     unsigned long rate)
{
	int res;

	res = clk_set_rate(priv->clk[clk], rate);
	if (res)
		QFEC_LOG_ERR("failed to set clk rate %s\n", qfec_clk_name[clk]);

	return res;
}

static int qfec_init(struct qfec_priv *priv)
{
	int res;

	res = qfec_clk_set_rate(priv, QFEC_TX_CLK, CLK_RATE_125M);
	if (res)
		return res;
	res = qfec_clk_set_rate(priv, QFEC_RX_CLK, CLK_RATE_125M);
	if (res)
		return res;
	res = qfec_clk_enable(priv, QFEC_TX_CLK);
	if (res)
		return res;
	res = qfec_clk_enable(priv, QFEC_RX_CLK);
	if (res)
		return res;

	res = qfec_hw_init(priv);

	return res;
}

/* qfec NSS wrapper */
enum qfec_nss_reg_base {
	QFEC_NSS_CSR,
	QFEC_QSGMII,
	QFEC_RGMII_CSR,
	TLMM_CSR,
	NUM_QFEC_NSS_REG_BASE,
};

struct qfec_nss {
	struct resource        *reg_res[NUM_QFEC_NSS_REG_BASE];
	void __iomem           *reg_base[NUM_QFEC_NSS_REG_BASE];

	unsigned int            ch_num;
	bool                    rgmii_capable;
};

static int qfec_nss_clk_enable(struct qfec_priv *priv, uint8_t clk)
{
	struct qfec_nss *nss = priv->pdata;
	int res = 0;

	switch (clk) {
	case QFEC_CORE_CLK:
		res = qfec_clk_enable(priv, clk);
		break;
	case QFEC_INTF_CLK:
		switch (priv->intf) {
		case INTFC_SGMII:
			res = qfec_clk_enable(priv, clk);
			break;
		default:
			break;
		}
		break;
	case QFEC_TX_CLK:
		switch (priv->intf) {
		case INTFC_MII:
		case INTFC_SGMII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_TX_CLKEN(priv->idx),
					GMAC_N_MII_TX_CLKEN(priv->idx));
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_SRC_CTL_REG,
					GMAC_N_CLK_MUX_SEL(priv->idx), 0);
			break;
		case INTFC_RGMII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_TX_CLKEN(priv->idx),
					(GMAC_N_RGMII_TX_CLKEN(priv->idx) |
					 GMAC_N_MII_TX_CLKEN(priv->idx)));
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_SRC_CTL_REG,
					GMAC_N_CLK_MUX_SEL(priv->idx),
					GMAC_N_CLK_MUX_SEL(priv->idx));
			break;
		default:
			res = -EOPNOTSUPP;
			break;
		}
		break;
	case QFEC_RX_CLK:
		switch (priv->intf) {
		case INTFC_SGMII:
		case INTFC_MII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_RX_CLKEN(priv->idx),
					GMAC_N_MII_RX_CLKEN(priv->idx));
			break;
		case INTFC_RGMII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_RX_CLKEN(priv->idx),
					(GMAC_N_RGMII_RX_CLKEN(priv->idx) |
					 GMAC_N_MII_RX_CLKEN(priv->idx)));
			break;
		default:
			res = -EOPNOTSUPP;
			break;
		}
		break;
	case QFEC_PTP_CLK:
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				GMAC_N_PTP_CLKEN(priv->idx),
				GMAC_N_PTP_CLKEN(priv->idx));
		break;
	default:
		res = -EOPNOTSUPP;
		break;
	}

	return res;
}

static int qfec_nss_clk_disable(struct qfec_priv *priv, uint8_t clk)
{
	struct qfec_nss *nss = priv->pdata;
	int res = 0;

	switch (clk) {
	case QFEC_CORE_CLK:
	case QFEC_INTF_CLK:
		res = qfec_clk_disable(priv, clk);
		break;
	case QFEC_TX_CLK:
		switch (priv->intf) {
		case INTFC_MII:
		case INTFC_SGMII:
		case INTFC_RGMII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_TX_CLKEN(priv->idx), 0);
			break;
		default:
			res = -EOPNOTSUPP;
			break;
		}
		break;
	case QFEC_RX_CLK:
		switch (priv->intf) {
		case INTFC_MII:
		case INTFC_SGMII:
		case INTFC_RGMII:
			qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
					CFG_CLK_GATE_CTL_REG,
					GMAC_N_RX_CLKEN(priv->idx), 0);
			break;
		default:
			res = -EOPNOTSUPP;
			break;
		}
		break;
	case QFEC_PTP_CLK:
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				GMAC_N_PTP_CLKEN(priv->idx), 0);
		break;
	default:
		res = -EOPNOTSUPP;
		break;
	}

	return res;
}

struct qfec_nss_clk_div_cfg {
	uint32_t intf;
	unsigned long rate;
	uint32_t clk_div;
};

static const struct qfec_nss_clk_div_cfg nss_clk_div_cfg_tbl[] = {
	{ INTFC_RGMII, CLK_RATE_2P5M, CLKDIV_RGMII_10 },
	{ INTFC_RGMII, CLK_RATE_25M, CLKDIV_RGMII_100 },
	{ INTFC_RGMII, CLK_RATE_125M, CLKDIV_RGMII_1000 },
	{ INTFC_SGMII, CLK_RATE_2P5M, CLKDIV_SGMII_10 },
	{ INTFC_SGMII, CLK_RATE_25M, CLKDIV_SGMII_100 },
	{ INTFC_SGMII, CLK_RATE_125M, CLKDIV_SGMII_1000 },
	{ INTFC_MII, CLK_RATE_2P5M, CLKDIV_SGMII_10 },
	{ INTFC_MII, CLK_RATE_25M, CLKDIV_SGMII_100 },
	{ INTFC_MII, CLK_RATE_125M, CLKDIV_SGMII_1000 },
};

static int qfec_nss_clk_set_rate(struct qfec_priv *priv,
				 uint8_t clk, unsigned long rate)
{
	struct qfec_nss *nss = priv->pdata;
	const struct qfec_nss_clk_div_cfg *p = nss_clk_div_cfg_tbl;
	int i;
	int res = 0;

	switch (clk) {
	case QFEC_CORE_CLK:
		res = qfec_clk_set_rate(priv, clk, rate);
		break;
	case QFEC_TX_CLK:
		for (i = 0; i < ARRAY_SIZE(nss_clk_div_cfg_tbl); i++, p++) {
			if (p->intf == priv->intf && p->rate == rate)
				break;
		}
		if (i == ARRAY_SIZE(nss_clk_div_cfg_tbl))
			return -EINVAL;
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR], CFG_CLK_DIV_REG,
				GMAC_N_CLKDIV_BMSK(priv->idx),
				GMAC_N_CLKDIV(p->clk_div, priv->idx));
		break;
	case QFEC_INTF_CLK:
	case QFEC_RX_CLK:
		break;
	default:
		res = -EOPNOTSUPP;
		break;
	}

	return res;
}

static int qsgmii_reset(struct qfec_priv *priv)
{
	struct qfec_nss *nss = priv->pdata;

	qfec_reg_write(nss->reg_base[QFEC_NSS_CSR], CFG_SPARE_CTL_REG,
		       SPARE_CTL_PCS_RESET);
	usleep_range(100, 150);
	qfec_reg_write(nss->reg_base[QFEC_NSS_CSR], CFG_SPARE_CTL_REG, 0);

	return 0;
}

static int qsgmii_pcs_cfg(struct qfec_priv *priv,
			  unsigned int spd, unsigned int dplx)
{
	struct qfec_nss *nss = priv->pdata;
	uint32_t ctl;

	switch (spd) {
	case SPD_10:
		ctl = CH_SPEED_25M_10(nss->ch_num);
		break;
	case SPD_100:
		ctl = CH_SPEED_25M_100(nss->ch_num);
		break;
	case SPD_1000:
		ctl = CH_SPEED_25M_1000(nss->ch_num);
		break;
	default:
		return -EINVAL;
	}
	qfec_reg_update(nss->reg_base[QFEC_QSGMII], PCS_ALL_CH_CTL_REG,
			PCS_CH_CTL_CH_BMSK(nss->ch_num), ctl);

	return 0;
}

static int qsgmii_init(struct qfec_priv *priv)
{
	struct qfec_nss *nss = priv->pdata;

	qfec_reg_write(nss->reg_base[QFEC_QSGMII], QSGMII_PHY_MODE_CTL_REG, 0);
	qfec_reg_write(nss->reg_base[QFEC_QSGMII],
		       PCS_QSGMII_SGMII_MODE_REG, 0);

	if (nss->ch_num <= 1)
		qfec_reg_write(nss->reg_base[QFEC_QSGMII],
			       QSGMII_PHY_QSGMII_CTL_REG,
			       QSGMII_PHY_CTL_SGMII);
	else
		qfec_reg_write(nss->reg_base[QFEC_QSGMII],
			       QSGMII_PHY_SGMII_1_CTL_REG,
			       SGMII_PHY_CTL_SGMII);
	qsgmii_reset(priv);
	qsgmii_pcs_cfg(priv, SPD_1000, 1);

	qfec_reg_update(nss->reg_base[QFEC_NSS_CSR], CFG_QSGMII_CLK_CTL_REG,
			RGMII_REF_CLK_SEL, 0);

	qfec_reg_update(nss->reg_base[QFEC_QSGMII], PCS_MODE_CRTL_REG,
			PCS_MODE_CRTL_CH_BMSK(nss->ch_num),
			CH_MODE_CRTL_SGMII(nss->ch_num));
	qfec_reg_update(nss->reg_base[QFEC_QSGMII], PCS_QSGMII_CTL_REG,
			CH_SELECT(nss->ch_num) |
			CH_SIGNAL_DETECT(nss->ch_num) | QSGMII_CTL_SGMII_BMSK,
			CH_SELECT(nss->ch_num) |
			CH_SIGNAL_DETECT(nss->ch_num) | QSGMII_CTL_SGMII);
	return 0;
}

static void tlmm_csr_cfg(struct qfec_priv *priv)
{
	struct qfec_nss *nss = priv->pdata;

	if (nss->rgmii_capable && nss->reg_base[TLMM_CSR]) {
		qfec_reg_write(nss->reg_base[TLMM_CSR],
			       TLMM_RGMII_HDRV_CTL,
			       (priv->intf == INTFC_RGMII) ?
			       TLMM_RGMII_HDRV_CTL_SEL : 0);
		qfec_reg_write(nss->reg_base[TLMM_CSR],
			       TLMM_RGMII_PULL_CTL, 0);
	}
}

static int qfec_nss_init(struct qfec_priv *priv)
{
	struct qfec_nss *nss = priv->pdata;
	int res = 0;

	switch (priv->intf) {
	case INTFC_SGMII:
	case INTFC_MII:
		tlmm_csr_cfg(priv);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				GMAC_N_TX_CLKEN(priv->idx) |
				GMAC_N_RX_CLKEN(priv->idx) |
				GMAC_N_PTP_CLKEN(priv->idx),
				0);

		if (priv->intf == INTFC_SGMII)
			qsgmii_init(priv);

		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_SRC_CTL_REG,
				GMAC_N_CLK_MUX_SEL(priv->idx), 0);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_DIV_REG,
				GMAC_N_CLKDIV_BMSK(priv->idx),
				GMAC_N_CLKDIV(CLKDIV_SGMII_1000, priv->idx));
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				GMAC_N_MII_CLKEN_BMSK(priv->idx),
				GMAC_N_MII_CLKEN_BMSK(priv->idx));
		qfec_reg_write(nss->reg_base[QFEC_NSS_CSR],
			       CFG_GMAC_N_CTL_REG(priv->idx),
			       GMAC_CTL_IFG_DEFAULT);
		break;
	case INTFC_RGMII:
		if (nss->rgmii_capable == false) {
			QFEC_LOG_ERR("%s: RGMII is not supported on intf%d\n",
				     __func__, priv->idx);
			res = -ENOTSUPP;
			goto done;
		}
		tlmm_csr_cfg(priv);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				(GMAC_N_TX_CLKEN(priv->idx) |
				 GMAC_N_RX_CLKEN(priv->idx) |
				 GMAC_N_PTP_CLKEN(priv->idx)),
				0);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_QSGMII_CLK_CTL_REG,
				RGMII_REF_CLK_SEL, 0);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_SRC_CTL_REG,
				GMAC_N_CLK_MUX_SEL(priv->idx), 0);
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_DIV_REG,
				GMAC_N_CLKDIV_BMSK(priv->idx),
				GMAC_N_CLKDIV(CLKDIV_RGMII_1000, priv->idx));
		qfec_reg_write(nss->reg_base[QFEC_NSS_CSR],
			       CFG_GMAC_N_CTL_REG(priv->idx),
			       (PHY_INTF_SEL | GMAC_CTL_IFG_DEFAULT));
		qfec_reg_update(nss->reg_base[QFEC_NSS_CSR],
				CFG_CLK_GATE_CTL_REG,
				(GMAC_N_TX_CLKEN(priv->idx) |
				 GMAC_N_RX_CLKEN(priv->idx)),
				(GMAC_N_MII_TX_CLKEN(priv->idx) |
				 GMAC_N_RGMII_CLKEN_BMSK(priv->idx)));
		break;
	default:
		QFEC_LOG_ERR("%s: unsupported intf %d\n", __func__, priv->intf);
		res = -ENOTSUPP;
		goto done;
		break;
	}

	res = qfec_hw_init(priv);

done:
	return res;
}

static int qfec_nss_link_cfg(struct qfec_priv *priv,
			     unsigned int spd, unsigned int dplx)
{
	int res = 0;

	switch (priv->intf) {
	case INTFC_SGMII:
		res = qsgmii_pcs_cfg(priv, spd, dplx);
		break;
	default:
		break;
	}

	return res;
}

static int qfec_nss_probe(struct platform_device *plat)
{
	struct net_device *dev = platform_get_drvdata(plat);
	struct qfec_priv *priv = netdev_priv(dev);
	struct device_node *of_node = plat->dev.of_node;
	struct qfec_nss *nss;
	int res;

	nss = kzalloc(sizeof(struct qfec_nss), GFP_KERNEL);
	if (!nss)
		return -ENOMEM;

	nss->rgmii_capable = of_property_read_bool(of_node,
						   "qcom,rgmii-capable");

	/* map register regions */
	res = qfec_map_resource(plat, "qfec_csr", &nss->reg_res[QFEC_NSS_CSR],
				&nss->reg_base[QFEC_NSS_CSR]);
	if (res) {
		QFEC_LOG_ERR("%s: IORESOURCE_MEM csr failed\n", __func__);
		goto err1;
	}

	switch (priv->intf) {
	case INTFC_SGMII:
		res = of_property_read_u32(of_node, "qcom,qsgmii-pcs-chan",
					   &nss->ch_num);
		if (res) {
			QFEC_LOG_ERR("%s: qsgmii-pcs-chan not specified\n",
				     __func__);
			goto err2;
		}
		res = qfec_map_resource(plat, "qfec_qsgmii",
					&nss->reg_res[QFEC_QSGMII],
					&nss->reg_base[QFEC_QSGMII]);
		if (res) {
			QFEC_LOG_ERR("%s: IORESOURCE_MEM qsgmii failed\n",
				     __func__);
			goto err2;
		}
		break;
	case INTFC_RGMII:
		if (nss->rgmii_capable == false) {
			QFEC_LOG_ERR("%s: rgmii_capable not specified\n",
				     __func__);
			res = -ENOTSUPP;
			goto err2;
		}
		res = qfec_map_resource(plat, "qfec_rgmii_csr",
					&nss->reg_res[QFEC_RGMII_CSR],
					&nss->reg_base[QFEC_RGMII_CSR]);
		if (res) {
			QFEC_LOG_ERR("%s: IORESOURCE_MEM rgmii_csr failed\n",
				     __func__);
			goto err2;
		}
		break;
	default:
		break;
	}

	res = qfec_map_resource(plat, "tlmm_csr",
				&nss->reg_res[TLMM_CSR],
				&nss->reg_base[TLMM_CSR]);

	priv->pdata = nss;

	return 0;
err2:
	qfec_free_res(nss->reg_res[QFEC_NSS_CSR], nss->reg_base[QFEC_NSS_CSR]);
err1:
	kfree(nss);
	return res;
}

static void qfec_nss_remove(struct platform_device *plat)
{
	struct net_device *dev = platform_get_drvdata(plat);
	struct qfec_priv *priv = netdev_priv(dev);
	struct qfec_nss *nss = priv->pdata;
	int i;

	if (nss) {
		for (i = 0; i < NUM_QFEC_NSS_REG_BASE; i++)
			qfec_free_res(nss->reg_res[i], nss->reg_base[i]);
		kfree(nss);
	}
	priv->pdata = 0;
}

/* qfec ops and of device table */
struct qfec_ops qfec_nss_ops = {
	.probe = qfec_nss_probe,
	.remove = qfec_nss_remove,
	.clk_enable = qfec_nss_clk_enable,
	.clk_disable = qfec_nss_clk_disable,
	.clk_set_rate = qfec_nss_clk_set_rate,
	.init = qfec_nss_init,
	.link_cfg = qfec_nss_link_cfg,
};

struct qfec_ops qfec_ops = {
	.clk_enable = qfec_clk_enable,
	.clk_disable = qfec_clk_disable,
	.clk_set_rate = qfec_clk_set_rate,
	.init = qfec_init,
};

static struct of_device_id qfec_dt_match[];

/* probe function that obtain configuration info and allocate net_device */
static int qfec_probe(struct platform_device *plat)
{
	struct net_device  *dev;
	struct qfec_priv   *priv;
	struct device_node *of_node = plat->dev.of_node;
	const struct of_device_id *of_id;
	struct pinctrl     *pinctrl;
	struct clk         *clk;
	const void         *maddr;
	int                 ret = 0;
	int                 i;

	/* allocate device */
	dev = alloc_etherdev(sizeof(struct qfec_priv));
	if (!dev) {
		QFEC_LOG_ERR("%s: alloc_etherdev failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	QFEC_LOG(QFEC_LOG_DBG, "%s: %08x dev\n",      __func__, (int)dev);

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
	/* initialize mdio clock */
	priv->mdio_clk    = GMII_ADR_REG_CR_62;

	of_id = of_match_device(qfec_dt_match, &plat->dev);
	priv->ops = (of_id) ? of_id->data : &qfec_ops;

	ret = of_property_read_u32(of_node, "cell-index", &priv->idx);
	if (ret) {
		QFEC_LOG_ERR("%s: cell-index not specified!\n", __func__);
		goto err1;
	}

	of_property_read_u32(of_node, "phy-addr", &priv->phy_id);
	priv->mii.phy_id = priv->phy_id;

	ret = of_get_phy_mode(of_node);
	if (ret < 0) {
		QFEC_LOG_ERR("%s: failed to get phy_mode!\n", __func__);
		goto err1;
	}

	priv->intf = phy_mode_to_intfc(ret);
	if (priv->intf < 0) {
		QFEC_LOG_ERR("%s: invalid phy_mode %d!\n", __func__, ret);
		goto err1;
	}

	/* map register regions */
	ret = qfec_map_resource(plat, "qfec_mac",
				&priv->mac_res, &priv->mac_base);
	if (ret)  {
		QFEC_LOG_ERR("%s: IORESOURCE_MEM mac failed\n", __func__);
		goto err1;
	}

	/* initialize MAC addr */
	maddr = of_get_mac_address(of_node);
	if (!maddr) {
		QFEC_LOG_ERR("%s: failed to get mac address\n", __func__);
		ret = -ENODEV;
		goto err2;
	}
	memcpy(dev->dev_addr, maddr, dev->addr_len);

	QFEC_LOG(QFEC_LOG_DBG, "%s: mac  %02x:%02x:%02x:%02x:%02x:%02x\n",
		__func__,
		dev->dev_addr[0], dev->dev_addr[1],
		dev->dev_addr[2], dev->dev_addr[3],
		dev->dev_addr[4], dev->dev_addr[5]);

	for (i = 0; i < NUM_QFEC_CLK; i++) {
		clk = clk_get(&plat->dev, qfec_clk_name[i]);
		if (IS_ERR(clk)) {
			if (i == QFEC_CORE_CLK) {
				QFEC_LOG_ERR("%s: failed to get clk(%s)\n",
					     __func__, qfec_clk_name[i]);
				goto err3;
			}
			continue;
		}
		priv->clk[i] = clk;
	}

	pinctrl = devm_pinctrl_get(&plat->dev);
	if (!IS_ERR(priv->pinctrl))
		priv->pinctrl = pinctrl;

	ret = qfec_ops_clk_enable(priv, QFEC_CORE_CLK);
	if (ret) {
		QFEC_LOG_ERR("%s: failed to enable core clock\n", __func__);
		goto err3;
	}

	priv->hw_feature = qfec_reg_read(priv->mac_base, HW_FEATURE_REG);

	if (priv->ops->probe) {
		ret = priv->ops->probe(plat);
		if (ret)
			goto err3;
	}

	ret = register_netdev(dev);
	if (ret) {
		QFEC_LOG_ERR("%s: register_netdev failed\n", __func__);
		goto err4;
	}

	spin_lock_init(&priv->mdio_lock);
	spin_lock_init(&priv->xmit_lock);
	spin_lock_init(&priv->rx_lock);
	qfec_sysfs_create(dev);

	pr_info("qfec[%d]: mac_base=0x%08x irq=%d intf=%s feature=0x%08x\n",
		priv->idx, (unsigned int)priv->mac_res->start,
		dev->irq, phy_intfc_name[priv->intf], priv->hw_feature);

	return 0;

	/* error handling */
err4:
	if (priv->ops->remove)
		priv->ops->remove(plat);
err3:
	for (i = 0; i < NUM_QFEC_CLK; i++) {
		if (!IS_ERR_OR_NULL(priv->clk[i]))
			clk_put(priv->clk[i]);
	}
err2:
	qfec_free_res(priv->mac_res, priv->mac_base);
err1:
	free_netdev(dev);
err:
	QFEC_LOG_ERR("%s: err\n", __func__);
	return ret;
}

/* module remove */
static int qfec_remove(struct platform_device *plat)
{
	struct net_device  *dev  = platform_get_drvdata(plat);
	struct qfec_priv   *priv = netdev_priv(dev);
	int                 i;

	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	unregister_netdev(dev);

	if (priv->ops->remove)
		priv->ops->remove(plat);

	for (i = 0; i < NUM_QFEC_CLK; i++) {
		if (!IS_ERR_OR_NULL(priv->clk[i]))
			clk_put(priv->clk[i]);
	}

	if (priv->pinctrl)
		devm_pinctrl_put(priv->pinctrl);

	platform_set_drvdata(plat, NULL);

	qfec_free_res(priv->mac_res, priv->mac_base);

	free_netdev(dev);

	return 0;
}

static struct of_device_id qfec_dt_match[] = {
	{
		.compatible = "qcom,qfec",
		.data = &qfec_ops,
	},
	{
		.compatible = "qcom,qfec-nss",
		.data = &qfec_nss_ops,
	},
	{}
};

/* module support
 *     the FSM9xxx is not a mobile device does not support power management
 */

static struct platform_driver qfec_driver = {
	.probe  = qfec_probe,
	.remove = qfec_remove,
	.driver = {
		.name   = QFEC_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = qfec_dt_match,
	},
};

/* module init */
static int __init qfec_init_module(void)
{
	int  res;

	QFEC_LOG(QFEC_LOG_DBG, "%s: %s\n", __func__, qfec_driver.driver.name);

	res = platform_driver_register(&qfec_driver);

	QFEC_LOG(QFEC_LOG_DBG, "%s: %d - platform_driver_register\n",
		__func__, res);

	return  res;
}

/* module exit */
static void __exit qfec_exit_module(void)
{
	QFEC_LOG(QFEC_LOG_DBG, "%s:\n", __func__);

	platform_driver_unregister(&qfec_driver);
}

MODULE_DESCRIPTION("FSM Network Driver");
MODULE_LICENSE("GPL v2");

module_init(qfec_init_module);
module_exit(qfec_exit_module);
