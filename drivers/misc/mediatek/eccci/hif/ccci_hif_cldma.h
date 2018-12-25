/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MODEM_CD_H__
#define __MODEM_CD_H__

#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <mt-plat/mtk_ccci_common.h>

#include "ccci_config.h"
#include "ccci_bm.h"
#include "ccci_hif_internal.h"
/*
 * hardcode, max queue number should be synced with port array in port_cfg.c
 * and macros in ccci_core.h following number should sync with MAX_TXQ/RXQ_NUM
 * in ccci_core.h and bitmask in modem_cldma.c
 */
#if MD_GENERATION >= (6293)
#define CLDMA_TXQ_NUM 4
#define CLDMA_RXQ_NUM 1
#define NET_TXQ_NUM 4
#define NET_RXQ_NUM 1
#define NORMAL_TXQ_NUM 0
#define NORMAL_RXQ_NUM 0
#else
#define CLDMA_TXQ_NUM 8
#define CLDMA_RXQ_NUM 8
#define NET_TXQ_NUM 4
#define NET_RXQ_NUM 4
#define NORMAL_TXQ_NUM 5
#define NORMAL_RXQ_NUM 5
#endif

#define MAX_BD_NUM (MAX_SKB_FRAGS + 1)
#define TRAFFIC_MONITOR_INTERVAL 10	/* seconds */
#define SKB_RX_QUEUE_MAX_LEN 200000
#define CLDMA_ACTIVE_T 20

#define CLDMA_AP_MTU_SIZE	(NET_RX_BUF)	/*sync with runtime data*/

/*
 * CLDMA feature options:
 * CLDMA_NO_TX_IRQ: mask all TX interrupts, collect TX_DONE skb
 * when get Rx interrupt or Tx busy.
 * ENABLE_CLDMA_TIMER: use a timer to detect TX packet sent or not.
 * not usable if TX interrupts are masked.
 * CLDMA_NET_TX_BD: use BD to support scatter/gather IO for net device
 */
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
	/* bit7: override or not; bit0: IOC setting */
	unsigned char ioc_override;
};

typedef enum {
	RING_GPD = 0,
	RING_GPD_BD = 1,
	RING_SPD = 2,
} CLDMA_RING_TYPE;

struct md_cd_queue;

/*
 * In a ideal design, all read/write pointers should be member of cldma_ring,
 * and they will complete a ring buffer object with buffer itself
 * and Tx/Rx funcitions. but this will change too much of the original
 * code and we have to drop it. so here the cldma_ring is quite light and
 * most of ring buffer opertions are still in queue struct.
 */
struct cldma_ring {
	struct list_head gpd_ring;	/* ring of struct cldma_request */
	int length;		/* number of struct cldma_request */
	int pkt_size;		/* size of each packet in ring */
	CLDMA_RING_TYPE type;

	int (*handle_tx_request)(struct md_cd_queue *queue,
			struct cldma_request *req,
			struct sk_buff *skb,
			unsigned int ioc_override);
	int (*handle_rx_done)(struct md_cd_queue *queue,
		int budget, int blocking);
	int (*handle_tx_done)(struct md_cd_queue *queue,
		int budget, int blocking);
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

static inline struct cldma_request *cldma_ring_step_forward(
	struct cldma_ring *ring, struct cldma_request *req)
{
	struct cldma_request *next_req;

	if (req->entry.next == &ring->gpd_ring)
		next_req = list_first_entry(&ring->gpd_ring,
			struct cldma_request, entry);
	else
		next_req = list_entry(req->entry.next,
			struct cldma_request, entry);
	return next_req;
}

static inline struct cldma_request *cldma_ring_step_backward(
	struct cldma_ring *ring, struct cldma_request *req)
{
	struct cldma_request *prev_req;

	if (req->entry.prev == &ring->gpd_ring)
		prev_req = list_last_entry(&ring->gpd_ring,
			struct cldma_request, entry);
	else
		prev_req = list_entry(req->entry.prev,
			struct cldma_request, entry);
	return prev_req;
}

struct md_cd_queue {
	unsigned char index;
	struct ccci_modem *modem;
	/*
	 * what we have here is not a typical ring buffer,
	 * as we have three players:
	 * for Tx: sender thread -> CLDMA -> tx_done thread
	 * -sender thread: set HWO bit, req->skb and gpd->buffer,
	 * when req->skb==NULL
	 * -tx_done thread: free skb only when req->skb!=NULL && HWO==0
	 * -CLDMA: send skb only when gpd->buffer!=NULL && HWO==1
	 * for Rx:  refill thread -> CLDMA -> rx_done thread
	 * -refill thread: set HWO bit, req->skb and gpd->buffer,
	 * when req->skb==NULL
	 * -rx_done thread: free skb only when req->skb!=NULL && HWO==0
	 * -CLDMA: send skb only when gpd->buffer!=NULL && HWO==1
	 *
	 * for Tx, although only sender thread is "writer"--who sets HWO bit,
	 * both tx_done thread and CLDMA
	 * only read this bit. BUT, other than HWO bit,
	 * sender thread also shares req->skb with tx_done thread,
	 * and gpd->buffer with CLDMA. so it must set HWO bit after set
	 * gpd->buffer and before set req->skb.
	 *
	 * for Rx, only refill thread is "writer"--who sets HWO bit,
	 * both rx_done thread and CLDMA only read this
	 * bit. other than HWO bit, refill thread also shares
	 * req->skb with rx_done thread and gpd->buffer with
	 * CLDMA. it also needs set HWO bit after set gpd->buffer
	 * and before set req->skb.
	 *
	 * so in a ideal world, we use HWO bit as an barrier,
	 * and this let us be able to avoid using lock.
	 * although, there are multiple sender threads on top of
	 * each Tx queue, they must be separated.
	 * therefore, we still have Tx lock.
	 *
	 * BUT, check this sequence: sender or refiller has set HWO=1,
	 * but doesn't set req->skb yet. CLDMA finishes
	 * this GPD and Tx_DONE or Rx_DONE will see HWO==0 but
	 * req->skb==NULL. so this skb will not be handled.
	 * therefore, as a conclusion, use lock!!!
	 *
	 * be aware, fot Tx this lock also protects TX_IRQ,
	 * TX_FULL, budget, sequence number usage.
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
	unsigned char hif_id;
	DIRECTION dir;
	unsigned int busy_count;
};

#define QUEUE_LEN(a) (sizeof(a)/sizeof(struct md_cd_queue))

struct md_cd_ctrl {
	struct md_cd_queue txq[CLDMA_TXQ_NUM];
	struct md_cd_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	unsigned short txq_started;
	atomic_t cldma_irq_enabled;

	spinlock_t cldma_timeout_lock;
	/* this lock is using to protect CLDMA,
	 * not only for timeout checking
	 */
	struct work_struct cldma_irq_work;
	struct workqueue_struct *cldma_irq_worker;
	unsigned char md_id;
	unsigned char hif_id;
	struct ccci_hif_traffic traffic_info;
	atomic_t wakeup_src;

#if TRAFFIC_MONITOR_INTERVAL
	unsigned int tx_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned int rx_traffic_monitor[CLDMA_RXQ_NUM];
	unsigned int tx_pre_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned long long tx_done_last_start_time[CLDMA_TXQ_NUM];
	unsigned int tx_done_last_count[CLDMA_TXQ_NUM];

	struct timer_list traffic_monitor;
	unsigned long traffic_stamp;
#endif
	unsigned int tx_busy_warn_cnt;

	/* here we assume T/R GPD/BD/SPD have the same size  */
	struct dma_pool *gpd_dmapool;
	struct cldma_ring net_tx_ring[NET_TXQ_NUM];
	struct cldma_ring net_rx_ring[NET_RXQ_NUM];
	struct cldma_ring normal_tx_ring[NORMAL_TXQ_NUM];
	struct cldma_ring normal_rx_ring[NORMAL_RXQ_NUM];

	void __iomem *cldma_ap_ao_base;
	void __iomem *cldma_md_ao_base;
	void __iomem *cldma_ap_pdn_base;
	void __iomem *cldma_md_pdn_base;

	struct tasklet_struct cldma_rxq0_task;

	unsigned int cldma_irq_id;

	unsigned long cldma_irq_flags;
	struct ccci_hif_ops *ops;
};

struct cldma_tgpd {
	u8 gpd_flags;
	/* original checksum bits, now for debug:
	 * 1 for Tx in;
	 * 2 for Tx done
	 */
	u8 non_used;
	union {
		u8 dbbdp:4; /* data_buff_bd_ptr high bit[35:32] */
		u8 ngpdp:4; /* next_gpd_ptr high bit[35:32] */
		u8 msb_byte;
	} msb;
	u8 netif; /* net interface id, 5bits */
	u32 next_gpd_ptr;
	u32 data_buff_bd_ptr;
	u16 data_buff_len;
	u16 psn; /* packet sequence number */
} __packed;

struct cldma_rgpd {
	u8 gpd_flags;
	u8 non_used; /* original checksum bits */
	u16 data_allow_len;
	u32 next_gpd_ptr;
	u32 data_buff_bd_ptr;
	u16 data_buff_len;
	union {
		u8 dbbdp:4; /* data_buff_bd_ptr high bit[35:32] */
		u8 ngpdp:4; /* next_gpd_ptr high bit[35:32] */
		u8 msb_byte;
	} msb;
	u8 non_used2;
} __packed;

struct cldma_tbd {
	u8 bd_flags;
	u8 non_used; /* original checksum bits */
	union {
		u8 dbbdp:4; /* data_buff_bd_ptr high bit[35:32] */
		u8 ngpdp:4; /* next_gpd_ptr high bit[35:32] */
		u8 msb_byte;
	} msb;
	u8 non_used2;
	u32 next_bd_ptr;
	u32 data_buff_ptr;
	u16 data_buff_len;
	u16 non_used3;
} __packed;

struct cldma_rbd {
	u8 bd_flags;
	u8 non_used; /* original checksum bits */
	u16 data_allow_len;
	u32 next_bd_ptr;
	u32 data_buff_ptr;
	u16 data_buff_len;
	union {
		u8 dbbdp:4; /* data_buff_bd_ptr high bit[35:32] */
		u8 ngpdp:4; /* next_gpd_ptr high bit[35:32] */
		u8 msb_byte;
	} msb;
	u8 non_used2;
} __packed;

typedef enum {
	ONCE_MORE,
	ALL_CLEAR,
	LOW_MEMORY,
} RX_COLLECT_RESULT;

enum {
	CCCI_TRACE_TX_IRQ = 0,
	CCCI_TRACE_RX_IRQ = 1,
};

static inline void md_cd_queue_struct_init(struct md_cd_queue *queue,
	unsigned char hif_id, DIRECTION dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->ring_lock);
	queue->busy_count = 0;
#ifdef ENABLE_FAST_HEADER
	queue->fast_hdr.gpd_count = 0;
#endif
}

int ccci_cldma_hif_init(unsigned char hif_id, unsigned char md_id);

static inline int ccci_cldma_hif_send_skb(unsigned char hif_id, int tx_qno,
	struct sk_buff *skb, int from_pool, int blocking)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->send_skb(hif_id, tx_qno, skb,
			from_pool, blocking);
	else
		return -1;
}

static inline int ccci_cldma_hif_write_room(unsigned char hif_id,
	unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->write_room(hif_id, qno);
	else
		return -1;

}
static inline int ccci_cldma_hif_give_more(unsigned char hif_id, int rx_qno)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->give_more(hif_id, rx_qno);
	else
		return -1;

}

static inline int ccci_cldma_hif_dump_status(unsigned char hif_id,
	MODEM_DUMP_FLAG dump_flag, int length)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->dump_status(hif_id, dump_flag, length);
	else
		return -1;

}

static inline int ccci_cldma_hif_set_wakeup_src(unsigned char hif_id,
	int value)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return atomic_set(&md_ctrl->wakeup_src, value);
	else
		return -1;

}

/*API for modem sys1*/
void cldma_start(unsigned char hif_id);
void cldma_stop(unsigned char hif_id);
void cldma_stop_for_ee(unsigned char hif_id);
void md_cldma_clear(unsigned char hif_id);
void cldma_reset(unsigned char hif_id);
int md_cd_late_init(unsigned char hif_id);
void md_cd_clear_all_queue(unsigned char hif_id, DIRECTION dir);
void md_cd_ccif_allQreset_work(unsigned char hif_id);

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
