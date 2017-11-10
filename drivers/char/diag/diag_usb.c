/* Copyright (c) 2014-2016, 2018 The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/kmemleak.h>
#include <linux/list.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include "diag_usb.h"
#include "diag_mux.h"
#include "diagmem.h"
#include "diag_ipc_logging.h"

#define DIAG_USB_STRING_SZ	10
#define DIAG_USB_MAX_SIZE	16384

struct diag_usb_info diag_usb[NUM_DIAG_USB_DEV] = {
	{
		.id = DIAG_USB_LOCAL,
		.name = DIAG_LEGACY,
		.enabled = 0,
		.mempool = POOL_TYPE_MUX_APPS,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.max_size = DIAG_USB_MAX_SIZE,
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = DIAG_USB_MDM,
		.name = DIAG_MDM,
		.enabled = 0,
		.mempool = POOL_TYPE_MDM_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.max_size = DIAG_USB_MAX_SIZE,
	},
	{
		.id = DIAG_USB_MDM2,
		.name = DIAG_MDM2,
		.enabled = 0,
		.mempool = POOL_TYPE_MDM2_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.max_size = DIAG_USB_MAX_SIZE,
	},
	{
		.id = DIAG_USB_QSC,
		.name = DIAG_QSC,
		.enabled = 0,
		.mempool = POOL_TYPE_QSC_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.max_size = DIAG_USB_MAX_SIZE,
	}
#endif
};

static int diag_usb_buf_tbl_add(struct diag_usb_info *usb_info,
				unsigned char *buf, uint32_t len, int ctxt)
{
	struct list_head *start, *temp;
	struct diag_usb_buf_tbl_t *entry = NULL;

	list_for_each_safe(start, temp, &usb_info->buf_tbl) {
		entry = list_entry(start, struct diag_usb_buf_tbl_t, track);
		if (entry->buf == buf) {
			atomic_inc(&entry->ref_count);
			return 0;
		}
	}

	/* New buffer, not found in the list */
	entry = kzalloc(sizeof(struct diag_usb_buf_tbl_t), GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	entry->buf = buf;
	entry->ctxt = ctxt;
	entry->len = len;
	atomic_set(&entry->ref_count, 1);
	INIT_LIST_HEAD(&entry->track);
	list_add_tail(&entry->track, &usb_info->buf_tbl);

	return 0;
}

static void diag_usb_buf_tbl_remove(struct diag_usb_info *usb_info,
				    unsigned char *buf)
{
	struct list_head *start, *temp;
	struct diag_usb_buf_tbl_t *entry = NULL;

	list_for_each_safe(start, temp, &usb_info->buf_tbl) {
		entry = list_entry(start, struct diag_usb_buf_tbl_t, track);
		if (entry->buf == buf) {
			DIAG_LOG(DIAG_DEBUG_MUX, "ref_count-- for %pK\n", buf);
			atomic_dec(&entry->ref_count);
			/*
			 * Remove reference from the table if it is the
			 * only instance of the buffer
			 */
			if (atomic_read(&entry->ref_count) == 0)
				list_del(&entry->track);
			break;
		}
	}
}

static struct diag_usb_buf_tbl_t *diag_usb_buf_tbl_get(
				struct diag_usb_info *usb_info,
				unsigned char *buf)
{
	struct list_head *start, *temp;
	struct diag_usb_buf_tbl_t *entry = NULL;

	list_for_each_safe(start, temp, &usb_info->buf_tbl) {
		entry = list_entry(start, struct diag_usb_buf_tbl_t, track);
		if (entry->buf == buf) {
			DIAG_LOG(DIAG_DEBUG_MUX, "ref_count-- for %pK\n", buf);
			atomic_dec(&entry->ref_count);
			return entry;
		}
	}

	return NULL;
}

/*
 * This function is called asynchronously when USB is connected and
 * synchronously when Diag wants to connect to USB explicitly.
 */
static void usb_connect(struct diag_usb_info *ch)
{
	int err = 0;
	int num_write = 0;
	int num_read = 1; /* Only one read buffer for any USB channel */

	if (!ch || !atomic_read(&ch->connected))
		return;

	num_write = diag_mempools[ch->mempool].poolsize;
	err = usb_diag_alloc_req(ch->hdl, num_write, num_read);
	if (err) {
		pr_err("diag: Unable to allocate usb requests for %s, write: %d read: %d, err: %d\n",
		       ch->name, num_write, num_read, err);
		return;
	}

	if (ch->ops && ch->ops->open) {
		if (atomic_read(&ch->diag_state)) {
			ch->ops->open(ch->ctxt, DIAG_USB_MODE);
		} else {
			/*
			 * This case indicates that the USB is connected
			 * but the logging is still happening in MEMORY
			 * DEVICE MODE. Continue the logging without
			 * resetting the buffers.
			 */
		}
	}
	/* As soon as we open the channel, queue a read */
	queue_work(ch->usb_wq, &(ch->read_work));
}

static void usb_connect_work_fn(struct work_struct *work)
{
	struct diag_usb_info *ch = container_of(work, struct diag_usb_info,
						connect_work);
	usb_connect(ch);
}

/*
 * This function is called asynchronously when USB is disconnected
 * and synchronously when Diag wants to disconnect from USB
 * explicitly.
 */
static void usb_disconnect(struct diag_usb_info *ch)
{
	if (!ch)
		return;

	if (!atomic_read(&ch->connected) &&
		driver->usb_connected && diag_mask_param())
		diag_clear_masks(0);

	if (ch && ch->ops && ch->ops->close)
		ch->ops->close(ch->ctxt, DIAG_USB_MODE);
}

static void usb_disconnect_work_fn(struct work_struct *work)
{
	struct diag_usb_info *ch = container_of(work, struct diag_usb_info,
						disconnect_work);
	usb_disconnect(ch);
}

static void usb_read_work_fn(struct work_struct *work)
{
	int err = 0;
	unsigned long flags;
	struct diag_request *req = NULL;
	struct diag_usb_info *ch = container_of(work, struct diag_usb_info,
						read_work);
	if (!ch)
		return;

	if (!atomic_read(&ch->connected) || !ch->enabled ||
	    atomic_read(&ch->read_pending) || !atomic_read(&ch->diag_state)) {
		pr_debug_ratelimited("diag: Discarding USB read, ch: %s e: %d, c: %d, p: %d, d: %d\n",
				     ch->name, ch->enabled,
				     atomic_read(&ch->connected),
				     atomic_read(&ch->read_pending),
				     atomic_read(&ch->diag_state));
		return;
	}

	spin_lock_irqsave(&ch->lock, flags);
	req = ch->read_ptr;
	if (req) {
		atomic_set(&ch->read_pending, 1);
		req->buf = ch->read_buf;
		req->length = USB_MAX_OUT_BUF;
		err = usb_diag_read(ch->hdl, req);
		if (err) {
			pr_debug("diag: In %s, error in reading from USB %s, err: %d\n",
				 __func__, ch->name, err);
			atomic_set(&ch->read_pending, 0);
			queue_work(ch->usb_wq, &(ch->read_work));
		}
	} else {
		pr_err_ratelimited("diag: In %s invalid read req\n", __func__);
	}
	spin_unlock_irqrestore(&ch->lock, flags);
}

static void usb_read_done_work_fn(struct work_struct *work)
{
	struct diag_request *req = NULL;
	struct diag_usb_info *ch = container_of(work, struct diag_usb_info,
						read_done_work);
	if (!ch)
		return;

	/*
	 * USB is disconnected/Disabled before the previous read completed.
	 * Discard the packet and don't do any further processing.
	 */
	if (!atomic_read(&ch->connected) || !ch->enabled ||
	    !atomic_read(&ch->diag_state))
		return;

	req = ch->read_ptr;
	ch->read_cnt++;

	if (ch->ops && ch->ops->read_done && req->status >= 0)
		ch->ops->read_done(req->buf, req->actual, ch->ctxt);
}

static void diag_usb_write_done(struct diag_usb_info *ch,
				struct diag_request *req)
{
	int ctxt = 0;
	int len = 0;
	struct diag_usb_buf_tbl_t *entry = NULL;
	unsigned char *buf = NULL;
	unsigned long flags;

	if (!ch || !req)
		return;

	ch->write_cnt++;
	entry = diag_usb_buf_tbl_get(ch, req->context);
	if (!entry) {
		pr_err_ratelimited("diag: In %s, unable to find entry %pK in the table\n",
				   __func__, req->context);
		return;
	}
	if (atomic_read(&entry->ref_count) != 0) {
		DIAG_LOG(DIAG_DEBUG_MUX, "partial write_done ref %d\n",
			 atomic_read(&entry->ref_count));
		diag_ws_on_copy_complete(DIAG_WS_MUX);
		diagmem_free(driver, req, ch->mempool);
		return;
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "full write_done, ctxt: %d\n",
		 ctxt);
	spin_lock_irqsave(&ch->write_lock, flags);
	list_del(&entry->track);
	ctxt = entry->ctxt;
	buf = entry->buf;
	len = entry->len;
	kfree(entry);
	diag_ws_on_copy_complete(DIAG_WS_MUX);

	if (ch->ops && ch->ops->write_done)
		ch->ops->write_done(buf, len, ctxt, DIAG_USB_MODE);
	buf = NULL;
	len = 0;
	ctxt = 0;
	spin_unlock_irqrestore(&ch->write_lock, flags);
	diagmem_free(driver, req, ch->mempool);
}

static void diag_usb_notifier(void *priv, unsigned event,
			      struct diag_request *d_req)
{
	int id = 0;
	unsigned long flags;
	struct diag_usb_info *usb_info = NULL;

	id = (int)(uintptr_t)priv;
	if (id < 0 || id >= NUM_DIAG_USB_DEV)
		return;
	usb_info = &diag_usb[id];

	switch (event) {
	case USB_DIAG_CONNECT:
		usb_info->max_size = usb_diag_request_size(usb_info->hdl);
		atomic_set(&usb_info->connected, 1);
		pr_info("diag: USB channel %s connected\n", usb_info->name);
		queue_work(usb_info->usb_wq,
			   &usb_info->connect_work);
		break;
	case USB_DIAG_DISCONNECT:
		atomic_set(&usb_info->connected, 0);
		pr_info("diag: USB channel %s disconnected\n", usb_info->name);
		queue_work(usb_info->usb_wq,
			   &usb_info->disconnect_work);
		break;
	case USB_DIAG_READ_DONE:
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->read_ptr = d_req;
		spin_unlock_irqrestore(&usb_info->lock, flags);
		atomic_set(&usb_info->read_pending, 0);
		queue_work(usb_info->usb_wq,
			   &usb_info->read_done_work);
		break;
	case USB_DIAG_WRITE_DONE:
		diag_usb_write_done(usb_info, d_req);
		break;
	default:
		pr_err_ratelimited("diag: Unknown event from USB diag\n");
		break;
	}
}

int diag_usb_queue_read(int id)
{
	if (id < 0 || id >= NUM_DIAG_USB_DEV) {
		pr_err_ratelimited("diag: In %s, Incorrect id %d\n",
				   __func__, id);
		return -EINVAL;
	}
	queue_work(diag_usb[id].usb_wq, &(diag_usb[id].read_work));
	return 0;
}

static int diag_usb_write_ext(struct diag_usb_info *usb_info,
			      unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	int write_len = 0;
	int bytes_remaining = len;
	int offset = 0;
	unsigned long flags;
	struct diag_request *req = NULL;

	if (!usb_info || !buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, usb_info: %pK buf: %pK, len: %d\n",
				   __func__, usb_info, buf, len);
		return -EINVAL;
	}

	spin_lock_irqsave(&usb_info->write_lock, flags);
	while (bytes_remaining > 0) {
		req = diagmem_alloc(driver, sizeof(struct diag_request),
				    usb_info->mempool);
		if (!req) {
			/*
			 * This should never happen. It either means that we are
			 * trying to write more buffers than the max supported
			 * by this particualar diag USB channel at any given
			 * instance, or the previous write ptrs are stuck in
			 * the USB layer.
			 */
			pr_err_ratelimited("diag: In %s, cannot retrieve USB write ptrs for USB channel %s\n",
					   __func__, usb_info->name);
			spin_unlock_irqrestore(&usb_info->write_lock, flags);
			return -ENOMEM;
		}

		write_len = (bytes_remaining > usb_info->max_size) ?
				usb_info->max_size : (bytes_remaining);

		req->buf = buf + offset;
		req->length = write_len;
		req->context = (void *)buf;

		if (!usb_info->hdl || !atomic_read(&usb_info->connected) ||
		    !atomic_read(&usb_info->diag_state)) {
			pr_debug_ratelimited("diag: USB ch %s is not connected\n",
					     usb_info->name);
			diagmem_free(driver, req, usb_info->mempool);
			spin_unlock_irqrestore(&usb_info->write_lock, flags);
			return -ENODEV;
		}

		if (diag_usb_buf_tbl_add(usb_info, buf, len, ctxt)) {
			diagmem_free(driver, req, usb_info->mempool);
			spin_unlock_irqrestore(&usb_info->write_lock, flags);
			return -ENOMEM;
		}

		diag_ws_on_read(DIAG_WS_MUX, len);
		err = usb_diag_write(usb_info->hdl, req);
		diag_ws_on_copy(DIAG_WS_MUX);
		if (err) {
			pr_err_ratelimited("diag: In %s, error writing to usb channel %s, err: %d\n",
					   __func__, usb_info->name, err);
			DIAG_LOG(DIAG_DEBUG_MUX,
				 "ERR! unable to write t usb, err: %d\n", err);
			diag_ws_on_copy_fail(DIAG_WS_MUX);
			diag_usb_buf_tbl_remove(usb_info, buf);
			diagmem_free(driver, req, usb_info->mempool);
			spin_unlock_irqrestore(&usb_info->write_lock, flags);
			return err;
		}
		offset += write_len;
		bytes_remaining -= write_len;
		DIAG_LOG(DIAG_DEBUG_MUX,
			 "bytes_remaining: %d write_len: %d, len: %d\n",
			 bytes_remaining, write_len, len);
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "done writing!");
	spin_unlock_irqrestore(&usb_info->write_lock, flags);

	return 0;
}

int diag_usb_write(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	struct diag_request *req = NULL;
	struct diag_usb_info *usb_info = NULL;
	unsigned long flags;

	if (id < 0 || id >= NUM_DIAG_USB_DEV) {
		pr_err_ratelimited("diag: In %s, Incorrect id %d\n",
				   __func__, id);
		return -EINVAL;
	}

	usb_info = &diag_usb[id];

	if (len > usb_info->max_size) {
		DIAG_LOG(DIAG_DEBUG_MUX, "len: %d, max_size: %d\n",
			 len, usb_info->max_size);
		return diag_usb_write_ext(usb_info, buf, len, ctxt);
	}

	req = diagmem_alloc(driver, sizeof(struct diag_request),
			    usb_info->mempool);
	if (!req) {
		/*
		 * This should never happen. It either means that we are
		 * trying to write more buffers than the max supported by
		 * this particualar diag USB channel at any given instance,
		 * or the previous write ptrs are stuck in the USB layer.
		 */
		pr_err_ratelimited("diag: In %s, cannot retrieve USB write ptrs for USB channel %s\n",
				   __func__, usb_info->name);
		return -ENOMEM;
	}

	req->buf = buf;
	req->length = len;
	req->context = (void *)buf;

	if (!usb_info->hdl || !atomic_read(&usb_info->connected) ||
	    !atomic_read(&usb_info->diag_state)) {
		pr_debug_ratelimited("diag: USB ch %s is not connected\n",
				     usb_info->name);
		diagmem_free(driver, req, usb_info->mempool);
		return -ENODEV;
	}

	spin_lock_irqsave(&usb_info->write_lock, flags);
	if (diag_usb_buf_tbl_add(usb_info, buf, len, ctxt)) {
		DIAG_LOG(DIAG_DEBUG_MUX,
					"ERR! unable to add buf %pK to table\n",
			 buf);
		diagmem_free(driver, req, usb_info->mempool);
		spin_unlock_irqrestore(&usb_info->write_lock, flags);
		return -ENOMEM;
	}

	diag_ws_on_read(DIAG_WS_MUX, len);
	err = usb_diag_write(usb_info->hdl, req);
	diag_ws_on_copy(DIAG_WS_MUX);
	if (err) {
		pr_err_ratelimited("diag: In %s, error writing to usb channel %s, err: %d\n",
				   __func__, usb_info->name, err);
		diag_ws_on_copy_fail(DIAG_WS_MUX);
		DIAG_LOG(DIAG_DEBUG_MUX,
			 "ERR! unable to write t usb, err: %d\n", err);
		diag_usb_buf_tbl_remove(usb_info, buf);
		diagmem_free(driver, req, usb_info->mempool);
	}
	spin_unlock_irqrestore(&usb_info->write_lock, flags);

	return err;
}

/*
 * This functions performs USB connect operations wrt Diag synchronously. It
 * doesn't translate to actual USB connect. This is used when Diag switches
 * logging to USB mode and wants to mimic USB connection.
 */
void diag_usb_connect_all(void)
{
	int i = 0;
	struct diag_usb_info *usb_info = NULL;

	for (i = 0; i < NUM_DIAG_USB_DEV; i++) {
		usb_info = &diag_usb[i];
		if (!usb_info->enabled)
			continue;
		atomic_set(&usb_info->diag_state, 1);
		usb_connect(usb_info);
	}
}

/*
 * This functions performs USB disconnect operations wrt Diag synchronously.
 * It doesn't translate to actual USB disconnect. This is used when Diag
 * switches logging from USB mode and want to mimic USB disconnect.
 */
void diag_usb_disconnect_all(void)
{
	int i = 0;
	struct diag_usb_info *usb_info = NULL;

	for (i = 0; i < NUM_DIAG_USB_DEV; i++) {
		usb_info = &diag_usb[i];
		if (!usb_info->enabled)
			continue;
		atomic_set(&usb_info->diag_state, 0);
		usb_disconnect(usb_info);
	}
}

int diag_usb_register(int id, int ctxt, struct diag_mux_ops *ops)
{
	struct diag_usb_info *ch = NULL;
	unsigned char wq_name[DIAG_USB_NAME_SZ + DIAG_USB_STRING_SZ];

	if (id < 0 || id >= NUM_DIAG_USB_DEV) {
		pr_err("diag: Unable to register with USB, id: %d\n", id);
		return -EIO;
	}

	if (!ops) {
		pr_err("diag: Invalid operations for USB\n");
		return -EIO;
	}

	ch = &diag_usb[id];
	ch->ops = ops;
	ch->ctxt = ctxt;
	spin_lock_init(&ch->lock);
	spin_lock_init(&ch->write_lock);
	ch->read_buf = kzalloc(USB_MAX_OUT_BUF, GFP_KERNEL);
	if (!ch->read_buf)
		goto err;
	ch->read_ptr = kzalloc(sizeof(struct diag_request), GFP_KERNEL);
	if (!ch->read_ptr)
		goto err;
	atomic_set(&ch->connected, 0);
	atomic_set(&ch->read_pending, 0);
	/*
	 * This function is called when the mux registers with Diag-USB.
	 * The registration happens during boot up and Diag always starts
	 * in USB mode. Set the state to 1.
	 */
	atomic_set(&ch->diag_state, 1);
	INIT_LIST_HEAD(&ch->buf_tbl);
	diagmem_init(driver, ch->mempool);
	INIT_WORK(&(ch->read_work), usb_read_work_fn);
	INIT_WORK(&(ch->read_done_work), usb_read_done_work_fn);
	INIT_WORK(&(ch->connect_work), usb_connect_work_fn);
	INIT_WORK(&(ch->disconnect_work), usb_disconnect_work_fn);
	strlcpy(wq_name, "DIAG_USB_", DIAG_USB_STRING_SZ);
	strlcat(wq_name, ch->name, sizeof(ch->name));
	ch->usb_wq = create_singlethread_workqueue(wq_name);
	if (!ch->usb_wq)
		goto err;
	ch->hdl = usb_diag_open(ch->name, (void *)(uintptr_t)id,
				diag_usb_notifier);
	if (IS_ERR(ch->hdl)) {
		pr_err("diag: Unable to open USB channel %s\n", ch->name);
		goto err;
	}
	ch->enabled = 1;
	pr_debug("diag: Successfully registered USB %s\n", ch->name);
	return 0;

err:
	if (ch->usb_wq)
		destroy_workqueue(ch->usb_wq);
	kfree(ch->read_ptr);
	kfree(ch->read_buf);
	return -ENOMEM;
}

void diag_usb_exit(int id)
{
	struct diag_usb_info *ch = NULL;

	if (id < 0 || id >= NUM_DIAG_USB_DEV) {
		pr_err("diag: In %s, incorrect id %d\n", __func__, id);
		return;
	}

	ch = &diag_usb[id];
	ch->ops = NULL;
	atomic_set(&ch->connected, 0);
	atomic_set(&ch->read_pending, 0);
	atomic_set(&ch->diag_state, 0);
	ch->enabled = 0;
	ch->ctxt = 0;
	ch->read_cnt = 0;
	ch->write_cnt = 0;
	diagmem_exit(driver, ch->mempool);
	ch->mempool = 0;
	if (ch->hdl) {
		usb_diag_close(ch->hdl);
		ch->hdl = NULL;
	}
	if (ch->usb_wq)
		destroy_workqueue(ch->usb_wq);
	kfree(ch->read_ptr);
	ch->read_ptr = NULL;
	kfree(ch->read_buf);
	ch->read_buf = NULL;
}

