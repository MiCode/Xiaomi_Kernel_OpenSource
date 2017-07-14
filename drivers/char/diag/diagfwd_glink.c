/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/diagchar.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#include <soc/qcom/glink.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_glink.h"
#include "diag_ipc_logging.h"

struct diag_glink_info glink_data[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DATA,
		.edge = "mpss",
		.name = "DIAG_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DATA,
		.edge = "lpass",
		.name = "DIAG_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DATA,
		.edge = "wcnss",
		.name = "DIAG_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DATA,
		.edge = "dsps",
		.name = "DIAG_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DATA,
		.edge = "wdsp",
		.name = "DIAG_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DATA,
		.edge = "cdsp",
		.name = "DIAG_DATA",
		.hdl = NULL
	}
};

struct diag_glink_info glink_cntl[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CNTL,
		.edge = "mpss",
		.name = "DIAG_CTRL",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CNTL,
		.edge = "lpass",
		.name = "DIAG_CTRL",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CNTL,
		.edge = "wcnss",
		.name = "DIAG_CTRL",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CNTL,
		.edge = "dsps",
		.name = "DIAG_CTRL",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CNTL,
		.edge = "wdsp",
		.name = "DIAG_CTRL",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CNTL,
		.edge = "cdsp",
		.name = "DIAG_CTRL",
		.hdl = NULL
	}
};

struct diag_glink_info glink_dci[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI,
		.edge = "mpss",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI,
		.edge = "lpass",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI,
		.edge = "wcnss",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI,
		.edge = "dsps",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI,
		.edge = "wdsp",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI,
		.edge = "cdsp",
		.name = "DIAG_DCI_DATA",
		.hdl = NULL
	}
};

struct diag_glink_info glink_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CMD,
		.edge = "mpss",
		.name = "DIAG_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CMD,
		.edge = "lpass",
		.name = "DIAG_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CMD,
		.edge = "wcnss",
		.name = "DIAG_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CMD,
		.edge = "dsps",
		.name = "DIAG_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CMD,
		.edge = "wdsp",
		.name = "DIAG_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CMD,
		.edge = "cdsp",
		.name = "DIAG_CMD",
		.hdl = NULL
	}
};

struct diag_glink_info glink_dci_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI_CMD,
		.edge = "mpss",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI_CMD,
		.edge = "lpass",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI_CMD,
		.edge = "wcnss",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI_CMD,
		.edge = "dsps",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI_CMD,
		.edge = "wdsp",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI_CMD,
		.edge = "cdsp",
		.name = "DIAG_DCI_CMD",
		.hdl = NULL
	}
};

static void diag_state_open_glink(void *ctxt);
static void diag_state_close_glink(void *ctxt);
static int diag_glink_write(void *ctxt, unsigned char *buf, int len);
static int diag_glink_read(void *ctxt, unsigned char *buf, int buf_len);
static void diag_glink_queue_read(void *ctxt);

static struct diag_peripheral_ops glink_ops = {
	.open = diag_state_open_glink,
	.close = diag_state_close_glink,
	.write = diag_glink_write,
	.read = diag_glink_read,
	.queue_read = diag_glink_queue_read
};

static void diag_state_open_glink(void *ctxt)
{
	struct diag_glink_info *glink_info = NULL;

	if (!ctxt)
		return;

	glink_info = (struct diag_glink_info *)(ctxt);
	atomic_set(&glink_info->diag_state, 1);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s setting diag state to 1", glink_info->name);
}

static void diag_glink_queue_read(void *ctxt)
{
	struct diag_glink_info *glink_info = NULL;

	if (!ctxt)
		return;

	glink_info = (struct diag_glink_info *)ctxt;
	if (glink_info->hdl && glink_info->wq &&
		atomic_read(&glink_info->opened))
		queue_work(glink_info->wq, &(glink_info->read_work));
}

static void diag_state_close_glink(void *ctxt)
{
	struct diag_glink_info *glink_info = NULL;

	if (!ctxt)
		return;

	glink_info = (struct diag_glink_info *)(ctxt);
	atomic_set(&glink_info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s setting diag state to 0", glink_info->name);
	wake_up_interruptible(&glink_info->read_wait_q);
	flush_workqueue(glink_info->wq);
}

int diag_glink_check_state(void *ctxt)
{
	struct diag_glink_info *info = NULL;

	if (!ctxt)
		return 0;

	info = (struct diag_glink_info *)ctxt;
	return (int)(atomic_read(&info->diag_state));
}

static int diag_glink_read(void *ctxt, unsigned char *buf, int buf_len)
{
	struct diag_glink_info *glink_info =  NULL;
	int ret_val = 0;

	if (!ctxt || !buf || buf_len <= 0)
		return -EIO;

	glink_info = (struct diag_glink_info *)ctxt;
	if (!glink_info || !atomic_read(&glink_info->opened) ||
		!glink_info->hdl || !glink_info->inited) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag:Glink channel not opened");
		return -EIO;
	}

	ret_val = glink_queue_rx_intent(glink_info->hdl, buf, buf_len);
	if (ret_val == 0)
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: queued an rx intent ch:%s perip:%d buf:%pK of len:%d\n",
		glink_info->name, glink_info->peripheral, buf, buf_len);

	return ret_val;
}

static void diag_glink_read_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							read_work);

	if (!glink_info || !atomic_read(&glink_info->opened))
		return;

	if (!glink_info->inited) {
		diag_ws_release();
		return;
	}

	diagfwd_channel_read(glink_info->fwd_ctxt);
}
struct diag_glink_read_work {
	struct diag_glink_info *glink_info;
	const void *ptr_read_done;
	const void *ptr_rx_done;
	size_t ptr_read_size;
	struct work_struct work;
};

static void diag_glink_notify_rx_work_fn(struct work_struct *work)
{
	struct diag_glink_read_work *read_work = container_of(work,
			struct diag_glink_read_work, work);
	struct diag_glink_info *glink_info = read_work->glink_info;

	if (!glink_info || !glink_info->hdl)
		return;

	diagfwd_channel_read_done(glink_info->fwd_ctxt,
			(unsigned char *)(read_work->ptr_read_done),
			read_work->ptr_read_size);

	glink_rx_done(glink_info->hdl, read_work->ptr_rx_done, false);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: Rx done for packet %pK of len: %d periph: %d ch: %d\n",
		read_work->ptr_rx_done, (int)read_work->ptr_read_size,
		glink_info->peripheral, glink_info->type);
}

static void diag_glink_notify_rx(void *hdl, const void *priv,
				const void *pkt_priv, const void *ptr,
				size_t size)
{
	struct diag_glink_info *glink_info = (struct diag_glink_info *)priv;
	struct diag_glink_read_work *read_work;

	if (!glink_info || !glink_info->hdl || !ptr || !pkt_priv || !hdl)
		return;

	if (size <= 0)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: received a packet %pK of len:%d from periph:%d ch:%d\n",
		ptr, (int)size, glink_info->peripheral, glink_info->type);

	read_work = kmalloc(sizeof(*read_work), GFP_ATOMIC);
	if (!read_work) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: Could not allocate read_work\n");
		return;
	}

	memcpy((void *)pkt_priv, ptr, size);

	read_work->glink_info = glink_info;
	read_work->ptr_read_done = pkt_priv;
	read_work->ptr_rx_done = ptr;
	read_work->ptr_read_size = size;
	INIT_WORK(&read_work->work, diag_glink_notify_rx_work_fn);
	queue_work(glink_info->wq, &read_work->work);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: Rx queued for packet %pK of len: %d periph: %d ch: %d\n",
		ptr, (int)size, glink_info->peripheral, glink_info->type);
}

static void diag_glink_notify_remote_rx_intent(void *hdl, const void *priv,
						size_t size)
{
	struct diag_glink_info *glink_info = (struct diag_glink_info *)priv;

	if (!glink_info)
		return;

	atomic_inc(&glink_info->tx_intent_ready);
	wake_up_interruptible(&glink_info->wait_q);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag:received remote rx intent for %d type %d\n",
		glink_info->peripheral, glink_info->type);
}

static void diag_glink_notify_tx_done(void *hdl, const void *priv,
					const void *pkt_priv,
					const void *ptr)
{
	struct diag_glink_info *glink_info = NULL;
	struct diagfwd_info *fwd_info = NULL;
	int found = 0;

	glink_info = (struct diag_glink_info *)priv;
	if (!glink_info)
		return;

	fwd_info = glink_info->fwd_ctxt;
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: Received glink tx done notify for ptr%pK pkt_priv %pK\n",
		ptr, pkt_priv);
	found = diagfwd_write_buffer_done(fwd_info, ptr);
	if (!found)
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Received Tx done on invalid buffer ptr %pK\n", ptr);
}

static int  diag_glink_write(void *ctxt, unsigned char *buf, int len)
{
	struct diag_glink_info *glink_info = NULL;
	int err = 0;
	uint32_t tx_flags = GLINK_TX_REQ_INTENT;

	if (!ctxt || !buf)
		return -EIO;

	glink_info = (struct diag_glink_info *)ctxt;
	if (!glink_info || len <= 0) {
		pr_err_ratelimited("diag: In %s, invalid params, glink_info: %pK, buf: %pK, len: %d\n",
				__func__, glink_info, buf, len);
		return -EINVAL;
	}

	if (!glink_info->inited || !glink_info->hdl ||
		!atomic_read(&glink_info->opened)) {
		pr_err_ratelimited("diag: In %s, glink not inited, glink_info: %pK, buf: %pK, len: %d\n",
				 __func__, glink_info, buf, len);
		return -ENODEV;
	}

	if (atomic_read(&glink_info->tx_intent_ready)) {
		atomic_dec(&glink_info->tx_intent_ready);
		err = glink_tx(glink_info->hdl, glink_info, buf, len, tx_flags);
		if (!err) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"%s wrote to glink, len: %d\n",
				glink_info->name, len);
		}
	} else
		err = -ENOMEM;

	return err;

}

static void diag_glink_connect_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							connect_work);
	if (!glink_info || !glink_info->hdl)
		return;
	atomic_set(&glink_info->opened, 1);
	diagfwd_channel_open(glink_info->fwd_ctxt);
	diagfwd_late_open(glink_info->fwd_ctxt);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "glink channel open: p: %d t: %d\n",
			glink_info->peripheral, glink_info->type);
}

static void diag_glink_remote_disconnect_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							remote_disconnect_work);
	if (!glink_info || !glink_info->hdl)
		return;
	atomic_set(&glink_info->opened, 0);
	diagfwd_channel_close(glink_info->fwd_ctxt);
	atomic_set(&glink_info->tx_intent_ready, 0);
}

static void diag_glink_late_init_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							late_init_work);
	if (!glink_info || !glink_info->hdl)
		return;
	diagfwd_channel_open(glink_info->fwd_ctxt);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "glink late init p: %d t: %d\n",
			glink_info->peripheral, glink_info->type);
}

static void diag_glink_transport_notify_state(void *handle, const void *priv,
					  unsigned event)
{
	struct diag_glink_info *glink_info = (struct diag_glink_info *)priv;

	if (!glink_info)
		return;

	switch (event) {
	case GLINK_CONNECTED:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s received channel connect for periph:%d\n",
			 glink_info->name, glink_info->peripheral);
		queue_work(glink_info->wq, &glink_info->connect_work);
		break;
	case GLINK_LOCAL_DISCONNECTED:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s received channel disconnect for periph:%d\n",
			glink_info->name, glink_info->peripheral);

		break;
	case GLINK_REMOTE_DISCONNECTED:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s received channel remote disconnect for periph:%d\n",
			 glink_info->name, glink_info->peripheral);
		queue_work(glink_info->wq, &glink_info->remote_disconnect_work);
		break;
	default:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s received invalid notification\n",
			glink_info->name);
		break;
	}

}
static void diag_glink_open_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							open_work);
	struct glink_open_config open_cfg;
	void *handle = NULL;

	if (!glink_info || glink_info->hdl)
		return;

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.priv = glink_info;
	open_cfg.edge = glink_info->edge;
	open_cfg.name = glink_info->name;
	open_cfg.notify_rx = diag_glink_notify_rx;
	open_cfg.notify_tx_done = diag_glink_notify_tx_done;
	open_cfg.notify_state = diag_glink_transport_notify_state;
	open_cfg.notify_remote_rx_intent = diag_glink_notify_remote_rx_intent;
	handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(handle)) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "error opening channel %s",
			glink_info->name);
	} else
		glink_info->hdl = handle;
}

static void diag_glink_close_work_fn(struct work_struct *work)
{
	struct diag_glink_info *glink_info = container_of(work,
							struct diag_glink_info,
							close_work);
	if (!glink_info || !glink_info->inited || !glink_info->hdl)
		return;

	glink_close(glink_info->hdl);
	atomic_set(&glink_info->opened, 0);
	atomic_set(&glink_info->tx_intent_ready, 0);
	glink_info->hdl = NULL;
	diagfwd_channel_close(glink_info->fwd_ctxt);
}

static void diag_glink_notify_cb(struct glink_link_state_cb_info *cb_info,
				void *priv)
{
	struct diag_glink_info *glink_info = NULL;

	glink_info = (struct diag_glink_info *)priv;
	if (!glink_info)
		return;
	if (!cb_info)
		return;

	switch (cb_info->link_state) {
	case GLINK_LINK_STATE_UP:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s channel opened for periph:%d\n",
			glink_info->name, glink_info->peripheral);
		queue_work(glink_info->wq, &glink_info->open_work);
		break;
	case GLINK_LINK_STATE_DOWN:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s channel closed for periph:%d\n",
			glink_info->name, glink_info->peripheral);
		queue_work(glink_info->wq, &glink_info->close_work);
		break;
	default:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Invalid link state notification for ch:%s\n",
			glink_info->name);
		break;

	}
}

static void glink_late_init(struct diag_glink_info *glink_info)
{
	struct diagfwd_info *fwd_info = NULL;

	if (!glink_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n",
		 glink_info->name);

	diagfwd_register(TRANSPORT_GLINK, glink_info->peripheral,
			glink_info->type, (void *)glink_info,
			&glink_ops, &glink_info->fwd_ctxt);
	fwd_info = glink_info->fwd_ctxt;
	if (!fwd_info)
		return;

	glink_info->inited = 1;

	if (atomic_read(&glink_info->opened))
		queue_work(glink_info->wq, &(glink_info->late_init_work));

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 glink_info->name);
}

int diag_glink_init_peripheral(uint8_t peripheral)
{
	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral %d\n",
		       __func__, peripheral);
		return -EINVAL;
	}

	glink_late_init(&glink_data[peripheral]);
	glink_late_init(&glink_dci[peripheral]);
	glink_late_init(&glink_cmd[peripheral]);
	glink_late_init(&glink_dci_cmd[peripheral]);

	return 0;
}

static void __diag_glink_init(struct diag_glink_info *glink_info)
{
	char wq_name[DIAG_GLINK_NAME_SZ + 12];
	struct glink_link_info link_info;
	void *link_state_handle = NULL;

	if (!glink_info)
		return;

	init_waitqueue_head(&glink_info->wait_q);
	init_waitqueue_head(&glink_info->read_wait_q);
	mutex_init(&glink_info->lock);
	strlcpy(wq_name, "DIAG_GLINK_", 12);
	strlcat(wq_name, glink_info->name, sizeof(glink_info->name));
	glink_info->wq = create_singlethread_workqueue(wq_name);
	if (!glink_info->wq) {
		pr_err("diag: In %s, unable to create workqueue for glink ch:%s\n",
			   __func__, glink_info->name);
		return;
	}
	INIT_WORK(&(glink_info->open_work), diag_glink_open_work_fn);
	INIT_WORK(&(glink_info->close_work), diag_glink_close_work_fn);
	INIT_WORK(&(glink_info->read_work), diag_glink_read_work_fn);
	INIT_WORK(&(glink_info->connect_work), diag_glink_connect_work_fn);
	INIT_WORK(&(glink_info->remote_disconnect_work),
		diag_glink_remote_disconnect_work_fn);
	INIT_WORK(&(glink_info->late_init_work), diag_glink_late_init_work_fn);
	link_info.glink_link_state_notif_cb = diag_glink_notify_cb;
	link_info.transport = NULL;
	link_info.edge = glink_info->edge;
	glink_info->link_state_handle = NULL;
	link_state_handle = glink_register_link_state_cb(&link_info,
							(void *)glink_info);
	if (IS_ERR_OR_NULL(link_state_handle)) {
		pr_err("diag: In %s, unable to register for glink channel %s\n",
			   __func__, glink_info->name);
		destroy_workqueue(glink_info->wq);
		return;
	}
	glink_info->link_state_handle = link_state_handle;
	glink_info->fwd_ctxt = NULL;
	atomic_set(&glink_info->tx_intent_ready, 0);
	atomic_set(&glink_info->opened, 0);
	atomic_set(&glink_info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"%s initialized fwd_ctxt: %pK hdl: %pK\n",
		glink_info->name, glink_info->fwd_ctxt,
		glink_info->link_state_handle);
}

void diag_glink_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt)
{
	struct diag_glink_info *info = NULL;

	if (!ctxt || !fwd_ctxt)
		return;

	info = (struct diag_glink_info *)ctxt;
	info->fwd_ctxt = fwd_ctxt;
}

int diag_glink_init(void)
{
	uint8_t peripheral;
	struct diag_glink_info *glink_info = NULL;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		if (peripheral != PERIPHERAL_WDSP)
			continue;
		glink_info = &glink_cntl[peripheral];
		__diag_glink_init(glink_info);
		diagfwd_cntl_register(TRANSPORT_GLINK, glink_info->peripheral,
					(void *)glink_info, &glink_ops,
					&(glink_info->fwd_ctxt));
		glink_info->inited = 1;
		__diag_glink_init(&glink_data[peripheral]);
		__diag_glink_init(&glink_cmd[peripheral]);
		__diag_glink_init(&glink_dci[peripheral]);
		__diag_glink_init(&glink_dci_cmd[peripheral]);
	}
	return 0;
}

static void __diag_glink_exit(struct diag_glink_info *glink_info)
{
	if (!glink_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n",
			 glink_info->name);

	diagfwd_deregister(glink_info->peripheral, glink_info->type,
					   (void *)glink_info);
	glink_info->fwd_ctxt = NULL;
	glink_info->hdl = NULL;
	if (glink_info->wq)
		destroy_workqueue(glink_info->wq);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
			 glink_info->name);
}

void diag_glink_early_exit(void)
{
	int peripheral = 0;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		if (peripheral != PERIPHERAL_WDSP)
			continue;
		__diag_glink_exit(&glink_cntl[peripheral]);
		glink_unregister_link_state_cb(&glink_cntl[peripheral].hdl);
	}
}

void diag_glink_exit(void)
{
	int peripheral = 0;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		if (peripheral != PERIPHERAL_WDSP)
			continue;
		__diag_glink_exit(&glink_data[peripheral]);
		__diag_glink_exit(&glink_cmd[peripheral]);
		__diag_glink_exit(&glink_dci[peripheral]);
		__diag_glink_exit(&glink_dci_cmd[peripheral]);
		glink_unregister_link_state_cb(&glink_data[peripheral].hdl);
		glink_unregister_link_state_cb(&glink_cmd[peripheral].hdl);
		glink_unregister_link_state_cb(&glink_dci[peripheral].hdl);
		glink_unregister_link_state_cb(&glink_dci_cmd[peripheral].hdl);
	}
}
