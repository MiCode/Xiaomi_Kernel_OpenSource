/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include "diag_memorydevice.h"
#include "diagfwd_bridge.h"
#include "diag_mux.h"
#include "diagmem.h"

struct diag_md_info diag_md[NUM_DIAG_MD_DEV] = {
	{
		.id = DIAG_MD_LOCAL,
		.ctx = 0,
		.mempool = POOL_TYPE_MUX_APPS,
		.num_tbl_entries = 0,
		.tbl = NULL,
		.ops = NULL,
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = DIAG_MD_MDM,
		.ctx = 0,
		.mempool = POOL_TYPE_MDM_MUX,
		.num_tbl_entries = 0,
		.tbl = NULL,
		.ops = NULL,
	},
	{
		.id = DIAG_MD_MDM2,
		.ctx = 0,
		.mempool = POOL_TYPE_MDM2_MUX,
		.num_tbl_entries = 0,
		.tbl = NULL,
		.ops = NULL,
	},
	{
		.id = DIAG_MD_SMUX,
		.ctx = 0,
		.mempool = POOL_TYPE_QSC_MUX,
		.num_tbl_entries = 0,
		.tbl = NULL,
		.ops = NULL,
	}
#endif
};

int diag_md_register(int id, int ctx, struct diag_mux_ops *ops)
{
	if (id < 0 || id >= NUM_DIAG_MD_DEV || !ops)
		return -EINVAL;

	diag_md[id].ops = ops;
	diag_md[id].ctx = ctx;
	return 0;
}

void diag_md_open_all()
{
	int i;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		if (ch->ops && ch->ops->open)
			ch->ops->open(ch->ctx, DIAG_MEMORY_DEVICE_MODE);
	}

	return;
}

void diag_md_close_all()
{
	int i, j;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		/*
		 * When we close the Memory device mode, make sure we flush the
		 * internal buffers in the table so that there are no stale
		 * entries.
		 */
		for (j = 0; j < ch->num_tbl_entries; j++) {
			entry = &ch->tbl[j];
			if (entry->len <= 0)
				continue;
			spin_lock_irqsave(&ch->lock, flags);
			if (ch->ops && ch->ops->write_done)
				ch->ops->write_done(entry->buf, entry->len,
						    entry->ctx, ch->ctx);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
			spin_unlock_irqrestore(&ch->lock, flags);
		}
		if (ch->ops && ch->ops->close)
			ch->ops->close(ch->ctx, DIAG_MEMORY_DEVICE_MODE);
	}

	diag_ws_reset(DIAG_WS_MD);
}

int diag_md_write(int id, unsigned char *buf, int len, int ctx)
{
	int i;
	uint8_t found = 0;
	unsigned long flags;
	struct diag_md_info *ch = NULL;

	if (id < 0 || id >= NUM_DIAG_MD_DEV)
		return -EINVAL;

	if (!buf || len < 0)
		return -EINVAL;

	ch = &diag_md[id];
	spin_lock_irqsave(&ch->lock, flags);
	for (i = 0; i < ch->num_tbl_entries && !found; i++) {
		if (ch->tbl[i].buf != buf)
			continue;
		found = 1;
		pr_err_ratelimited("diag: trying to write the same buffer buf: %p, ctxt: %d len: %d at i: %d back to the table, proc: %d, mode: %d\n",
				   buf, ctx, ch->tbl[i].len,
				   i, id, driver->logging_mode);
	}
	spin_unlock_irqrestore(&ch->lock, flags);

	if (found)
		return -ENOMEM;

	spin_lock_irqsave(&ch->lock, flags);
	for (i = 0; i < ch->num_tbl_entries && !found; i++) {
		if (ch->tbl[i].len == 0) {
			ch->tbl[i].buf = buf;
			ch->tbl[i].len = len;
			ch->tbl[i].ctx = ctx;
			found = 1;
			diag_ws_on_read(DIAG_WS_MD, len);
		}
	}
	spin_unlock_irqrestore(&ch->lock, flags);

	if (!found) {
		pr_err_ratelimited("diag: Unable to find an empty space in table, please reduce logging rate, proc: %d\n",
				   id);
		return -ENOMEM;
	}

	found = 0;
	for (i = 0; i < driver->num_clients && !found; i++) {
		if (driver->client_map[i].pid != driver->logging_process_id)
			continue;
		found = 1;
		driver->data_ready[i] |= USER_SPACE_DATA_TYPE;
		pr_debug("diag: wake up logging process\n");
		wake_up_interruptible(&driver->wait_q);
	}

	if (!found)
		return -EINVAL;

	return 0;
}

int diag_md_copy_to_user(char __user *buf, int *pret)
{

	int i, j;
	int err = 0;
	int ret = *pret;
	int num_data = 0;
	int remote_token;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV && !err; i++) {
		ch = &diag_md[i];
		for (j = 0; j < ch->num_tbl_entries && !err; j++) {
			entry = &ch->tbl[j];
			if (entry->len <= 0)
				continue;
			/*
			 * If the data is from remote processor, copy the remote
			 * token first
			 */
			if (i > 0) {
				remote_token = diag_get_remote(i);
				err = copy_to_user(buf + ret, &remote_token,
						   sizeof(int));
				if (err)
					goto drop_data;
				ret += sizeof(int);
			}

			/* Copy the length of data being passed */
			err = copy_to_user(buf + ret, (void *)&(entry->len),
					   sizeof(int));
			if (err)
				goto drop_data;
			ret += sizeof(int);

			/* Copy the actual data being passed */
			err = copy_to_user(buf + ret, (void *)entry->buf,
					   entry->len);
			if (err)
				goto drop_data;
			ret += entry->len;

			/*
			 * The data is now copied to the user space client,
			 * Notify that the write is complete and delete its
			 * entry from the table
			 */
			num_data++;
drop_data:
			spin_lock_irqsave(&ch->lock, flags);
			if (ch->ops && ch->ops->write_done)
				ch->ops->write_done(entry->buf, entry->len,
						    entry->ctx, ch->ctx);
			diag_ws_on_copy(DIAG_WS_MD);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
			spin_unlock_irqrestore(&ch->lock, flags);
		}
	}

	*pret = ret;
	err = copy_to_user(buf + sizeof(int), (void *)&num_data, sizeof(int));
	diag_ws_on_copy_complete(DIAG_WS_MD);
	return err;
}

int diag_md_init()
{
	int i, j;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		ch->num_tbl_entries = diag_mempools[ch->mempool].poolsize;
		ch->tbl = kzalloc(ch->num_tbl_entries *
				  sizeof(struct diag_buf_tbl_t),
				  GFP_KERNEL);
		if (!ch->tbl)
			goto fail;

		for (j = 0; j < ch->num_tbl_entries; j++) {
			ch->tbl[j].buf = NULL;
			ch->tbl[j].len = 0;
			ch->tbl[j].ctx = 0;
			spin_lock_init(&(ch->lock));
		}
	}

	return 0;

fail:
	diag_md_exit();
	return -ENOMEM;
}

void diag_md_exit()
{
	int i;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		kfree(ch->tbl);
		ch->num_tbl_entries = 0;
		ch->ops = NULL;
	}
}
