/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_CCIF_H__
#define __MODEM_CCIF_H__

#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/atomic.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_ringbuf.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_hif_internal.h"

#if (MD_GENERATION >= 6295)
#define QUEUE_NUM   16
#else
#define QUEUE_NUM   8
#endif

/* speciall for user: ccci_fsd data[0] */
#define CCCI_FS_AP_CCCI_WAKEUP (0x40000000)
#define CCCI_FS_REQ_SEND_AGAIN 0x80000000

/*#define FLOW_CTRL_ENABLE*/
#define FLOW_CTRL_HEAD		0x464C4F57	/*FLOW*/
#define FLOW_CTRL_TAIL		0x4354524C	/*CTRL*/
#define FLOW_CTRL_THRESHOLD	3

#define CCIF_TRAFFIC_MONITOR_INTERVAL 10

#define RX_BUGDET 16
#define NET_RX_QUEUE_MASK 0x4

struct ccif_flow_control {
	unsigned int head_magic;
	unsigned int ap_busy_queue;
	unsigned int md_busy_queue;
	unsigned int tail_magic;
};

struct  ccci_hif_ccif_val {
	struct regmap *infra_ao_base;
	unsigned int md_gen;
	unsigned long offset_epof_md1;
	void __iomem *md_plat_info;
};

struct ccif_sram_layout {
	struct ccci_header dl_header;
	struct md_query_ap_feature md_rt_data;
	struct ccci_header up_header;
	struct ap_query_md_feature ap_rt_data;
};

enum ringbuf_id {
	RB_NORMAL,
	RB_EXP,
	RB_MAX,
};

struct md_ccif_queue {
	enum DIRECTION dir;
	unsigned char index;
	unsigned char hif_id;
	unsigned char resume_cnt;
	unsigned char debug_id;
	unsigned char wakeup;
	atomic_t rx_on_going;
	int budget;
	unsigned int ccif_ch;
	struct ccci_modem *modem;
	struct ccci_port *napi_port;
	struct ccci_ringbuf *ringbuf;		/*ringbuf in use*/
	struct ccci_ringbuf *ringbuf_bak[RB_MAX];
	spinlock_t rx_lock;	/* lock for the counter, only for rx */
	spinlock_t tx_lock;	/* lock for the counter, only for Tx */
	wait_queue_head_t req_wq;	/* only for Tx */
	struct work_struct qwork;
	struct workqueue_struct *worker;
};

enum hifccif_state {
	HIFCCIF_STATE_MIN  = 0,
	HIFCCIF_STATE_PWROFF,
	HIFCCIF_STATE_PWRON,
	HIFCCIF_STATE_EXCEPTION,
	HIFCCIF_STATE_MAX,
};
struct md_ccif_ctrl {
	enum hifccif_state ccif_state;
	struct md_ccif_queue txq[QUEUE_NUM];
	struct md_ccif_queue rxq[QUEUE_NUM];
	unsigned int total_smem_size;
	atomic_t reset_on_going;

	unsigned long channel_id;	/* CCIF channel */
	unsigned int sram_size;
	struct ccif_sram_layout *ccif_sram_layout;
	struct work_struct ccif_sram_work;
	struct timer_list bus_timeout_timer;
	void __iomem *ccif_ap_base;
	void __iomem *ccif_md_base;
	void __iomem *md_pcore_pccif_base;
	void __iomem *md_ccif4_base;
	void __iomem *md_ccif5_base;
	unsigned int ap_ccif_irq0_id;
	unsigned int ap_ccif_irq1_id;
	unsigned long ap_ccif_irq0_flags;
	unsigned long ap_ccif_irq1_flags;
	atomic_t ccif_irq_enabled;
	atomic_t ccif_irq1_enabled;
	unsigned long wakeup_ch;
	atomic_t wakeup_src;
	unsigned int wakeup_count;

	struct work_struct wdt_work;
	struct ccif_flow_control *flow_ctrl;

	unsigned char md_id;
	unsigned char hif_id;
	struct ccci_hif_traffic traffic_info;
	struct timer_list traffic_monitor;

	unsigned short heart_beat_counter;
	struct ccci_hif_ops *ops;
	struct platform_device *plat_dev;
	struct ccci_hif_ccif_val plat_val;
	unsigned long long isr_cnt[CCIF_CH_NUM];
};

static inline void ccif_set_busy_queue(struct md_ccif_ctrl *md_ctrl,
	unsigned int qno)
{
	if (!md_ctrl->flow_ctrl)
		return;
	/* set busy bit */
	md_ctrl->flow_ctrl->ap_busy_queue |= (0x1 << qno);
}

static inline void ccif_clear_busy_queue(struct md_ccif_ctrl *md_ctrl,
	unsigned int qno)
{
	if (!md_ctrl->flow_ctrl)
		return;
	/* clear busy bit */
	md_ctrl->flow_ctrl->ap_busy_queue &= ~(0x1 << qno);
}

static inline void ccif_reset_busy_queue(struct md_ccif_ctrl *md_ctrl)
{
#ifdef	FLOW_CTRL_ENABLE
	if (!md_ctrl->flow_ctrl)
		return;
	/* reset busy bit */
	md_ctrl->flow_ctrl->head_magic = FLOW_CTRL_HEAD;
	md_ctrl->flow_ctrl->ap_busy_queue = 0x0;
	md_ctrl->flow_ctrl->md_busy_queue = 0x0;
	/* Notice: tail will be set by modem
	 * if it supports flow control
	 */
#endif
}

static inline int ccif_is_md_queue_busy(struct md_ccif_ctrl *md_ctrl,
	unsigned int qno)
{
	/*caller should handle error*/
	if (!md_ctrl->flow_ctrl)
		return -1;
	if (unlikely(md_ctrl->flow_ctrl->head_magic != FLOW_CTRL_HEAD ||
			md_ctrl->flow_ctrl->tail_magic != FLOW_CTRL_TAIL))
		return -1;

	return (md_ctrl->flow_ctrl->md_busy_queue & (0x1 << qno));
}

static inline int ccif_is_md_flow_ctrl_supported(struct md_ccif_ctrl *md_ctrl)
{
	if (!md_ctrl->flow_ctrl)
		return -1;
	/*both head and tail are right. make sure MD support flow control too*/
	if (likely(md_ctrl->flow_ctrl->head_magic == FLOW_CTRL_HEAD &&
			md_ctrl->flow_ctrl->tail_magic == FLOW_CTRL_TAIL))
		return 1;
	else
		return 0;
}

static inline void ccif_wake_up_tx_queue(struct md_ccif_ctrl *md_ctrl,
	unsigned int qno)
{
	struct md_ccif_queue *queue = &md_ctrl->txq[qno];

	ccif_clear_busy_queue(md_ctrl, qno);
	queue->wakeup = 1;
	wake_up(&queue->req_wq);
}

static inline int ccci_ccif_hif_send_skb(unsigned char hif_id, int tx_qno,
	struct sk_buff *skb, int from_pool, int blocking)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->send_skb(hif_id, tx_qno,
			skb, from_pool, blocking);
	else
		return -1;
}
static inline int ccci_ccif_hif_write_room(unsigned char hif_id,
	unsigned char qno)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->write_room(hif_id, qno);
	else
		return -1;

}
static inline int ccci_ccif_hif_give_more(unsigned char hif_id, int rx_qno)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->give_more(hif_id, rx_qno);
	else
		return -1;
}
static inline int ccci_ccif_hif_dump_status(unsigned int hif_id,
	enum MODEM_DUMP_FLAG dump_flag, void *buff, int length)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (md_ctrl)
		return md_ctrl->ops->dump_status(hif_id, dump_flag, buff,
			length);
	else
		return -1;

}

static inline int ccci_ccif_hif_set_wakeup_src(unsigned char hif_id, int value)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	unsigned int ccif_rx_ch = 0;

	if (md_ctrl) {

		if (md_ctrl->ccif_ap_base)
			ccif_rx_ch = ccif_read32(md_ctrl->ccif_ap_base,
					APCCIF_RCHNUM);
		CCCI_NORMAL_LOG(0, "WK", "CCIF RX bitmap:0x%x\r\n",
				ccif_rx_ch);
#if (MD_GENERATION >= 6297)
		if (ccif_rx_ch & AP_MD_CCB_WAKEUP)
			mtk_ccci_ccb_info_peek();
#endif
		return atomic_set(&md_ctrl->wakeup_src, value);
	} else
		return -1;
}

#ifdef CCCI_KMODULE_ENABLE

#define ccci_write32(b, a, v)  \
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write16(b, a, v)  \
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write8(b, a, v)  \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))
#endif


void md_ccif_reset_queue(unsigned char hif_id, unsigned char for_start);

void ccif_polling_ready(unsigned char hif_id, int step);

void md_ccif_sram_reset(unsigned char hif_id);
int md_ccif_ring_buf_init(unsigned char hif_id);
void md_ccif_switch_ringbuf(unsigned char hif_id, enum ringbuf_id rb_id);
void ccci_reset_ccif_hw(unsigned char md_id,
			int ccif_id, void __iomem *baseA,
			void __iomem *baseB,
			struct md_ccif_ctrl *md_ctrl);

/* always keep this in mind:
 * what if there are more than 1 modems using CLDMA...
 */
extern struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np,
	const char *property);
extern int regmap_write(struct regmap *map, unsigned int reg, unsigned int val);
extern int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
/* used for throttling feature - start */
extern unsigned long ccci_modem_boot_count[];
extern int md_fsm_exp_info(int md_id, unsigned int channel_id);
extern void mt_irq_dump_status(int irq);
/* used for throttling feature - end */
#endif				/* __MODEM_CCIF_H__ */
