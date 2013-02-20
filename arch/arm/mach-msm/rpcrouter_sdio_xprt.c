/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

/*
 * RPCROUTER SDIO XPRT module.
 */

#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include <mach/sdio_al.h>
#include "smd_rpcrouter.h"

enum {
	MSM_SDIO_XPRT_DEBUG = 1U << 0,
	MSM_SDIO_XPRT_INFO = 1U << 1,
};

static int msm_sdio_xprt_debug_mask;
module_param_named(debug_mask, msm_sdio_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(CONFIG_MSM_RPC_SDIO_DEBUG)
#define SDIO_XPRT_DBG(x...) do {                \
	if (msm_sdio_xprt_debug_mask & MSM_SDIO_XPRT_DEBUG)     \
		printk(KERN_DEBUG x);           \
	} while (0)

#define SDIO_XPRT_INFO(x...) do {               \
	if (msm_sdio_xprt_debug_mask & MSM_SDIO_XPRT_INFO)      \
		printk(KERN_INFO x);            \
	} while (0)
#else
#define SDIO_XPRT_DBG(x...) do { } while (0)
#define SDIO_XPRT_INFO(x...) do { } while (0)
#endif

#define MAX_SDIO_WRITE_RETRY 5
#define SDIO_BUF_SIZE (RPCROUTER_MSGSIZE_MAX + sizeof(struct rr_header) - 8)
#define NUM_SDIO_BUFS 20
#define MAX_TX_BUFS 10
#define MAX_RX_BUFS 10

struct sdio_xprt {
	struct sdio_channel *handle;

	struct list_head write_list;
	spinlock_t write_list_lock;

	struct list_head read_list;
	spinlock_t read_list_lock;

	struct list_head free_list;
	spinlock_t free_list_lock;

	struct wake_lock read_wakelock;
};

struct rpcrouter_sdio_xprt {
	struct rpcrouter_xprt xprt;
	struct sdio_xprt *channel;
};

static struct rpcrouter_sdio_xprt sdio_remote_xprt;

static void sdio_xprt_read_data(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_read_data, sdio_xprt_read_data);
static struct workqueue_struct *sdio_xprt_read_workqueue;

struct sdio_buf_struct {
	struct list_head list;
	uint32_t size;
	uint32_t read_index;
	uint32_t write_index;
	unsigned char data[SDIO_BUF_SIZE];
};

static void sdio_xprt_write_data(struct work_struct *work);
static DECLARE_WORK(work_write_data, sdio_xprt_write_data);
static wait_queue_head_t write_avail_wait_q;
static uint32_t num_free_bufs;
static uint32_t num_tx_bufs;
static uint32_t num_rx_bufs;

static DEFINE_MUTEX(modem_reset_lock);
static uint32_t modem_reset;

static void free_sdio_xprt(struct sdio_xprt *chnl)
{
	struct sdio_buf_struct *buf;
	unsigned long flags;

	if (!chnl) {
		printk(KERN_ERR "Invalid chnl to free\n");
		return;
	}

	spin_lock_irqsave(&chnl->free_list_lock, flags);
	while (!list_empty(&chnl->free_list)) {
		buf = list_first_entry(&chnl->free_list,
					struct sdio_buf_struct, list);
		list_del(&buf->list);
		kfree(buf);
	}
	num_free_bufs = 0;
	spin_unlock_irqrestore(&chnl->free_list_lock, flags);

	spin_lock_irqsave(&chnl->write_list_lock, flags);
	while (!list_empty(&chnl->write_list)) {
		buf = list_first_entry(&chnl->write_list,
					struct sdio_buf_struct, list);
		list_del(&buf->list);
		kfree(buf);
	}
	num_tx_bufs = 0;
	spin_unlock_irqrestore(&chnl->write_list_lock, flags);

	spin_lock_irqsave(&chnl->read_list_lock, flags);
	while (!list_empty(&chnl->read_list)) {
		buf = list_first_entry(&chnl->read_list,
					struct sdio_buf_struct, list);
		list_del(&buf->list);
		kfree(buf);
	}
	num_rx_bufs = 0;
	spin_unlock_irqrestore(&chnl->read_list_lock, flags);
	wake_unlock(&chnl->read_wakelock);
}

static struct sdio_buf_struct *alloc_from_free_list(struct sdio_xprt *chnl)
{
	struct sdio_buf_struct *buf;
	unsigned long flags;

	spin_lock_irqsave(&chnl->free_list_lock, flags);
	if (list_empty(&chnl->free_list)) {
		spin_unlock_irqrestore(&chnl->free_list_lock, flags);
		SDIO_XPRT_DBG("%s: Free list empty\n", __func__);
		return NULL;
	}
	buf = list_first_entry(&chnl->free_list, struct sdio_buf_struct, list);
	list_del(&buf->list);
	num_free_bufs--;
	spin_unlock_irqrestore(&chnl->free_list_lock, flags);

	buf->size = 0;
	buf->read_index = 0;
	buf->write_index = 0;

	return buf;
}

static void return_to_free_list(struct sdio_xprt *chnl,
				struct sdio_buf_struct *buf)
{
	unsigned long flags;

	if (!chnl || !buf) {
		pr_err("%s: Invalid chnl or buf\n", __func__);
		return;
	}

	buf->size = 0;
	buf->read_index = 0;
	buf->write_index = 0;

	spin_lock_irqsave(&chnl->free_list_lock, flags);
	list_add_tail(&buf->list, &chnl->free_list);
	num_free_bufs++;
	spin_unlock_irqrestore(&chnl->free_list_lock, flags);

}

static int rpcrouter_sdio_remote_read_avail(void)
{
	int read_avail = 0;
	unsigned long flags;
	struct sdio_buf_struct *buf;

	spin_lock_irqsave(&sdio_remote_xprt.channel->read_list_lock, flags);
	list_for_each_entry(buf, &sdio_remote_xprt.channel->read_list, list) {
		read_avail += buf->size;
	}
	spin_unlock_irqrestore(&sdio_remote_xprt.channel->read_list_lock,
				flags);
	return read_avail;
}

static int rpcrouter_sdio_remote_read(void *data, uint32_t len)
{
	struct sdio_buf_struct *buf;
	unsigned char *buf_data;
	unsigned long flags;

	SDIO_XPRT_DBG("sdio_xprt Called %s\n", __func__);
	if (len < 0 || !data)
		return -EINVAL;
	else if (len == 0)
		return 0;

	spin_lock_irqsave(&sdio_remote_xprt.channel->read_list_lock, flags);
	if (list_empty(&sdio_remote_xprt.channel->read_list)) {
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->read_list_lock, flags);
		return -EINVAL;
	}

	buf = list_first_entry(&sdio_remote_xprt.channel->read_list,
				struct sdio_buf_struct, list);
	if (buf->size < len) {
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->read_list_lock, flags);
		return -EINVAL;
	}

	buf_data = buf->data + buf->read_index;
	memcpy(data, buf_data, len);
	buf->read_index += len;
	buf->size -= len;
	if (buf->size == 0) {
		list_del(&buf->list);
		num_rx_bufs--;
		return_to_free_list(sdio_remote_xprt.channel, buf);
	}

	if (list_empty(&sdio_remote_xprt.channel->read_list))
		wake_unlock(&sdio_remote_xprt.channel->read_wakelock);
	spin_unlock_irqrestore(&sdio_remote_xprt.channel->read_list_lock,
				flags);
	return len;
}

static int rpcrouter_sdio_remote_write_avail(void)
{
	uint32_t write_avail = 0;
	unsigned long flags;

	SDIO_XPRT_DBG("sdio_xprt Called %s\n", __func__);
	spin_lock_irqsave(&sdio_remote_xprt.channel->write_list_lock, flags);
	write_avail = (MAX_TX_BUFS - num_tx_bufs) * SDIO_BUF_SIZE;
	spin_unlock_irqrestore(&sdio_remote_xprt.channel->write_list_lock,
				flags);
	return write_avail;
}

static int rpcrouter_sdio_remote_write(void *data, uint32_t len,
					enum write_data_type type)
{
	unsigned long flags;
	static struct sdio_buf_struct *buf;
	unsigned char *buf_data;

	switch (type) {
	case HEADER:
		spin_lock_irqsave(&sdio_remote_xprt.channel->write_list_lock,
				  flags);
		if (num_tx_bufs == MAX_TX_BUFS) {
			spin_unlock_irqrestore(
				&sdio_remote_xprt.channel->write_list_lock,
				flags);
			return -ENOMEM;
		}
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->write_list_lock, flags);

		SDIO_XPRT_DBG("sdio_xprt WRITE HEADER %s\n", __func__);
		buf = alloc_from_free_list(sdio_remote_xprt.channel);
		if (!buf) {
			pr_err("%s: alloc_from_free_list failed\n", __func__);
			return -ENOMEM;
		}
		buf_data = buf->data + buf->write_index;
		memcpy(buf_data, data, len);
		buf->write_index += len;
		buf->size += len;
		return len;
	case PACKMARK:
		SDIO_XPRT_DBG("sdio_xprt WRITE PACKMARK %s\n",	__func__);
		if (!buf) {
			pr_err("%s: HEADER not written or alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		buf_data = buf->data + buf->write_index;
		memcpy(buf_data, data, len);
		buf->write_index += len;
		buf->size += len;
		return len;
	case PAYLOAD:
		SDIO_XPRT_DBG("sdio_xprt WRITE PAYLOAD %s\n",	__func__);
		if (!buf) {
			pr_err("%s: HEADER not written or alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		buf_data = buf->data + buf->write_index;
		memcpy(buf_data, data, len);
		buf->write_index += len;
		buf->size += len;

		SDIO_XPRT_DBG("sdio_xprt flush %d bytes\n", buf->size);
		spin_lock_irqsave(&sdio_remote_xprt.channel->write_list_lock,
				   flags);
		list_add_tail(&buf->list,
			      &sdio_remote_xprt.channel->write_list);
		num_tx_bufs++;
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->write_list_lock, flags);
		queue_work(sdio_xprt_read_workqueue, &work_write_data);
		buf = NULL;
		return len;
	default:
		return -EINVAL;
	}
}

static void sdio_xprt_write_data(struct work_struct *work)
{
	int rc = 0, sdio_write_retry = 0;
	unsigned long flags;
	struct sdio_buf_struct *buf;

	mutex_lock(&modem_reset_lock);
	if (modem_reset) {
		mutex_unlock(&modem_reset_lock);
		return;
	}

	spin_lock_irqsave(&sdio_remote_xprt.channel->write_list_lock, flags);
	while (!list_empty(&sdio_remote_xprt.channel->write_list)) {
		buf = list_first_entry(&sdio_remote_xprt.channel->write_list,
					struct sdio_buf_struct, list);
		list_del(&buf->list);
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->write_list_lock, flags);
		mutex_unlock(&modem_reset_lock);

		wait_event(write_avail_wait_q,
			   (!(modem_reset) && (sdio_write_avail(
			   sdio_remote_xprt.channel->handle) >=
			   buf->size)));

		mutex_lock(&modem_reset_lock);
		while (!(modem_reset) &&
			((rc = sdio_write(sdio_remote_xprt.channel->handle,
					buf->data, buf->size)) < 0) &&
			(sdio_write_retry++ < MAX_SDIO_WRITE_RETRY)) {
			printk(KERN_ERR "sdio_write failed with RC %d\n", rc);
			mutex_unlock(&modem_reset_lock);
			msleep(250);
			mutex_lock(&modem_reset_lock);
		}
		if (modem_reset) {
			mutex_unlock(&modem_reset_lock);
			kfree(buf);
			return;
		} else {
			return_to_free_list(sdio_remote_xprt.channel, buf);
		}

		if (!rc)
			SDIO_XPRT_DBG("sdio_write %d bytes completed\n",
					buf->size);

		spin_lock_irqsave(&sdio_remote_xprt.channel->write_list_lock,
				   flags);
		num_tx_bufs--;
	}
	spin_unlock_irqrestore(&sdio_remote_xprt.channel->write_list_lock,
				flags);
	mutex_unlock(&modem_reset_lock);
}

static int rpcrouter_sdio_remote_close(void)
{
	SDIO_XPRT_DBG("sdio_xprt Called %s\n", __func__);
	flush_workqueue(sdio_xprt_read_workqueue);
	sdio_close(sdio_remote_xprt.channel->handle);
	free_sdio_xprt(sdio_remote_xprt.channel);
	return 0;
}

static void sdio_xprt_read_data(struct work_struct *work)
{
	int size = 0, read_avail;
	unsigned long flags;
	struct sdio_buf_struct *buf;
	SDIO_XPRT_DBG("sdio_xprt Called %s\n", __func__);

	mutex_lock(&modem_reset_lock);
	while (!(modem_reset) &&
		((read_avail =
		sdio_read_avail(sdio_remote_xprt.channel->handle)) > 0)) {
		spin_lock_irqsave(&sdio_remote_xprt.channel->read_list_lock,
				  flags);
		if (num_rx_bufs == MAX_RX_BUFS) {
			spin_unlock_irqrestore(
				&sdio_remote_xprt.channel->read_list_lock,
				flags);
			queue_delayed_work(sdio_xprt_read_workqueue,
					   &work_read_data,
					   msecs_to_jiffies(100));
			break;
		}
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->read_list_lock, flags);

		buf = alloc_from_free_list(sdio_remote_xprt.channel);
		if (!buf) {
			SDIO_XPRT_DBG("%s: Failed to alloc_from_free_list"
				      " Try again later\n", __func__);
			queue_delayed_work(sdio_xprt_read_workqueue,
					   &work_read_data,
					   msecs_to_jiffies(100));
			break;
		}

		size = sdio_read(sdio_remote_xprt.channel->handle,
				 buf->data, read_avail);
		if (size < 0) {
			printk(KERN_ERR "sdio_read failed,"
					" read %d bytes, expected %d\n",
					size, read_avail);
			return_to_free_list(sdio_remote_xprt.channel, buf);
			queue_delayed_work(sdio_xprt_read_workqueue,
					   &work_read_data,
					   msecs_to_jiffies(100));
			break;
		}

		if (size == 0)
			size = read_avail;

		buf->size = size;
		buf->write_index = size;
		spin_lock_irqsave(&sdio_remote_xprt.channel->read_list_lock,
				   flags);
		list_add_tail(&buf->list,
			      &sdio_remote_xprt.channel->read_list);
		num_rx_bufs++;
		spin_unlock_irqrestore(
			&sdio_remote_xprt.channel->read_list_lock, flags);
		wake_lock(&sdio_remote_xprt.channel->read_wakelock);
	}

	if (!modem_reset && !list_empty(&sdio_remote_xprt.channel->read_list))
		msm_rpcrouter_xprt_notify(&sdio_remote_xprt.xprt,
				  RPCROUTER_XPRT_EVENT_DATA);
	mutex_unlock(&modem_reset_lock);
}

static void rpcrouter_sdio_remote_notify(void *_dev, unsigned event)
{
	if (event == SDIO_EVENT_DATA_READ_AVAIL) {
		SDIO_XPRT_DBG("%s Received Notify"
			      "SDIO_EVENT_DATA_READ_AVAIL\n", __func__);
		queue_delayed_work(sdio_xprt_read_workqueue,
				   &work_read_data, 0);
	}
	if (event == SDIO_EVENT_DATA_WRITE_AVAIL) {
		SDIO_XPRT_DBG("%s Received Notify"
			      "SDIO_EVENT_DATA_WRITE_AVAIL\n", __func__);
		wake_up(&write_avail_wait_q);
	}
}

static int allocate_sdio_xprt(struct sdio_xprt **sdio_xprt_chnl)
{
	struct sdio_buf_struct *buf;
	struct sdio_xprt *chnl;
	int i;
	unsigned long flags;
	int rc = -ENOMEM;

	if (!(*sdio_xprt_chnl)) {
		chnl = kmalloc(sizeof(struct sdio_xprt), GFP_KERNEL);
		if (!chnl) {
			printk(KERN_ERR "sdio_xprt channel"
					" allocation failed\n");
			return rc;
		}

		spin_lock_init(&chnl->write_list_lock);
		spin_lock_init(&chnl->read_list_lock);
		spin_lock_init(&chnl->free_list_lock);

		INIT_LIST_HEAD(&chnl->write_list);
		INIT_LIST_HEAD(&chnl->read_list);
		INIT_LIST_HEAD(&chnl->free_list);
		wake_lock_init(&chnl->read_wakelock,
				WAKE_LOCK_SUSPEND, "rpc_sdio_xprt_read");
	} else {
		chnl = *sdio_xprt_chnl;
	}

	for (i = 0; i < NUM_SDIO_BUFS; i++) {
		buf = kzalloc(sizeof(struct sdio_buf_struct), GFP_KERNEL);
		if (!buf) {
			printk(KERN_ERR "sdio_buf_struct alloc failed\n");
			goto alloc_failure;
		}
		spin_lock_irqsave(&chnl->free_list_lock, flags);
		list_add_tail(&buf->list, &chnl->free_list);
		spin_unlock_irqrestore(&chnl->free_list_lock, flags);
	}
	num_free_bufs = NUM_SDIO_BUFS;

	*sdio_xprt_chnl = chnl;
	return 0;

alloc_failure:
	spin_lock_irqsave(&chnl->free_list_lock, flags);
	while (!list_empty(&chnl->free_list)) {
		buf = list_first_entry(&chnl->free_list,
					struct sdio_buf_struct,
					list);
		list_del(&buf->list);
		kfree(buf);
	}
	spin_unlock_irqrestore(&chnl->free_list_lock, flags);
	wake_lock_destroy(&chnl->read_wakelock);

	kfree(chnl);
	*sdio_xprt_chnl = NULL;
	return rc;
}

static int rpcrouter_sdio_remote_probe(struct platform_device *pdev)
{
	int rc;

	SDIO_XPRT_INFO("%s Called\n", __func__);

	mutex_lock(&modem_reset_lock);
	if (!modem_reset) {
		sdio_xprt_read_workqueue =
			create_singlethread_workqueue("sdio_xprt");
		if (!sdio_xprt_read_workqueue) {
			mutex_unlock(&modem_reset_lock);
			return -ENOMEM;
		}

		sdio_remote_xprt.xprt.name = "rpcrotuer_sdio_xprt";
		sdio_remote_xprt.xprt.read_avail =
			rpcrouter_sdio_remote_read_avail;
		sdio_remote_xprt.xprt.read = rpcrouter_sdio_remote_read;
		sdio_remote_xprt.xprt.write_avail =
			rpcrouter_sdio_remote_write_avail;
		sdio_remote_xprt.xprt.write = rpcrouter_sdio_remote_write;
		sdio_remote_xprt.xprt.close = rpcrouter_sdio_remote_close;
		sdio_remote_xprt.xprt.priv = NULL;

		init_waitqueue_head(&write_avail_wait_q);
	}
	modem_reset = 0;

	rc = allocate_sdio_xprt(&sdio_remote_xprt.channel);
	if (rc) {
		destroy_workqueue(sdio_xprt_read_workqueue);
		mutex_unlock(&modem_reset_lock);
		return rc;
	}

	/* Open up SDIO channel */
	rc = sdio_open("SDIO_RPC", &sdio_remote_xprt.channel->handle, NULL,
		      rpcrouter_sdio_remote_notify);

	if (rc < 0) {
		free_sdio_xprt(sdio_remote_xprt.channel);
		destroy_workqueue(sdio_xprt_read_workqueue);
		mutex_unlock(&modem_reset_lock);
		return rc;
	}
	mutex_unlock(&modem_reset_lock);

	msm_rpcrouter_xprt_notify(&sdio_remote_xprt.xprt,
				  RPCROUTER_XPRT_EVENT_OPEN);

	SDIO_XPRT_INFO("%s Completed\n", __func__);

	return 0;
}

static int rpcrouter_sdio_remote_remove(struct platform_device *pdev)
{
	SDIO_XPRT_INFO("%s Called\n", __func__);

	mutex_lock(&modem_reset_lock);
	modem_reset = 1;
	wake_up(&write_avail_wait_q);
	free_sdio_xprt(sdio_remote_xprt.channel);
	mutex_unlock(&modem_reset_lock);

	msm_rpcrouter_xprt_notify(&sdio_remote_xprt.xprt,
				  RPCROUTER_XPRT_EVENT_CLOSE);

	SDIO_XPRT_INFO("%s Completed\n", __func__);

	return 0;
}

/*Remove this platform driver after mainline of SDIO_AL update*/
static struct platform_driver rpcrouter_sdio_remote_driver = {
	.probe		= rpcrouter_sdio_remote_probe,
	.driver		= {
			.name	= "SDIO_AL",
			.owner	= THIS_MODULE,
	},
};

static struct platform_driver rpcrouter_sdio_driver = {
	.probe		= rpcrouter_sdio_remote_probe,
	.remove		= rpcrouter_sdio_remote_remove,
	.driver		= {
			.name	= "SDIO_RPC",
			.owner	= THIS_MODULE,
	},
};

static int __init rpcrouter_sdio_init(void)
{
	int rc;
	msm_sdio_xprt_debug_mask = 0x2;
	rc = platform_driver_register(&rpcrouter_sdio_remote_driver);
	if (rc < 0)
		return rc;
	return platform_driver_register(&rpcrouter_sdio_driver);
}

module_init(rpcrouter_sdio_init);
MODULE_DESCRIPTION("RPC Router SDIO XPRT");
MODULE_LICENSE("GPL v2");
