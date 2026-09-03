/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <misc/wcn_bus.h>
#include <uapi/linux/sched/types.h>
#include "bus_common.h"
#include "wcn_integrate.h"
#include "wcn_sipc.h"
#include "../platform/wcn_procfs.h"

#define SIPC_WCN_DST 3

#define SIPC_TYPE_SBUF 0
#define SIPC_TYPE_SBLOCK 1

#define SIPC_CHANNEL_UNCREATED 0
#define SIPC_CHANNEL_CREATED 1

#define SIPC_NOTIFIER_UNREGISTER  0
#define SIPC_NOTIFIER_REGISTER 1

/* default value of sipc channel */
#define SIPC_CHN_ATCMD 4
#define SIPC_CHN_LOOPCHECK 11
#define SIPC_CHN_ASSERT 12
#define SIPC_CHN_LOG 5
#define SIPC_CHN_BT 4
#define SIPC_CHN_FM 4
#define SIPC_CHN_WIFI_CMD 7
#define SIPC_CHN_WIFI_DATA0 8
#define SIPC_CHN_WIFI_DATA1 9

#define INIT_SIPC_CHN_SBUF(idx, in_out, channel, bid,\
		 blen, bnum, tbsize, rbsize)\
{.index = idx, .inout = in_out, .chntype = SIPC_TYPE_SBUF,\
.chn = channel, .dst = SIPC_WCN_DST, .sbuf.bufid = bid,\
.sbuf.len = blen, .sbuf.bufnum = bnum,\
.sbuf.txbufsize = tbsize, .sbuf.rxbufsize = rbsize}

#define INIT_SIPC_CHN_SBLOCK(idx, in_out, channel,\
		 tbnum, tbsize, rbnum, rbsize,\
		 _basemem, _alignsize, _mapped_smem_base)\
{.index = idx, .inout = in_out, .chntype = SIPC_TYPE_SBLOCK,\
.chn = channel, .dst = SIPC_WCN_DST,\
.sblk.txblocknum = tbnum, .sblk.txblocksize = tbsize,\
.sblk.rxblocknum = rbnum, .sblk.rxblocksize = rbsize,\
.sblk.basemem = _basemem, .sblk.alignsize = _alignsize,\
.sblk.mapped_smem_base = _mapped_smem_base}

static struct wcn_sipc_info_t g_sipc_info = {0};

/* default sipc channel info */
/* at/bt/fm use sbuf channel 4:  */
/* at bufid 5  bt bufid(tx 11 rx 10) fm bufid(tx 14 rx 13) */

/* default value */
static struct sipc_chn_info g_sipc_chn[SIPC_CHN_NUM] = {
	INIT_SIPC_CHN_SBUF(SIPC_ATCMD_TX, WCNBUS_TX, SIPC_CHN_ATCMD,
			   5, 128, 16, 0x2400, 0x2400),
	INIT_SIPC_CHN_SBUF(SIPC_ATCMD_RX, WCNBUS_RX, SIPC_CHN_ATCMD,
			   5, 128, 16, 0x2400, 0x2400),
	INIT_SIPC_CHN_SBUF(SIPC_LOOPCHECK_RX, WCNBUS_RX, SIPC_CHN_LOOPCHECK,
			   0, 128, 1, 0x400, 0x400),
	INIT_SIPC_CHN_SBUF(SIPC_ASSERT_RX, WCNBUS_RX, SIPC_CHN_ASSERT,
			   0, 1024, 1, 0x400, 0x400),
	INIT_SIPC_CHN_SBUF(SIPC_LOG_RX, WCNBUS_RX, SIPC_CHN_LOG,
			   0, 8 * 1024, 1, 0x8000, 0x30000),
	{0},
	{0},
	{0},
	INIT_SIPC_CHN_SBUF(SIPC_BT_TX, WCNBUS_TX, SIPC_CHN_BT,
			   11, 4096, 16, 0x2400, 0x2400),
	INIT_SIPC_CHN_SBUF(SIPC_BT_RX, WCNBUS_RX, SIPC_CHN_BT,
			   10, 4096, 16, 0x2400, 0x2400),
	{0},
	{0},
	INIT_SIPC_CHN_SBUF(SIPC_FM_TX, WCNBUS_TX, SIPC_CHN_FM,
			   14, 128, 16, 0x2400, 0x2400),
	INIT_SIPC_CHN_SBUF(SIPC_FM_RX, WCNBUS_RX, SIPC_CHN_FM,
			   13, 128, 16, 0x2400, 0x2400),
	{0},
	{0},
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_CMD_TX, WCNBUS_TX, SIPC_CHN_WIFI_CMD,
			     4, 2048, 8, 2048, 0, 0, 0),
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_CMD_RX, WCNBUS_RX, SIPC_CHN_WIFI_CMD,
			     4, 2048, 8, 2048, 0, 0, 0),
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_DATA0_TX, WCNBUS_TX, SIPC_CHN_WIFI_DATA0,
			     64, 1664, 256, 1664, 0, 0, 0),
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_DATA0_RX, WCNBUS_RX, SIPC_CHN_WIFI_DATA0,
			     64, 1664, 256, 1664, 0, 0, 0),
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_DATA1_TX, WCNBUS_TX, SIPC_CHN_WIFI_DATA1,
			     64, 1664, 8, 1664, 0, 0, 0),
	INIT_SIPC_CHN_SBLOCK(SIPC_WIFI_DATA1_RX, WCNBUS_RX, SIPC_CHN_WIFI_DATA1,
			     64, 1664, 8, 1664, 0, 0, 0),
};

#define SIPC_INVALID_CHN(index) ((index >= SIPC_CHN_NUM) ? 1 : 0)
#define SIPC_TYPE(index) (g_sipc_chn[index].chntype)
#define SIPC_CHN_TYPE_SBUF(index)\
	(g_sipc_chn[index].chntype == SIPC_TYPE_SBUF)
#define SIPC_CHN_TYPE_SBLK(index)\
	(g_sipc_chn[index].chntype == SIPC_TYPE_SBLOCK)
#define SIPC_CHN(index) (&g_sipc_chn[index])
#define SIPC_CHN_DIR_TX(index)\
	(g_sipc_chn[index].inout == WCNBUS_TX ? 1 : 0)
#define SIPC_CHN_DIR_RX(index)\
	(g_sipc_chn[index].inout == WCNBUS_RX ? 1 : 0)

#define SIPC_CHN_STATUS(chn) (g_sipc_info.sipc_channel_state[chn])
#define SIPC_CHN_STATICTICS(index) (&g_sipc_chn[index].chn_static)

static int wcn_sipc_sbuf_push(u8 index,
		struct mbuf_t *head, struct mbuf_t *tail, int num);
static void wcn_sipc_sbuf_notifer(int event, void *data);
static void wcn_sipc_sbuf_push_list_dequeue(struct sipc_chn_info *sipc_chn);
static int wcn_sipc_sblk_push(u8 index,
		struct mbuf_t *head, struct mbuf_t *tail, int num);
static void wcn_sipc_sblk_notifer(int event, void *data);
static void wcn_sipc_sblk_push_list_dequeue(struct sipc_chn_info *sipc);

struct wcn_sipc_data_ops {
	int (*sipc_send)(u8 channel,
			struct mbuf_t *head, struct mbuf_t *tail, int num);
	void (*sipc_notifer)(int event, void *data);
	void (*sipc_push_list_dequeue)(struct sipc_chn_info *sipc_chn);
};

struct wcn_sipc_data_ops  sipc_data_ops[] = {
	{
		.sipc_send = wcn_sipc_sbuf_push,
		.sipc_notifer = wcn_sipc_sbuf_notifer,
		.sipc_push_list_dequeue = wcn_sipc_sbuf_push_list_dequeue,
	},
	{
		.sipc_send = wcn_sipc_sblk_push,
		.sipc_notifer = wcn_sipc_sblk_notifer,
		.sipc_push_list_dequeue = wcn_sipc_sblk_push_list_dequeue,
	},
};

static inline char *sipc_chn_tostr(int chn, int bufid)
{
	switch (chn) {
	case SIPC_CHN_ATCMD:
		if (bufid == 5)
			return "ATCMD";
		else if (bufid == 10 || bufid == 11)
			return "BT";
		else if (bufid == 13 || bufid == 14)
			return "FM";
		else
			return "Unknown Channel";
	case SIPC_CHN_LOG:
		return "LOG";
	case SIPC_CHN_LOOPCHECK:
		return "LOOPCHECK";
	case SIPC_CHN_ASSERT:
		return "ASSERT";
	case SIPC_CHN_WIFI_CMD:
		return "WIFICMD";
	case SIPC_CHN_WIFI_DATA0:
		return "WIFIDATA0";
	case SIPC_CHN_WIFI_DATA1:
		return "WIFIDATA1";
	default:
		return "Unknown Channel";
	}
}

static inline void wcn_sipc_record_buf_alloc_num(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->buf_alloc_num += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static inline void wcn_sipc_record_buf_free_num(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->buf_free_num += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static inline void wcn_sipc_record_mbuf_send_to_bus(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_send_to_bus += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static inline void wcn_sipc_record_mbuf_recv_from_bus(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_recv_from_bus += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static inline void wcn_sipc_record_mbuf_alloc_num(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_alloc_num += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static inline void wcn_sipc_record_mbuf_free_num(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_free_num += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static void wcn_sipc_record_mbuf_giveback_to_user(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_giveback_to_user += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

static void wcn_sipc_record_mbuf_recv_from_user(int index, int num)
{
	unsigned long flag;
	struct sipc_channel_statictics  *chn_static;

	chn_static = SIPC_CHN_STATICTICS(index);
	spin_lock_irqsave(&chn_static->lock, flag);
	chn_static->mbuf_recv_from_user += num;
	spin_unlock_irqrestore(&chn_static->lock, flag);
}

int wcn_sipc_channel_dir(int index)
{
	return g_sipc_chn[index].inout;
}

struct sipc_chn_info *wcn_sipc_channel_get(int index)
{
	return &g_sipc_chn[index];
}

static inline int wcn_sipc_buf_list_alloc(int chn,
					  struct mbuf_t **head,
					  struct mbuf_t **tail,
					  int *num)
{
	int ret = 0;

	WCN_HERE_CHN(chn);
	ret = buf_list_alloc(chn, head, tail, num);
	wcn_sipc_record_mbuf_alloc_num(chn, *num);

	return ret;
}

static inline int wcn_sipc_buf_list_free(int chn,
					 struct mbuf_t *head,
					 struct mbuf_t *tail,
					 int num)
{
	int ret = 0;

	WCN_HERE_CHN(chn);
	if (tail)
		tail->next = NULL;
	ret =  buf_list_free(chn, head, tail, num);
	wcn_sipc_record_mbuf_free_num(chn, num);

	return ret;
}

static void wcn_sipc_wakeup_tx(struct sipc_chn_info *sipc_chn)
{
	struct sipc_chn_info *tx_sipc_chn;

	WCN_HERE_CHN(sipc_chn->index);
	tx_sipc_chn = SIPC_CHN(sipc_chn->relate_index);
	WCN_HERE_CHN(tx_sipc_chn->index);
	complete(&tx_sipc_chn->callback_complete);
}

void wcn_sipc_pop_list_enqueue(struct sipc_chn_info *sipc_chn,
	struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct mbuf_t_list	*pop_queue = &sipc_chn->pop_queue;

	mutex_lock(&sipc_chn->popq_lock);
	if (!pop_queue->mbuf_head) {
		pop_queue->mbuf_head  = head;
		pop_queue->mbuf_tail = tail;
		pop_queue->mbuf_num = num;
	} else {
		pop_queue->mbuf_tail->next = head;
		pop_queue->mbuf_tail = tail;
		pop_queue->mbuf_num += num;
	}
	mutex_unlock(&sipc_chn->popq_lock);
	WCN_HERE_CHN(sipc_chn->index);
}

void wcn_sipc_pop_list_flush(struct sipc_chn_info *sipc_chn)
{
	struct mbuf_t_list	*pop_queue = &sipc_chn->pop_queue;

	mutex_lock(&sipc_chn->popq_lock);
	WCN_HERE_CHN(sipc_chn->index);
	if (pop_queue->mbuf_num) {
		WCN_DEBUG("index:%d  pop_queue->mbuf_num:%d",
			  sipc_chn->index, pop_queue->mbuf_num);
		pop_queue->mbuf_tail->next = NULL;
		if (sipc_chn->ops != NULL && sipc_chn->ops->pop_link != NULL)
			sipc_chn->ops->pop_link(sipc_chn->index,
				pop_queue->mbuf_head, pop_queue->mbuf_tail,
				pop_queue->mbuf_num);
		wcn_sipc_record_mbuf_giveback_to_user(sipc_chn->index,
			pop_queue->mbuf_num);
		pop_queue->mbuf_head = pop_queue->mbuf_tail = NULL;
		pop_queue->mbuf_num = 0;
	}
	mutex_unlock(&sipc_chn->popq_lock);

	/* re-send mbufs in push_list */
	if (sipc_chn->push_queue.mbuf_num &&
	    SIPC_CHN_TYPE_SBLK(sipc_chn->index)) {
		WCN_HERE_CHN(sipc_chn->index);
		wcn_sipc_wakeup_tx(sipc_chn);
	}
}

void wcn_sipc_push_list_enqueue(struct sipc_chn_info *sipc_chn,
		struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct mbuf_t_list	*push_queue = &sipc_chn->push_queue;

	mutex_lock(&sipc_chn->pushq_lock);
	if (!push_queue->mbuf_head) {
		push_queue->mbuf_head = head;
		push_queue->mbuf_tail = tail;
		push_queue->mbuf_num = num;
	} else {
		push_queue->mbuf_tail->next = head;
		push_queue->mbuf_tail = tail;
		push_queue->mbuf_num += num;
	}
	mutex_unlock(&sipc_chn->pushq_lock);
	WCN_HERE_CHN(sipc_chn->index);
}

static int wcn_sipc_recv(struct sipc_chn_info *sipc_chn,
			void *buf, int len)
{
	int ret;
	int num = 1;
	struct mbuf_t *head = NULL, *tail = NULL;

	WCN_DEBUG("sipc_recv: sipc_chn->index %d sipc_chn->chn %d\n",
		 sipc_chn->index, sipc_chn->chn);
	ret = wcn_sipc_buf_list_alloc(sipc_chn->index, &head, &tail, &num);
	if (ret || head == NULL || tail == NULL) {
		WCN_ERR("[%s] sprdwcn_bus_list_alloc fail, chn: %d\n",
			__func__, sipc_chn->index);
		return -1;
	}
	head->buf = buf;
	head->len = len;
	head->next = NULL;
	tail = head;

	wcn_sipc_pop_list_enqueue(sipc_chn, head, tail, num);
	wcn_sipc_pop_list_flush(sipc_chn);/* or in task */
	WCN_HERE_CHN(sipc_chn->index);

	return 0;
}

static int wcn_sipc_sbuf_send(struct sipc_chn_info *sipc_chn,
			void *buf, int len)
{
	int ret;
	void *xmit_buf = buf;

	/* wcn mdbg */
	if (sipc_chn->need_reserve)
		xmit_buf += PUB_HEAD_RSV;
	ret = sbuf_write(sipc_chn->dst, sipc_chn->chn,
			sipc_chn->sbuf.bufid, xmit_buf, len, 3000);
	WCN_DEBUG("sbuf index %d  chn[%d] write cnt=%d\n",
		 sipc_chn->index, sipc_chn->chn, ret);

	return ret;
}

static void wcn_sipc_sbuf_push_list_dequeue(struct sipc_chn_info *sipc_chn)
{
	int ret;
	struct mbuf_t *mbuf = NULL;

	WCN_HERE_CHN(sipc_chn->index);
	/* nothing to do */
	mutex_lock(&sipc_chn->pushq_lock);
	if (!sipc_chn->push_queue.mbuf_num) {
		mutex_unlock(&sipc_chn->pushq_lock);
		WCN_HERE_CHN(sipc_chn->index);
		return;
	}
	mbuf = sipc_chn->push_queue.mbuf_head;
	while (mbuf) {
		ret  = wcn_sipc_sbuf_send(sipc_chn, mbuf->buf, mbuf->len);
		if (ret < 0)
			break;
		wcn_sipc_record_mbuf_send_to_bus(sipc_chn->index, 1);
		wcn_sipc_pop_list_enqueue(sipc_chn, mbuf, mbuf, 1);
		mbuf = mbuf->next;
		sipc_chn->push_queue.mbuf_head = mbuf;
		sipc_chn->push_queue.mbuf_num--;
		if (!mbuf) {
			sipc_chn->push_queue.mbuf_tail = NULL;
			sipc_chn->push_queue.mbuf_num = 0;
		}
	}
	mutex_unlock(&sipc_chn->pushq_lock);
	wcn_sipc_pop_list_flush(sipc_chn);
}

int wcn_check_sbuf_status(u8 dst, u8 channel)
{
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(1000);
	while (1) {
		if (!sbuf_status(dst, channel)) {
			break;
		} else if (time_after(jiffies, timeout)) {
			WCN_INFO("channel %d-%d is not ready!\n",
				 dst, channel);
			return -E_INVALIDPARA;
		}
		msleep(20);
	}

	return 0;
}

static int wcn_sipc_sbuf_push(u8 index,
			struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct sipc_chn_info *sipc_chn;

	if (SIPC_INVALID_CHN(index))
		return -E_INVALIDPARA;
	sipc_chn = SIPC_CHN(index);
	if (wcn_check_sbuf_status(sipc_chn->dst, sipc_chn->chn))
		return -E_INVALIDPARA;

	wcn_sipc_record_mbuf_recv_from_user(index, num);
	wcn_sipc_push_list_enqueue(sipc_chn, head, tail, num);
	wcn_sipc_sbuf_push_list_dequeue(sipc_chn);

	return 0;
}

static void wcn_sipc_sbuf_notifer(int event, void *data)
{
	int cnt;
	int ret;
	void *recv_buf = NULL;
	u32 buf_len = 0;
	struct bus_puh_t *puh = NULL;
	struct sipc_chn_info *sipc_chn = (struct sipc_chn_info *)data;

	if (unlikely(!sipc_chn))
		return;

	switch (event) {
	case SBUF_NOTIFY_WRITE:
		break;
	case SBUF_NOTIFY_READ:
		buf_len = sipc_chn->sbuf.len;
		if (sipc_chn->need_reserve)
			buf_len += PUB_HEAD_RSV;
		do {
			recv_buf = kzalloc(buf_len, GFP_KERNEL);
			if (unlikely(!recv_buf)) {
				WCN_ERR("[%s]:mem alloc fail!\n", __func__);
				return;
			}
			wcn_sipc_record_buf_alloc_num(sipc_chn->chn, 1);

			if (sipc_chn->need_reserve)
				recv_buf += PUB_HEAD_RSV;
			WCN_DEBUG("sbuf index %d chn[%d]\n",
						sipc_chn->index, sipc_chn->chn);

			cnt = sbuf_read(sipc_chn->dst,
					sipc_chn->chn,
					sipc_chn->sbuf.bufid,
					recv_buf,
					sipc_chn->sbuf.len, 0);
			if (sipc_chn->need_reserve) {
				puh = (struct bus_puh_t *)recv_buf;
				puh->len = cnt;
			}

			WCN_DEBUG("sbuf index %d chn[%d] read cnt=%d\n",
				  sipc_chn->index, sipc_chn->chn, cnt);
			if (cnt < 0) {
				WCN_DEBUG("sbuf read cnt[%d] invalid\n", cnt);
				kfree(recv_buf);
				return;
			}
			wcn_sipc_record_mbuf_recv_from_bus(sipc_chn->index, 1);
			ret = wcn_sipc_recv(sipc_chn, recv_buf, cnt);
			if (ret < 0) {
				WCN_DEBUG("sbuf recv fail[%d]\n", ret);
				kfree(recv_buf);
				return;
			}
		} while (cnt == buf_len);
		break;
	default:
		WCN_ERR("sbuf read event[%d] invalid\n", event);
	}
}

static int wcn_sipc_sblk_send(struct sipc_chn_info *sipc_chn,
						void *buf, int len)
{
	int ret;
	u8 *addr = NULL;
	struct sblock blk;

	WCN_HERE_CHN(sipc_chn->index);
	/* get a free sblock. */
	ret = sblock_get(sipc_chn->dst, sipc_chn->chn, &blk, 0);
	if (ret) {
		WCN_ERR("[%s]:Failed to get free sblock(%d)!\n",
			sipc_chn_tostr(sipc_chn->chn, 0), ret);
		return -ENOMEM;
	}
	WCN_HERE_CHN(sipc_chn->index);
	if (blk.length < len) {
		WCN_ERR("[%s]:The size of sblock is so tiny!len:%d,blk.length:%d\n",
			sipc_chn_tostr(sipc_chn->chn, 0), len, blk.length);
		sblock_put(sipc_chn->dst, sipc_chn->chn, &blk);
		WARN_ON(1);
		return E_INVALIDPARA;
	}
	addr = (u8 *)blk.addr + SIPC_SBLOCK_HEAD_RESERV;
	blk.length = len + SIPC_SBLOCK_HEAD_RESERV;
	if (sipc_chn->index == SIPC_WIFI_DATA0_TX)
	    WCN_DEBUG("sipc sblk send. buf: %p, addr: %p, blk.length: %d\n",
		     buf, addr, blk.length);
	if (!buf) {
		WCN_ERR("buf is null. buf: %p\n", buf);
		return E_INVALIDPARA;
	}

	memcpy_toio(((u8 *)addr), buf, len);
	ret = sblock_send(sipc_chn->dst, sipc_chn->chn, &blk);
	WCN_HERE_CHN(sipc_chn->index);
	if (ret) {
		WCN_ERR("[%s]:err:%d\n", sipc_chn_tostr(sipc_chn->chn, 0), ret);
		sblock_put(sipc_chn->dst, sipc_chn->chn, &blk);
	}

	return ret;
}

static void wcn_sipc_sblk_push_list_dequeue(struct sipc_chn_info *sipc_chn)
{
	int ret;
	int free_blk_num = 0;
	struct mbuf_t *mbuf = NULL;

	WCN_HERE_CHN(sipc_chn->index);
	mutex_lock(&sipc_chn->pushq_lock);
	/* nothing to do */
	if (!sipc_chn->push_queue.mbuf_num) {
		mutex_unlock(&sipc_chn->pushq_lock);
		WCN_INFO("channel %d-%d(%d), chn_deinit?\n",
			sipc_chn->dst, sipc_chn->chn, sipc_chn->index);
		WCN_HERE_CHN(sipc_chn->index);
		return;
	}
	free_blk_num  = sblock_get_free_count(sipc_chn->dst, sipc_chn->chn);
	/* sblock busy */
	if (free_blk_num <= 0) {
		WCN_HERE_CHN(sipc_chn->index);
		mutex_unlock(&sipc_chn->pushq_lock);
		return;
	}

	mbuf = sipc_chn->push_queue.mbuf_head;
	while (free_blk_num-- && mbuf) {
		ret  = wcn_sipc_sblk_send(sipc_chn, mbuf->buf, mbuf->len);
		WCN_DEBUG("%s %d free_blk_num %d ret %d ",
			  __func__, __LINE__, free_blk_num, ret);
		if (ret)
			break;
		WCN_HERE_CHN(sipc_chn->index);
		wcn_sipc_record_mbuf_send_to_bus(sipc_chn->index, 1);
		wcn_sipc_pop_list_enqueue(sipc_chn, mbuf, mbuf, 1);
		WCN_HERE_CHN(sipc_chn->index);
		WCN_DEBUG("mbuf %p\n", mbuf);
		mbuf = mbuf->next;
		sipc_chn->push_queue.mbuf_head = mbuf;
		sipc_chn->push_queue.mbuf_num--;
		if (!mbuf) {
			sipc_chn->push_queue.mbuf_tail = NULL;
			sipc_chn->push_queue.mbuf_num = 0;
		}
	}
	wcn_sipc_pop_list_flush(sipc_chn);
	mutex_unlock(&sipc_chn->pushq_lock);
	WCN_HERE_CHN(sipc_chn->index);
}

int wcn_sipc_sblk_chn_rx_status_check(u8 index)
{
	struct sipc_chn_info *sipc_chn;

	sipc_chn = SIPC_CHN(index + 1);
	if (!sipc_chn->sipc_chn_status) {
		WCN_ERR("chn status(%d)! sipc_chn(%d)\n",
				 sipc_chn->sipc_chn_status, sipc_chn->chn);
		return -E_INVALIDPARA;
	} else
	return 0;
}

static int wcn_sipc_sblk_push(u8 index,
			struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct sipc_chn_info *sipc_chn;

	if (unlikely(SIPC_INVALID_CHN(index)))
		return -E_INVALIDPARA;

	sipc_chn = SIPC_CHN(index);
	if (wcn_sipc_sblk_chn_rx_status_check(index) != 0) {
		WCN_ERR("sipc chn %d not created!", sipc_chn->chn);
		return -E_INVALIDPARA;
	}
	wcn_sipc_record_mbuf_recv_from_user(index, num);
	wcn_sipc_push_list_enqueue(sipc_chn, head, tail, num);
	wcn_sipc_wakeup_tx(sipc_chn);

	WCN_HERE_CHN(index);
	return 0;
}

static void wcn_sipc_sblk_recv(struct sipc_chn_info *sipc_chn)
{
	u32 length = 0;
	int ret;
	struct sblock blk;

	WCN_DEBUG("[%s] idx %d recv sblock msg",
		  sipc_chn_tostr(sipc_chn->chn, 0), sipc_chn->index);

	while (!sblock_receive(sipc_chn->dst, sipc_chn->chn, &blk, 0)) {
		length = blk.length - SIPC_SBLOCK_HEAD_RESERV;
		WCN_DEBUG("sblk length %d", length);
		wcn_sipc_record_mbuf_recv_from_bus(sipc_chn->index, 1);
		if (sipc_chn->index == SIPC_WIFI_DATA0_RX)
			WCN_DEBUG("sipc sblk send. blk.addr: %p, length: %d\n",
				 blk.addr, length);
		wcn_sipc_recv(sipc_chn,
			      (u8 *)blk.addr + SIPC_SBLOCK_HEAD_RESERV, length);
		ret = sblock_release(sipc_chn->dst, sipc_chn->chn, &blk);
		if (ret)
			WCN_ERR("release sblock[%d] err:%d\n",
				sipc_chn->chn, ret);
	}
}

void wcn_sipc_chn_set_status(void *data, bool flag)
{
	struct sipc_chn_info *sipc_chn = (struct sipc_chn_info *)data;
	WCN_INFO("wcn_sipc_chn_set_status chn: %d ,  flag:%d\n", sipc_chn->chn, flag);
	if (flag)
		sipc_chn->sipc_chn_status = true;
	else
		sipc_chn->sipc_chn_status = false;
}

void wcn_sipc_chn_set_status_all_false(void)
{
	int index;

	for (index = 0; index < SIPC_CHN_NUM; index++) {
		if (g_sipc_chn[index].dst != SIPC_WCN_DST)
			continue;
		wcn_sipc_chn_set_status(&g_sipc_chn[index], false);
	}
}

static void wcn_sipc_sblk_notifer(int event, void *data)
{
	struct sipc_chn_info *sipc_chn = (struct sipc_chn_info *)data;

	if (unlikely(!sipc_chn))
		return;
	WCN_INFO("%s  %d index:%d  event:%x",
		  __func__, __LINE__, sipc_chn->index, event);
	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		wcn_sipc_sblk_recv(sipc_chn);
		break;
	/* SBLOCK_NOTIFY_GET sblock release */
	case SBLOCK_NOTIFY_GET:
		wcn_sipc_wakeup_tx(sipc_chn);
		break;
	case SBLOCK_NOTIFY_OPEN:
		wcn_sipc_chn_set_status(sipc_chn, true);
		break;
	case SBLOCK_NOTIFY_CLOSE:
		wcn_sipc_chn_set_status(sipc_chn, false);
		break;
	default:
		WCN_ERR("Invalid event sblock notify:%d\n", event);
		break;
	}
}

static int wcn_sipc_push_list(int index, struct mbuf_t *head,
			     struct mbuf_t *tail, int num)
{
	int ret = -1;
	struct mchn_ops_t *wcn_sipc_ops = NULL;
	struct mbuf_t *mbuf;
	int i = 0;

	wcn_sipc_ops = chn_ops(index);
	if (unlikely(!wcn_sipc_ops))
		return -E_NULLPOINT;

	if (wcn_sipc_ops->inout == WCNBUS_TX) {
		if (!wcn_push_list_condition_check(head, tail, num)) {
			WCN_INFO("%s WCN is asserting, cancel send.index=%d",
				__func__, index);
			return -E_INVALIDPARA;
		}

		ret = sipc_data_ops[SIPC_TYPE(index)].sipc_send(
					index, head, tail, num);
		if (ret < 0)
			return ret;
	} else if (wcn_sipc_ops->inout == WCNBUS_RX) {
		if (SIPC_CHN_TYPE_SBUF(index)) {
			WCN_HERE_CHN(index);
			/* free mbuf smem */
			mbuf_list_iter(head, num, mbuf, i) {
				kfree(mbuf->buf);
			}
			wcn_sipc_record_buf_free_num(index, num);
		}
		WCN_HERE_CHN(index);
		/* free mbuf */
		wcn_sipc_buf_list_free(index, head, tail, num);
		ret = 0;
	} else {
		return -E_INVALIDPARA;
	}

	return ret;
}

extern char wcn_assert_str[128];
static inline unsigned int wcn_sipc_get_status(void)
{
	if (g_sipc_info.sipc_chn_status != 0)
		WCN_ERR("fw assert:%s\n", wcn_assert_str);

	return g_sipc_info.sipc_chn_status;
}

static inline void wcn_sipc_set_status(unsigned int flag)
{
	mutex_lock(&g_sipc_info.status_lock);
	g_sipc_info.sipc_chn_status = flag;
	mutex_unlock(&g_sipc_info.status_lock);
}

static unsigned long long wcn_sipc_get_rxcnt(void)
{
	return wcn_get_cp2_comm_rx_count();
}

static enum wcn_hard_intf_type wcn_sipc_get_hwintf_type(void)
{
	return HW_TYPE_SIPC;
}

int wcn_sipc_work_func(void *work)
{
	u8 chntype = 0;
	struct sipc_chn_info *sipc_chn = (struct sipc_chn_info *)work;
	struct sched_param param = {.sched_priority = 91};

	/* set the thread as a real time thread, and its priority is 90 */
	sched_setscheduler(current, SCHED_RR, &param);

RETRY:
	if (SIPC_CHN_STATUS(sipc_chn->chn) == SIPC_CHANNEL_UNCREATED)
		return -1;
	reinit_completion(&sipc_chn->callback_complete);
	WCN_HERE_CHN(sipc_chn->index);
	chntype = SIPC_TYPE(sipc_chn->index);
	sipc_data_ops[chntype].sipc_push_list_dequeue(sipc_chn);
	wait_for_completion(&sipc_chn->callback_complete);
	WCN_HERE_CHN(sipc_chn->index);
	goto RETRY;

	return 0;
}
int wcn_sipc_chn_work_init(struct sipc_chn_info *sipc_chn)
{
	sipc_chn->wcn_sipc_thread = kthread_create(wcn_sipc_work_func, sipc_chn,
			"WCN_SIPC_TX_THREAD%u", sipc_chn->index);
	init_completion(&sipc_chn->callback_complete);
	if (sipc_chn->wcn_sipc_thread)
		wake_up_process(sipc_chn->wcn_sipc_thread);
	else
		WCN_ERR("%s create a new thread failed\n", __func__);

	return 0;
}

static int wcn_sipc_chn_init(struct mchn_ops_t *ops)
{
	int ret;
	u8 chntype = 0;
	int idx = 0;
	struct sipc_chn_info *sipc_chn;

	idx = ops->channel;
	if (unlikely(SIPC_INVALID_CHN(idx)))
		return -E_INVALIDPARA;
	sipc_chn = SIPC_CHN(idx);
	WCN_INFO("[%s]:index[%d] chn[%d]\n", __func__, idx, sipc_chn->chn);
	ops->inout = sipc_chn->inout;
	chntype = sipc_chn->chntype;
	if (SIPC_CHN_TYPE_SBUF(idx)) {
		/* sbuf */
		WCN_DEBUG("bufid[%d] len[%d] bufnum[%d]\n",
			  sipc_chn->sbuf.bufid,
			  sipc_chn->sbuf.len,
			  sipc_chn->sbuf.bufnum);
		/* if marlin2-integrate, create spipe(4) in dts*/
		if (g_sipc_info.sipc_wcn_version == 0 &&
				sipc_chn->chn == SIPC_CHN_ATCMD) {
			WCN_DEBUG("sipc channel 4 already created in dts!\n");
			SIPC_CHN_STATUS(sipc_chn->chn) = SIPC_CHANNEL_CREATED;
		}

		if (SIPC_CHN_STATUS(sipc_chn->chn) == SIPC_CHANNEL_UNCREATED) {
			ret = sbuf_create(sipc_chn->dst, sipc_chn->chn,
					  sipc_chn->sbuf.bufnum,
					  sipc_chn->sbuf.txbufsize,
					  sipc_chn->sbuf.rxbufsize);
			if (ret < 0) {
				WCN_ERR("sbuf chn[%d] create fail!\n", idx);
				return ret;
			}
			SIPC_CHN_STATUS(sipc_chn->chn) = SIPC_CHANNEL_CREATED;
		}
		if (ops->inout == WCNBUS_RX) {
			ret = sbuf_register_notifier(
					sipc_chn->dst,
					sipc_chn->chn,
					sipc_chn->sbuf.bufid,
					sipc_data_ops[chntype].sipc_notifer,
					sipc_chn);
			if (ret < 0) {
				WCN_ERR("sbuf chn[%d] register fail!\n", idx);
				return ret;
			}
		}
		WCN_INFO("sbuf chn[%d] create success!\n", idx);
	} else if (SIPC_CHN_TYPE_SBLK(idx)) {
		WCN_DEBUG("tbnum[%d] tbsz[%d] rbnum[%d] rbsz[%d]\n",
			  sipc_chn->sblk.txblocknum,
			  sipc_chn->sblk.txblocksize,
			  sipc_chn->sblk.rxblocknum,
			  sipc_chn->sblk.rxblocksize);

		/* rx chn record tx chn */
		sipc_chn->relate_index = sipc_chn->index;
		/* sblock */
		if (SIPC_CHN_STATUS(sipc_chn->chn) == SIPC_CHANNEL_UNCREATED) {
			ret = sblock_create(sipc_chn->dst, sipc_chn->chn, sipc_chn->sblk.txblocknum,	\
				sipc_chn->sblk.txblocksize, sipc_chn->sblk.rxblocknum, sipc_chn->sblk.rxblocksize);
			if (ret < 0) {
				WCN_ERR("sblock chn[%d] create fail!\n", idx);
				return ret;
			}
			SIPC_CHN_STATUS(sipc_chn->chn) = SIPC_CHANNEL_CREATED;
		}
		if (SIPC_CHN_DIR_RX(idx)) {
			ret = sblock_register_notifier(
					sipc_chn->dst,
					sipc_chn->chn,
					sipc_data_ops[chntype].sipc_notifer,
					sipc_chn);
			if (ret < 0) {
				WCN_ERR("sblock chn[%d] register fail!\n",
					idx);
				sblock_destroy(sipc_chn->dst, sipc_chn->chn);
				return ret;
			}
			sipc_chn->relate_index = sipc_chn->index - 1;
		} else if (SIPC_CHN_DIR_TX(idx)) {
			/* tx init work task */
			if (!sipc_chn->wcn_sipc_thread) {
				ret = wcn_sipc_chn_work_init(sipc_chn);
				if (ret < 0) {
					WCN_ERR("work queue create fail!");
					return ret;
				}
			}
		}
		WCN_INFO("sblock chn[%d] create success!\n", idx);
	} else {
		WCN_ERR("invalid sipc type!");
		return -E_INVALIDPARA;
	}

	bus_chn_init(ops, HW_TYPE_SIPC);
	sipc_chn->ops = ops;
	WCN_INFO("sipc chn[%d] init success!\n", idx);

	return 0;
}

static int wcn_sipc_chn_deinit(struct mchn_ops_t *ops)
{
	int idx = ops->channel;

	struct sipc_chn_info *sipc_chn;
	struct sipc_chn_info *tx_sipc_chn = NULL;

	sipc_chn = SIPC_CHN(idx);
	sipc_chn->ops = NULL;
	WCN_INFO("[%s]:index[%d] chn[%d], sipc_chn->ops = null.\n", __func__, idx, sipc_chn->chn);

	tx_sipc_chn = SIPC_CHN(sipc_chn->relate_index);
	if (SIPC_CHN_TYPE_SBLK(idx) && SIPC_CHN_DIR_TX(idx)) {
		WCN_INFO("Wait %d-%d index%d push\n",
			tx_sipc_chn->dst, tx_sipc_chn->chn, tx_sipc_chn->index);
		mutex_lock(&tx_sipc_chn->pushq_lock);
		/* WARNING: wcn_sipc_sblk_push_list_dequeue done */
		tx_sipc_chn->push_queue.mbuf_num = 0;
	}

	bus_chn_deinit(ops);

	if (SIPC_CHN_TYPE_SBLK(idx) && SIPC_CHN_DIR_TX(idx))
		mutex_unlock(&tx_sipc_chn->pushq_lock);

	/* only destroy when chn created fail so it can create again.  */
	if (SIPC_CHN_TYPE_SBLK(idx)) {
		if (SIPC_CHN_DIR_TX(idx) && wcn_sipc_sblk_chn_rx_status_check(idx) != 0) {
			sblock_destroy(sipc_chn->dst, sipc_chn->chn);
			SIPC_CHN_STATUS(sipc_chn->chn) = SIPC_CHANNEL_UNCREATED;
			WCN_INFO("sipc chn[%d] deinit and destroy!\n", idx);
		}
	}

	/* for chn created success,we don't release sipc resource for now */
	WCN_INFO("sipc chn[%d] deinit success!\n", idx);

	return 0;
}

static void wcn_sipc_resource_init(void)
{
	int index;

	mutex_init(&g_sipc_info.status_lock);
	for (index = 0; index < SIPC_CHN_NUM; index++) {
		if (g_sipc_chn[index].dst != SIPC_WCN_DST)
			continue;
		mutex_init(&g_sipc_chn[index].pushq_lock);
		mutex_init(&g_sipc_chn[index].popq_lock);
		spin_lock_init(&g_sipc_chn[index].chn_static.lock);
	}
}
static void wcn_sipc_resource_deinit(void)
{
	int index;

	for (index = 0; index < SIPC_CHN_NUM; index++) {
		if (g_sipc_chn[index].dst != SIPC_WCN_DST)
			continue;
		mutex_destroy(&g_sipc_chn[index].pushq_lock);
		mutex_destroy(&g_sipc_chn[index].popq_lock);
		spin_unlock(&g_sipc_chn[index].chn_static.lock);
	}
	mutex_destroy(&g_sipc_info.status_lock);
}

static void wcn_sipc_module_init(void)
{
	wcn_sipc_resource_init();
	WCN_INFO("sipc module init success\n");
}

static void wcn_sipc_module_deinit(void)
{
	wcn_sipc_resource_deinit();
	WCN_INFO("sipc module deinit success\n");
}

static int wcn_sipc_parse_dt(void)
{
	int ret;
	struct device_node *np;

	np = of_find_node_by_name(NULL, "cpwcn-btwf");
	if (!np) {
		pr_err("cpwcn-btwf dts node not found");
		return -1;
	}

	g_sipc_info.np = np;

	ret = of_property_read_u32(np, "sprd,wcn-sipc-ver",
				   &g_sipc_info.sipc_wcn_version);
	if (ret)
		WCN_ERR("wcn-sipc-ver parse fail!\n");
	WCN_INFO("wcn-sipc-ver:%d\n", g_sipc_info.sipc_wcn_version);

	return ret;
}

#if defined(CONFIG_DEBUG_FS)
static int wcn_sipc_debug_show(struct seq_file *m, void *private)
{
	struct sipc_chn_info *sipc_chn;
	int i;

	seq_puts(m, "sipc chn info\n");
	seq_puts(m, "************************************\n");
	for (i = 0; i < SIPC_CHN_NUM; i++) {
		sipc_chn = SIPC_CHN(i);
		if (!sipc_chn || !sipc_chn->chn)
			continue;
		seq_printf(m,
			"index:%d inout:%d chntype:%d channel: %d",
			sipc_chn->index,
			sipc_chn->inout,
			sipc_chn->chntype,
			sipc_chn->chn);
		seq_printf(m,
			"chn_status %d pushq_num %d popq_num %d\n",
			sipc_chn->sipc_chn_status,
			sipc_chn->push_queue.mbuf_num,
			sipc_chn->pop_queue.mbuf_num);
	}
	seq_puts(m, "sipc chn statics info\n");
	seq_puts(m, "************************************\n");
	for (i = 0; i < SIPC_CHN_NUM; i++) {
		sipc_chn = SIPC_CHN(i);
		if (!sipc_chn || !sipc_chn->chn)
			continue;
		seq_printf(m, "\tindex:%.8u", sipc_chn->index);
		seq_printf(m, "\tuser_send %.8lld\tgivebackto_user %.8lld",
			sipc_chn->chn_static.mbuf_recv_from_user,
			sipc_chn->chn_static.mbuf_giveback_to_user);
		seq_printf(m, "\tbus_send %.8lld \tbus_recv %.8lld",
			sipc_chn->chn_static.mbuf_send_to_bus,
			sipc_chn->chn_static.mbuf_recv_from_bus);
		seq_printf(m, "\tmbuf_alloc %.8lld \tmbuf_free %.8lld",
			sipc_chn->chn_static.mbuf_alloc_num,
			sipc_chn->chn_static.mbuf_free_num);
		seq_printf(m, "\tbuf_alloc %.8lld \tbuf_free %.8lld\n",
			sipc_chn->chn_static.buf_alloc_num,
			sipc_chn->chn_static.buf_free_num);
	}

	return 0;
}

static int wcn_sipc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, wcn_sipc_debug_show, inode->i_private);
}

static const struct file_operations wcn_sipc_debug_fops = {
	.open = wcn_sipc_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int wcn_sipc_init_debugfs(void)
{
	struct dentry *root = debugfs_create_dir("wcn_sipc", NULL);

	if (!root)
		return -ENXIO;
	debugfs_create_file("sipc_chn", 0x0444,
			    (struct dentry *)root,
			    NULL, &wcn_sipc_debug_fops);
	return 0;
}
#endif

int wcn_sipc_preinit(void)
{
	WCN_INFO("sipc module preinit\n");

#if defined(CONFIG_DEBUG_FS)
	wcn_sipc_init_debugfs();
#endif

	/* parse sipc config from dts */
	wcn_sipc_parse_dt();

	return 0;
}

static struct sprdwcn_bus_ops sipc_bus_ops = {
	.preinit = wcn_sipc_preinit,
	.chn_init = wcn_sipc_chn_init,
	.chn_deinit = wcn_sipc_chn_deinit,
	.list_alloc = wcn_sipc_buf_list_alloc,
	.list_free = wcn_sipc_buf_list_free,
	.push_list = wcn_sipc_push_list,
	.get_hwintf_type = wcn_sipc_get_hwintf_type,
	.get_carddump_status = wcn_sipc_get_status,
	.set_carddump_status = wcn_sipc_set_status,
	.get_rx_total_cnt = wcn_sipc_get_rxcnt,
};

void module_bus_sipc_init(void)
{
	wcn_sipc_module_init();
	module_ops_register(&sipc_bus_ops);
	WCN_INFO("sipc bus init success\n");
}
EXPORT_SYMBOL(module_bus_sipc_init);

void module_bus_sipc_deinit(void)
{
	module_ops_unregister();
	wcn_sipc_module_deinit();
	WCN_INFO("sipc bus deinit success\n");
}
EXPORT_SYMBOL(module_bus_sipc_deinit);
