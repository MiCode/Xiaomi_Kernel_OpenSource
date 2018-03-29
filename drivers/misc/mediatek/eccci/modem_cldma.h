/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MODEM_CD_H__
#define __MODEM_CD_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <mt-plat/mt_ccci_common.h>

#include "ccci_config.h"
#include "ccci_bm.h"
/*
  * hardcode, max queue number should be synced with port array in port_cfg.c and macros in ccci_core.h
  * following number should sync with MAX_TXQ/RXQ_NUM in ccci_core.h and bitmask in modem_cldma.c
  */
#define CLDMA_TXQ_NUM 8
#define CLDMA_RXQ_NUM 8
#define NET_TXQ_NUM 3
#define NET_RXQ_NUM 3
#define NORMAL_TXQ_NUM 6
#define NORMAL_RXQ_NUM 6
#define MAX_BD_NUM (MAX_SKB_FRAGS + 1)
#define TRAFFIC_MONITOR_INTERVAL 10	/* seconds */
#define ENABLE_HS1_POLLING_TIMER
#define SKB_RX_QUEUE_MAX_LEN 200000

/*
 * CLDMA feature options:
 * CHECKSUM_SIZE: 0 to disable checksum function, non-zero for number of checksum bytes
 * CLDMA_NO_TX_IRQ: mask all TX interrupts, collect TX_DONE skb when get Rx interrupt or Tx busy.
 * ENABLE_CLDMA_TIMER: use a timer to detect TX packet sent or not. not usable if TX interrupts are masked.
 * CLDMA_NET_TX_BD: use BD to support scatter/gather IO for net device
 */
#define CHECKSUM_SIZE 0		/* 12 */
/* #define CLDMA_NO_TX_IRQ */
#ifndef CLDMA_NO_TX_IRQ
/* #define ENABLE_CLDMA_TIMER */
#endif
#define CLDMA_NET_TX_BD

struct cldma_request {
	void *gpd;		/* virtual address for CPU */
	dma_addr_t gpd_addr;	/* physical address for DMA */
	struct sk_buff *skb;
	dma_addr_t data_buffer_ptr_saved;
	struct list_head entry;
	struct list_head bd;

	/* inherit from skb */
	unsigned char ioc_override;	/* bit7: override or not; bit0: IOC setting */
};

typedef enum {
	RING_GPD = 0,
	RING_GPD_BD = 1,
	RING_SPD = 2,
} CLDMA_RING_TYPE;

struct md_cd_queue;

/*
 * In a ideal design, all read/write pointers should be member of cldma_ring, and they will complete
 * a ring buffer object with buffer itself and Tx/Rx funcitions. but this will change too much of the original
 * code and we have to drop it. so here the cldma_ring is quite light and most of ring buffer opertions are
 * still in queue struct.
 */
struct cldma_ring {
	struct list_head gpd_ring;	/* ring of struct cldma_request */
	int length;		/* number of struct cldma_request */
	int pkt_size;		/* size of each packet in ring */
	CLDMA_RING_TYPE type;

	int (*handle_tx_request)(struct md_cd_queue *queue, struct cldma_request *req,
				  struct sk_buff *skb, unsigned int ioc_override);
	int (*handle_rx_done)(struct md_cd_queue *queue, int budget, int blocking);
	int (*handle_tx_done)(struct md_cd_queue *queue, int budget, int blocking);
};

#ifdef ENABLE_FAST_HEADER
struct ccci_fast_header {
	u32 data0;
	u16 packet_length:16;
	u16 gpd_count:15;
	u16 has_hdr_room:1;
	u16 channel:16;
	u16 seq_num:15;
	u16 assert_bit:1;
	u32 reserved;
};
#endif

static inline struct cldma_request *cldma_ring_step_forward(struct cldma_ring *ring, struct cldma_request *req)
{
	struct cldma_request *next_req;

	if (req->entry.next == &ring->gpd_ring)
		next_req = list_first_entry(&ring->gpd_ring, struct cldma_request, entry);
	else
		next_req = list_entry(req->entry.next, struct cldma_request, entry);
	return next_req;
}

struct md_cd_queue {
	unsigned char index;
	struct ccci_modem *modem;
	/*
	 * what we have here is not a typical ring buffer, as we have three players:
	 * for Tx: sender thread -> CLDMA -> tx_done thread
	 *              -sender thread: set HWO bit, req->skb and gpd->buffer, when req->skb==NULL
	 *              -tx_done thread: free skb only when req->skb!=NULL && HWO==0
	 *              -CLDMA: send skb only when gpd->buffer!=NULL && HWO==1
	 * for Rx:  refill thread -> CLDMA -> rx_done thread
	 *              -refill thread: set HWO bit, req->skb and gpd->buffer, when req->skb==NULL
	 *              -rx_done thread: free skb only when req->skb!=NULL && HWO==0
	 *              -CLDMA: send skb only when gpd->buffer!=NULL && HWO==1
	 *
	 * for Tx, although only sender thread is "writer"--who sets HWO bit, both tx_done thread and CLDMA
	 * only read this bit. BUT, other than HWO bit, sender thread also shares req->skb with tx_done thread,
	 * and gpd->buffer with CLDMA. so it must set HWO bit after set gpd->buffer and before set req->skb.
	 *
	 * for Rx, only refill thread is "writer"--who sets HWO bit, both rx_done thread and CLDMA only read this
	 * bit. other than HWO bit, refill thread also shares req->skb with rx_done thread and gpd->buffer with
	 * CLDMA. it also needs set HWO bit after set gpd->buffer and before set req->skb.
	 *
	 * so in a ideal world, we use HWO bit as an barrier, and this let us be able to avoid using lock.
	 * although, there are multiple sender threads on top of each Tx queue, they must be separated.
	 * therefore, we still have Tx lock.
	 *
	 * BUT, check this sequence: sender or refiller has set HWO=1, but doesn't set req->skb yet. CLDMA finishes
	 * this GPD and Tx_DONE or Rx_DONE will see HWO==0 but req->skb==NULL. so this skb will not be handled.
	 * therefore, as a conclusion, use lock!!!
	 *
	 * be aware, fot Tx this lock also protects TX_IRQ, TX_FULL, budget, sequence number usage.
	 */
	struct cldma_ring *tr_ring;
	struct cldma_request *tr_done;
	int budget;		/* same as ring buffer size by default */
	struct cldma_request *rx_refill;	/* only for Rx */
	struct cldma_request *tx_xmit;	/* only for Tx */
	wait_queue_head_t req_wq;	/* only for Tx */
	spinlock_t ring_lock;
	struct ccci_skb_queue skb_list; /* only for network Rx */

	struct workqueue_struct *worker;
	struct work_struct cldma_rx_work;
	struct delayed_work cldma_tx_work;

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;

#ifdef ENABLE_CLDMA_TIMER
	struct timer_list timeout_timer;
	unsigned long long timeout_start;
	unsigned long long timeout_end;
#endif
#ifdef ENABLE_FAST_HEADER
	struct ccci_fast_header fast_hdr;
#endif
	u16 debug_id;
	DIRECTION dir;
	unsigned int busy_count;
};

#define QUEUE_LEN(a) (sizeof(a)/sizeof(struct md_cd_queue))

struct md_cd_ctrl {
	struct ccci_modem *modem;
	struct md_cd_queue txq[CLDMA_TXQ_NUM];
	struct md_cd_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
#ifdef NO_START_ON_SUSPEND_RESUME
	unsigned short txq_started;
#endif

	atomic_t reset_on_going;
	atomic_t wdt_enabled;
	atomic_t cldma_irq_enabled;
	atomic_t ccif_irq_enabled;
	char trm_wakelock_name[32];
	struct wake_lock trm_wake_lock;
	char peer_wakelock_name[32];
	struct wake_lock peer_wake_lock;
	struct work_struct ccif_work;
#ifdef ENABLE_HS1_POLLING_TIMER
	struct timer_list hs1_polling_timer;
#endif
	spinlock_t cldma_timeout_lock;	/* this lock is using to protect CLDMA, not only for timeout checking */
	struct work_struct cldma_irq_work;
	struct workqueue_struct *cldma_irq_worker;
	int channel_id;		/* CCIF channel */

#if TRAFFIC_MONITOR_INTERVAL
	unsigned tx_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned rx_traffic_monitor[CLDMA_RXQ_NUM];
	unsigned tx_pre_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned long long tx_done_last_start_time[CLDMA_TXQ_NUM];
	unsigned int tx_done_last_count[CLDMA_TXQ_NUM];

	struct timer_list traffic_monitor;
	unsigned long traffic_stamp;
#endif

	struct dma_pool *gpd_dmapool;	/* here we assume T/R GPD/BD/SPD have the same size  */
	struct cldma_ring net_tx_ring[NET_TXQ_NUM];
	struct cldma_ring net_rx_ring[NET_RXQ_NUM];
	struct cldma_ring normal_tx_ring[NORMAL_TXQ_NUM];
	struct cldma_ring normal_rx_ring[NORMAL_RXQ_NUM];

	void __iomem *cldma_ap_ao_base;
	void __iomem *cldma_md_ao_base;
	void __iomem *cldma_ap_pdn_base;
	void __iomem *cldma_md_pdn_base;
	void __iomem *md_rgu_base;
	void __iomem *l1_rgu_base;
	void __iomem *md_boot_slave_Vector;
	void __iomem *md_boot_slave_Key;
	void __iomem *md_boot_slave_En;
	void __iomem *md_global_con0;
	void __iomem *ap_ccif_base;
	void __iomem *md_ccif_base;
#ifdef MD_PEER_WAKEUP
	void __iomem *md_peer_wakeup;
#endif
	void __iomem *md_bus_status;
	void __iomem *md_pc_monitor;
	void __iomem *md_topsm_status;
	void __iomem *md_ost_status;
	void __iomem *md_pll;
	struct md_pll_reg *md_pll_base;
	struct tasklet_struct cldma_rxq0_task;

	unsigned int cldma_irq_id;
	unsigned int ap_ccif_irq_id;
	unsigned int md_wdt_irq_id;

	unsigned long cldma_irq_flags;
	unsigned long ap_ccif_irq_flags;
	unsigned long md_wdt_irq_flags;

	struct md_hw_info *hw_info;
};

struct cldma_tgpd {
	u8 gpd_flags;
	u8 gpd_checksum;
	/* Use debug_id low btye to support 6GM address
	*	bit[0..3]: storedata_buff_bd_ptr high bit[35:32]
	*	bit[4..7]:	store next_gpd_ptr high bit[35:32]
	*/
	u16 debug_id;
	u32 next_gpd_ptr;
	u32 data_buff_bd_ptr;
	u16 data_buff_len;
	u8 desc_ext_len;
	u8 non_used;		/* debug:1 for Tx in; 2 for Tx done */
} __packed;

struct cldma_rgpd {
	u8 gpd_flags;
	u8 gpd_checksum;
	u16 data_allow_len;
	u32 next_gpd_ptr;
	u32 data_buff_bd_ptr;
	u16 data_buff_len;
	/* Use debug_id low btye to support 6GM address
	*	bit[0..3]: storedata_buff_bd_ptr high bit[35:32]
	*	bit[4..7]:	store next_gpd_ptr high bit[35:32]
	*/
	u16 debug_id;
} __packed;

struct cldma_tbd {
	u8 bd_flags;
	u8 bd_checksum;
	/* Use reserved low btye to support 6GM address
	*	bit[0..3]: data_buff_ptr high bit[35:32]
	*	bit[4..7]:	store next_bd_ptr high bit[35:32]
	*/
	u16 reserved;
	u32 next_bd_ptr;
	u32 data_buff_ptr;
	u16 data_buff_len;
	u8 desc_ext_len;
	u8 non_used;
} __packed;

struct cldma_rbd {
	u8 bd_flags;
	u8 bd_checksum;
	u16 data_allow_len;
	u32 next_bd_ptr;
	u32 data_buff_ptr;
	u16 data_buff_len;
	/* Use reserved low btye to support 6GM address
	*	bit[0..3]: data_buff_ptr high bit[35:32]
	*	bit[4..7]:	store next_bd_ptr high bit[35:32]
	*/
	u16 reserved;
} __packed;

struct cldma_rspd {
	u8 spd_flags;
	u8 spd_checksum;
	u16 data_allow_len;
	u32 next_spd_ptr;
	u32 data_buff_ptr;
	u16 data_buff_len;
	/* Use reserved low btye to support 6GM address
	*	bit[0..3]: data_buff_ptr high bit[35:32]
	*	bit[4..7]: store next_bd_ptr high bit[35:32]
	*/
	u16 reserved;
} __packed;

typedef enum {
	ONCE_MORE,
	ALL_CLEAR,
} RX_COLLECT_RESULT;

enum {
	CCCI_TRACE_TX_IRQ = 0,
	CCCI_TRACE_RX_IRQ = 1,
};

static inline void md_cd_queue_struct_init(struct md_cd_queue *queue, struct ccci_modem *md,
					   DIRECTION dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->modem = md;
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->ring_lock);
	queue->debug_id = 0;
	queue->busy_count = 0;
#ifdef ENABLE_FAST_HEADER
	queue->fast_hdr.gpd_count = 0;
#endif
}
extern void mt_irq_dump_status(int irq);
extern unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);

extern u32 mt_irq_get_pending(unsigned int irq);
/* used for throttling feature - start */
extern unsigned long ccci_modem_boot_count[];
/* used for throttling feature - end */

#define GF_PORT_LIST_MAX 128
extern int gf_port_list_reg[GF_PORT_LIST_MAX];
extern int gf_port_list_unreg[GF_PORT_LIST_MAX];
extern int ccci_ipc_set_garbage_filter(struct ccci_modem *md, int reg);
extern bool spm_is_md1_sleep(void);
extern void spm_ap_mdsrc_req(u8 lock);
#endif				/* __MODEM_CD_H__ */
