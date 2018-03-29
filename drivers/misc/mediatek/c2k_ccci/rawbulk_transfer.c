/*
 * Rawbulk Driver from VIA Telecom
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Karfield Chen (kfchen@via-telecom.com)
 * Copyright (C) 2012 VIA Telecom, Inc.
 * Author: Juelun Guo (jlguo@via-telecom.com)
 * Changes:
 *
 * Sep 2012: Juelun Guo <jlguo@via-telecom.com>
 *           Version 1.0.4
 *           changed to support for sdio bypass.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *
 * Rawbulk is transfer performer between CBP host driver and Gadget driver
 *
 *
 * upstream:    CBP Driver ---> Gadget IN
 * downstream:  Gadget OUT ---> CBP Driver
 *
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#define DRIVER_AUTHOR   "Juelun Guo <jlguo@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Driver - perform bypass for QingCheng"
#define DRIVER_VERSION  "1.0.4"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "viatel_rawbulk.h"
#include <linux/moduleparam.h>
#include "modem_sdio.h"

#define terr(t, fmt, args...) pr_err("Rawbulk [%s]:" fmt "\n", t->name,  ##args)
#define STOP_UPSTREAM   0x1
#define STOP_DOWNSTREAM 0x2

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
char *transfer_name[] = { "modem", "ets", "at", "pcv", "gps" };
#else
char *transfer_name[] = { "pcv", "modem", "ets", "at", "gps" };
#endif

unsigned int upstream_data[_MAX_TID] = { 0 };
unsigned int upstream_cnt[_MAX_TID] = { 0 };
unsigned int total_drop[_MAX_TID] = { 0 };
unsigned int alloc_fail[_MAX_TID] = { 0 };
unsigned int total_tran[_MAX_TID] = { 0 };

static unsigned long drop_check_timeout;
static unsigned int udata[_MAX_TID] = { 0 };
static unsigned int ucnt[_MAX_TID] = { 0 };

struct rawbulk_transfer {
	enum transfer_id id;
	spinlock_t lock;
	int control;

	struct usb_function *function;
	struct usb_interface *interface;
	rawbulk_autoreconn_callback_t autoreconn;
	struct {
		int ntrans;
		struct list_head transactions;
		struct usb_ep *ep;
	} upstream, downstream, repush2modem, cache_buf_lists;

	int sdio_block;
	int down_flow;
	spinlock_t usb_down_lock;
	spinlock_t modem_block_lock;
	struct delayed_work delayed;
	struct workqueue_struct *flow_wq;

	struct work_struct read_work;
	struct work_struct write_work;
	struct workqueue_struct *rx_wq;
	struct workqueue_struct *tx_wq;
	struct mutex modem_up_mutex;
	struct mutex usb_up_mutex;
	struct timer_list timer;
	spinlock_t flow_lock;
};

static inline int get_epnum(struct usb_host_endpoint *ep)
{
	return (int)(ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
}

static inline int get_maxpacksize(struct usb_host_endpoint *ep)
{
	return (int)(le16_to_cpu(ep->desc.wMaxPacketSize));
}

struct cache_buf {
	int length;
	struct list_head clist;
	struct rawbulk_transfer *transfer;
	int state;
	/* unsigned char buffer[0]; */
	char *buffer;
};

#define MAX_RESPONSE    32
struct rawbulk_transfer_model {
	struct usb_device *udev;
	struct usb_composite_dev *cdev;
	char ctrl_response[MAX_RESPONSE];
	struct rawbulk_transfer transfer[_MAX_TID];
};
static struct rawbulk_transfer_model *rawbulk;

static struct rawbulk_transfer *id_to_transfer(int transfer_id)
{
	if (transfer_id < 0 || transfer_id >= _MAX_TID)
		return NULL;
	return &rawbulk->transfer[transfer_id];
}

/* extern int rawbulk_usb_state_check(void); */

/*
 * upstream
 */

#define UPSTREAM_STAT_FREE          0
#define UPSTREAM_STAT_UPLOADING     2

struct upstream_transaction {
	int state;
	int stalled;
	char name[32];
	struct list_head tlist;
	struct delayed_work delayed;
	struct rawbulk_transfer *transfer;
	struct usb_request *req;
	int buffer_length;
	/* unsigned char buffer[0]; */
	char *buffer;
};

static unsigned int dump_mask;
static unsigned int full_dump;
static unsigned int max_cache_cnt = 2048;
static unsigned int base_cache_cnt = 1024;
static unsigned int up_note_sz = 1024 * 1024;
static unsigned int drop_check_interval = 1;
unsigned int c2k_usb_dbg_level = C2K_LOG_NOTICE;

module_param(c2k_usb_dbg_level, uint, S_IRUGO | S_IWUSR);
module_param(dump_mask, uint, S_IRUGO | S_IWUSR);
module_param(full_dump, uint, S_IRUGO | S_IWUSR);
module_param(max_cache_cnt, uint, S_IRUGO | S_IWUSR);
module_param(base_cache_cnt, uint, S_IRUGO | S_IWUSR);
module_param(drop_check_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dump_mask, "Set data dump mask for each transfers");

#ifdef C2K_USB_UT
int delay_set = 1200;
module_param(delay_set, uint, S_IRUGO | S_IWUSR);
#endif

static inline void dump_data(struct rawbulk_transfer *trans,
			     const char *str, const unsigned char *data, int size)
{
	int i;
	char verb[128], *pbuf;

	if (!(dump_mask & (1 << trans->id)))
		return;

	pbuf = verb;
	pbuf += sprintf(pbuf, "DUMP tid = %d, %s: len = %d, chars = \"", trans->id, str, size);

	/* data in ascii */
#if 0
	for (i = 0; i < size; ++i) {
		char c = data[i];

		if (c > 0x20 && c < 0x7e)
			pbuf += sprintf(pbuf, "%c", c);
		else
			pbuf += sprintf(pbuf, ".");

		if (i > 7)
			break;
	}
#endif

	pbuf += sprintf(pbuf, "\", data = ");
	for (i = 0; i < size; ++i) {
		pbuf += sprintf(pbuf, "%.2x ", data[i]);
		if (!full_dump) {
			if (i > 7)
				break;
		}
	}
	if (full_dump || size < 8) {
		C2K_NOTE("%s\n", verb);
		return;
	}
	/* data in tail */
#if 1
	else if (i < size - 8) {
		pbuf += sprintf(pbuf, "... ");
		i = size - 8;
	}
	for (; i < size; ++i)
		pbuf += sprintf(pbuf, "%.2x ", data[i]);
#endif
	C2K_NOTE("%s\n", verb);
}

static struct upstream_transaction *alloc_upstream_transaction(struct rawbulk_transfer *transfer,
							       int bufsz)
{
	struct upstream_transaction *t;

	C2K_NOTE("%s\n", __func__);

	/* t = kmalloc(sizeof *t + bufsz * sizeof(unsigned char), GFP_KERNEL); */
	t = kmalloc(sizeof(struct upstream_transaction), GFP_KERNEL);
	if (!t)
		return NULL;

#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
	t->buffer = (char *)__get_free_page(GFP_KERNEL | GFP_DMA);
#else
	t->buffer = (char *)__get_free_page(GFP_KERNEL);
#endif
	/* t->buffer = kmalloc(bufsz, GFP_KERNEL); */
	if (!t->buffer) {
		kfree(t);
		return NULL;
	}
	t->buffer_length = bufsz;

	t->req = usb_ep_alloc_request(transfer->upstream.ep, GFP_KERNEL);
	if (!t->req)
		goto failto_alloc_usb_request;
	t->req->context = t;
	t->name[0] = 0;
	sprintf(t->name, "U%d ( G:%s)", transfer->upstream.ntrans, transfer->upstream.ep->name);

	INIT_LIST_HEAD(&t->tlist);
	list_add_tail(&t->tlist, &transfer->upstream.transactions);
	transfer->upstream.ntrans++;
	t->transfer = transfer;
	t->state = UPSTREAM_STAT_FREE;
	return t;

failto_alloc_usb_request:
	/* kfree(t->buffer); */
	free_page((unsigned long)t->buffer);
	kfree(t);
	return NULL;
}

static void free_upstream_transaction(struct rawbulk_transfer *transfer)
{
	struct list_head *p, *n;

	C2K_DBG("%s\n", __func__);

	mutex_lock(&transfer->usb_up_mutex);
	list_for_each_safe(p, n, &transfer->upstream.transactions) {
		struct upstream_transaction *t = list_entry(p, struct
							    upstream_transaction, tlist);

		list_del(p);
		/* kfree(t->buffer); */
		free_page((unsigned long)t->buffer);
		usb_ep_free_request(transfer->upstream.ep, t->req);
		kfree(t);

		transfer->upstream.ntrans--;
	}
	mutex_unlock(&transfer->usb_up_mutex);
}

static void free_upstream_sdio_buf(struct rawbulk_transfer *transfer)
{
	struct list_head *p, *n;

	C2K_DBG("%s\n", __func__);

	mutex_lock(&transfer->modem_up_mutex);
	list_for_each_safe(p, n, &transfer->cache_buf_lists.transactions) {
		struct cache_buf *c = list_entry(p, struct
						 cache_buf, clist);
		list_del(p);
		/* kfree(c->buffer); */
		free_page((unsigned long)c->buffer);
		kfree(c);
		transfer->cache_buf_lists.ntrans--;
	}
	mutex_unlock(&transfer->modem_up_mutex);
}

static void upstream_complete(struct usb_ep *ep, struct usb_request
			      *req);

static void start_upstream(struct work_struct *work)
{
	int ret = -1, got = 0;
	struct upstream_transaction *t;
	struct rawbulk_transfer *transfer = container_of(work, struct rawbulk_transfer, write_work);
	struct cache_buf *c;
	int length;
	char *buffer;
	int retry = 0;
	struct usb_request *req;

	C2K_DBG("%s\n", __func__);

	mutex_lock(&transfer->modem_up_mutex);

	list_for_each_entry(c, &transfer->cache_buf_lists.transactions, clist) {
		if (c && (c->state == UPSTREAM_STAT_UPLOADING)
		    && !(transfer->control & STOP_UPSTREAM)) {
			ret = 0;
			break;
		}
	}
	mutex_unlock(&transfer->modem_up_mutex);

	if (ret < 0) {
		C2K_DBG("%s\n", __func__);
		return;
	}

	if (!c) {
		C2K_DBG("%s\n", __func__);
		return;
	}

	length = c->length;
	buffer = c->buffer;

reget:
	mutex_lock(&transfer->usb_up_mutex);
	list_for_each_entry(t, &transfer->upstream.transactions, tlist) {
		if (t && (t->state == UPSTREAM_STAT_FREE) && !(transfer->control & STOP_UPSTREAM)) {
			ret = 0;
			retry = 0;
			got = 1;
			break;
		}
	}
	mutex_unlock(&transfer->usb_up_mutex);
	if (ret < 0) {
		if (transfer->control & STOP_UPSTREAM) {
			C2K_NOTE("%s\n", __func__);
			return;
		}
		retry = 1;
	}

	if (retry) {
		C2K_NOTE("%s: up request is buzy, try to get usb request\n", __func__);
		goto reget;
	}
	if (!t->req || got == 0) {
		C2K_DBG("%s\n", __func__);
		return;
	}
	req = t->req;

	memcpy(t->buffer, buffer, length);
	dump_data(transfer, "pushing up", t->buffer, length);
	req->length = length;
	req->buf = t->buffer;
	req->complete = upstream_complete;
	req->zero = ((length % transfer->upstream.ep->maxpacket) == 0);
	t->state = UPSTREAM_STAT_UPLOADING;
	ret = usb_ep_queue(transfer->upstream.ep, req, GFP_ATOMIC);
	if (ret < 0) {
		terr(t, "fail to queue request, %d", ret);
		t->state = UPSTREAM_STAT_FREE;
		return;
	}
	c->state = UPSTREAM_STAT_FREE;
}

static void upstream_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct upstream_transaction *t = req->context;
	struct rawbulk_transfer *transfer = t->transfer;

	C2K_DBG("%s\n", __func__);

	t->state = UPSTREAM_STAT_FREE;

	if (req->status < 0) {
		/*if (req->status == -ESHUTDOWN)
		   return;
		   else
		   terr(t, "req status %d", req->status); */
		C2K_DBG(" %s: req status %d\n", __func__, req->status);
		return;
	}

	if (!req->actual)
		terr(t, "req actual 0");

	/* update statistics */
	upstream_data[transfer->id] += req->actual;
	upstream_cnt[transfer->id]++;
	udata[transfer->id] += req->actual;
	ucnt[transfer->id]++;

	if (udata[transfer->id] >= up_note_sz) {
		C2K_NOTE("t<%d>,%d Bytes upload\n", transfer->id, udata[transfer->id]);
		udata[transfer->id] = 0;
		ucnt[transfer->id] = 0;
	}

	queue_work(transfer->tx_wq, &transfer->write_work);
}

static void stop_upstream(struct upstream_transaction *t)
{
	struct rawbulk_transfer *transfer = t->transfer;

	C2K_DBG("%s, %p, %p\n", __func__, transfer->upstream.ep, t->req);

	if (t->state == UPSTREAM_STAT_UPLOADING) {
		C2K_NOTE("%s\n", __func__);
		usb_ep_dequeue(transfer->upstream.ep, t->req);
	}
	t->state = UPSTREAM_STAT_FREE;
}

int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer, unsigned int length)
{
	int ret = -ENOENT;
	struct rawbulk_transfer *transfer;
	int count = length;
	struct cache_buf *c;

	C2K_DBG("%s\n", __func__);

	if (transfer_id > (FLS_CH_ID - 1))
		transfer_id--;
	else if (transfer_id == (FLS_CH_ID - 1)) {
		C2K_ERR("channal %d is flashless, no nessesory to bypass\n", (FLS_CH_ID - 1));
		return 0;
	}

	C2K_DBG("%s:transfer_id = %d, length = %d\n", __func__, transfer_id, length);

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return -ENODEV;

	mutex_lock(&transfer->modem_up_mutex);
	list_for_each_entry(c, &transfer->cache_buf_lists.transactions, clist) {
		if (c && (c->state == UPSTREAM_STAT_FREE) && !(transfer->control & STOP_UPSTREAM)) {
			list_move_tail(&c->clist, &transfer->cache_buf_lists.transactions);

			c->state = UPSTREAM_STAT_UPLOADING;
			ret = 0;
			break;
		}
	}

	/* dynamic got cache pool */
	if (ret < 0 && transfer->cache_buf_lists.ntrans < max_cache_cnt) {
		c = kmalloc(sizeof(struct cache_buf), GFP_KERNEL);
		if (!c)
			C2K_NOTE("fail to allocate upstream sdio buf n %d\n", transfer_id);

		c->buffer = (char *)__get_free_page(GFP_KERNEL);
		/* c->buffer = kmalloc(upsz, GFP_KERNEL); */
		if (!c) {
			kfree(c);
			C2K_NOTE("fail to allocate upstream sdio buf n %d\n", transfer_id);
		}
		c->state = UPSTREAM_STAT_UPLOADING;
		INIT_LIST_HEAD(&c->clist);
		list_add_tail(&c->clist, &transfer->cache_buf_lists.transactions);
		transfer->cache_buf_lists.ntrans++;
		total_tran[transfer_id] = transfer->cache_buf_lists.ntrans;
		C2K_NOTE("new cache, t<%d>, trans<%d>, alloc_fail<%d>, upstream<%d,%d>\n",
			 transfer_id,
			 transfer->cache_buf_lists.ntrans,
			 alloc_fail[transfer_id],
			 upstream_data[transfer_id], upstream_cnt[transfer_id]);
		ret = 0;
	}

	if (ret < 0) {
		total_drop[transfer_id] += length;

		if (time_after(jiffies, drop_check_timeout)) {
			C2K_NOTE("cahce full, t<%d>, drop<%d>, total_drop<%d>\n"
			     , transfer_id, length, total_drop[transfer_id]);

			C2K_NOTE("trans<%d>, alloc_fail<%d>, upstream<%d,%d>\n"
			     , transfer->cache_buf_lists.ntrans, alloc_fail[transfer_id],
			     upstream_data[transfer_id], upstream_cnt[transfer_id]);

			drop_check_timeout = jiffies + HZ * drop_check_interval;
		}
		mutex_unlock(&transfer->modem_up_mutex);
		return -ENOMEM;
	}
	mutex_unlock(&transfer->modem_up_mutex);

	memcpy(c->buffer, buffer, count);
	c->length = count;
	dump_data(transfer, "pushing up", c->buffer, count);
	queue_work(transfer->tx_wq, &transfer->write_work);
	return count;
}
EXPORT_SYMBOL_GPL(rawbulk_push_upstream_buffer);

/*
 * downstream
 */

#define DOWNSTREAM_STAT_FREE        0
#define DOWNSTREAM_STAT_DOWNLOADING 2

struct downstream_transaction {
	int state;
	int stalled;
	char name[32];
	struct list_head tlist;
	struct rawbulk_transfer *transfer;
	struct usb_request *req;
	int buffer_length;
	/* unsigned char buffer[0]; */
	char *buffer;
};

static void downstream_delayed_work(struct work_struct *work);

static void downstream_complete(struct usb_ep *ep, struct usb_request *req);

static struct downstream_transaction *alloc_downstream_transaction(struct rawbulk_transfer
								   *transfer, int bufsz)
{
	struct downstream_transaction *t;

	C2K_NOTE("%s\n", __func__);

	/* t = kzalloc(sizeof *t + bufsz * sizeof(unsigned char), GFP_ATOMIC); */
	t = kmalloc(sizeof(struct downstream_transaction), GFP_ATOMIC);
	if (!t)
		return NULL;

#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
	t->buffer = (char *)__get_free_page(GFP_ATOMIC | GFP_DMA);
#else
	t->buffer = (char *)__get_free_page(GFP_ATOMIC);
#endif
	/* t->buffer = kmalloc(bufsz, GFP_ATOMIC); */
	if (!t->buffer) {
		kfree(t);
		return NULL;
	}
	t->buffer_length = bufsz;
	t->req = usb_ep_alloc_request(transfer->downstream.ep, GFP_ATOMIC);
	if (!t->req)
		goto failto_alloc_usb_request;

	t->name[0] = 0;

	INIT_LIST_HEAD(&t->tlist);
	list_add_tail(&t->tlist, &transfer->downstream.transactions);

	transfer->downstream.ntrans++;
	t->transfer = transfer;
	t->state = DOWNSTREAM_STAT_FREE;
	t->stalled = 0;
	t->req->context = t;

	return t;

failto_alloc_usb_request:
	/* kfree(t->buffer); */
	free_page((unsigned long)t->buffer);
	kfree(t);
	return NULL;
}

static void free_downstream_transaction(struct rawbulk_transfer *transfer)
{
	struct list_head *p, *n;
	unsigned long flags;

	C2K_NOTE("%s\n", __func__);

	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_safe(p, n, &transfer->downstream.transactions) {
		struct downstream_transaction *t = list_entry(p, struct
							      downstream_transaction, tlist);

		list_del(p);
		/* kfree(t->buffer); */
		if (t->buffer)	/*NULL pointer when ETS switch */
			free_page((unsigned long)t->buffer);
		usb_ep_free_request(transfer->downstream.ep, t->req);
		kfree(t);

		transfer->downstream.ntrans--;
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
}

static void stop_downstream(struct downstream_transaction *t)
{
	struct rawbulk_transfer *transfer = t->transfer;

	if (t->state == DOWNSTREAM_STAT_DOWNLOADING) {
		usb_ep_dequeue(transfer->downstream.ep, t->req);
		t->state = DOWNSTREAM_STAT_FREE;
	}
}

static int queue_downstream(struct downstream_transaction *t)
{
	int rc = 0;
	struct rawbulk_transfer *transfer = t->transfer;
	struct usb_request *req = t->req;

	C2K_DBG("%s\n", __func__);

	req->buf = t->buffer;
	req->length = t->buffer_length;
	req->complete = downstream_complete;
	/* if (rawbulk_usb_state_check()) */
	rc = usb_ep_queue(transfer->downstream.ep, req, GFP_ATOMIC);
	/* else */
	/* return; */
	if (rc < 0) {
		C2K_ERR("queue err:%d\n", rc);
		return rc;
	}

	t->state = DOWNSTREAM_STAT_DOWNLOADING;
	return 0;
}

static int start_downstream(struct downstream_transaction *t)
{
	int rc = 0;
	struct rawbulk_transfer *transfer = t->transfer;
	struct usb_request *req = t->req;
	int time_delayed = msecs_to_jiffies(1);

	C2K_DBG("%s\n", __func__);

	if (transfer->control & STOP_DOWNSTREAM) {
		/* t->state = DOWNSTREAM_STAT_FREE; */
		return -EPIPE;
	}
	rc = modem_buffer_push(transfer->id, t->req->buf, t->req->actual);
	if (rc < 0) {
		if (rc == -ENOMEM) {
			spin_lock(&transfer->modem_block_lock);
			transfer->sdio_block = 1;
			spin_unlock(&transfer->modem_block_lock);
			spin_lock(&transfer->usb_down_lock);
			list_move_tail(&t->tlist, &transfer->repush2modem.transactions);
			spin_unlock(&transfer->usb_down_lock);
			transfer->repush2modem.ntrans++;
			transfer->downstream.ntrans--;
			queue_delayed_work(transfer->flow_wq, &transfer->delayed, time_delayed);
			return -EPIPE;
		} else
			return -EPIPE;
	}

	req->buf = t->buffer;
	req->length = t->buffer_length;
	req->complete = downstream_complete;
	/* if (rawbulk_usb_state_check()) */
	rc = usb_ep_queue(transfer->downstream.ep, req, GFP_ATOMIC);
	/* else */
	/* return; */
	if (rc < 0) {
		terr(t, "fail to queue request, %d", rc);
		return rc;
	}

	t->state = DOWNSTREAM_STAT_DOWNLOADING;
	return 0;
}

static void downstream_complete(struct usb_ep *ep, struct usb_request *req)
{
#ifdef C2K_USB_UT
	int i;
	static unsigned char last_c;
	unsigned char c;
	char verb[64];
	char compare_val;
	char *ptr;
	char *pbuf;
#endif

	/* struct downstream_transaction *t = container_of(req->buf, */
	/* struct downstream_transaction, buffer); */

	/* struct downstream_transaction *t = container_of(req->buf, */
	/* struct downstream_transaction, buffer); */
	struct downstream_transaction *t = req->context;
	struct rawbulk_transfer *transfer = t->transfer;

	C2K_DBG("%s\n", __func__);

	t->state = DOWNSTREAM_STAT_FREE;

	if (req->status < 0) {
		/*if (req->status == -ESHUTDOWN)
		   return;
		   else
		   terr(t, "req status %d", req->status); */
		C2K_DBG("req status %d\n", req->status);
		return;
	}
#ifdef C2K_USB_UT
#define PRINT_LIMIT 8
	ptr = (char *)t->req->buf;
	pbuf = (char *)verb;

	pbuf += sprintf(pbuf, "down len(%d), %d, ", t->req->actual, (int)sizeof(unsigned char));
	for (i = 0; i < t->req->actual; i++) {
		c = *(ptr + i);
		if (last_c == 0xff)
			compare_val = 0;
		else
			compare_val = last_c + 1;
		if (c != compare_val || ut_err == 1) {
			if (c != compare_val) {
				C2K_NOTE("<%x,%x, %x>, sizeof(unsigned char):%d\n", c, last_c,
					 compare_val, (int)sizeof(unsigned char));
			}
			ut_err = 1;
		}

		if (i < PRINT_LIMIT)
			pbuf += sprintf(pbuf, "%c ", c);
		last_c = c;	/* keep updating data */
	}
	C2K_DBG("%s, last_c(%x)\n", verb, last_c);
	if (ut_err)
		C2K_ERR("errrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr\n");
#endif

	dump_data(transfer, "downstream", t->buffer, req->actual);

	spin_lock(&transfer->modem_block_lock);
	if (!!transfer->sdio_block) {
		spin_unlock(&transfer->modem_block_lock);

		spin_lock(&transfer->usb_down_lock);
		list_move_tail(&t->tlist, &transfer->repush2modem.transactions);
		spin_unlock(&transfer->usb_down_lock);
		transfer->repush2modem.ntrans++;
		transfer->downstream.ntrans--;
		return;
	}

	spin_unlock(&transfer->modem_block_lock);
	start_downstream(t);
}

static void downstream_delayed_work(struct work_struct *work)
{
	int rc = 0;
	unsigned long flags;

	struct downstream_transaction *downstream, *downstream_copy;
	struct usb_request *req;
	int time_delayed = msecs_to_jiffies(1);

	struct rawbulk_transfer *transfer = container_of(work, struct
							 rawbulk_transfer, delayed.work);
	C2K_NOTE("%s\n", __func__);

	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_entry_safe(downstream, downstream_copy, &transfer->repush2modem.transactions,
				 tlist) {
		spin_unlock_irqrestore(&transfer->usb_down_lock, flags);

		rc = modem_buffer_push(transfer->id, downstream->req->buf, downstream->req->actual);
		if (rc < 0) {
			if (rc != -ENOMEM)
				terr(downstream, "port is not presence\n");
			if (!(transfer->control & STOP_DOWNSTREAM)) {
				queue_delayed_work(transfer->flow_wq, &transfer->delayed,
						   time_delayed);
			}
			return;
		}
		spin_lock_irqsave(&transfer->usb_down_lock, flags);
		list_move_tail(&downstream->tlist, &transfer->downstream.transactions);
		spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
		downstream->stalled = 0;
		downstream->state = DOWNSTREAM_STAT_FREE;

		req = downstream->req;
		req->buf = downstream->buffer;
		req->length = downstream->buffer_length;
		req->complete = downstream_complete;
		/* if (rawbulk_usb_state_check()) */
		rc = usb_ep_queue(transfer->downstream.ep, req, GFP_ATOMIC);
		/* else */
		/* return; */
		if (rc < 0) {
			terr(downstream, "fail to queue request, %d", rc);
			downstream->stalled = 1;
			return;
		}
		downstream->state = DOWNSTREAM_STAT_DOWNLOADING;
		transfer->repush2modem.ntrans--;
		transfer->downstream.ntrans++;
		spin_lock_irqsave(&transfer->usb_down_lock, flags);
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);

	spin_lock_irqsave(&transfer->modem_block_lock, flags);
	transfer->sdio_block = 0;
	spin_unlock_irqrestore(&transfer->modem_block_lock, flags);
}

int rawbulk_start_transactions(int transfer_id, int nups, int ndowns, int upsz, int downsz)
{
	int n;
	int rc, ret, up_cache_cnt;
	unsigned long flags;
	struct rawbulk_transfer *transfer;
	struct upstream_transaction *upstream;	/* upstream_copy; */
	struct downstream_transaction *downstream, *downstream_copy;
	struct cache_buf *c;

	C2K_NOTE("%s\n", __func__);

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return -ENODEV;

	if (!rawbulk->cdev)
		return -ENODEV;

	if (!transfer->function)
		return -ENODEV;

	C2K_NOTE("start transactions on id %d, nups %d ndowns %d upsz %d downsz %d\n",
		 transfer_id, nups, ndowns, upsz, downsz);

	/* stop host transfer 1stly */
	ret = sdio_rawbulk_intercept(transfer->id, 1);
	if (ret < 0) {
		C2K_ERR("bypass sdio failed, channel id = %d\n", transfer->id);
		return ret;
	}
	transfer->sdio_block = 0;

	spin_lock(&transfer->flow_lock);
	transfer->down_flow = 0;
	spin_unlock(&transfer->flow_lock);

	mutex_lock(&transfer->usb_up_mutex);
	for (n = 0; n < nups; n++) {
		upstream = alloc_upstream_transaction(transfer, upsz);
		if (!upstream) {
			rc = -ENOMEM;
			mutex_unlock(&transfer->usb_up_mutex);
			C2K_NOTE("fail to allocate upstream transaction n %d", n);
			goto failto_alloc_upstream;
		}
	}
	mutex_unlock(&transfer->usb_up_mutex);


	mutex_lock(&transfer->modem_up_mutex);

	if (transfer_id == RAWBULK_TID_ETS || transfer_id == RAWBULK_TID_MODEM)
		up_cache_cnt = base_cache_cnt;
	else
		up_cache_cnt = 8 * nups;

	C2K_NOTE("t<%d>, up_cache_cnt<%d>\n", transfer_id, up_cache_cnt);
	for (n = 0; n < up_cache_cnt; n++) {
		/* c = kzalloc(sizeof *c + upsz * sizeof(unsigned char), GFP_KERNEL); */
		c = kmalloc(sizeof(struct cache_buf), GFP_KERNEL);
		if (!c) {
			rc = -ENOMEM;
			mutex_unlock(&transfer->modem_up_mutex);
			C2K_NOTE("fail to allocate upstream sdio buf n %d", n);
			alloc_fail[transfer_id] = 1;
			goto failto_alloc_up_sdiobuf;
		}

		c->buffer = (char *)__get_free_page(GFP_KERNEL);
		/* c->buffer = kmalloc(upsz, GFP_KERNEL); */
		if (!c) {
			rc = -ENOMEM;
			kfree(c);
			mutex_unlock(&transfer->modem_up_mutex);
			C2K_NOTE("fail to allocate upstream sdio buf n %d", n);
			alloc_fail[transfer_id] = 1;
			goto failto_alloc_up_sdiobuf;
		}
		c->state = UPSTREAM_STAT_FREE;
		INIT_LIST_HEAD(&c->clist);
		list_add_tail(&c->clist, &transfer->cache_buf_lists.transactions);
		transfer->cache_buf_lists.ntrans++;
	}
	total_tran[transfer_id] = transfer->cache_buf_lists.ntrans;
	mutex_unlock(&transfer->modem_up_mutex);

	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	for (n = 0; n < ndowns; n++) {
		downstream = alloc_downstream_transaction(transfer, downsz);
		if (!downstream) {
			rc = -ENOMEM;
			spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
			C2K_NOTE("fail to allocate downstream transaction n %d", n);
			goto failto_alloc_downstream;
		}
	}

	transfer->control &= ~STOP_UPSTREAM;
	transfer->control &= ~STOP_DOWNSTREAM;

	list_for_each_entry_safe(downstream, downstream_copy, &transfer->downstream.transactions,
				 tlist) {
		if (downstream->state == DOWNSTREAM_STAT_FREE && !downstream->stalled) {
			rc = queue_downstream(downstream);
			if (rc < 0) {
				spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
				C2K_NOTE("fail to start downstream %s rc %d\n", downstream->name,
					 rc);
				goto failto_start_downstream;
			}
		}
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
	return 0;

failto_start_downstream:
	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_entry(downstream, &transfer->downstream.transactions, tlist)
		stop_downstream(downstream);
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
failto_alloc_up_sdiobuf:
	free_upstream_sdio_buf(transfer);
failto_alloc_downstream:
	free_downstream_transaction(transfer);
failto_alloc_upstream:
	free_upstream_transaction(transfer);
	/* recover host transfer */
	sdio_rawbulk_intercept(transfer->id, 0);
	return rc;
}
EXPORT_SYMBOL_GPL(rawbulk_start_transactions);
void rawbulk_stop_transactions(int transfer_id)
{
	unsigned long flags;
	struct rawbulk_transfer *transfer;
	struct upstream_transaction *upstream;
	struct downstream_transaction *downstream, *downstream_copy;
	struct list_head *p, *n;

	C2K_NOTE("t-%d\n", transfer_id);

	transfer = id_to_transfer(transfer_id);
	if (!transfer) {
		C2K_NOTE("t-%d, NULL\n", transfer_id);
		return;
	}
	if (transfer->control) {
		C2K_NOTE("t-%d,ctrl:%d\n", transfer_id, transfer->control);
		return;
	}

	spin_lock(&transfer->lock);
	transfer->control |= (STOP_UPSTREAM | STOP_DOWNSTREAM);
	spin_unlock(&transfer->lock);

	sdio_rawbulk_intercept(transfer->id, 0);

	cancel_delayed_work(&transfer->delayed);
	flush_workqueue(transfer->flow_wq);
	flush_workqueue(transfer->tx_wq);

	mutex_lock(&transfer->usb_up_mutex);
	list_for_each_entry(upstream, &transfer->upstream.transactions, tlist) {
		C2K_DBG("t-%d,upstresm<%p>\n", transfer_id, upstream);
		stop_upstream(upstream);
	}
	mutex_unlock(&transfer->usb_up_mutex);
	/* this one got lock inside */
	free_upstream_transaction(transfer);

	free_upstream_sdio_buf(transfer);

	list_for_each_entry_safe(downstream, downstream_copy, &transfer->downstream.transactions,
				 tlist) {
		stop_downstream(downstream);
	}

	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_safe(p, n, &transfer->repush2modem.transactions) {
		struct downstream_transaction *delayed_t = list_entry(p, struct
								      downstream_transaction,
								      tlist);
		list_move_tail(&delayed_t->tlist, &transfer->downstream.transactions);
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);

	spin_lock_irqsave(&transfer->modem_block_lock, flags);
	transfer->sdio_block = 0;
	spin_unlock_irqrestore(&transfer->modem_block_lock, flags);

	free_downstream_transaction(transfer);

}
EXPORT_SYMBOL_GPL(rawbulk_stop_transactions);

static char *state2string(int state, int upstream)
{
	if (upstream) {
		switch (state) {
		case UPSTREAM_STAT_FREE:
			return "FREE";
		case UPSTREAM_STAT_UPLOADING:
			return "UPLOADING";
		default:
			return "UNKNOWN";
		}
	} else {
		switch (state) {
		case DOWNSTREAM_STAT_FREE:
			return "FREE";
		case DOWNSTREAM_STAT_DOWNLOADING:
			return "DOWNLOADING";
		default:
			return "UNKNOWN";
		}
	}
}

int rawbulk_transfer_statistics(int transfer_id, char *buf)
{
	char *pbuf = buf;
	struct rawbulk_transfer *transfer;
	struct upstream_transaction *upstream;
	struct downstream_transaction *downstream;
	struct cache_buf *c;
	unsigned long flags;

	C2K_NOTE("%s\n", __func__);

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return sprintf(pbuf, "-ENODEV, id %d\n", transfer_id);

	pbuf += sprintf(pbuf, "rawbulk statistics:\n");
	if (rawbulk->cdev && rawbulk->cdev->config)
		pbuf += sprintf(pbuf, " gadget device: %s\n", rawbulk->cdev->config->label);
	else
		pbuf += sprintf(pbuf, " gadget device: -ENODEV\n");
	pbuf += sprintf(pbuf, " upstreams (total %d transactions)\n", transfer->upstream.ntrans);
	mutex_lock(&transfer->usb_up_mutex);
	list_for_each_entry(upstream, &transfer->upstream.transactions, tlist) {
		pbuf += sprintf(pbuf, "  %s state: %s", upstream->name,
				state2string(upstream->state, 1));
		pbuf += sprintf(pbuf, ", maxbuf: %d bytes", upstream->buffer_length);
		if (upstream->stalled)
			pbuf += sprintf(pbuf, " (stalled!)");
		pbuf += sprintf(pbuf, "\n");
	}
	mutex_unlock(&transfer->usb_up_mutex);

	pbuf += sprintf(pbuf, " cache_buf_lists (total %d transactions)\n",
			transfer->cache_buf_lists.ntrans);
	mutex_lock(&transfer->modem_up_mutex);
	list_for_each_entry(c, &transfer->cache_buf_lists.transactions, clist) {
		pbuf += sprintf(pbuf, "  %s state:", state2string(c->state, 1));
		pbuf += sprintf(pbuf, ", maxbuf: %d bytes", c->length);
		pbuf += sprintf(pbuf, "\n");
	}
	mutex_unlock(&transfer->modem_up_mutex);

	pbuf += sprintf(pbuf, " downstreams (total %d transactions)\n",
			transfer->downstream.ntrans);
	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_entry(downstream, &transfer->downstream.transactions, tlist) {
		spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
		pbuf += sprintf(pbuf, "  %s state: %s", downstream->name,
				state2string(downstream->state, 0));
		pbuf += sprintf(pbuf, ", maxbuf: %d bytes", downstream->buffer_length);
		if (downstream->stalled)
			pbuf += sprintf(pbuf, " (stalled!)");
		pbuf += sprintf(pbuf, "\n");
		spin_lock_irqsave(&transfer->usb_down_lock, flags);
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);

	pbuf += sprintf(pbuf, " repush2modem (total %d transactions)\n",
			transfer->downstream.ntrans);
	spin_lock_irqsave(&transfer->usb_down_lock, flags);
	list_for_each_entry(downstream, &transfer->repush2modem.transactions, tlist) {
		spin_unlock_irqrestore(&transfer->usb_down_lock, flags);
		pbuf += sprintf(pbuf, "  %s state: %s", downstream->name,
				state2string(downstream->state, 0));
		pbuf += sprintf(pbuf, ", maxbuf: %d bytes", downstream->buffer_length);
		if (downstream->stalled)
			pbuf += sprintf(pbuf, " (stalled!)");
		pbuf += sprintf(pbuf, "\n");
		spin_lock_irqsave(&transfer->usb_down_lock, flags);
	}
	spin_unlock_irqrestore(&transfer->usb_down_lock, flags);

	return (int)(pbuf - buf);
}
EXPORT_SYMBOL_GPL(rawbulk_transfer_statistics);

int rawbulk_bind_function(int transfer_id, struct usb_function *function, struct
			  usb_ep * bulk_out, struct usb_ep *bulk_in,
			  rawbulk_autoreconn_callback_t autoreconn_callback)
{

	struct rawbulk_transfer *transfer;

	C2K_NOTE("%s\n", __func__);

	if (!function || !bulk_out || !bulk_in)
		return -EINVAL;

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return -ENODEV;

	transfer->downstream.ep = bulk_out;
	transfer->upstream.ep = bulk_in;
	transfer->function = function;
	rawbulk->cdev = function->config->cdev;

	transfer->autoreconn = autoreconn_callback;
	return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_bind_function);

void rawbulk_unbind_function(int transfer_id)
{
	int n;
	int no_functions = 1;
	struct rawbulk_transfer *transfer;

	C2K_NOTE("%s\n", __func__);

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return;

	rawbulk_stop_transactions(transfer_id);

	/* mark this for disable->work->stop_transaction not compelte */
	/* transfer->downstream.ep = NULL; */
	/* transfer->upstream.ep = NULL; */

	transfer->function = NULL;

	for (n = 0; n < _MAX_TID; n++) {
		if (!!rawbulk->transfer[n].function)
			no_functions = 0;
	}

	if (no_functions)
		rawbulk->cdev = NULL;
}
EXPORT_SYMBOL_GPL(rawbulk_unbind_function);

int rawbulk_bind_sdio_channel(int transfer_id)
{
	struct rawbulk_transfer *transfer;
	struct rawbulk_function *fn;

	C2K_NOTE("%d\n", transfer_id);

	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return -ENODEV;
	fn = rawbulk_lookup_function(transfer_id);
	if (fn)
		fn->cbp_reset = 0;
	if (transfer->autoreconn)
		transfer->autoreconn(transfer->id);
	return 0;
}
EXPORT_SYMBOL_GPL(rawbulk_bind_sdio_channel);

void rawbulk_unbind_sdio_channel(int transfer_id)
{
	struct rawbulk_transfer *transfer;
	struct rawbulk_function *fn;

	C2K_NOTE("%d\n", transfer_id);
	transfer = id_to_transfer(transfer_id);
	if (!transfer)
		return;
	rawbulk_stop_transactions(transfer_id);
	fn = rawbulk_lookup_function(transfer_id);
	if (fn) {
		fn->cbp_reset = 1;
		rawbulk_disable_function(fn);
	}
}
EXPORT_SYMBOL_GPL(rawbulk_unbind_sdio_channel);

static __init int rawbulk_init(void)
{
	int n;
	char name[20];

	C2K_NOTE("%s\n", __func__);
	drop_check_timeout = jiffies;

	rawbulk = kzalloc(sizeof(*rawbulk), GFP_KERNEL);
	if (!rawbulk)
		return -ENOMEM;

	for (n = 0; n < _MAX_TID; n++) {
		struct rawbulk_transfer *t = &rawbulk->transfer[n];

		t->id = n;
		INIT_LIST_HEAD(&t->upstream.transactions);
		INIT_LIST_HEAD(&t->downstream.transactions);
		INIT_LIST_HEAD(&t->repush2modem.transactions);
		INIT_LIST_HEAD(&t->cache_buf_lists.transactions);
		INIT_DELAYED_WORK(&t->delayed, downstream_delayed_work);
		memset(name, 0, 20);
		sprintf(name, "%s_flow_ctrl", transfer_name[n]);
		t->flow_wq = create_singlethread_workqueue(name);
		if (!t->flow_wq)
			return -ENOMEM;

		INIT_WORK(&t->write_work, start_upstream);
		memset(name, 0, 20);
		sprintf(name, "%s_tx_wq", transfer_name[n]);
		t->tx_wq = create_singlethread_workqueue(name);
		if (!t->tx_wq)
			return -ENOMEM;

		mutex_init(&t->modem_up_mutex);
		mutex_init(&t->usb_up_mutex);
		spin_lock_init(&t->lock);
		spin_lock_init(&t->usb_down_lock);
		spin_lock_init(&t->modem_block_lock);
		spin_lock_init(&t->flow_lock);

		t->control = STOP_UPSTREAM | STOP_DOWNSTREAM;
	}

	return 0;
}
module_init(rawbulk_init);

static __exit void rawbulk_exit(void)
{
	int n;
	struct rawbulk_transfer *t;

	for (n = 0; n < _MAX_TID; n++) {
		t = &rawbulk->transfer[n];
		rawbulk_stop_transactions(n);
		destroy_workqueue(t->flow_wq);
		destroy_workqueue(t->tx_wq);
	}
	kfree(rawbulk);
}
module_exit(rawbulk_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
