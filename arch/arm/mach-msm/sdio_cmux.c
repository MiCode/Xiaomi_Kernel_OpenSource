/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#define DEBUG

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/debugfs.h>

#include <mach/sdio_al.h>
#include <mach/sdio_cmux.h>

#include "modem_notifier.h"

#define MAX_WRITE_RETRY 5
#define MAGIC_NO_V1 0x33FC

static int msm_sdio_cmux_debug_mask;
module_param_named(debug_mask, msm_sdio_cmux_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

enum cmd_type {
	DATA = 0,
	OPEN,
	CLOSE,
	STATUS,
	NUM_CMDS
};

#define DSR_POS 0x1
#define CTS_POS 0x2
#define RI_POS 0x4
#define CD_POS 0x8

struct sdio_cmux_ch {
	int lc_id;

	struct mutex lc_lock;
	wait_queue_head_t open_wait_queue;
	int is_remote_open;
	int is_local_open;
	int is_channel_reset;

	char local_status;
	char remote_status;

	struct mutex tx_lock;
	struct list_head tx_list;

	void *priv;
	void (*receive_cb)(void *, int, void *);
	void (*write_done)(void *, int, void *);
	void (*status_callback)(int, void *);
} logical_ch[SDIO_CMUX_NUM_CHANNELS];

struct sdio_cmux_hdr {
	uint16_t magic_no;
	uint8_t status;         /* This field is reserved for commands other
				 * than STATUS */
	uint8_t cmd;
	uint8_t pad_bytes;
	uint8_t lc_id;
	uint16_t pkt_len;
};

struct sdio_cmux_pkt {
	struct sdio_cmux_hdr *hdr;
	void *data;
};

struct sdio_cmux_list_elem {
	struct list_head list;
	struct sdio_cmux_pkt cmux_pkt;
};

#define logical_ch_is_local_open(x)                        \
	(logical_ch[(x)].is_local_open)

#define logical_ch_is_remote_open(x)                       \
	(logical_ch[(x)].is_remote_open)

static void sdio_cdemux_fn(struct work_struct *work);
static DECLARE_WORK(sdio_cdemux_work, sdio_cdemux_fn);
static struct workqueue_struct *sdio_cdemux_wq;

static DEFINE_MUTEX(write_lock);
static uint32_t bytes_to_write;
static DEFINE_MUTEX(temp_rx_lock);
static LIST_HEAD(temp_rx_list);

static void sdio_cmux_fn(struct work_struct *work);
static DECLARE_WORK(sdio_cmux_work, sdio_cmux_fn);
static struct workqueue_struct *sdio_cmux_wq;

static struct sdio_channel *sdio_qmi_chl;
static uint32_t sdio_cmux_inited;

static uint32_t abort_tx;
static DEFINE_MUTEX(modem_reset_lock);

static DEFINE_MUTEX(probe_lock);

enum {
	MSM_SDIO_CMUX_DEBUG = 1U << 0,
	MSM_SDIO_CMUX_DUMP_BUFFER = 1U << 1,
};

static struct platform_device sdio_ctl_dev = {
	.name		= "SDIO_CTL",
	.id		= -1,
};

#if defined(DEBUG)
#define D_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_sdio_cmux_debug_mask & MSM_SDIO_CMUX_DUMP_BUFFER) { \
		int i; \
		pr_debug("%s", prestr); \
		for (i = 0; i < cnt; i++) \
			pr_info("%.2x", buf[i]); \
		pr_debug("\n"); \
	} \
} while (0)

#define D(x...) \
do { \
	if (msm_sdio_cmux_debug_mask & MSM_SDIO_CMUX_DEBUG) \
		pr_debug(x); \
} while (0)

#else
#define D_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#define D(x...) do {} while (0)
#endif

static int sdio_cmux_ch_alloc(int id)
{
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid lc_id - %d\n", __func__, id);
		return -EINVAL;
	}

	logical_ch[id].lc_id = id;
	mutex_init(&logical_ch[id].lc_lock);
	init_waitqueue_head(&logical_ch[id].open_wait_queue);
	logical_ch[id].is_remote_open = 0;
	logical_ch[id].is_local_open = 0;
	logical_ch[id].is_channel_reset = 0;

	INIT_LIST_HEAD(&logical_ch[id].tx_list);
	mutex_init(&logical_ch[id].tx_lock);

	logical_ch[id].priv = NULL;
	logical_ch[id].receive_cb = NULL;
	logical_ch[id].write_done = NULL;
	return 0;
}

static int sdio_cmux_ch_clear_and_signal(int id)
{
	struct sdio_cmux_list_elem *list_elem;

	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid lc_id - %d\n", __func__, id);
		return -EINVAL;
	}

	mutex_lock(&logical_ch[id].lc_lock);
	logical_ch[id].is_remote_open = 0;
	mutex_lock(&logical_ch[id].tx_lock);
	while (!list_empty(&logical_ch[id].tx_list)) {
		list_elem = list_first_entry(&logical_ch[id].tx_list,
					     struct sdio_cmux_list_elem,
					     list);
		list_del(&list_elem->list);
		kfree(list_elem->cmux_pkt.hdr);
		kfree(list_elem);
	}
	mutex_unlock(&logical_ch[id].tx_lock);
	if (logical_ch[id].receive_cb)
		logical_ch[id].receive_cb(NULL, 0, logical_ch[id].priv);
	if (logical_ch[id].write_done)
		logical_ch[id].write_done(NULL, 0, logical_ch[id].priv);
	mutex_unlock(&logical_ch[id].lc_lock);
	wake_up(&logical_ch[id].open_wait_queue);
	return 0;
}

static int sdio_cmux_write_cmd(const int id, enum cmd_type type)
{
	int write_size = 0;
	void *write_data = NULL;
	struct sdio_cmux_list_elem *list_elem;

	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid lc_id - %d\n", __func__, id);
		return -EINVAL;
	}

	if (type < 0 || type > NUM_CMDS) {
		pr_err("%s: Invalid cmd - %d\n", __func__, type);
		return -EINVAL;
	}

	write_size = sizeof(struct sdio_cmux_hdr);
	list_elem = kmalloc(sizeof(struct sdio_cmux_list_elem), GFP_KERNEL);
	if (!list_elem) {
		pr_err("%s: list_elem alloc failed\n", __func__);
		return -ENOMEM;
	}

	write_data = kmalloc(write_size, GFP_KERNEL);
	if (!write_data) {
		pr_err("%s: write_data alloc failed\n", __func__);
		kfree(list_elem);
		return -ENOMEM;
	}

	list_elem->cmux_pkt.hdr = (struct sdio_cmux_hdr *)write_data;
	list_elem->cmux_pkt.data = NULL;

	list_elem->cmux_pkt.hdr->lc_id = (uint8_t)id;
	list_elem->cmux_pkt.hdr->pkt_len = (uint16_t)0;
	list_elem->cmux_pkt.hdr->cmd = (uint8_t)type;
	list_elem->cmux_pkt.hdr->status = (uint8_t)0;
	if (type == STATUS)
		list_elem->cmux_pkt.hdr->status = logical_ch[id].local_status;
	list_elem->cmux_pkt.hdr->pad_bytes = (uint8_t)0;
	list_elem->cmux_pkt.hdr->magic_no = (uint16_t)MAGIC_NO_V1;

	mutex_lock(&logical_ch[id].tx_lock);
	list_add_tail(&list_elem->list, &logical_ch[id].tx_list);
	mutex_unlock(&logical_ch[id].tx_lock);

	mutex_lock(&write_lock);
	bytes_to_write += write_size;
	mutex_unlock(&write_lock);
	queue_work(sdio_cmux_wq, &sdio_cmux_work);

	return 0;
}

int sdio_cmux_open(const int id,
		   void (*receive_cb)(void *, int, void *),
		   void (*write_done)(void *, int, void *),
		   void (*status_callback)(int, void *),
		   void *priv)
{
	int r;
	struct sdio_cmux_list_elem *list_elem, *list_elem_tmp;

	if (!sdio_cmux_inited)
		return -ENODEV;
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid id - %d\n", __func__, id);
		return -EINVAL;
	}

	r = wait_event_timeout(logical_ch[id].open_wait_queue,
				logical_ch[id].is_remote_open, (1 * HZ));
	if (r < 0) {
		pr_err("ERROR %s: wait_event_timeout() failed for"
		       " ch%d with rc %d\n", __func__, id, r);
		return r;
	}
	if (r == 0) {
		pr_err("ERROR %s: Wait Timed Out for ch%d\n", __func__, id);
		return -ETIMEDOUT;
	}

	mutex_lock(&logical_ch[id].lc_lock);
	if (!logical_ch[id].is_remote_open) {
		pr_err("%s: Remote ch%d not opened\n", __func__, id);
		mutex_unlock(&logical_ch[id].lc_lock);
		return -EINVAL;
	}
	if (logical_ch[id].is_local_open) {
		mutex_unlock(&logical_ch[id].lc_lock);
		return 0;
	}
	logical_ch[id].is_local_open = 1;
	logical_ch[id].priv = priv;
	logical_ch[id].receive_cb = receive_cb;
	logical_ch[id].write_done = write_done;
	logical_ch[id].status_callback = status_callback;
	if (logical_ch[id].receive_cb) {
		mutex_lock(&temp_rx_lock);
		list_for_each_entry_safe(list_elem, list_elem_tmp,
					 &temp_rx_list, list) {
			if ((int)list_elem->cmux_pkt.hdr->lc_id == id) {
				logical_ch[id].receive_cb(
					list_elem->cmux_pkt.data,
					(int)list_elem->cmux_pkt.hdr->pkt_len,
					logical_ch[id].priv);
				list_del(&list_elem->list);
				kfree(list_elem->cmux_pkt.hdr);
				kfree(list_elem);
			}
		}
		mutex_unlock(&temp_rx_lock);
	}
	mutex_unlock(&logical_ch[id].lc_lock);
	sdio_cmux_write_cmd(id, OPEN);
	return 0;
}
EXPORT_SYMBOL(sdio_cmux_open);

int sdio_cmux_close(int id)
{
	struct sdio_cmux_ch *ch;

	if (!sdio_cmux_inited)
		return -ENODEV;
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid channel close\n", __func__);
		return -EINVAL;
	}

	ch = &logical_ch[id];
	mutex_lock(&ch->lc_lock);
	ch->receive_cb = NULL;
	mutex_lock(&ch->tx_lock);
	ch->write_done = NULL;
	mutex_unlock(&ch->tx_lock);
	ch->is_local_open = 0;
	ch->priv = NULL;
	mutex_unlock(&ch->lc_lock);
	sdio_cmux_write_cmd(ch->lc_id, CLOSE);
	return 0;
}
EXPORT_SYMBOL(sdio_cmux_close);

int sdio_cmux_write_avail(int id)
{
	int write_avail;

	mutex_lock(&logical_ch[id].lc_lock);
	if (logical_ch[id].is_channel_reset) {
		mutex_unlock(&logical_ch[id].lc_lock);
		return -ENETRESET;
	}
	mutex_unlock(&logical_ch[id].lc_lock);
	write_avail = sdio_write_avail(sdio_qmi_chl);
	return write_avail - bytes_to_write;
}
EXPORT_SYMBOL(sdio_cmux_write_avail);

int sdio_cmux_write(int id, void *data, int len)
{
	struct sdio_cmux_list_elem *list_elem;
	uint32_t write_size;
	void *write_data = NULL;
	struct sdio_cmux_ch *ch;
	int ret;

	if (!sdio_cmux_inited)
		return -ENODEV;
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS) {
		pr_err("%s: Invalid channel id %d\n", __func__, id);
		return -ENODEV;
	}

	ch = &logical_ch[id];
	if (len <= 0) {
		pr_err("%s: Invalid len %d bytes to write\n",
			__func__, len);
		return -EINVAL;
	}

	write_size = sizeof(struct sdio_cmux_hdr) + len;
	list_elem = kmalloc(sizeof(struct sdio_cmux_list_elem), GFP_KERNEL);
	if (!list_elem) {
		pr_err("%s: list_elem alloc failed\n", __func__);
		return -ENOMEM;
	}

	write_data = kmalloc(write_size, GFP_KERNEL);
	if (!write_data) {
		pr_err("%s: write_data alloc failed\n", __func__);
		kfree(list_elem);
		return -ENOMEM;
	}

	list_elem->cmux_pkt.hdr = (struct sdio_cmux_hdr *)write_data;
	list_elem->cmux_pkt.data = (void *)((char *)write_data +
						sizeof(struct sdio_cmux_hdr));
	memcpy(list_elem->cmux_pkt.data, data, len);

	list_elem->cmux_pkt.hdr->lc_id = (uint8_t)ch->lc_id;
	list_elem->cmux_pkt.hdr->pkt_len = (uint16_t)len;
	list_elem->cmux_pkt.hdr->cmd = (uint8_t)DATA;
	list_elem->cmux_pkt.hdr->status = (uint8_t)0;
	list_elem->cmux_pkt.hdr->pad_bytes = (uint8_t)0;
	list_elem->cmux_pkt.hdr->magic_no = (uint16_t)MAGIC_NO_V1;

	mutex_lock(&ch->lc_lock);
	if (!ch->is_remote_open || !ch->is_local_open) {
		pr_err("%s: Local ch%d sending data before sending/receiving"
		       " OPEN command\n", __func__, ch->lc_id);
		if (ch->is_channel_reset)
			ret = -ENETRESET;
		else
			ret = -ENODEV;
		mutex_unlock(&ch->lc_lock);
		kfree(write_data);
		kfree(list_elem);
		return ret;
	}
	mutex_lock(&ch->tx_lock);
	list_add_tail(&list_elem->list, &ch->tx_list);
	mutex_unlock(&ch->tx_lock);
	mutex_unlock(&ch->lc_lock);

	mutex_lock(&write_lock);
	bytes_to_write += write_size;
	mutex_unlock(&write_lock);
	queue_work(sdio_cmux_wq, &sdio_cmux_work);

	return len;
}
EXPORT_SYMBOL(sdio_cmux_write);

int is_remote_open(int id)
{
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS)
		return -ENODEV;

	return logical_ch_is_remote_open(id);
}
EXPORT_SYMBOL(is_remote_open);

int sdio_cmux_is_channel_reset(int id)
{
	int ret;
	if (id < 0 || id >= SDIO_CMUX_NUM_CHANNELS)
		return -ENODEV;

	mutex_lock(&logical_ch[id].lc_lock);
	ret = logical_ch[id].is_channel_reset;
	mutex_unlock(&logical_ch[id].lc_lock);
	return ret;
}
EXPORT_SYMBOL(sdio_cmux_is_channel_reset);

int sdio_cmux_tiocmget(int id)
{
	int ret =  (logical_ch[id].remote_status & DSR_POS ? TIOCM_DSR : 0) |
		(logical_ch[id].remote_status & CTS_POS ? TIOCM_CTS : 0) |
		(logical_ch[id].remote_status & CD_POS ? TIOCM_CD : 0) |
		(logical_ch[id].remote_status & RI_POS ? TIOCM_RI : 0) |
		(logical_ch[id].local_status & CTS_POS ? TIOCM_RTS : 0) |
		(logical_ch[id].local_status & DSR_POS ? TIOCM_DTR : 0);
	return ret;
}
EXPORT_SYMBOL(sdio_cmux_tiocmget);

int sdio_cmux_tiocmset(int id, unsigned int set, unsigned int clear)
{
	if (set & TIOCM_DTR)
		logical_ch[id].local_status |= DSR_POS;

	if (set & TIOCM_RTS)
		logical_ch[id].local_status |= CTS_POS;

	if (clear & TIOCM_DTR)
		logical_ch[id].local_status &= ~DSR_POS;

	if (clear & TIOCM_RTS)
		logical_ch[id].local_status &= ~CTS_POS;

	sdio_cmux_write_cmd(id, STATUS);
	return 0;
}
EXPORT_SYMBOL(sdio_cmux_tiocmset);

static int copy_packet(void *pkt, int size)
{
	struct sdio_cmux_list_elem *list_elem = NULL;
	void *temp_pkt = NULL;

	list_elem = kmalloc(sizeof(struct sdio_cmux_list_elem), GFP_KERNEL);
	if (!list_elem) {
		pr_err("%s: list_elem alloc failed\n", __func__);
		return -ENOMEM;
	}
	temp_pkt = kmalloc(size, GFP_KERNEL);
	if (!temp_pkt) {
		pr_err("%s: temp_pkt alloc failed\n", __func__);
		kfree(list_elem);
		return -ENOMEM;
	}

	memcpy(temp_pkt, pkt, size);
	list_elem->cmux_pkt.hdr = temp_pkt;
	list_elem->cmux_pkt.data = (void *)((char *)temp_pkt +
					    sizeof(struct sdio_cmux_hdr));
	mutex_lock(&temp_rx_lock);
	list_add_tail(&list_elem->list, &temp_rx_list);
	mutex_unlock(&temp_rx_lock);
	return 0;
}

static int process_cmux_pkt(void *pkt, int size)
{
	struct sdio_cmux_hdr *mux_hdr;
	uint32_t id, data_size;
	void *data;
	char *dump_buf = (char *)pkt;

	D_DUMP_BUFFER("process_cmux_pkt:", size, dump_buf);
	mux_hdr = (struct sdio_cmux_hdr *)pkt;
	switch (mux_hdr->cmd) {
	case OPEN:
		id = (uint32_t)(mux_hdr->lc_id);
		D("%s: Received OPEN command for ch%d\n", __func__, id);
		mutex_lock(&logical_ch[id].lc_lock);
		logical_ch[id].is_remote_open = 1;
		if (logical_ch[id].is_channel_reset) {
			sdio_cmux_write_cmd(id, OPEN);
			logical_ch[id].is_channel_reset = 0;
		}
		mutex_unlock(&logical_ch[id].lc_lock);
		wake_up(&logical_ch[id].open_wait_queue);
		break;

	case CLOSE:
		id = (uint32_t)(mux_hdr->lc_id);
		D("%s: Received CLOSE command for ch%d\n", __func__, id);
		sdio_cmux_ch_clear_and_signal(id);
		break;

	case DATA:
		id = (uint32_t)(mux_hdr->lc_id);
		D("%s: Received DATA for ch%d\n", __func__, id);
		/*Channel is not locally open & if single packet received
		  then drop it*/
		mutex_lock(&logical_ch[id].lc_lock);
		if (!logical_ch[id].is_remote_open) {
			mutex_unlock(&logical_ch[id].lc_lock);
			pr_err("%s: Remote Ch%d sent data before sending/"
			       "receiving OPEN command\n", __func__, id);
			return -ENODEV;
		}

		data = (void *)((char *)pkt + sizeof(struct sdio_cmux_hdr));
		data_size = (int)(((struct sdio_cmux_hdr *)pkt)->pkt_len);
		if (logical_ch[id].receive_cb)
			logical_ch[id].receive_cb(data, data_size,
						logical_ch[id].priv);
		else
			copy_packet(pkt, size);
		mutex_unlock(&logical_ch[id].lc_lock);
		break;

	case STATUS:
		id = (uint32_t)(mux_hdr->lc_id);
		D("%s: Received STATUS command for ch%d\n", __func__, id);
		if (logical_ch[id].remote_status != mux_hdr->status) {
			mutex_lock(&logical_ch[id].lc_lock);
			logical_ch[id].remote_status = mux_hdr->status;
			mutex_unlock(&logical_ch[id].lc_lock);
			if (logical_ch[id].status_callback)
				logical_ch[id].status_callback(
							sdio_cmux_tiocmget(id),
							logical_ch[id].priv);
		}
		break;
	}
	return 0;
}

static void parse_cmux_data(void *data, int size)
{
	int data_parsed = 0, pkt_size;
	char *temp_ptr;

	D("Entered %s\n", __func__);
	temp_ptr = (char *)data;
	while (data_parsed < size) {
		pkt_size = sizeof(struct sdio_cmux_hdr) +
			   (int)(((struct sdio_cmux_hdr *)temp_ptr)->pkt_len);
		D("Parsed %d bytes, Current Pkt Size %d bytes,"
		  " Total size %d bytes\n", data_parsed, pkt_size, size);
		process_cmux_pkt((void *)temp_ptr, pkt_size);
		data_parsed += pkt_size;
		temp_ptr += pkt_size;
	}

	kfree(data);
}

static void sdio_cdemux_fn(struct work_struct *work)
{
	int r = 0, read_avail = 0;
	void *cmux_data;

	while (1) {
		read_avail = sdio_read_avail(sdio_qmi_chl);
		if (read_avail < 0) {
			pr_err("%s: sdio_read_avail failed with rc %d\n",
				__func__, read_avail);
			return;
		}

		if (read_avail == 0) {
			D("%s: Nothing to read\n", __func__);
			return;
		}

		D("%s: kmalloc %d bytes\n", __func__, read_avail);
		cmux_data = kmalloc(read_avail, GFP_KERNEL);
		if (!cmux_data) {
			pr_err("%s: kmalloc Failed\n", __func__);
			return;
		}

		D("%s: sdio_read %d bytes\n", __func__, read_avail);
		r = sdio_read(sdio_qmi_chl, cmux_data, read_avail);
		if (r < 0) {
			pr_err("%s: sdio_read failed with rc %d\n",
				__func__, r);
			kfree(cmux_data);
			return;
		}

		parse_cmux_data(cmux_data, read_avail);
	}
	return;
}

static void sdio_cmux_fn(struct work_struct *work)
{
	int i, r = 0;
	void *write_data;
	uint32_t write_size, write_avail, write_retry = 0;
	int bytes_written;
	struct sdio_cmux_list_elem *list_elem = NULL;
	struct sdio_cmux_ch *ch;

	for (i = 0; i < SDIO_CMUX_NUM_CHANNELS; ++i) {
		ch = &logical_ch[i];
		bytes_written = 0;
		mutex_lock(&ch->tx_lock);
		while (!list_empty(&ch->tx_list)) {
			list_elem = list_first_entry(&ch->tx_list,
					     struct sdio_cmux_list_elem,
					     list);
			list_del(&list_elem->list);
			mutex_unlock(&ch->tx_lock);

			write_data = (void *)list_elem->cmux_pkt.hdr;
			write_size = sizeof(struct sdio_cmux_hdr) +
				(uint32_t)list_elem->cmux_pkt.hdr->pkt_len;

			mutex_lock(&modem_reset_lock);
			while (!(abort_tx) &&
				((write_avail = sdio_write_avail(sdio_qmi_chl))
						< write_size)) {
				mutex_unlock(&modem_reset_lock);
				pr_err("%s: sdio_write_avail %d bytes, "
				       "write size %d bytes. Waiting...\n",
					__func__, write_avail, write_size);
				msleep(250);
				mutex_lock(&modem_reset_lock);
			}
			while (!(abort_tx) &&
				((r = sdio_write(sdio_qmi_chl,
						write_data, write_size)) < 0)
				&& (r != -ENODEV)
				&& (write_retry++ < MAX_WRITE_RETRY)) {
				mutex_unlock(&modem_reset_lock);
				pr_err("%s: sdio_write failed with rc %d."
				       "Retrying...", __func__, r);
				msleep(250);
				mutex_lock(&modem_reset_lock);
			}
			if (!r && !abort_tx) {
				D("%s: sdio_write_completed %dbytes\n",
				  __func__, write_size);
				bytes_written += write_size;
			} else if (r == -ENODEV) {
				pr_err("%s: aborting_tx because sdio_write"
				       " returned %d\n", __func__, r);
				r = 0;
				abort_tx = 1;
			}
			mutex_unlock(&modem_reset_lock);
			kfree(list_elem->cmux_pkt.hdr);
			kfree(list_elem);
			mutex_lock(&write_lock);
			bytes_to_write -= write_size;
			mutex_unlock(&write_lock);
			mutex_lock(&ch->tx_lock);
		}
		if (ch->write_done)
			ch->write_done(NULL, bytes_written, ch->priv);
		mutex_unlock(&ch->tx_lock);
	}
	return;
}

static void sdio_qmi_chl_notify(void *priv, unsigned event)
{
	if (event == SDIO_EVENT_DATA_READ_AVAIL) {
		D("%s: Received SDIO_EVENT_DATA_READ_AVAIL\n", __func__);
		queue_work(sdio_cdemux_wq, &sdio_cdemux_work);
	}
}

#ifdef CONFIG_DEBUG_FS

static int debug_tbl(char *buf, int max)
{
	int i = 0;
	int j;

	for (j = 0; j < SDIO_CMUX_NUM_CHANNELS; ++j) {
		i += scnprintf(buf + i, max - i,
			"ch%02d  local open=%s  remote open=%s\n",
			j, logical_ch_is_local_open(j) ? "Y" : "N",
			logical_ch_is_remote_open(j) ? "Y" : "N");
	}

	return i;
}

#define DEBUG_BUFMAX 4096
static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
				struct dentry *dent,
				int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}

#endif

static int sdio_cmux_probe(struct platform_device *pdev)
{
	int i, r;

	mutex_lock(&probe_lock);
	D("%s Begins\n", __func__);
	if (sdio_cmux_inited) {
		mutex_lock(&modem_reset_lock);
		r =  sdio_open("SDIO_QMI", &sdio_qmi_chl, NULL,
				sdio_qmi_chl_notify);
		if (r < 0) {
			mutex_unlock(&modem_reset_lock);
			pr_err("%s: sdio_open() failed\n", __func__);
			goto error0;
		}
		abort_tx = 0;
		mutex_unlock(&modem_reset_lock);
		mutex_unlock(&probe_lock);
		return 0;
	}

	for (i = 0; i < SDIO_CMUX_NUM_CHANNELS; ++i)
		sdio_cmux_ch_alloc(i);
	INIT_LIST_HEAD(&temp_rx_list);

	sdio_cmux_wq = create_singlethread_workqueue("sdio_cmux");
	if (IS_ERR(sdio_cmux_wq)) {
		pr_err("%s: create_singlethread_workqueue() ENOMEM\n",
			__func__);
		r = -ENOMEM;
		goto error0;
	}

	sdio_cdemux_wq = create_singlethread_workqueue("sdio_cdemux");
	if (IS_ERR(sdio_cdemux_wq)) {
		pr_err("%s: create_singlethread_workqueue() ENOMEM\n",
			__func__);
		r = -ENOMEM;
		goto error1;
	}

	r = sdio_open("SDIO_QMI", &sdio_qmi_chl, NULL, sdio_qmi_chl_notify);
	if (r < 0) {
		pr_err("%s: sdio_open() failed\n", __func__);
		goto error2;
	}

	platform_device_register(&sdio_ctl_dev);
	sdio_cmux_inited = 1;
	D("SDIO Control MUX Driver Initialized.\n");
	mutex_unlock(&probe_lock);
	return 0;

error2:
	destroy_workqueue(sdio_cdemux_wq);
error1:
	destroy_workqueue(sdio_cmux_wq);
error0:
	mutex_unlock(&probe_lock);
	return r;
}

static int sdio_cmux_remove(struct platform_device *pdev)
{
	int i;

	mutex_lock(&modem_reset_lock);
	abort_tx = 1;

	for (i = 0; i < SDIO_CMUX_NUM_CHANNELS; ++i) {
		mutex_lock(&logical_ch[i].lc_lock);
		logical_ch[i].is_channel_reset = 1;
		mutex_unlock(&logical_ch[i].lc_lock);
		sdio_cmux_ch_clear_and_signal(i);
	}
	sdio_qmi_chl = NULL;
	mutex_unlock(&modem_reset_lock);

	return 0;
}

static struct platform_driver sdio_cmux_driver = {
	.probe          = sdio_cmux_probe,
	.remove         = sdio_cmux_remove,
	.driver         = {
			.name   = "SDIO_QMI",
			.owner  = THIS_MODULE,
	},
};

static int __init sdio_cmux_init(void)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *dent;

	dent = debugfs_create_dir("sdio_cmux", 0);
	if (!IS_ERR(dent))
		debug_create("tbl", 0444, dent, debug_tbl);
#endif

	msm_sdio_cmux_debug_mask = 0;
	return platform_driver_register(&sdio_cmux_driver);
}

module_init(sdio_cmux_init);
MODULE_DESCRIPTION("MSM SDIO Control MUX");
MODULE_LICENSE("GPL v2");
