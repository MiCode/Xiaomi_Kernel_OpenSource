/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/timer.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>
#include <linux/pci.h>
#include <linux/pcie-rc-sprd.h>
#include "edma_engine.h"
#include "mchn.h"
#include "pcie_dbg.h"
#include "pcie.h"
#include "sprd_wcn.h"
#include <linux/time.h>

#define TX 1
#define RX 0
#define MPOOL_SIZE	0x10000
#define MAX_PRINT_BYTE_NUM 8
#define EDMA_TX_TIMER_INTERVAL_MS	1000

#define KTIME_MAX			((s64)~((u64)1 << 63))
#define KTIME_MIN			(-KTIME_MAX - 1)
#define KTIME_SEC_MAX			(KTIME_MAX / NSEC_PER_SEC)
#define KTIME_SEC_MIN			(KTIME_MIN / NSEC_PER_SEC)

static int hisrfunc_debug;
static int hisrfunc_line;
static int hisrfunc_last_msg;
static struct edma_info g_edma = { 0 };

static unsigned char *mpool_buffer;
static struct dma_buf mpool_dm = {0};

static inline s64 timespec_to_ns_64(const struct timespec64 *ts)
{
	/* Prevent multiplication overflow / underflow */
	if (ts->tv_sec >= KTIME_SEC_MAX)
		return KTIME_MAX;

	if (ts->tv_sec <= KTIME_SEC_MIN)
		return KTIME_MIN;

	return ((s64) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

void edma_print_mbuf_data(int channel, struct mbuf_t *head,
			  struct mbuf_t *tail, const char *func)
{
	unsigned short print_len;
	char print_str[64];

	/* only print bt tx/rx buffer for debug */
	if ((channel != 1) && (channel != 2))
		return;

	if (!head) {
		WARN_ON(1);
		return;
	}
	print_len = head->len;
	sprintf(print_str, "WCN PCIE: %s bt:  ", func);
	print_hex_dump(KERN_INFO, print_str, DUMP_PREFIX_NONE,
		16, 1, head->buf, (print_len < MAX_PRINT_BYTE_NUM ?
		print_len : MAX_PRINT_BYTE_NUM), true);
}

void dump_dscr_reg(struct desc *dscr)
{
	WCN_INFO("%08x  %08x  %08x  %08x  %08x  %08x\n",
		 dscr->chn_trans_len.reg, dscr->chn_ptr_high.reg,
		 dscr->rf_chn_tx_next_dscr_ptr_low,
		 dscr->rf_chn_rx_next_dscr_ptr_low,
		 dscr->rf_chn_data_src_addr_low,
		 dscr->rf_chn_data_dst_addr_low);
}

int time_sub_us(struct timespec64 *start, struct timespec64 *end)
{
	return (timespec_to_ns_64(end) - timespec_to_ns_64(start))/1000;
	/*return (end->tv_sec - start->tv_sec)*1000000 +
		(end->tv_usec - start->tv_usec);*/
}

void *mpool_vir_to_phy(void *p)
{
	unsigned long offset;

	offset = (unsigned long)p - mpool_dm.vir;
	return (void *)(mpool_dm.phy + offset);
}

void *mpool_phy_to_vir(void *p)
{
	unsigned long offset;

	offset = (unsigned long)p - mpool_dm.phy;
	return (void *)(mpool_dm.vir + offset);
}

void *mpool_malloc(int len)
{
	int ret;
	unsigned char *p;
	static int total_len;
	struct edma_info *edma = edma_info();

	mutex_lock(&edma->mpool_lock);
	if (mpool_buffer == NULL) {
		ret = dmalloc(edma->pcie_info, &mpool_dm, MPOOL_SIZE);
		if (ret != 0) {
			WCN_ERR("%s dmalloc fail\n", __func__);
			mutex_unlock(&edma->mpool_lock);
			return NULL;
		}
		/* reset total length */
		total_len = 0;
		mpool_buffer = (unsigned char *)(mpool_dm.vir);
		WCN_INFO("%s {0x%lx,0x%lx} -- {0x%lx,0x%lx}\n",
			 __func__, mpool_dm.vir, mpool_dm.phy,
			 mpool_dm.vir + 0x10000,
			 mpool_dm.phy + 0x10000);
	}
	if (len <= 0) {
		mutex_unlock(&edma->mpool_lock);
		return NULL;
	}
	p = mpool_buffer;
	memset(p, 0x56, len);
	mpool_buffer += len;
	total_len += len;
	if (total_len > MPOOL_SIZE) {
		WCN_ERR("mpool used done!size=0x%x\n", total_len);
		mutex_unlock(&edma->mpool_lock);
		return NULL;
	}
	WCN_DBG("%s(0x%x) totle:0x%x= {0x%p, 0x%p}\n", __func__, len, total_len,
		 p, mpool_vir_to_phy((void *)p));

	mutex_unlock(&edma->mpool_lock);

	return p;
}

int mpool_free(void)
{
	struct edma_info *edma = edma_info();

	if ((mpool_dm.vir != 0) && (mpool_dm.phy != 0))
		dmfree(edma->pcie_info, &mpool_dm);
	mpool_buffer = NULL;

	return 0;
}

static int create_wcnevent(struct event_t *event, int id)
{
	WCN_DBG("create event(0x%p)[+]\n", event);
	memset((unsigned char *)event, 0x00, sizeof(struct event_t));
	sema_init(&(event->wait_sem), 0);

	return 0;
}

static int wait_wcnevent(struct event_t *event, int timeout)
{
	if (timeout < 0) {
		int dt;
		struct timespec64 time;

		ktime_get_real_ts64(&time);
		if (event->wait_sem.count == 0) {
			if (event->flag == 0) {
				event->flag = 1;
				event->time = time;
				return 0;
			}
			dt = time_sub_us(&(event->time), &time);
			if (dt < 200)
				return 0;
			event->flag = 0;
		} else
			event->flag = 0;
		down(&event->wait_sem);
		return 0;
	}
	return down_timeout(&(event->wait_sem), msecs_to_jiffies(timeout));
}

static int set_wcnevent(struct event_t *event)
{
	unsigned long flags;
	struct edma_info *edma = edma_info();

	spin_lock_irqsave(&edma->tasklet_lock, flags);
	if (event->tasklet != NULL)
		tasklet_schedule(event->tasklet);
	else
		up(&(event->wait_sem));
	spin_unlock_irqrestore(&edma->tasklet_lock, flags);

	return 0;
}

static int edma_spin_lock_init(struct irq_lock_t *lock)
{
	lock->irq_spinlock_p = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	lock->flag = 0;
	spin_lock_init(lock->irq_spinlock_p);

	return 0;
}

void *pcie_alloc_memory(int len)
{
	int ret;
	unsigned char *p;
	struct edma_info *edma = edma_info();

	if (mpool_buffer == NULL) {
		ret = dmalloc(edma->pcie_info, &mpool_dm, 0x8000);
		if (ret != 0)
			return NULL;
		mpool_buffer = (unsigned char *)(mpool_dm.vir);
		WCN_INFO("%s {0x%lx,0x%lx} -- {0x%lx,0x%lx}\n",
			 __func__, mpool_dm.vir, mpool_dm.phy,
			 mpool_dm.vir + 0x20000,
			 mpool_dm.phy + 0x20000);
	}
	if (len <= 0)
		return NULL;
	p = mpool_buffer;
	memset(p, 0x56, len);
	mpool_buffer += len;
	WCN_INFO("%s(%d) = {0x%p, 0x%p}\n", __func__, len, p,
		 mpool_vir_to_phy((void *)p));

	return p;
}

struct edma_info *edma_info(void)
{
	return &g_edma;
}

static int create_queue(struct msg_q *q, int size, int num)
{
	int ret;

	WCN_DBG("[+]%s(0x%p, %d, %d)\n", __func__,
		 (void *)virt_to_phys((void *)(q)), size, num);
	q->mem = kmalloc(size * num, GFP_KERNEL);
	if (q->mem == NULL) {
		WCN_INFO("%s malloc err\n", __func__);
		return ERROR;
	}

	ret = edma_spin_lock_init(&(q->lock));
	if (ret) {
		WCN_INFO("%s spin_lock_init err\n", __func__);
		return ERROR;
	}
	ret = create_wcnevent(&(q->event), 0);
	if (ret != 0) {
		WCN_INFO("%s event_create err\n", __func__);
		return ERROR;
	}
	q->wt = 0;
	q->rd = 0;
	q->max = num;
	q->size = size;
	WCN_DBG("[-]%s\n", __func__);

	return OK;
}

int delete_queue(struct msg_q *q)
{
	WCN_INFO("[+]%s\n", __func__);
	kfree(q->mem);
	memset((unsigned char *)(q), 0x00, sizeof(struct msg_q));
	WCN_INFO("[-]%s\n", __func__);

	return OK;
}

static int dequeue(struct msg_q *q, unsigned char *msg, int timeout)
{
	if (q->wt == q->rd)
		return ERROR;

	/* spin_lock_irqsave */
	spin_lock_irqsave(q->lock.irq_spinlock_p, q->lock.flag);
	memcpy(msg, q->mem + (q->size) * (q->rd), q->size);
	q->rd = INCR_RING_BUFF_INDX(q->rd, q->max);
	/* spin_unlock_irqrestore */
	spin_unlock_irqrestore(q->lock.irq_spinlock_p, q->lock.flag);

	return OK;
}

static int enqueue(struct msg_q *q, unsigned char *msg)
{
	if ((q->wt + 1) % (q->max) == q->rd) {
		WCN_INFO("%s full\n", __func__);
		return ERROR;
	}
	q->seq++;
	*((unsigned int *)msg) = q->seq;
	memcpy((q->mem + (q->size) * (q->wt)), msg, q->size);
	q->wt = INCR_RING_BUFF_INDX(q->wt, q->max);

	return OK;
}

/*
 * (a) "volatile" on kernel data is basically always a bug, and you should
 *  use locking. "volatile" doesn't help anything at all with memory
 *  ordering and friends, so it's insane to think it "solves" anything on
 *  its own.
 * (b) on "iomem" pointers it does make sense, but those need special
 *  accessor functions _anyway_, so things like test_bit() wouldn't work
 *  on them.
 * (c)if you spin on a value [that's] changing, you should use "cpu_relax()" or
 *  "barrier()" anyway, which will force gcc to re-load any values from
 *  memory over the loop.
 */
static int dscr_polling(struct desc *dscr, int loop)
{
	do {
		if (dscr->chn_trans_len.bit.rf_chn_done)
			return 0;
		/*
		 * Make sure to reread dscr->chn_trans_len.bit.rf_chn_done each
		 * time. or use cpu_relax()
		 */
		barrier();
	} while (loop--);

	return -1;
}

static int edma_hw_next_dscr(int chn, int inout, struct desc **next)
{
	int i;
	unsigned int ptr_l[2];
	struct desc *hw_next = NULL;
	union dma_dscr_ptr_high_reg local_chn_ptr_high;
	struct edma_info *edma = edma_info();

	if (inout == TX) {
		local_chn_ptr_high.reg =
		    edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg;
		SET_8_OF_40(hw_next,
			(local_chn_ptr_high.bit.rf_chn_tx_next_dscr_ptr_high &
			    (~(1 << 7))));
		SET_32_OF_40(hw_next,
		edma->dma_chn_reg[chn].dma_dscr.rf_chn_tx_next_dscr_ptr_low);
	} else {
		for (i = 0; i < 5; i++) {
			local_chn_ptr_high.reg =
			    edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg;
			SET_8_OF_40(hw_next,
			 (local_chn_ptr_high.bit.rf_chn_rx_next_dscr_ptr_high &
				     (~(1 << 7))));
			if (local_chn_ptr_high.bit
			    .rf_chn_rx_next_dscr_ptr_high) {
			}

			ptr_l[0] = edma->dma_chn_reg[chn]
				   .dma_dscr.rf_chn_rx_next_dscr_ptr_low;
			if ((ptr_l[0] == 0) || (ptr_l[0] == 0xFFFFFFFF)) {
				udelay(1);
				ptr_l[1] =
				    edma->dma_chn_reg[chn].dma_dscr
						.rf_chn_rx_next_dscr_ptr_low;
				WCN_INFO(
				    "%s(%d,%d) err hw_next:0x%p, 0x%x, 0x%x\n",
					__func__, chn, inout, hw_next,
					ptr_l[0], ptr_l[1]);
			} else {
				SET_32_OF_40(hw_next, ptr_l[0]);
				break;
			}
		}
		if (i == 5) {
			WCN_ERR(
				"%s(%d,%d) timeout hw_next:0x%p, 0x%x, 0x%x\n",
				__func__, chn, inout, hw_next,
				ptr_l[0], ptr_l[1]);
		}

	}
	*next = hw_next;
	if (hw_next == NULL) {
		WCN_ERR("%s(%d, %d) err, hw_next == NULL\n",
			__func__, chn, inout);
	}

	return 0;
}

int edma_sw_link_done_dscr(struct desc *head, struct desc **tail)
{
	struct desc *dscr;

	for (dscr = head;;) {
		if (dscr->chn_trans_len.bit.rf_chn_done == 0)
			break;
		dscr = dscr->next.p;
	}
	*tail = dscr;

	if (dscr == head)
		return ERROR;

	return 0;
}

static int dscr_ring_empty(int chn)
{
	struct desc *dscr;
	struct edma_info *edma = edma_info();

	edma_hw_next_dscr(chn, edma->chn_sw[chn].inout, &dscr);
	dscr = mpool_phy_to_vir(dscr);
	if (dscr == edma->chn_sw[chn].dscr_ring.head)
		return ERROR;
	return OK;
}

static int dscr_zero(struct desc *dscr)
{
	dscr->chn_trans_len.reg = 0;
	dscr->chn_trans_len.bit.rf_chn_tx_intr = 0;
	dscr->chn_trans_len.bit.rf_chn_rx_intr = 0;
	dscr->chn_ptr_high.bit.rf_chn_src_data_addr_high = 0x00;
	dscr->chn_ptr_high.bit.rf_chn_dst_data_addr_high = 0x00;
	dscr->rf_chn_data_src_addr_low = 0x10b000;
	dscr->rf_chn_data_dst_addr_low = 0x10b000;

	return 0;
}

static int dscr_link_mbuf(int inout, struct desc *dscr, struct mbuf_t *mbuf)
{
	if (inout == TX) {
		dscr->rf_chn_data_src_addr_low =
					(unsigned int)(mbuf->phy & 0xFFFFFFFF);
		dscr->chn_ptr_high.bit.rf_chn_src_data_addr_high =
							GET_8_OF_40(mbuf->phy);
		dscr->chn_trans_len.bit.rf_chn_trsc_len =
						(unsigned short)(mbuf->len);
	} else {
		dscr->rf_chn_data_dst_addr_low =
					(unsigned int)(mbuf->phy & 0xFFFFFFFF);
		dscr->chn_ptr_high.bit.rf_chn_dst_data_addr_high =
							GET_8_OF_40(mbuf->phy);
		dscr->chn_trans_len.bit.rf_chn_trsc_len = 0;
	}
	dscr->link.p = mbuf;

	return 0;
}

int dscr_link_cpdu(int inout, struct desc *dscr, struct cpdu_head *cpdu)
{
	unsigned char *buf = (unsigned char *)cpdu + sizeof(struct cpdu_head);
	int len = cpdu->len;

	if (inout == TX) {
		dscr->rf_chn_data_src_addr_low = GET_32_OF_40((buf));
		dscr->chn_ptr_high.bit.rf_chn_src_data_addr_high =
							GET_8_OF_40((buf));
		dscr->chn_trans_len.bit.rf_chn_trsc_len = (unsigned short)(len);
	} else {
		dscr->rf_chn_data_dst_addr_low = GET_32_OF_40((buf));
		dscr->chn_ptr_high.bit.rf_chn_dst_data_addr_high =
							GET_8_OF_40((buf));
		dscr->chn_trans_len.bit.rf_chn_trsc_len = 0;
	}
	dscr->link.p = cpdu;

	return 0;
}

static int edma_pop_link(int chn, struct desc *__head, struct desc *__tail,
		  void **head__, void **tail__, int *node)
{
	struct mbuf_t *mbuf = NULL;
	struct desc *dscr = __head;
	struct edma_info *edma = edma_info();

	if ((__head == NULL) || (__tail == NULL)) {
		WCN_ERR("[+]%s(%d) dscr(0x%p--0x%p)\n", __func__,
			chn, __head, __tail);
	}
	*head__ = *tail__ = NULL;
	(*node) = 0;
	spin_lock_irqsave(edma->chn_sw[chn].dscr_ring.lock.irq_spinlock_p,
			edma->chn_sw[chn].dscr_ring.lock.flag);
	do {
		if (dscr == NULL) {
			WCN_ERR("%s(0x%p, 0x%p) dscr=NULL, error\n",
				__func__, __head, __tail);
			spin_unlock_irqrestore(edma->chn_sw[chn].dscr_ring.lock
					       .irq_spinlock_p,
					       edma->chn_sw[chn].dscr_ring.lock
					       .flag);
			return -1;
		}
		if (dscr_polling(dscr, 500000)) {
			WCN_ERR("%s(%d, 0x%p, 0x%p, 0x%p, free=%d) not done\n",
				__func__, chn, __head, __tail, dscr,
				edma->chn_sw[chn].dscr_ring.free);
			dump_dscr_reg(dscr);
			edma_dump_chn_reg(chn);
			spin_unlock_irqrestore(edma->chn_sw[chn].dscr_ring.lock
					       .irq_spinlock_p,
					       edma->chn_sw[chn].dscr_ring.lock
					       .flag);
			return -1;
		}

		if (*head__) {
			mbuf->next = dscr->link.p;
			mbuf = mbuf->next;
		} else {
			mbuf = *head__ = dscr->link.p;
		}

		if (!mbuf) {
			WCN_ERR("%s line:%d err\n", __func__, __LINE__);
			spin_unlock_irqrestore(edma->chn_sw[chn].dscr_ring.lock
					       .irq_spinlock_p,
					       edma->chn_sw[chn].dscr_ring.lock
					       .flag);

			return -1;
		}
		mbuf->len = dscr->chn_trans_len.bit.rf_chn_trsc_len;

		(*node)++;
		edma->chn_sw[chn].dscr_ring.head =
		    edma->chn_sw[chn].dscr_ring.head->next.p;
		edma->chn_sw[chn].dscr_ring.free++;
		dscr_zero(dscr);
		if (COMPARE_40_BIT(dscr->next.p, __tail))
			break;
		dscr = dscr->next.p;
	} while (1);

	mbuf->next = NULL;
	*tail__ = mbuf;

	spin_unlock_irqrestore(edma->chn_sw[chn].dscr_ring.lock.irq_spinlock_p,
					edma->chn_sw[chn].dscr_ring.lock.flag);

	return 0;
}

static int edma_hw_tx_req(int chn)
{
	struct edma_info *edma = edma_info();

	/* 1s timeout */
	mod_timer(&edma->edma_tx_timer, jiffies +
		  EDMA_TX_TIMER_INTERVAL_MS * HZ / 1000);
	wcn_set_tx_complete_status(2);
	edma->dma_chn_reg[chn].dma_tx_req.reg = 1;

	set_bit(chn, &edma->cur_chn_status);

	return 0;
}

static int edma_hw_rx_req(int chn)
{
	struct edma_info *edma = edma_info();
	union dma_chn_rx_req_reg local_dma_rx_req;

	local_dma_rx_req.reg = edma->dma_chn_reg[chn].dma_rx_req.reg;
	local_dma_rx_req.bit.rf_chn_rx_req = 1;

	edma->dma_chn_reg[chn].dma_rx_req.reg = local_dma_rx_req.reg;

	return 0;
}

int edma_hw_pause(void)
{
	struct edma_info *edma = edma_info();
	union dma_glb_pause_reg tmp;
	u32 retries;
	struct wcn_pcie_info *priv = get_wcn_device_info();

	tmp.reg = readl((void *)(&edma->dma_glb_reg->dma_pause.reg));
	if (!tmp.reg)
		wcn_dump_ep_regs(priv);
	tmp.bit.rf_dma_pause = 1;
	writel(tmp.reg, (void *)(&edma->dma_glb_reg->dma_pause.reg));

	for (retries = 0; retries < 5; retries++) {
		tmp.reg = readl((void *)(&edma->dma_glb_reg->dma_pause.reg));
		if (tmp.bit.rf_dma_pause_status == 1)
			return 0;
		WCN_INFO("%s:retries=%d, value=0x%x\n", __func__, retries,
			 tmp.reg);
		udelay(10);
	}
	edma_dump_glb_reg();
	WCN_INFO("%s  fail\n", __func__);

	return -1;
}

int edma_hw_restore(void)
{
	struct edma_info *edma = edma_info();
	union dma_glb_pause_reg tmp;
	struct wcn_pcie_info *priv = get_wcn_device_info();

	tmp.reg = readl((void *)(&edma->dma_glb_reg->dma_pause.reg));
	if (!tmp.reg)
		wcn_dump_ep_regs(priv);
	tmp.bit.rf_dma_pause = 0;
	writel(tmp.reg, (void *)(&edma->dma_glb_reg->dma_pause.reg));

	return 0;
}

#ifdef __FOR_THREADX_H__
int edma_one_link_dscr_buf_bind(struct desc *dscr, unsigned char *dst,
				unsigned char *src, unsigned short len)
{
	addr_t dst__ = { 0 }, src__ = {
	0};
	struct edma_info *edma = edma_info();
	unsigned int tmp[2];

	WCN_INFO("[+]%s(0x%x, 0x%x, 0x%x, %d)\n", __func__, dscr, dst,
		 src, len);

	AHB32_AXI40(&dst__, dst);
	AHB32_AXI40(&src__, src);

	tmp[0] = src__.l;
	tmp[1] = dst__.l;

	dscr->chn_trans_len.bit.rf_chn_trsc_len = len;
	dscr->chn_trans_len.bit.rf_chn_tx_intr = 0;

	memcpy((unsigned char *)(&(dscr->rf_chn_data_src_addr_low)),
	       (unsigned char *)(&tmp[0]), 4);
	memcpy((unsigned char *)(&(dscr->rf_chn_data_dst_addr_low)),
	       (unsigned char *)(&tmp[1]), 4);

	dscr->chn_ptr_high.bit.rf_chn_src_data_addr_high = src__.h;
	dscr->chn_ptr_high.bit.rf_chn_dst_data_addr_high = dst__.h;
	dscr->chn_trans_len.bit.rf_chn_eof = 0;

	if (sizeof(unsigned long) == sizeof(unsigned int)) {
		memcpy((unsigned char *)(&dscr->link.src),
		       (unsigned char *)(&src), 4);
		memcpy((unsigned char *)(&dscr->buf.dst),
		       (unsigned char *)(&dst), 4);
	} else {
		memcpy((unsigned char *)(&dscr->link.src),
		       (unsigned char *)(&src), 8);
		memcpy((unsigned char *)(&dscr->buf.dst),
		       (unsigned char *)(&dst), 8);
	}

	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int edma_one_link_copy(int chn, struct desc *head, struct desc *tail, int num)
{
	union dma_chn_cfg_reg dma_cfg = { 0 };
	struct edma_info *edma = edma_info();
	union dma_dscr_ptr_high_reg local_chn_ptr_high;

	WCN_INFO("[+]%s(%d, 0x%x, 0x%x, %d)\n", __func__, chn, head,
		 tail, num);
	tail->chn_trans_len.bit.rf_chn_eof = 1;

	dma_cfg.reg = edma->dma_chn_reg[chn].dma_cfg.reg;
	local_chn_ptr_high.reg =
	    edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg;
	local_chn_ptr_high.bit.rf_chn_tx_next_dscr_ptr_high = GET_8_OF_40(head);
	dma_cfg.bit.rf_chn_en = 1;

	edma->dma_chn_reg[chn].dma_dscr.rf_chn_tx_next_dscr_ptr_low =
	    GET_32_OF_40((unsigned char *)(head));
	edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg =
	    local_chn_ptr_high.reg;
	edma->dma_chn_reg[chn].dma_cfg.reg = dma_cfg.reg;

	edma_hw_tx_req(chn);
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int edma_none_link_copy(int chn, addr_t *dst, addr_t *src, unsigned short len,
			int timeout)
{
	union dma_chn_cfg_reg dma_cfg = { 0 };
	union dma_dscr_trans_len_reg chn_trans_len = { 0 };
	union dma_dscr_ptr_high_reg chn_ptr_high = { 0 };
	struct edma_info *edma = edma_info();

	WCN_INFO("[+]%s(%d, {0x%x,0x%x}, {0x%x,0x%x}, %d)\n",
		 __func__, chn, dst->h, dst->l, src->h, src->l, len);

	dma_cfg.reg = edma->dma_chn_reg[chn].dma_cfg.reg;
	chn_trans_len.reg = edma->dma_chn_reg[chn].dma_dscr.chn_trans_len.reg;
	chn_ptr_high.reg = edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg;

	dma_cfg.bit.rf_chn_en = 1;
	chn_trans_len.bit.rf_chn_trsc_len = len;
	chn_ptr_high.bit.rf_chn_src_data_addr_high = src->h;
	chn_ptr_high.bit.rf_chn_dst_data_addr_high = dst->h;
	edma->dma_chn_reg[chn].dma_dscr.rf_chn_data_src_addr_low = src->l;
	edma->dma_chn_reg[chn].dma_dscr.rf_chn_data_dst_addr_low = dst->l;
	edma->dma_chn_reg[chn].dma_dscr.chn_trans_len.reg = chn_trans_len.reg;
	edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg = chn_ptr_high.reg;
	edma->dma_chn_reg[chn].dma_cfg.reg = dma_cfg.reg;
	edma_hw_tx_req(chn);
	while (timeout--) {
		if (edma->dma_chn_reg[chn].dma_int.bit
				.rf_chn_tx_complete_int_raw_status == 1) {
			edma->dma_chn_reg[chn].dma_int.bit
						.rf_chn_tx_complete_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.bit
						.rf_chn_tx_pop_int_clr = 1;
			WCN_INFO("[-]%s\n", __func__);

			return 0;
		}
		udelay(1);
	}
	WCN_INFO("[-]%s timeout\n", __func__);

	return -1;
}
#endif

int edma_push_link(int chn, void *head, void *tail, int num)
{
	int i, j, inout;
	struct mbuf_t *mbuf;
	struct cpdu_head *cpdu;
	struct desc *last = NULL;
	union dma_chn_cfg_reg dma_cfg;
	struct edma_info *edma = edma_info();

	inout = edma->chn_sw[chn].inout;
	if ((head == NULL) || (tail == NULL) || (num == 0)) {
		WCN_ERR("%s(%d, 0x%p, 0x%p, %d) err\n", __func__,
			chn, head, tail, num);
		return -1;
	}
	if (num > edma->chn_sw[chn].dscr_ring.free) {
		WCN_INFO("%s@%d err,chn:%d num:%d free:%d\n",
			 __func__, __LINE__, chn, num,
			  edma->chn_sw[chn].dscr_ring.free);
		/* dscr not enough */
		return -1;
	}

	WCN_DBG("%s(chn=%d, head=0x%p, tail=0x%p, num=%d)\n",
		 __func__, chn, head, tail, num);

	if (edma->chn_sw[chn].dscr_ring.tail == NULL) {
		WCN_ERR("%s: dscr_ring.tail is NULL\n", __func__);
		WARN_ON(1);
		return -1;
	}
	if (inout == TX)
		edma_print_mbuf_data(chn, head, tail, __func__);

	if (!wcn_get_edma_status()) {
		WCN_ERR("%s:don not push the data, card removed, chn=%d\n", __func__, chn);
		return -1;
	}

	if (!atomic_read(&edma->pcie_info->is_suspending))
		__pm_stay_awake(edma->edma_push_ws);

	spin_lock_irqsave(edma->chn_sw[chn].dscr_ring.lock.irq_spinlock_p,
			edma->chn_sw[chn].dscr_ring.lock.flag);

	for (i = 0, j = 0, mbuf = head, cpdu = head; i < num; i++) {
		dscr_zero(edma->chn_sw[chn].dscr_ring.tail);

		dscr_link_mbuf(inout,
				       edma->chn_sw[chn].dscr_ring.tail, mbuf);

		if ((edma->chn_sw[chn].interval) &&
			((++j) == edma->chn_sw[chn].interval)) {
			if (inout == TX)
				edma->chn_sw[chn].dscr_ring
				.tail->chn_trans_len.bit.rf_chn_tx_intr = 1;
			else
				edma->chn_sw[chn].dscr_ring
				.tail->chn_trans_len.bit.rf_chn_rx_intr = 1;
			j = 0;
		}
		last = edma->chn_sw[chn].dscr_ring.tail;
		edma->chn_sw[chn].dscr_ring.tail =
				edma->chn_sw[chn].dscr_ring.tail->next.p;
		edma->chn_sw[chn].dscr_ring.free--;

		mbuf = mbuf->next;
	}
	if (inout == TX) {
		last->chn_trans_len.bit.rf_chn_eof = 1;
	} else {
		last->chn_trans_len.bit.rf_chn_rx_intr = 1;
		last->chn_trans_len.bit.rf_chn_pause = 1;
	}
	spin_unlock_irqrestore(edma->chn_sw[chn].dscr_ring.lock.irq_spinlock_p,
				edma->chn_sw[chn].dscr_ring.lock.flag);

	if (inout == TX) {

		dma_cfg.reg = edma->dma_chn_reg[chn].dma_cfg.reg;
		dma_cfg.bit.rf_chn_en = 1;
		edma->dma_chn_reg[chn].dma_cfg.reg = dma_cfg.reg;
		edma_hw_tx_req(chn);
	} else {
		edma_hw_rx_req(chn);
	}

	if (!atomic_read(&edma->pcie_info->is_suspending))
		__pm_relax(edma->edma_push_ws);

	return 0;
}

static int edma_pending_q_buffer(int chn, void *head, void *tail, int num)
{
	struct edma_pending_q *q;
	struct edma_info *edma = edma_info();

	q = &(edma->chn_sw[chn].pending_q);

	if ((q->wt + 1) % (q->max) == q->rd) {
		WCN_ERR("%s(%d) full\n", __func__, chn);
		return ERROR;
	}
	q->ring[q->wt].head = head;
	q->ring[q->wt].tail = tail;
	q->ring[q->wt].num  = num;
	q->wt = INCR_RING_BUFF_INDX(q->wt, q->max);

	return OK;
}

int edma_push_link_async(int chn, void *head, void *tail, int num)
{
	int ret;

	struct edma_info *edma = edma_info();
	struct edma_pending_q *q = &(edma->chn_sw[chn].pending_q);

	if (edma->chn_sw[chn].inout == RX) {
		ret = edma_push_link(chn, head, tail, num);
		return ret;
	}
	spin_lock_irqsave(q->lock.irq_spinlock_p, q->lock.flag);
	if (q->status) {
		ret = edma_pending_q_buffer(chn, head, tail, num);
		spin_unlock_irqrestore(q->lock.irq_spinlock_p, q->lock.flag);
		return ret;
	}
	edma->chn_sw[chn].pending_q.status = 1;
	spin_unlock_irqrestore(q->lock.irq_spinlock_p, q->lock.flag);
	ret = edma_push_link(chn, head, tail, num);

	return ret;
}

int edma_push_link_wait_complete(int chn, void *head, void *tail, int num,
				 int timeout)
{
	int ret;
	struct edma_info *edma = edma_info();

	edma->chn_sw[chn].wait = timeout;
	edma_push_link(chn, head, tail, num);
	ret = wait_wcnevent(&(edma->chn_sw[chn].event), timeout);

	return ret;
}


static int edma_pending_q_num(int chn)
{
	int ret;
	struct edma_pending_q *q;
	struct edma_info *edma = edma_info();

	q = &(edma->chn_sw[chn].pending_q);

	if (q->wt >= q->rd)
		ret = q->wt - q->rd;
	else
		ret = q->wt + (q->max - q->rd);
	return ret;
}

static int edma_pending_q_flush(int chn)
{
	int num, ret;
	void *head, *tail;
	struct edma_pending_q *q;
	struct edma_info *edma = edma_info();

	q = &(edma->chn_sw[chn].pending_q);
	spin_lock_irqsave(q->lock.irq_spinlock_p, q->lock.flag);
	if (edma_pending_q_num(chn) <= 0) {
		edma->chn_sw[chn].pending_q.status = 0;
		spin_unlock_irqrestore(q->lock.irq_spinlock_p, q->lock.flag);

		return OK;
	}
	head = q->ring[q->rd].head;
	tail = q->ring[q->rd].tail;
	num  = q->ring[q->rd].num;
	q->rd = INCR_RING_BUFF_INDX(q->rd, q->max);
	spin_unlock_irqrestore(q->lock.irq_spinlock_p, q->lock.flag);

	ret = edma_push_link(chn, head, tail, num);

	return ret;
}

int edma_tx_complete_isr(int chn, int mode)
{
	struct desc *start, *end;
	void *head, *tail;
	int node = 0, ret;
	struct edma_info *edma = edma_info();

	switch (mode) {
	case TWO_LINK_MODE:
		start = edma->chn_sw[chn].dscr_ring.head;
		edma_hw_next_dscr(chn, edma->chn_sw[chn].inout, &end);
		end = mpool_phy_to_vir(end);
		if (start != end) {
			edma_pop_link(chn, start, end, (void **)(&head),
				      (void **)(&tail), &node);
		}
		if (edma->chn_sw[chn].wait == 0) {
			if (node > 0)
				ret = mchn_hw_pop_link(chn, head, tail, node);

			if ((edma->chn_sw[chn].inout == TX) &&
			   (mchn_hw_max_pending(chn) > 0))
				ret = edma_pending_q_flush(chn);

			mchn_hw_tx_complete(chn, 0);
		} else {
			edma->chn_sw[chn].wait = 0;
			set_wcnevent(&(edma->chn_sw[chn].event));
		}
		break;
	case ONE_LINK_MODE:
		set_wcnevent(&(edma->chn_sw[chn].event));
		break;
	case NON_LINK_MODE:
	case 3:
		set_wcnevent(&(edma->chn_sw[chn].event));
		break;
	default:

		break;
	}
	return 0;
}

static int edma_tx_pop_isr(int chn)
{
	int node = 0;
	struct desc *start, *end;
	void *head, *tail;
	struct edma_info *edma = edma_info();

	start = edma->chn_sw[chn].dscr_ring.head;
	edma_hw_next_dscr(chn, edma->chn_sw[chn].inout, &end);
	end = mpool_phy_to_vir(end);
	if (start == end) {
		WCN_INFO("[-]%s empty\n", __func__);
		return 0;
	}
	if (edma->chn_sw[chn].wait == 0) {
		edma_pop_link(chn, start, end, (void **)(&head),
			      (void **)(&tail), &node);
		if (node > 0)
			mchn_hw_pop_link(chn, head, tail, node);
	}

	return 0;
}

static int edma_rx_push_isr(int chn)
{
	int ret, node = 0;
	struct desc *end;
	void *head, *tail;

	struct edma_info *edma = edma_info();

	ret = dscr_ring_empty(chn);
	if (!ret) {
		edma_hw_next_dscr(chn, edma->chn_sw[chn].inout, &end);
		end = mpool_phy_to_vir(end);
		if (end != edma->chn_sw[chn].dscr_ring.head)
			edma_pop_link(chn, edma->chn_sw[chn].dscr_ring.head,
				      end, (void **)(&head), (void **)(&tail),
				      &node);
		if (node > 0)
			mchn_hw_pop_link(chn, head, tail, node);
	}
	if (edma->chn_sw[chn].dscr_ring.free > 0)
		mchn_hw_req_push_link(chn, edma->chn_sw[chn].dscr_ring.free);

	return 0;
}

static int edma_rx_pop_isr(int chn)
{
	int node;
	struct desc *end;
	void *head, *tail;
	struct edma_info *edma = edma_info();

	if ((marlin_get_power() == 0) && (chn == 15))
		return 0;
	edma_hw_next_dscr(chn, edma->chn_sw[chn].inout, &end);
	end = mpool_phy_to_vir(end);
	if (end == edma->chn_sw[chn].dscr_ring.head)
		return 0;

	edma_pop_link(chn, edma->chn_sw[chn].dscr_ring.head, end,
		      (void **)(&head), (void **)(&tail), &node);
	if (node > 0) {
		edma_print_mbuf_data(chn, head, tail, __func__);
		mchn_hw_pop_link(chn, head, tail, node);
	}
	return 0;
}

static int hisrfunc(struct isr_msg_queue *msg)
{
	int chn;
	union dma_chn_int_reg dma_int;
	struct edma_info *edma = edma_info();

	chn = msg->chn;
	switch (msg->evt) {

	case ISR_MSG_INTx:
		dma_int.reg = msg->dma_int.reg;
		switch (edma->chn_sw[chn].mode) {
		case TWO_LINK_MODE:
			if (edma->chn_sw[chn].inout) {
				if (dma_int.bit.rf_chn_tx_pop_int_mask_status)
					edma_tx_pop_isr(chn);

				if (dma_int.bit
				    .rf_chn_tx_complete_int_mask_status)
					edma_tx_complete_isr(chn,
							     TWO_LINK_MODE);
			} else {
				if (dma_int.bit.rf_chn_rx_pop_int_mask_status)
					edma_rx_pop_isr(chn);
				if (dma_int.bit
					   .rf_chn_rx_push_int_mask_status)
					edma_rx_push_isr(chn);
			}
			/* fallthrough; */
		case ONE_LINK_MODE:
			break;
		case NON_LINK_MODE:
			break;
		default:
			WCN_INFO("%s unknown mode\n", __func__);
			break;
		}
		break;
	case ISR_MSG_TX_POP:
		edma_tx_pop_isr(chn);
		break;
	case ISR_MSG_TX_COMPLETE:
		edma_tx_complete_isr(chn, TWO_LINK_MODE);
		break;
	case ISR_MSG_RX_POP:
		edma_rx_pop_isr(chn);
		break;
	case ISR_MSG_RX_PUSH:
		edma_rx_push_isr(chn);
		break;
	case ISR_MSG_EXIT_FUNC:
		break;
	default:
		pcie_hexdump("isr unknown msg", (unsigned char *)(msg),
			     sizeof(struct isr_msg_queue));
		break;
	}

	return 0;
}

int q_info(int debug)
{
	struct edma_info *edma = edma_info();
	struct msg_q *q = &(edma->isr_func.q);

	WCN_INFO("seq(%d,%d), line:%d, sem:%d\n", hisrfunc_last_msg,
		 q->seq, hisrfunc_line, q->event.wait_sem.count);

	hisrfunc_debug = debug;
	set_wcnevent(&(edma->isr_func.q.event));

	return 0;
}

int legacy_irq_handle(int data)
{
	unsigned long irq_flags;
	int chn, discard;
	int ret = 0;
	unsigned int dma_int_mask_status;
	union dma_chn_int_reg dma_int;
	struct isr_msg_queue msg = { 0 };
	struct edma_info *edma = edma_info();

	local_irq_save(irq_flags);
	dma_int_mask_status = edma->dma_glb_reg->dma_int_mask_status;
	for (chn = 0; chn < 32; chn++) {
		if (!(dma_int_mask_status & (1 << chn)))
			continue;
		dma_int.reg = edma->dma_chn_reg[chn].dma_int.reg;
		if (dma_int.bit.rf_chn_cfg_err_int_mask_status) {
			WCN_ERR("%s chn %d assert(0x%x)\n", __func__,
				chn, dma_int.reg);
			dma_int.bit.rf_chn_cfg_err_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			continue;
		}
		discard = 1;
		msg.chn = chn;
		msg.evt = ISR_MSG_INTx;
		msg.dma_int.reg = dma_int.reg;
		switch (edma->chn_sw[chn].mode) {
		case TWO_LINK_MODE:
			if (edma->chn_sw[chn].inout) {
				if (dma_int.bit.rf_chn_tx_pop_int_mask_status) {
					dma_int.bit.rf_chn_tx_pop_int_clr = 1;
					discard = 0;
				}
				if (dma_int.bit
					 .rf_chn_tx_complete_int_mask_status) {

					dma_int.bit
						.rf_chn_tx_complete_int_clr = 1;
					discard = 0;
				}
			} else {
				if (dma_int.bit.rf_chn_rx_pop_int_mask_status) {
					dma_int.bit.rf_chn_rx_pop_int_clr = 1;
					discard = 0;
				}
				if (dma_int.bit.rf_chn_rx_push_int_mask_status
									== 1) {
					dma_int.bit.rf_chn_rx_push_int_clr = 1;
					discard = 0;
				}
			}
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			if (!discard) {
				if (mchn_hw_cb_in_irq(chn) == 0) {
					enqueue(&(edma->isr_func.q),
						(unsigned char *)(&msg));
					set_wcnevent(&(edma->isr_func
								.q.event));
				} else if (mchn_hw_cb_in_irq(chn) == -1) {
					ret = -1;
					break;
				}

				hisrfunc(&msg);
			}
			break;
		case ONE_LINK_MODE:
			if (dma_int.bit.rf_chn_tx_complete_int_mask_status == 1)
				dma_int.bit.rf_chn_tx_complete_int_clr = 1;
			if (dma_int.bit.rf_chn_tx_pop_int_mask_status == 1)
				dma_int.bit.rf_chn_tx_pop_int_clr = 1;
			if (dma_int.bit.rf_chn_tx_complete_int_mask_status == 1)
				dma_int.bit.rf_chn_tx_complete_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			edma_tx_complete_isr(chn, ONE_LINK_MODE);
			break;
		case NON_LINK_MODE:
			if (dma_int.bit.rf_chn_tx_pop_int_mask_status == 1)
				dma_int.bit.rf_chn_tx_pop_int_clr = 1;
			if (dma_int.bit
				.rf_chn_tx_complete_int_mask_status == 1) {
				dma_int.bit.rf_chn_tx_complete_int_clr = 1;
				edma_tx_complete_isr(chn, NON_LINK_MODE);
			}
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			break;
		default:
			if (dma_int.bit.rf_chn_tx_pop_int_mask_status)
				dma_int.bit.rf_chn_tx_pop_int_clr = 1;
			if (dma_int.bit.rf_chn_tx_complete_int_mask_status)
				dma_int.bit.rf_chn_tx_complete_int_clr = 1;
			if (dma_int.bit.rf_chn_rx_pop_int_mask_status)
				dma_int.bit.rf_chn_rx_pop_int_clr = 1;
			if (dma_int.bit.rf_chn_rx_push_int_mask_status)
				dma_int.bit.rf_chn_rx_push_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			WCN_INFO("%s chn %d not ready\n", __func__, chn);
			break;
		}
	}
	local_irq_restore(irq_flags);

	return ret;
}

int msi_irq_handle(int irq)
{
	int chn, i = 0;
	unsigned long irq_flags;
	union dma_chn_int_reg dma_int;
	struct isr_msg_queue msg = { 0 };
	struct edma_info *edma = edma_info();
	struct wcn_pcie_info *priv;

	WCN_INFO("irq msi handle=%d\n", irq);
	if (!wcn_get_edma_status()) {
		WCN_ERR("do not handle this irq, card removed\n");
		return -1;
	}
	local_irq_save(irq_flags);
	chn = (irq - 0) / 2;
	dma_int.reg = edma->dma_chn_reg[chn].dma_int.reg;
	msg.chn = chn;

	//__pm_wakeup_event(edma->edma_pop_ws, jiffies_to_msecs(HZ / 2));

	if (edma->chn_sw[chn].inout == TX) {
		wcn_set_tx_complete_status(1);
		clear_bit(chn, &edma->cur_chn_status);
		del_timer(&edma->edma_tx_timer);
		if (irq % 2 == 0) {
			dma_int.bit.rf_chn_tx_pop_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			msg.evt = ISR_MSG_TX_POP;
		} else {
			dma_int.bit.rf_chn_tx_complete_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			msg.evt = ISR_MSG_TX_COMPLETE;
		}
	} else {
		if (irq % 2 == 0) {
			do {
				i++;
				dma_int.bit.rf_chn_rx_pop_int_clr = 1;
				edma->dma_chn_reg[chn].dma_int.reg =
								dma_int.reg;
				if ((edma->dma_chn_reg[chn].dma_int.reg ==
				    0xFFFFFFFF) || (i > 3000)) {
					WCN_ERR("i=%d, dma_int=0x%08x\n", i,
						edma->dma_chn_reg[chn].dma_int
						.reg);
					priv = get_wcn_device_info();
					if (!priv) {
						WCN_ERR("%s:pcie ep is null\n", __func__);
						local_irq_restore(irq_flags);
						return -1;
					}
					if (priv->rc_pd)
						sprd_pcie_dump_rc_regs(priv->rc_pd);
					local_irq_restore(irq_flags);
					return -1;
				}
			} while (edma->dma_chn_reg[chn].dma_int.reg & 0x040400);

			msg.evt = ISR_MSG_RX_POP;

		} else {
			dma_int.bit.rf_chn_rx_push_int_clr = 1;
			edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
			msg.evt = ISR_MSG_RX_PUSH;
		}
	}
	if (mchn_hw_cb_in_irq(chn) == 0) {
		enqueue(&(edma->isr_func.q), (unsigned char *)(&msg));
		WCN_DBG(" callback not in irq\n");
		set_wcnevent(&(edma->isr_func.q.event));
		WCN_INFO("cb not irq=%ld, chn=%d\n", irq_flags, chn);
		local_irq_restore(irq_flags);
		return 0;
	} else if (mchn_hw_cb_in_irq(chn) == -1) {
		local_irq_restore(irq_flags);
		WCN_ERR(" irq=%ld, chn=%d\n", irq_flags, chn);
		return -1;
	}

	WCN_DBG("callback in irq\n");
	hisrfunc(&msg);

	WCN_INFO("cb in irq=%ld, chn=%d\n", irq_flags, chn);
	local_irq_restore(irq_flags);

	return 0;
}

int edma_task(void *a)
{
	struct isr_msg_queue msg = {};
	struct edma_info *edma = edma_info();
	struct msg_q *q = &(edma->isr_func.q);

	edma->isr_func.state = 1;
	WCN_INFO("[+]%s\n", __func__);
	do {
		hisrfunc_line = __LINE__;
		wait_wcnevent(&(q->event), -1);
		hisrfunc_line = __LINE__;
		if (hisrfunc_debug)
			WCN_INFO("#\n");
		while (dequeue(q, (unsigned char *)(&msg), -1) == OK) {
			hisrfunc_line = __LINE__;
			hisrfunc_last_msg = msg.seq;
			hisrfunc(&msg);
			hisrfunc_line = __LINE__;
			if (msg.evt == ISR_MSG_EXIT_FUNC)
				goto EXIT;
		}
	} while (1);
EXIT:
	edma->isr_func.state = 0;
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

static void edma_tasklet(unsigned long data)
{
	struct isr_msg_queue msg = { 0 };
	struct edma_info *edma = edma_info();
	struct msg_q *q = &(edma->isr_func.q);

	/*debug tasklet schedule when edma_tasklet_deinit*/
	if (!wcn_get_edma_status())
		WCN_INFO("%s:card removed before tasklet deinit\n", __func__);

	while (dequeue(q, (unsigned char *)(&msg), -1) == OK)
		hisrfunc(&msg);


}

static void dscr_ring_deinit(int chn)
{
	struct desc *dscr;
	struct dscr_ring *dscr_ring;
	struct edma_info *edma = edma_info();

	dscr_ring = &(edma->chn_sw[chn].dscr_ring);
	dscr_ring->free = dscr_ring->size;
	dscr_ring->head = dscr_ring->tail = dscr =
					(struct desc *) dscr_ring->mem;
	/*resolve mem lead*/
	if (dscr_ring->lock.irq_spinlock_p)
		kfree(dscr_ring->lock.irq_spinlock_p);
	dscr_ring->lock.irq_spinlock_p = NULL;
	if (!dscr)
		return;
	dscr_zero(dscr);
}

static int dscr_ring_init(int chn, struct dscr_ring *dscr_ring,
			int inout, int size)
{
	int i;
	unsigned int tmp;
	struct desc *dscr;

	WCN_DBG("[+]%s(0x%p, 0x%p)\n", __func__, dscr_ring,
			 dscr_ring->mem);

	/** mpool not free, so dscr_ring->mem not change and
	 *  don't need re-init.
	 */
	if (dscr_ring->mem == NULL) {
		dscr_ring->mem =
		    (unsigned char *)mpool_malloc(sizeof(struct desc) *
						     (size + 1));
		if (dscr_ring->mem == NULL)
			return ERROR;
	}

	dscr_ring->size = size;
	memset(dscr_ring->mem, 0x00, sizeof(struct desc) * (size + 1));

	edma_spin_lock_init(&(dscr_ring->lock));

	dscr_ring->head = dscr_ring->tail = dscr =
						(struct desc *) dscr_ring->mem;
	for (i = 0; i < size; i++) {
		if (inout == TX) {
			dscr[i].chn_ptr_high.bit.rf_chn_tx_next_dscr_ptr_high =
				GET_8_OF_40(mpool_vir_to_phy(&dscr[i + 1]));
			tmp = GET_32_OF_40((unsigned char *)(&dscr[i + 1]));
			memcpy((unsigned char *)(&dscr[i]
				.rf_chn_tx_next_dscr_ptr_low),
			       (unsigned char *)(&tmp), 4);
		} else {
			dscr[i].chn_ptr_high.bit.rf_chn_rx_next_dscr_ptr_high =
				GET_8_OF_40(mpool_vir_to_phy(&dscr[i + 1]));
			tmp = GET_32_OF_40((unsigned char *)(&dscr[i + 1]));
			memcpy((unsigned char *)(&dscr[i]
						.rf_chn_rx_next_dscr_ptr_low),
			       (unsigned char *)(&tmp), 4);
		}
		WCN_DBG("dscr(0x%p-->0x%p)\n",
			mpool_vir_to_phy(&dscr[i]),
			mpool_vir_to_phy(&dscr[i + 1]));
		dscr[i].next.p = &dscr[i + 1];
	}
	if (inout == TX) {
		dscr[i].chn_ptr_high.bit.rf_chn_tx_next_dscr_ptr_high =
			GET_8_OF_40(mpool_vir_to_phy(&dscr[0]));
		tmp = GET_32_OF_40((unsigned char *)(&dscr[0]));
		memcpy((unsigned char *)(&dscr[i].rf_chn_tx_next_dscr_ptr_low),
		       (unsigned char *)(&tmp), 4);
		dscr[0].chn_trans_len.bit.rf_chn_eof = 1;

	} else {
		dscr[i].chn_ptr_high.bit.rf_chn_rx_next_dscr_ptr_high =
			GET_8_OF_40(mpool_vir_to_phy(&dscr[0]));
		tmp = GET_32_OF_40((unsigned char *)(&dscr[0]));
		memcpy((unsigned char *)(&dscr[i].rf_chn_rx_next_dscr_ptr_low),
		       (unsigned char *)(&tmp), 4);
		dscr[0].chn_trans_len.bit.rf_chn_pause = 1;
	}
	WCN_DBG("dscr(0x%p-->0x%p)\n",
		 mpool_vir_to_phy(&dscr[i]),
		 mpool_vir_to_phy(&dscr[0]));
	dscr[i].next.p = &dscr[0];
	dscr_ring->free = size;
	WCN_DBG("[-]%s(0x%p, 0x%p, %d, %d)\n", __func__, dscr_ring,
		 dscr_ring->mem, dscr_ring->size, dscr_ring->free);

	return 0;
}

static int edma_pending_q_init(int chn, int max)
{
	struct edma_pending_q *q;
	struct edma_info *edma = edma_info();

	q = &(edma->chn_sw[chn].pending_q);
	memset((char *)q, 0x00, sizeof(struct edma_pending_q));
	q->max = max;
	q->chn = chn;
	edma_spin_lock_init(&(q->lock));

	return OK;
}

int edma_chn_init(int chn, int mode, int inout, int max_trans)
{
	int ret, dir = 0;
	struct dscr_ring *dscr_ring;
	struct edma_info *edma = edma_info();
	union dma_chn_int_reg dma_int = { 0 };
	union dma_chn_cfg_reg dma_cfg = { 0 };
	struct desc local_DSCR;

	if (inout == RX)
		/* int direction. int send to ap */
		dir = 1;
	WCN_INFO("[+]%s(chn=%d,mode=%d,dir=%d,inout=%d,max_trans=%d)\n",
		 __func__, chn, mode, dir, inout, max_trans);

	dma_int.reg = edma->dma_chn_reg[chn].dma_int.reg;
	dma_cfg.reg = edma->dma_chn_reg[chn].dma_cfg.reg;
	local_DSCR = edma->dma_chn_reg[chn].dma_dscr;
	/*
	 * First power on, dscr reg is random val, so need reset it
	 * specially chn_ptr_high need be reset, avoid tx/rx next dscr high
	 * addr error;
	 * other regs will be setting in the next initial process.
	 */
	switch (mode) {
	case TWO_LINK_MODE:
		dscr_ring = &(edma->chn_sw[chn].dscr_ring);
		ret = dscr_ring_init(chn, dscr_ring, inout, max_trans);
		if (ret)
			return ERROR;
		/* 1:enable channel; 0:disable channel */
		if (dma_cfg.bit.rf_chn_en == 0) {
			dma_cfg.bit.rf_chn_en = 1;
			/* 0:trans done, 1:linklist done */
			dma_cfg.bit.rf_chn_req_mode = 1;
			dma_cfg.bit.rf_chn_list_mode = TWO_LINK_MODE;
			if (!inout)
			    /* source data from CP */
				dma_cfg.bit.rf_chn_dir = 1;
			else
				/* source data from AP */
				dma_cfg.bit.rf_chn_dir = 0;
		}
		if (inout == TX) {
			/* tx_list link point */
			local_DSCR.rf_chn_tx_next_dscr_ptr_low =
			    GET_32_OF_40((unsigned char *)(dscr_ring->head));
			/* tx_list link point */
			local_DSCR.chn_ptr_high.bit
						.rf_chn_tx_next_dscr_ptr_high =
					GET_8_OF_40(mpool_vir_to_phy(dscr_ring->head));
			dma_int.bit.rf_chn_tx_complete_int_en = 1;
			dma_int.bit.rf_chn_tx_pop_int_clr = 1;
			dma_int.bit.rf_chn_tx_complete_int_clr = 1;
		} else {
			local_DSCR.rf_chn_rx_next_dscr_ptr_low =
			    GET_32_OF_40((unsigned char *)(dscr_ring->head));
			local_DSCR.chn_ptr_high.bit
						.rf_chn_rx_next_dscr_ptr_high =
					GET_8_OF_40(mpool_vir_to_phy(dscr_ring->head));
			dma_int.bit.rf_chn_rx_push_int_en = 1;
			/* clear semaphore value */
			dma_cfg.bit.rf_chn_sem_value = 0xFF;
		}
		edma_pending_q_init(chn, mchn_hw_max_pending(chn));
		break;
	case ONE_LINK_MODE:
		/* 0:to cp . 1:to AP */
		dma_cfg.bit.rf_chn_int_out_sel = dir;
		/* 1:enable channel; 0:disable channel */
		dma_cfg.bit.rf_chn_en = 1;
		/* 0:trans done, 1:linklist done */
		dma_cfg.bit.rf_chn_req_mode = 1;
		dma_cfg.bit.rf_chn_list_mode = ONE_LINK_MODE;
		dma_int.bit.rf_chn_tx_complete_int_en = 1;
		dma_int.bit.rf_chn_tx_pop_int_en = 1;
		break;

	case NON_LINK_MODE:
	case 3:
		dma_int.bit.rf_chn_tx_complete_int_en = 0;
		dma_int.bit.rf_chn_tx_pop_int_en = 0;
		dma_int.bit.rf_chn_rx_push_int_en = 0;
		dma_int.bit.rf_chn_rx_pop_int_en = 0;
		/* 0:tx_int to CP; 1:rx_int to AP */
		dma_cfg.bit.rf_chn_int_out_sel = dir;
		dma_cfg.bit.rf_chn_en = 1;
		dma_cfg.bit.rf_chn_list_mode = NON_LINK_MODE;
		break;
	default:
		break;
	}

	switch (mode) {
	case TWO_LINK_MODE:
		if (inout) {
			edma->dma_chn_reg[chn].dma_dscr
					      .rf_chn_tx_next_dscr_ptr_low =
					local_DSCR.rf_chn_tx_next_dscr_ptr_low;
		} else {
			edma->dma_chn_reg[chn].dma_dscr
					      .rf_chn_rx_next_dscr_ptr_low =
					local_DSCR.rf_chn_rx_next_dscr_ptr_low;
		}
		edma->dma_chn_reg[chn].dma_dscr.chn_ptr_high.reg =
		    local_DSCR.chn_ptr_high.reg;

		break;
	default:
		break;
	}
	edma->chn_sw[chn].dir = dir;
	edma->chn_sw[chn].inout = inout;
	edma->chn_sw[chn].mode = mode;
	edma->dma_chn_reg[chn].dma_int.reg = dma_int.reg;
	edma->dma_chn_reg[chn].dma_cfg.reg = dma_cfg.reg;
	dma_cfg.reg = edma->dma_chn_reg[chn].dma_cfg.reg;
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int edma_chn_deinit(int chn)
{
	/*resolve mem leak*/
	struct edma_info *edma = edma_info();

	/* TODO: need add more deinit operation */
	dscr_ring_deinit(chn);

	/*resolve mem lead*/
	if (edma->chn_sw[chn].pending_q.lock.irq_spinlock_p)
		kfree(edma->chn_sw[chn].pending_q.lock.irq_spinlock_p);
	edma->chn_sw[chn].pending_q.lock.irq_spinlock_p = NULL;
	WCN_INFO("release irq_spinlock_p\n");
	return 0;
}

int edma_tp_count(int chn, void *head, void *tail, int num)
{
	int i, dt;
	struct mbuf_t *mbuf;
	static int bytecount;
	static struct timespec64 start_time, time;

	for (i = 0, mbuf = (struct mbuf_t *)head; i < num; i++) {
		ktime_get_real_ts64(&time);
		if (bytecount == 0)
			start_time = time;
		bytecount += mbuf->len;
		dt = time_sub_us(&start_time, &time);
		if (dt >= 1000000) {
			WCN_INFO("edma-tp:%d/%d (byte/us)\n",
				 bytecount, dt);
			bytecount = 0;
		}
		mbuf = mbuf->next;
	}

	return 0;
}

int edma_dump_chn_reg(int chn)
{
	struct wcn_pcie_info *pdev;
	u32 value;
	int reg_base;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		reg_base = EDMA_CHN_REG_BASE_M3E;
	else
		reg_base = EDMA_CHN_REG_BASE;

	pdev = get_wcn_device_info();
	if (!pdev) {
		WCN_ERR("%s:pcie device is null\n", __func__);
		return -1;
	}

	WCN_INFO("------------[ chn=%d ]------------\n", chn);
	value = sprd_pcie_read_reg32(pdev, CHN_DMA_INT(reg_base, chn));
	WCN_INFO("[dma_int  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_DMA_TX_REQ(reg_base, chn));
	WCN_INFO("[tx_req   ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_DMA_RX_REQ(reg_base, chn));
	WCN_INFO("[rx_req   ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_DMA_CFG(reg_base, chn));
	WCN_INFO("[dma_cfg  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_TRANS_LEN(reg_base, chn));
	WCN_INFO("[tran_len ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_PTR_HIGH(reg_base, chn));
	WCN_INFO("[PTR_high ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_TX_NEXT_DSCR_PTR_LOW(reg_base, chn));
	WCN_INFO("[tx_next  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_RX_NEXT_DSCR_PTR_LOW(reg_base, chn));
	WCN_INFO("[rx_next  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_DATA_SRC_ADDR_LOW(reg_base, chn));
	WCN_INFO("[src_addr ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, CHN_DATA_DEST_ADDR_LOW(reg_base, chn));
	WCN_INFO("[dest_addr] = 0x%08x\n",  value);

	return 0;
}

int edma_dump_glb_reg(void)
{
	struct wcn_pcie_info *pdev;
	u32 value;
	struct edma_info *edma = edma_info();
	int reg_base;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		reg_base = EDMA_GLB_REG_BASE_M3E;
	else
		reg_base = EDMA_GLB_REG_BASE;

	pdev = get_wcn_device_info();
	if (!pdev) {
		pr_err("%s:pcie device is null\n", __func__);
		return -1;
	}
	WCN_INFO("------------[ DMA glb Reg ]------------\n");
	value = sprd_pcie_read_reg32(pdev, DMA_PAUSE(reg_base));
	WCN_INFO("[dma_pause  ] = 0x%08x\n", value);
	value = sprd_pcie_read_reg32(pdev, DMA_INT_RAW_STATUS(reg_base));
	WCN_INFO("[int_sts    ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_INT_MASK_STATUS(reg_base));
	WCN_INFO("[mask_sts   ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_REQ_STATUS(reg_base));
	WCN_INFO("[req_sts    ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_DEBUG_STATUS(reg_base));
	WCN_INFO("[debug_sts  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_ARB_SEL_STATUS(reg_base));
	WCN_INFO("[arb_sel_sts] = 0x%08x\n",  value);

	if ((value > 0) && (value < 31))
		set_bit((value - 1), &edma->cur_chn_status);
	else if (value == 0xffffffff) {
		if (pdev->rc_pd)
			sprd_pcie_dump_rc_regs(pdev->rc_pd);
		return -1;
	}
	WCN_INFO("[arb_sel_sts] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_CHN_ARPROT(reg_base));
	WCN_INFO("[arport     ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_CHN_AWPROT(reg_base));
	WCN_INFO("[awport     ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_CHN_PROT_FLAG(reg_base));
	WCN_INFO("[prot_flag  ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_GLB_PROT(reg_base));
	WCN_INFO("[glb_port   ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_REQ_CID_PROT(reg_base));
	WCN_INFO("[req_cid    ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_SYNC_SEC_NORMAL(reg_base));
	WCN_INFO("[sync       ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_PCIE_MSIX_REG_ADDR_LO(reg_base));
	WCN_INFO("[msix_reg   ] = 0x%08x\n",  value);
	value = sprd_pcie_read_reg32(pdev, DMA_PCIE_MSIX_VALUE(reg_base));
	WCN_INFO("[msix_val   ] = 0x%08x\n",  value);

	if (pdev->rc_pd)
		sprd_pcie_dump_rc_regs(pdev->rc_pd);
	return 0;
}

static void edma_tx_timer_expire(struct timer_list *t)
{
	struct edma_info *edma = from_timer(edma, t, edma_tx_timer);
	int i;

	WCN_ERR("edma tx send timeout\n");
	if (!wcn_get_edma_status())
		return;
	wcn_set_tx_complete_status(1);
	if (edma_dump_glb_reg() < 0)
		return;
	for (i = 0; i < 16; i++) {
		if (test_bit(i, &edma->cur_chn_status))
			edma_dump_chn_reg(i);
	}

}

void edma_del_tx_timer(void)
{
	struct edma_info *edma = edma_info();

	del_timer(&edma->edma_tx_timer);
}

int edma_init(struct wcn_pcie_info *pcie_info)
{
	unsigned int i, ret, *reg;
	struct edma_info *edma = edma_info();
	unsigned int edma_glb_reg_base, edma_chn_reg_base;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_m3e) {
		edma_glb_reg_base = EDMA_GLB_REG_BASE_M3E;
		edma_chn_reg_base = EDMA_CHN_REG_BASE_M3E;
	} else {
		edma_glb_reg_base = EDMA_GLB_REG_BASE;
		edma_chn_reg_base = EDMA_CHN_REG_BASE;
	}

	memset((char *)edma, 0x00, sizeof(struct edma_info));
	edma->pcie_info = pcie_info;

	WCN_INFO("new edma(0x%p--0x%p)\n", edma,
		 (void *)virt_to_phys((void *)(edma)));
	ret = create_queue(&(edma->isr_func.q), sizeof(struct isr_msg_queue),
			   50);
	if (ret != 0) {
		WCN_ERR("create_queue fail\n");
		return -1;
	}
#if TASKLET_SUPPORT
	edma->isr_func.q.event.tasklet = kmalloc(sizeof(struct tasklet_struct),
						 GFP_KERNEL);
	tasklet_init(edma->isr_func.q.event.tasklet, edma_tasklet, 0);
#else
	edma->isr_func.entity = kthread_create(edma_task, edma, "edma_task");
	if (edma->isr_func.entity == NULL) {
		WCN_ERR("create isr_func fail\n");

		return -1;
	}
	do {
		struct sched_param param;

		param.sched_priority = 90;
		ret = sched_setscheduler((struct task_struct *)edma->isr_func
					  .entity, SCHED_FIFO, &param);
		WCN_INFO("sched_setscheduler(SCHED_FIFO), prio:%d,ret:%d\n",
			 param.sched_priority, ret);
	} while (0);

	wake_up_process(edma->isr_func.entity);
#endif
	reg = (unsigned int *)(pcie_bar_vmem(edma->pcie_info, 0) + 0x130004);
	*reg = ((*reg) | 1 << 7);
	edma->dma_glb_reg = (struct edma_glb_reg *)
			(pcie_bar_vmem(edma->pcie_info, 0) + edma_glb_reg_base);
	edma->dma_chn_reg = (struct edma_chn_reg *)
			(pcie_bar_vmem(edma->pcie_info, 0) + edma_chn_reg_base);
	WCN_INFO("WCN dma_chn_reg size is %ld\n", sizeof(struct edma_chn_reg));
	for (i = 0; i < 16; i++) {
		WCN_DBG("edma chn[%d] dma_int:0x%p, event:%p\n", i,
			 &edma->dma_chn_reg[i].dma_int.reg,
			 &edma->chn_sw[i].event);
		WCN_DBG("0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		     edma->dma_chn_reg[i].dma_dscr.chn_trans_len.reg,
		     edma->dma_chn_reg[i].dma_dscr.chn_ptr_high.reg,
		     edma->dma_chn_reg[i].dma_dscr.rf_chn_tx_next_dscr_ptr_low,
		     edma->dma_chn_reg[i].dma_dscr.rf_chn_rx_next_dscr_ptr_low,
		     edma->dma_chn_reg[i].dma_dscr.rf_chn_data_src_addr_low,
		     edma->dma_chn_reg[i].dma_dscr.rf_chn_data_dst_addr_low);
		create_wcnevent(&(edma->chn_sw[i].event), i);
		edma->chn_sw[i].mode = -1;
		edma->dma_chn_reg[i].dma_dscr.chn_trans_len.reg = 0;
		edma->dma_chn_reg[i].dma_dscr.chn_ptr_high.reg = 0;
		edma->dma_chn_reg[i].dma_dscr.rf_chn_tx_next_dscr_ptr_low = 0;
		edma->dma_chn_reg[i].dma_dscr.rf_chn_rx_next_dscr_ptr_low = 0;
		edma->dma_chn_reg[i].dma_dscr.rf_chn_data_src_addr_low = 0;
		edma->dma_chn_reg[i].dma_dscr.rf_chn_data_dst_addr_low = 0;
	}

	edma->edma_push_ws = wakeup_source_register(NULL, "wcn edma txrx push");
	edma->edma_pop_ws = wakeup_source_register(NULL, "wcn edma txrx callback");
	mutex_init(&edma->mpool_lock);
	spin_lock_init(&edma->tasklet_lock);

	/* Init edma tx send timeout timer */
	timer_setup(&edma->edma_tx_timer, edma_tx_timer_expire, 0);

	WCN_INFO("%s done\n", __func__);

	return 0;
}

int edma_tasklet_deinit(void)
{
	struct edma_info *edma = edma_info();

#if TASKLET_SUPPORT
	WCN_INFO("tasklet exit start status=0x%lx, count=%d\n",
		 edma->isr_func.q.event.tasklet->state,
		 atomic_read(&edma->isr_func.q.event.tasklet->count));
	tasklet_kill(edma->isr_func.q.event.tasklet);
	kfree(edma->isr_func.q.event.tasklet);
	edma->isr_func.q.event.tasklet = NULL;
	WCN_INFO("tasklet exit end\n");
#endif

	return 0;
}

int edma_deinit(void)
{
	struct isr_msg_queue msg = { 0 };
	struct edma_info *edma = edma_info();
	struct msg_q *q = &(edma->isr_func.q);

	WCN_INFO("[+]%s:fun_status=%d\n", __func__, edma->isr_func.state);
	do {
		usleep_range_state(10000, 11000, TASK_UNINTERRUPTIBLE);
		if (edma->isr_func.state == 0)
			break;

		msg.evt = ISR_MSG_EXIT_FUNC;
		enqueue(&(edma->isr_func.q), (unsigned char *)&msg);
		set_wcnevent(&(edma->isr_func.q.event));
	} while (edma->isr_func.state);

	WCN_INFO("wakeup_source exit\n");
	wakeup_source_unregister(edma->edma_push_ws);
	wakeup_source_unregister(edma->edma_pop_ws);
	mutex_destroy(&edma->mpool_lock);
	kfree(q->lock.irq_spinlock_p);
	delete_queue(q);
	/* TODO: need free mpool */
	mpool_free();
	memset(edma, 0x00, sizeof(edma_info));
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

