/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/diagchar.h>
#include <linux/kmemleak.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/ratelimit.h>
#include <linux/platform_device.h>
#include <linux/smux.h>
#include "diag_mux.h"
#include "diagfwd_bridge.h"
#include "diagfwd_hsic.h"
#include "diagfwd_smux.h"
#include "diagfwd_mhi.h"
#include "diag_dci.h"

#ifdef CONFIG_MSM_MHI
#define diag_mdm_init		diag_mhi_init
#else
#define diag_mdm_init		diag_hsic_init
#endif

#define BRIDGE_TO_MUX(x)	(x + DIAG_MUX_BRIDGE_BASE)

struct diagfwd_bridge_info bridge_info[NUM_REMOTE_DEV] = {
	{
		.id = DIAGFWD_MDM,
		.type = DIAG_DATA_TYPE,
		.name = "MDM",
		.inited = 0,
		.ctxt = 0,
		.dev_ops = NULL,
		.dci_read_ptr = NULL,
		.dci_read_buf = NULL,
		.dci_read_len = 0,
		.dci_wq = NULL,
	},
	{
		.id = DIAGFWD_SMUX,
		.type = DIAG_DATA_TYPE,
		.name = "SMUX",
		.inited = 0,
		.ctxt = 0,
		.dci_read_ptr = NULL,
		.dev_ops = NULL,
		.dci_read_buf = NULL,
		.dci_read_len = 0,
		.dci_wq = NULL,
	},
	{
		.id = DIAGFWD_MDM_DCI,
		.type = DIAG_DCI_TYPE,
		.name = "MDM_DCI",
		.inited = 0,
		.ctxt = 0,
		.dci_read_ptr = NULL,
		.dev_ops = NULL,
		.dci_read_buf = NULL,
		.dci_read_len = 0,
		.dci_wq = NULL,
	},
};

static int diagfwd_bridge_mux_connect(int id, int mode)
{
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	bridge_info[id].dev_ops->open(bridge_info[id].ctxt);
	return 0;
}

static int diagfwd_bridge_mux_disconnect(int id, int mode)
{
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	bridge_info[id].dev_ops->close(bridge_info[id].ctxt);
	return 0;
}

static int diagfwd_bridge_mux_read_done(unsigned char *buf, int len, int id)
{
	return diagfwd_bridge_write(id, buf, len);
}

static int diagfwd_bridge_mux_write_done(unsigned char *buf, int len,
					 int buf_ctx, int id)
{
	struct diagfwd_bridge_info *ch = NULL;

	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	ch = &bridge_info[id];
	ch->dev_ops->fwd_complete(ch->ctxt, buf, len, 0);
	return 0;
}

static struct diag_mux_ops diagfwd_bridge_mux_ops = {
	.open = diagfwd_bridge_mux_connect,
	.close = diagfwd_bridge_mux_disconnect,
	.read_done = diagfwd_bridge_mux_read_done,
	.write_done = diagfwd_bridge_mux_write_done
};

static void bridge_dci_read_work_fn(struct work_struct *work)
{
	struct diagfwd_bridge_info *ch = container_of(work,
					struct diagfwd_bridge_info,
					dci_read_work);
	if (!ch)
		return;
	diag_process_remote_dci_read_data(ch->id, ch->dci_read_buf,
					  ch->dci_read_len);
	ch->dev_ops->fwd_complete(ch->ctxt, ch->dci_read_ptr,
				  ch->dci_read_len, 0);
}

int diagfwd_bridge_register(int id, int ctxt, struct diag_remote_dev_ops *ops)
{
	int err = 0;
	struct diagfwd_bridge_info *ch = NULL;
	char wq_name[DIAG_BRIDGE_NAME_SZ + 10];

	if (!ops) {
		pr_err("diag: Invalid pointers ops: %p ctxt: %d\n", ops, ctxt);
		return -EINVAL;
	}

	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;

	ch = &bridge_info[id];
	ch->ctxt = ctxt;
	ch->dev_ops = ops;
	switch (ch->type) {
	case DIAG_DATA_TYPE:
		err = diag_mux_register(BRIDGE_TO_MUX(id), id,
					&diagfwd_bridge_mux_ops);
		if (err)
			return err;
		break;
	case DIAG_DCI_TYPE:
		ch->dci_read_buf = kzalloc(DIAG_MDM_BUF_SIZE, GFP_KERNEL);
		if (!ch->dci_read_buf)
			return -ENOMEM;
		ch->dci_read_len = 0;
		strlcpy(wq_name, "diag_dci_", 10);
		strlcat(wq_name, ch->name, sizeof(ch->name));
		INIT_WORK(&(ch->dci_read_work), bridge_dci_read_work_fn);
		ch->dci_wq = create_singlethread_workqueue(wq_name);
		if (!ch->dci_wq) {
			kfree(ch->dci_read_buf);
			return -ENOMEM;
		}
		break;
	default:
		pr_err("diag: Invalid channel type %d in %s\n", ch->type,
		       __func__);
		return -EINVAL;
	}
	return 0;
}

int diag_remote_dev_open(int id)
{
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	bridge_info[id].inited = 1;
	if (bridge_info[id].type == DIAG_DATA_TYPE)
		return diag_mux_queue_read(BRIDGE_TO_MUX(id));
	else if (bridge_info[id].type == DIAG_DCI_TYPE)
		return diag_dci_send_handshake_pkt(bridge_info[id].id);

	return 0;
}

void diag_remote_dev_close(int id)
{
	return;
}

int diag_remote_dev_read_done(int id, unsigned char *buf, int len)
{
	int err = 0;
	struct diagfwd_bridge_info *ch = NULL;

	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	ch = &bridge_info[id];
	if (ch->type == DIAG_DATA_TYPE) {
		err = diag_mux_write(BRIDGE_TO_MUX(id), buf, len, id);
		ch->dev_ops->queue_read(ch->ctxt);
		return err;
	}
	/*
	 * For DCI channels copy to the internal buffer. Don't queue any
	 * further reads. A read should be queued once we are done processing
	 * the current packet
	 */
	if (len <= 0 || len > DIAG_MDM_BUF_SIZE) {
		pr_err_ratelimited("diag: Invalid len %d in %s, ch: %s\n",
				   len, __func__, ch->name);
		return -EINVAL;
	}
	ch->dci_read_ptr = buf;
	memcpy(ch->dci_read_buf, buf, len);
	ch->dci_read_len = len;
	queue_work(ch->dci_wq, &ch->dci_read_work);
	return 0;
}

int diag_remote_dev_write_done(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;

	if (bridge_info[id].type == DIAG_DATA_TYPE) {
		if (buf == driver->cb_buf)
			driver->cb_buf_len = 0;
		if (buf == driver->user_space_data_buf)
			driver->user_space_data_busy = 0;
		err = diag_mux_queue_read(BRIDGE_TO_MUX(id));
	} else {
		err = diag_dci_write_done_bridge(id, buf, len);
	}
	return err;
}

int diagfwd_bridge_init()
{
	int err = 0;

	err = diag_mdm_init();
	if (err)
		goto fail;
	err = diag_smux_init();
	if (err)
		goto fail;
	return 0;

fail:
	pr_err("diag: Unable to initialze diagfwd bridge, err: %d\n", err);
	return err;
}

void diagfwd_bridge_exit()
{
	diag_hsic_exit();
	diag_smux_exit();
}

int diagfwd_bridge_close(int id)
{
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	return bridge_info[id].dev_ops->close(bridge_info[id].ctxt);
}

int diagfwd_bridge_write(int id, unsigned char *buf, int len)
{
	if (id < 0 || id >= NUM_REMOTE_DEV)
		return -EINVAL;
	return bridge_info[id].dev_ops->write(bridge_info[id].ctxt,
					      buf, len, 0);
}

uint16_t diag_get_remote_device_mask()
{
	int i;
	uint16_t remote_dev = 0;

	for (i = 0; i < NUM_REMOTE_DEV; i++) {
		if (bridge_info[i].inited &&
		    bridge_info[i].type == DIAG_DATA_TYPE) {
			remote_dev |= 1 << i;
		}
	}

	return remote_dev;
}

