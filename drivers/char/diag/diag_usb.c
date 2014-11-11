/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include "diag_usb.h"
#include "diag_mux.h"
#include "diagmem.h"

#define DIAG_USB_STRING_SZ	10

struct diag_usb_info diag_usb[NUM_DIAG_USB_DEV] = {
	{
		.id = DIAG_USB_LOCAL,
		.name = DIAG_LEGACY,
		.connected = 0,
		.enabled = 0,
		.mempool = POOL_TYPE_MUX_APPS,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.read_pending = 0,
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = DIAG_USB_MDM,
		.name = DIAG_MDM,
		.connected = 0,
		.enabled = 0,
		.mempool = POOL_TYPE_MDM_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.read_pending = 0,
	},
	{
		.id = DIAG_USB_MDM2,
		.name = DIAG_MDM2,
		.connected = 0,
		.enabled = 0,
		.mempool = POOL_TYPE_MDM2_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.read_pending = 0,
	},
	{
		.id = DIAG_USB_QSC,
		.name = DIAG_QSC,
		.connected = 0,
		.enabled = 0,
		.mempool = POOL_TYPE_QSC_MUX,
		.hdl = NULL,
		.ops = NULL,
		.read_buf = NULL,
		.read_ptr = NULL,
		.usb_wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.read_pending = 0,
	}
#endif
};

/*
 * This function is called asynchronously when USB is connected and
 * synchronously when Diag wants to connect to USB explicitly.
 */
static void usb_connect(struct diag_usb_info *ch)
{
	int err = 0;
	int num_write = 0;
	int num_read = 1; /* Only one read buffer for any USB channel */

	if (!ch)
		return;

	num_write = diag_mempools[ch->mempool].poolsize;
	err = usb_diag_alloc_req(ch->hdl, num_write, num_read);
	if (err) {
		pr_err("diag: Unable to allocate usb requests for %s, write: %d read: %d, err: %d\n",
		       ch->name, num_write, num_read, err);
		return;
	}

	if (ch->ops && ch->ops->open)
		ch->ops->open(ch->ctxt, DIAG_USB_MODE);
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
	unsigned long flags;
	struct diag_request *req = NULL;
	struct diag_usb_info *ch = container_of(work, struct diag_usb_info,
						read_work);
	if (!ch)
		return;

	if (!ch->connected || !ch->enabled || ch->read_pending) {
		pr_debug_ratelimited("diag: Discarding USB read, ch: %s connected: %d, enabled: %d, pending: %d\n",
				     ch->name, ch->connected, ch->enabled,
				     ch->read_pending);
		return;
	}

	spin_lock_irqsave(&ch->lock, flags);
	req = ch->read_ptr;
	if (req) {
		ch->read_pending = 1;
		req->buf = ch->read_buf;
		req->length = USB_MAX_OUT_BUF;
		usb_diag_read(ch->hdl, req);
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
	if (!ch->connected || !ch->enabled)
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

	if (!ch || !req)
		return;

	ch->write_cnt++;
	ctxt = (int)(uintptr_t)req->context;
	if (ch->ops && ch->ops->write_done)
		ch->ops->write_done(req->buf, req->actual, ctxt, ch->ctxt);
	diagmem_free(driver, req, ch->mempool);
	queue_work(ch->usb_wq, &(ch->read_work));
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
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->connected = 1;
		spin_unlock_irqrestore(&usb_info->lock, flags);
		pr_info("diag: USB channel %s connected\n", usb_info->name);
		queue_work(usb_info->usb_wq,
			   &usb_info->connect_work);
		break;
	case USB_DIAG_DISCONNECT:
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->connected = 0;
		spin_unlock_irqrestore(&usb_info->lock, flags);
		pr_info("diag: USB channel %s disconnected\n", usb_info->name);
		queue_work(usb_info->usb_wq,
			   &usb_info->disconnect_work);
		break;
	case USB_DIAG_READ_DONE:
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->read_ptr = d_req;
		usb_info->read_pending = 0;
		spin_unlock_irqrestore(&usb_info->lock, flags);
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

int diag_usb_write(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	struct diag_request *req = NULL;
	struct diag_usb_info *usb_info = NULL;

	if (id < 0 || id >= NUM_DIAG_USB_DEV) {
		pr_err_ratelimited("diag: In %s, Incorrect id %d\n",
				   __func__, id);
		return -EINVAL;
	}

	usb_info = &diag_usb[id];

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
	req->context = (void *)(uintptr_t)ctxt;

	if (!usb_info->hdl || !usb_info->connected) {
		pr_debug_ratelimited("diag: USB ch %s is not connected\n",
				     usb_info->name);
		diagmem_free(driver, req, usb_info->mempool);
		return -ENODEV;
	}
	err = usb_diag_write(usb_info->hdl, req);
	if (err) {
		pr_err_ratelimited("diag: In %s, error writing to usb channel %s, err: %d\n",
				   __func__, usb_info->name, err);
		diagmem_free(driver, req, usb_info->mempool);
	}

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
	unsigned long flags;
	struct diag_usb_info *usb_info = NULL;

	for (i = 0; i < NUM_DIAG_USB_DEV; i++) {
		usb_info = &diag_usb[i];
		if (!usb_info->enabled)
			continue;
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->connected = 1;
		spin_unlock_irqrestore(&usb_info->lock, flags);
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
	unsigned long flags;
	struct diag_usb_info *usb_info = NULL;

	for (i = 0; i < NUM_DIAG_USB_DEV; i++) {
		usb_info = &diag_usb[i];
		if (!usb_info->enabled)
			continue;
		spin_lock_irqsave(&usb_info->lock, flags);
		usb_info->connected = 0;
		spin_unlock_irqrestore(&usb_info->lock, flags);
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
	ch->read_buf = kzalloc(USB_MAX_OUT_BUF, GFP_KERNEL);
	if (!ch->read_buf)
		goto err;
	ch->read_ptr = kzalloc(sizeof(struct diag_request), GFP_KERNEL);
	if (!ch->read_ptr)
		goto err;
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
	ch->connected = 0;
	ch->enabled = 0;
	ch->ctxt = 0;
	ch->read_cnt = 0;
	ch->write_cnt = 0;
	ch->read_pending = 0;
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

