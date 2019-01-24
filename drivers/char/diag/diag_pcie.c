/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include "diag_pcie.h"
#include "diag_mux.h"
#include "diagmem.h"
#include "diag_ipc_logging.h"
#define DIAG_LEGACY "DIAG_PCIE"

struct diag_pcie_info diag_pcie[NUM_DIAG_PCIE_DEV] = {
	{
		.id = DIAG_PCIE_LOCAL,
		.name = DIAG_LEGACY,
		.enabled = {0},
		.mempool = POOL_TYPE_MUX_APPS,
		.ops = NULL,
		.wq = NULL,
		.read_cnt = 0,
		.write_cnt = 0,
		.in_chan_attr = {
			.max_pkt_size = DIAG_MAX_PKT_SZ,
			.nr_trbs = 1,
			.read_buffer = NULL,
		},
		.out_chan_attr = {
			.max_pkt_size = DIAG_MAX_PCIE_PKT_SZ,
		},
		.in_chan = MHI_CLIENT_DIAG_OUT,
		.out_chan = MHI_CLIENT_DIAG_IN,
	}
};

static void diag_pcie_event_notifier(struct mhi_dev_client_cb_reason *reason)
{
	int i;
	struct diag_pcie_info *pcie_info = NULL;

	for (i = 0; i < NUM_DIAG_PCIE_DEV; i++) {
		pcie_info = &diag_pcie[i];
		if (reason->reason == MHI_DEV_TRE_AVAILABLE)
			if (reason->ch_id == pcie_info->in_chan) {
				queue_work(pcie_info->wq,
					&pcie_info->read_work);
				break;
			}
	}
}

void diag_pcie_read_work_fn(struct work_struct *work)
{
	struct mhi_req ureq;
	struct diag_pcie_info *pcie_info = container_of(work,
						      struct diag_pcie_info,
						      read_work);
	unsigned int bytes_avail = 0;

	if (!pcie_info || !atomic_read(&pcie_info->enabled) ||
		!atomic_read(&pcie_info->diag_state))
		return;

	ureq.chan = pcie_info->in_chan;
	ureq.client = pcie_info->in_handle;
	ureq.mode = IPA_DMA_SYNC;
	ureq.buf = pcie_info->in_chan_attr.read_buffer;
	ureq.len = pcie_info->in_chan_attr.read_buffer_size;
	ureq.transfer_len = 0;
	bytes_avail = mhi_dev_read_channel(&ureq);
	if (bytes_avail < 0)
		return;
	DIAG_LOG(DIAG_DEBUG_MUX, "read total bytes %d from chan:%d",
		bytes_avail, pcie_info->in_chan);
	pcie_info->read_cnt++;

	if (pcie_info->ops && pcie_info->ops->read_done)
		pcie_info->ops->read_done(pcie_info->in_chan_attr.read_buffer,
					ureq.transfer_len, pcie_info->ctxt);

}

static void diag_pcie_buf_tbl_remove(struct diag_pcie_info *pcie_info,
				    unsigned char *buf)
{
	struct diag_pcie_buf_tbl_t *temp = NULL;
	struct diag_pcie_buf_tbl_t *entry = NULL;

	list_for_each_entry_safe(entry, temp, &pcie_info->buf_tbl, track) {
		if (entry->buf == buf) {
			DIAG_LOG(DIAG_DEBUG_MUX, "ref_count-- for %pK\n", buf);
			atomic_dec(&entry->ref_count);
			/*
			 * Remove reference from the table if it is the
			 * only instance of the buffer
			 */
			if (atomic_read(&entry->ref_count) == 0) {
				list_del(&entry->track);
				kfree(entry);
				entry = NULL;
			}
			break;
		}
	}
}

static struct diag_pcie_buf_tbl_t *diag_pcie_buf_tbl_get(
				struct diag_pcie_info *pcie_info,
				unsigned char *buf)
{
	struct diag_pcie_buf_tbl_t *temp = NULL;
	struct diag_pcie_buf_tbl_t *entry = NULL;

	list_for_each_entry_safe(entry, temp, &pcie_info->buf_tbl, track) {
		if (entry->buf == buf) {
			DIAG_LOG(DIAG_DEBUG_MUX, "ref_count-- for %pK\n", buf);
			atomic_dec(&entry->ref_count);
			return entry;
		}
	}

	return NULL;
}

void diag_pcie_write_complete_cb(void *req)
{
	struct diag_pcie_context *ctxt = NULL;
	struct diag_pcie_info *ch;
	struct diag_pcie_buf_tbl_t *entry = NULL;
	struct mhi_req *ureq = req;
	unsigned long flags;

	if (!ureq)
		return;
	ctxt = (struct diag_pcie_context *)ureq->context;
	if (!ctxt)
		return;
	ch = ctxt->ch;
	if (!ch)
		return;
	spin_lock_irqsave(&ch->write_lock, flags);
	ch->write_cnt++;
	entry = diag_pcie_buf_tbl_get(ch, ctxt->buf);
	if (!entry) {
		pr_err_ratelimited("diag: In %s, unable to find entry %pK in the table\n",
				   __func__, ctxt->buf);
		spin_unlock_irqrestore(&ch->write_lock, flags);
		return;
	}
	if (atomic_read(&entry->ref_count) != 0) {
		DIAG_LOG(DIAG_DEBUG_MUX, "partial write_done ref %d\n",
			 atomic_read(&entry->ref_count));
		diag_ws_on_copy_complete(DIAG_WS_MUX);
		spin_unlock_irqrestore(&ch->write_lock, flags);
		diagmem_free(driver, req, ch->mempool);
		kfree(ctxt);
		ctxt = NULL;
		return;
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "full write_done, ctxt: %pK\n",
		 ctxt->buf);
	list_del(&entry->track);
	kfree(entry);
	entry = NULL;
	if (ch->ops && ch->ops->write_done)
		ch->ops->write_done(ureq->buf, ureq->len,
				ctxt->buf_ctxt, DIAG_PCIE_MODE);
	spin_unlock_irqrestore(&ch->write_lock, flags);
	diagmem_free(driver, req, ch->mempool);
	kfree(ctxt);
	ctxt = NULL;
}

static int diag_pcie_buf_tbl_add(struct diag_pcie_info *pcie_info,
				unsigned char *buf, uint32_t len, int ctxt)
{
	struct diag_pcie_buf_tbl_t *temp = NULL;
	struct diag_pcie_buf_tbl_t *entry = NULL;

	list_for_each_entry_safe(entry, temp, &pcie_info->buf_tbl, track) {
		if (entry->buf == buf) {
			atomic_inc(&entry->ref_count);
			return 0;
		}
	}

	/* New buffer, not found in the list */
	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	entry->buf = buf;
	entry->ctxt = ctxt;
	entry->len = len;
	atomic_set(&entry->ref_count, 1);
	INIT_LIST_HEAD(&entry->track);
	list_add_tail(&entry->track, &pcie_info->buf_tbl);

	return 0;
}

static int diag_pcie_write_ext(struct diag_pcie_info *pcie_info,
			      unsigned char *buf, int len, int ctxt)
{
	int write_len = 0;
	int bytes_remaining = len;
	int offset = 0;
	struct mhi_req *req;
	struct diag_pcie_context *context;
	int bytes_to_write;
	unsigned long flags;

	if (!pcie_info || !buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, pcie_info: %pK buf: %pK, len: %d\n",
				   __func__, pcie_info, buf, len);
		return -EINVAL;
	}

	while (bytes_remaining > 0) {
		req = diagmem_alloc(driver, sizeof(struct mhi_req),
				    pcie_info->mempool);
		if (!req) {
			pr_err_ratelimited("diag: In %s, cannot retrieve pcie write ptrs for pcie channel %s\n",
					   __func__, pcie_info->name);
			return -ENOMEM;
		}

		write_len = (bytes_remaining >
				pcie_info->out_chan_attr.max_pkt_size) ?
				pcie_info->out_chan_attr.max_pkt_size :
				bytes_remaining;
		req->client = pcie_info->out_handle;
		context = kzalloc(sizeof(*context), GFP_KERNEL);
		if (!context)
			return -ENOMEM;

		context->ch = pcie_info;
		context->buf_ctxt = ctxt;
		context->buf = buf;
		req->context = context;
		req->buf = buf + offset;
		req->len = write_len;
		req->chan = pcie_info->out_chan;
		req->mode = IPA_DMA_ASYNC;
		req->client_cb = diag_pcie_write_complete_cb;
		req->snd_cmpl = 1;
		if (!pcie_info->out_handle ||
			!atomic_read(&pcie_info->enabled) ||
			!atomic_read(&pcie_info->diag_state)) {
			pr_debug_ratelimited("diag: pcie ch %s is not opened\n",
					     pcie_info->name);
			kfree(req->context);
			diagmem_free(driver, req, pcie_info->mempool);
			return -ENODEV;
		}
		spin_lock_irqsave(&pcie_info->write_lock, flags);
		if (diag_pcie_buf_tbl_add(pcie_info, buf, len, ctxt)) {
			kfree(req->context);
			diagmem_free(driver, req, pcie_info->mempool);
			spin_unlock_irqrestore(&pcie_info->write_lock, flags);
			return -ENOMEM;
		}
		spin_unlock_irqrestore(&pcie_info->write_lock, flags);
		diag_ws_on_read(DIAG_WS_MUX, len);
		bytes_to_write = mhi_dev_write_channel(req);
		diag_ws_on_copy(DIAG_WS_MUX);
		if (bytes_to_write != write_len) {
			pr_err_ratelimited("diag: In %s, error writing to pcie channel %s, err: %d, write_len: %d\n",
					   __func__, pcie_info->name,
					bytes_to_write, write_len);
			DIAG_LOG(DIAG_DEBUG_MUX,
				 "ERR! unable to write to pcie, err: %d, write_len: %d\n",
				bytes_to_write, write_len);
			diag_ws_on_copy_fail(DIAG_WS_MUX);
			spin_lock_irqsave(&pcie_info->write_lock, flags);
			diag_pcie_buf_tbl_remove(pcie_info, buf);
			kfree(req->context);
			diagmem_free(driver, req, pcie_info->mempool);
			spin_unlock_irqrestore(&pcie_info->write_lock, flags);
			return -EINVAL;
		}
		offset += write_len;
		bytes_remaining -= write_len;
		DIAG_LOG(DIAG_DEBUG_MUX,
			 "bytes_remaining: %d write_len: %d, len: %d\n",
			 bytes_remaining, write_len, len);
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "done writing!");

	return 0;
}

int diag_pcie_write(int id, unsigned char *buf, int len, int ctxt)
{
	struct mhi_req *req;
	struct diag_pcie_context *context;
	int bytes_to_write;
	struct diag_pcie_info *pcie_info;
	unsigned long flags;

	pcie_info = &diag_pcie[id];

	if (len > pcie_info->out_chan_attr.max_pkt_size) {
		DIAG_LOG(DIAG_DEBUG_MUX, "len: %d, max_size: %zu\n",
			 len, pcie_info->out_chan_attr.max_pkt_size);
		return diag_pcie_write_ext(pcie_info, buf, len, ctxt);
	}
	req = (struct mhi_req *)diagmem_alloc(driver, sizeof(struct mhi_req),
		    pcie_info->mempool);
	if (!req) {
		pr_err_ratelimited("diag: In %s, cannot retrieve pcie write ptrs for pcie channel %s\n",
				 __func__, pcie_info->name);
		return -ENOMEM;
	}
	req->client = pcie_info->out_handle;
	context = kzalloc(sizeof(struct diag_pcie_context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	context->ch = &diag_pcie[id];
	context->buf_ctxt = ctxt;
	context->buf = buf;
	req->context = context;
	req->buf = buf;
	req->len = len;
	req->chan = pcie_info->out_chan;
	req->mode = IPA_DMA_ASYNC;
	req->client_cb = diag_pcie_write_complete_cb;
	req->snd_cmpl = 1;
	if (!pcie_info->out_handle || !atomic_read(&pcie_info->enabled) ||
		!atomic_read(&pcie_info->diag_state)) {
		pr_debug_ratelimited("diag: pcie ch %s is not opened\n",
					pcie_info->name);
		kfree(req->context);
		diagmem_free(driver, req, pcie_info->mempool);
		return -ENODEV;
	}
	spin_lock_irqsave(&pcie_info->write_lock, flags);
	if (diag_pcie_buf_tbl_add(pcie_info, buf, len, ctxt)) {
		DIAG_LOG(DIAG_DEBUG_MUX,
			"ERR! unable to add buf %pK to table\n", buf);
		kfree(req->context);
		diagmem_free(driver, req, pcie_info->mempool);
		spin_unlock_irqrestore(&pcie_info->write_lock, flags);
		return -ENOMEM;
	}
	spin_unlock_irqrestore(&pcie_info->write_lock, flags);
	diag_ws_on_read(DIAG_WS_MUX, len);
	bytes_to_write = mhi_dev_write_channel(req);
	diag_ws_on_copy(DIAG_WS_MUX);
	if (bytes_to_write != len) {
		pr_err_ratelimited("diag: In %s, error writing to pcie channel %s, err: %d len: %d\n",
			__func__, pcie_info->name, bytes_to_write, len);
		diag_ws_on_copy_fail(DIAG_WS_MUX);
		DIAG_LOG(DIAG_DEBUG_MUX,
			 "ERR! unable to write to pcie, err: %d len: %d\n",
			bytes_to_write, len);
		spin_lock_irqsave(&pcie_info->write_lock, flags);
		diag_pcie_buf_tbl_remove(pcie_info, buf);
		spin_unlock_irqrestore(&pcie_info->write_lock, flags);
		kfree(req->context);
		diagmem_free(driver, req, pcie_info->mempool);
		return -EINVAL;
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "wrote packet to pcie chan:%d, len:%d",
		pcie_info->out_chan, len);

	return 0;
}

static int pcie_init_read_chan(struct diag_pcie_info *ptr,
		enum mhi_client_channel chan)
{
	int rc = 0;
	size_t buf_size;
	void *data_loc;

	if (ptr == NULL) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "Bad Input data, quitting\n");
		return -EINVAL;
	}

	buf_size = ptr->in_chan_attr.max_pkt_size;
	data_loc = kzalloc(buf_size, GFP_KERNEL);
	if (!data_loc)
		return -ENOMEM;

	kmemleak_not_leak(data_loc);
	ptr->in_chan_attr.read_buffer = data_loc;
	ptr->in_chan_attr.read_buffer_size = buf_size;

	return rc;

}

void diag_pcie_client_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct diag_pcie_info *pcie_info = NULL;

	if (!cb_data)
		return;

	pcie_info = cb_data->user_data;
	if (!pcie_info)
		return;

	switch (cb_data->ctrl_info) {
	case  MHI_STATE_CONNECTED:
		if (cb_data->channel == pcie_info->out_chan) {
			DIAG_LOG(DIAG_DEBUG_MUX,
				" Received connect event from MHI for %d",
				pcie_info->out_chan);
			if (atomic_read(&pcie_info->enabled))
				return;
			queue_work(pcie_info->wq, &pcie_info->open_work);
		}
		break;
	case MHI_STATE_DISCONNECTED:
		if (cb_data->channel == pcie_info->out_chan) {
			DIAG_LOG(DIAG_DEBUG_MUX,
				" Received disconnect event from MHI for %d",
				pcie_info->out_chan);
			if (!atomic_read(&pcie_info->enabled))
				return;
			queue_work(pcie_info->wq, &pcie_info->close_work);
		}
		break;
	default:
		break;
	}
}

static int diag_register_pcie_channels(struct diag_pcie_info *pcie_info)
{
	int rc = 0;

	if (!pcie_info)
		return -EIO;

	pcie_info->event_notifier = diag_pcie_event_notifier;

	DIAG_LOG(DIAG_DEBUG_MUX,
		"Initializing inbound chan %d.\n",
		pcie_info->in_chan);
	rc = pcie_init_read_chan(pcie_info, pcie_info->in_chan);
	if (rc < 0) {
		DIAG_LOG(DIAG_DEBUG_MUX,
			"Failed to init inbound 0x%x, ret 0x%x\n",
			pcie_info->in_chan, rc);
		return rc;
	}
	/* Register for state change notifications from mhi*/
	rc = mhi_register_state_cb(diag_pcie_client_cb, pcie_info,
						pcie_info->out_chan);
	if (rc < 0)
		return rc;

	return 0;
}

static void diag_pcie_connect(struct diag_pcie_info *ch)
{
	if (!ch || !atomic_read(&ch->enabled))
		return;

	if (ch->ops && ch->ops->open)
		if (atomic_read(&ch->diag_state))
			ch->ops->open(ch->ctxt, DIAG_PCIE_MODE);

	/* As soon as we open the channel, queue a read */
	queue_work(ch->wq, &(ch->read_work));
}

void diag_pcie_open_work_fn(struct work_struct *work)
{
	int rc = 0;
	struct diag_pcie_info *pcie_info = container_of(work,
						      struct diag_pcie_info,
						      open_work);

	if (!pcie_info || atomic_read(&pcie_info->enabled))
		return;

	mutex_lock(&pcie_info->out_chan_lock);
	mutex_lock(&pcie_info->in_chan_lock);
	/* Open write channel*/
	rc = mhi_dev_open_channel(pcie_info->out_chan,
			&pcie_info->out_handle,
			pcie_info->event_notifier);
	if (rc < 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Failed to open chan %d, ret %d\n",
			pcie_info->in_chan, rc);
		goto handle_not_rdy_err;
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "opened write channel %d",
		pcie_info->out_chan);

	/* Open read channel*/
	rc = mhi_dev_open_channel(pcie_info->in_chan,
			&pcie_info->in_handle,
			pcie_info->event_notifier);
	if (rc < 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Failed to open chan %d, ret 0x%x\n",
			pcie_info->in_chan, rc);
		goto handle_in_err;
	}
	DIAG_LOG(DIAG_DEBUG_MUX, "opened read channel %d", pcie_info->in_chan);
	mutex_unlock(&pcie_info->in_chan_lock);
	mutex_unlock(&pcie_info->out_chan_lock);
	atomic_set(&pcie_info->enabled, 1);
	atomic_set(&pcie_info->diag_state, 1);
	diag_pcie_connect(pcie_info);
	return;
handle_in_err:
	mhi_dev_close_channel(pcie_info->out_handle);
	atomic_set(&pcie_info->enabled, 0);
handle_not_rdy_err:
	mutex_unlock(&pcie_info->in_chan_lock);
	mutex_unlock(&pcie_info->out_chan_lock);
}

/*
 * This function performs pcie connect operations wrt Diag synchronously. It
 * doesn't translate to actual pcie connect. This is used when Diag switches
 * logging to pcie mode and wants to mimic pcie connection.
 */
void diag_pcie_connect_all(void)
{
	int i = 0;
	struct diag_pcie_info *pcie_info = NULL;

	for (i = 0; i < NUM_DIAG_PCIE_DEV; i++) {
		pcie_info = &diag_pcie[i];
		if (!atomic_read(&pcie_info->enabled))
			continue;
		atomic_set(&pcie_info->diag_state, 1);
		diag_pcie_connect(pcie_info);
	}
}

static void diag_pcie_disconnect(struct diag_pcie_info *ch)
{
	if (!ch)
		return;

	if (!atomic_read(&ch->enabled) &&
		driver->pcie_connected && diag_mask_param())
		diag_clear_masks(0);

	if (ch && ch->ops && ch->ops->close)
		ch->ops->close(ch->ctxt, DIAG_PCIE_MODE);
}

/*
 * This function performs pcie disconnect operations wrt Diag synchronously.
 * It doesn't translate to actual pcie disconnect. This is used when Diag
 * switches logging from pcie mode and want to mimic pcie disconnect.
 */
void diag_pcie_disconnect_all(void)
{
	int i = 0;
	struct diag_pcie_info *pcie_info = NULL;

	for (i = 0; i < NUM_DIAG_PCIE_DEV; i++) {
		pcie_info = &diag_pcie[i];
		if (!atomic_read(&pcie_info->enabled))
			continue;
		atomic_set(&pcie_info->diag_state, 0);
		diag_pcie_disconnect(pcie_info);
	}
}

void diag_pcie_close_work_fn(struct work_struct *work)
{
	int rc = 0;
	struct diag_pcie_info *pcie_info = container_of(work,
						      struct diag_pcie_info,
						      open_work);

	if (!pcie_info || !atomic_read(&pcie_info->enabled))
		return;
	mutex_lock(&pcie_info->out_chan_lock);
	mutex_lock(&pcie_info->in_chan_lock);
	rc = mhi_dev_close_channel(pcie_info->in_handle);
	DIAG_LOG(DIAG_DEBUG_MUX, " closed in bound channel %d",
		pcie_info->in_chan);
	rc = mhi_dev_close_channel(pcie_info->out_handle);
	DIAG_LOG(DIAG_DEBUG_MUX, " closed out bound channel %d",
		pcie_info->out_chan);
	mutex_unlock(&pcie_info->in_chan_lock);
	mutex_unlock(&pcie_info->out_chan_lock);
	atomic_set(&pcie_info->enabled, 0);
	diag_pcie_disconnect(pcie_info);
}

int diag_pcie_register(int id, int ctxt, struct diag_mux_ops *ops)
{
	struct diag_pcie_info *ch = NULL;
	int rc = 0;
	unsigned char wq_name[DIAG_PCIE_NAME_SZ + DIAG_PCIE_STRING_SZ];

	if (id < 0 || id >= NUM_DIAG_PCIE_DEV) {
		pr_err("diag: Unable to register with PCIE, id: %d\n", id);
		return -EIO;
	}

	if (!ops) {
		pr_err("diag: Invalid operations for PCIE\n");
		return -EIO;
	}

	ch = &diag_pcie[id];
	ch->ops = ops;
	ch->ctxt = ctxt;
	atomic_set(&ch->diag_state, 0);
	atomic_set(&ch->enabled, 0);
	INIT_LIST_HEAD(&ch->buf_tbl);
	spin_lock_init(&ch->write_lock);
	INIT_WORK(&(ch->read_work), diag_pcie_read_work_fn);
	INIT_WORK(&(ch->open_work), diag_pcie_open_work_fn);
	INIT_WORK(&(ch->close_work), diag_pcie_close_work_fn);
	strlcpy(wq_name, "DIAG_PCIE_", sizeof(wq_name));
	strlcat(wq_name, ch->name, sizeof(wq_name));
	ch->wq = create_singlethread_workqueue(wq_name);
	if (!ch->wq)
		return -ENOMEM;
	diagmem_init(driver, ch->mempool);
	mutex_init(&ch->in_chan_lock);
	mutex_init(&ch->out_chan_lock);
	rc = diag_register_pcie_channels(ch);
	if (rc < 0) {
		if (ch->wq)
			destroy_workqueue(ch->wq);
		kfree(ch->in_chan_attr.read_buffer);
		return rc;
	}
	return 0;
}
