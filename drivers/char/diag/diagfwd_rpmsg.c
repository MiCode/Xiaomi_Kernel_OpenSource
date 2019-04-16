/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include <linux/rpmsg.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_rpmsg.h"
#include "diag_ipc_logging.h"

struct diag_rpmsg_read_work {
	struct diag_rpmsg_info *rpmsg_info;
	const void *ptr_read_done;
	const void *ptr_rx_done;
	size_t ptr_read_size;
	struct work_struct work;
};

struct diag_rpmsg_info rpmsg_data[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DATA,
		.edge = "mpss",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DATA,
		.edge = "lpass",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DATA,
		.edge = "wcnss",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DATA,
		.edge = "dsps",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DATA,
		.edge = "wdsp",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DATA,
		.edge = "cdsp",
		.name = "DIAG_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	}
};

struct diag_rpmsg_info rpmsg_cntl[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CNTL,
		.edge = "mpss",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CNTL,
		.edge = "lpass",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CNTL,
		.edge = "wcnss",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CNTL,
		.edge = "dsps",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CNTL,
		.edge = "wdsp",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CNTL,
		.edge = "cdsp",
		.name = "DIAG_CTRL",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	}
};

struct diag_rpmsg_info rpmsg_dci[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI,
		.edge = "mpss",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI,
		.edge = "lpass",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI,
		.edge = "wcnss",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI,
		.edge = "dsps",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI,
		.edge = "wdsp",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI,
		.edge = "cdsp",
		.name = "DIAG_DCI_DATA",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	}
};

struct diag_rpmsg_info rpmsg_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CMD,
		.edge = "mpss",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CMD,
		.edge = "lpass",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CMD,
		.edge = "wcnss",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CMD,
		.edge = "dsps",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CMD,
		.edge = "wdsp",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CMD,
		.edge = "cdsp",
		.name = "DIAG_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	}
};

struct diag_rpmsg_info rpmsg_dci_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI_CMD,
		.edge = "mpss",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI_CMD,
		.edge = "lpass",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI_CMD,
		.edge = "wcnss",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI_CMD,
		.edge = "dsps",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI_CMD,
		.edge = "wdsp",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI_CMD,
		.edge = "cdsp",
		.name = "DIAG_DCI_CMD",
		.buf1 = NULL,
		.buf2 = NULL,
		.hdl = NULL
	}
};

static void diag_state_open_rpmsg(void *ctxt);
static void diag_state_close_rpmsg(void *ctxt);
static int diag_rpmsg_write(void *ctxt, unsigned char *buf, int len);
static int diag_rpmsg_read(void *ctxt, unsigned char *buf, int buf_len);
static void diag_rpmsg_queue_read(void *ctxt);
static void diag_rpmsg_notify_rx_work_fn(struct work_struct *work);
static struct diag_peripheral_ops rpmsg_ops = {
	.open = diag_state_open_rpmsg,
	.close = diag_state_close_rpmsg,
	.write = diag_rpmsg_write,
	.read = diag_rpmsg_read,
	.queue_read = diag_rpmsg_queue_read
};

static void diag_state_open_rpmsg(void *ctxt)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;

	if (!ctxt)
		return;

	rpmsg_info = (struct diag_rpmsg_info *)(ctxt);
	atomic_set(&rpmsg_info->diag_state, 1);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s setting diag state to 1", rpmsg_info->name);
}

static void diag_rpmsg_queue_read(void *ctxt)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;

	if (!ctxt)
		return;

	rpmsg_info = (struct diag_rpmsg_info *)ctxt;
	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (rpmsg_info->hdl && rpmsg_info->wq &&
		atomic_read(&rpmsg_info->opened))
		queue_work(rpmsg_info->wq, &(rpmsg_info->read_work));
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
}

static void diag_state_close_rpmsg(void *ctxt)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;

	if (!ctxt)
		return;

	rpmsg_info = (struct diag_rpmsg_info *)(ctxt);
	atomic_set(&rpmsg_info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s setting diag state to 0", rpmsg_info->name);
	wake_up_interruptible(&rpmsg_info->read_wait_q);
	flush_workqueue(rpmsg_info->wq);
}

int diag_rpmsg_check_state(void *ctxt)
{
	struct diag_rpmsg_info *info = NULL;

	if (!ctxt)
		return 0;

	info = (struct diag_rpmsg_info *)ctxt;
	return (int)(atomic_read(&info->diag_state));
}

static int diag_rpmsg_read(void *ctxt, unsigned char *buf, int buf_len)
{
	struct diag_rpmsg_info *rpmsg_info =  NULL;
	struct diagfwd_info *fwd_info = NULL;
	int ret_val = 0;

	if (!ctxt || !buf || buf_len <= 0)
		return -EIO;

	rpmsg_info = (struct diag_rpmsg_info *)ctxt;
	if (!rpmsg_info || !rpmsg_info->fwd_ctxt) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "diag:Invalid rpmsg context");
		return -EIO;
	}

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (!atomic_read(&rpmsg_info->opened) ||
		!rpmsg_info->hdl || !rpmsg_info->inited) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag:RPMSG channel not opened");
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return -EIO;
	}
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	fwd_info = rpmsg_info->fwd_ctxt;

	mutex_lock(&driver->diagfwd_channel_mutex[rpmsg_info->peripheral]);

	if (!rpmsg_info->buf1 && !fwd_info->buffer_status[BUF_1_INDEX] &&
		atomic_read(&fwd_info->buf_1->in_busy)) {
		rpmsg_info->buf1 = buf;
	} else if (!rpmsg_info->buf2 && (fwd_info->type == TYPE_DATA) &&
		!fwd_info->buffer_status[BUF_2_INDEX] &&
		atomic_read(&fwd_info->buf_2->in_busy)) {
		rpmsg_info->buf2 = buf;
	}
	mutex_unlock(&driver->diagfwd_channel_mutex[rpmsg_info->peripheral]);

	return ret_val;
}

static void diag_rpmsg_read_work_fn(struct work_struct *work)
{
	struct diag_rpmsg_info *rpmsg_info = container_of(work,
							struct diag_rpmsg_info,
							read_work);

	if (!rpmsg_info)
		return;

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	if (!atomic_read(&rpmsg_info->opened)) {
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return;
	}
	if (!rpmsg_info->inited) {
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		diag_ws_release();
		return;
	}
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	diagfwd_channel_read(rpmsg_info->fwd_ctxt);
}

static int  diag_rpmsg_write(void *ctxt, unsigned char *buf, int len)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;
	int err = 0;
	struct rpmsg_device *rpdev = NULL;

	if (!ctxt || !buf)
		return -EIO;

	rpmsg_info = (struct diag_rpmsg_info *)ctxt;
	if (!rpmsg_info || len <= 0) {
		pr_err_ratelimited("diag: In %s, invalid params, rpmsg_info: %pK, buf: %pK, len: %d\n",
				__func__, rpmsg_info, buf, len);
		return -EINVAL;
	}

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (!rpmsg_info->inited || !rpmsg_info->hdl ||
		!atomic_read(&rpmsg_info->opened)) {
		pr_err_ratelimited("diag: In %s, rpmsg not inited, rpmsg_info: %pK, buf: %pK, len: %d\n",
				 __func__, rpmsg_info, buf, len);
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return -ENODEV;
	}
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	rpdev = (struct rpmsg_device *)rpmsg_info->hdl;
	err = rpmsg_send(rpdev->ept, buf, len);
	if (!err) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s wrote to rpmsg, len: %d\n",
			 rpmsg_info->name, len);
	} else
		err = -ENOMEM;

	return err;

}

static void diag_rpmsg_late_init_work_fn(struct work_struct *work)
{
	struct diag_rpmsg_info *rpmsg_info = container_of(work,
							struct diag_rpmsg_info,
							late_init_work);
	if (!rpmsg_info)
		return;

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (!rpmsg_info->hdl) {
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return;
	}
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	diagfwd_channel_open(rpmsg_info->fwd_ctxt);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "rpmsg late init p: %d t: %d\n",
			rpmsg_info->peripheral, rpmsg_info->type);
}


static void diag_rpmsg_open_work_fn(struct work_struct *work)
{
	struct diag_rpmsg_info *rpmsg_info = container_of(work,
							struct diag_rpmsg_info,
							open_work);
	if (!rpmsg_info)
		return;

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (!rpmsg_info->inited) {
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return;
	}
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

	if (rpmsg_info->type != TYPE_CNTL) {
		diagfwd_channel_open(rpmsg_info->fwd_ctxt);
		diagfwd_late_open(rpmsg_info->fwd_ctxt);
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 rpmsg_info->name);
}

static void diag_rpmsg_close_work_fn(struct work_struct *work)
{
	struct diag_rpmsg_info *rpmsg_info = container_of(work,
							struct diag_rpmsg_info,
							close_work);
	if (!rpmsg_info)
		return;

	mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	if (!rpmsg_info->inited || !rpmsg_info->hdl) {
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		return;
	}
	rpmsg_info->hdl = NULL;
	mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
	diagfwd_channel_close(rpmsg_info->fwd_ctxt);
}

static int diag_rpmsg_notify_cb(struct rpmsg_device *rpdev, void *data, int len,
						void *priv, u32 src)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;
	struct diagfwd_info *fwd_info = NULL;
	struct diag_rpmsg_read_work *read_work = NULL;
	void *buf = NULL;

	rpmsg_info = dev_get_drvdata(&rpdev->dev);
	if (!rpmsg_info || !rpmsg_info->fwd_ctxt) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "diag: Invalid rpmsg info\n");
		return 0;
	}

	if (!rpmsg_info->buf1 && !rpmsg_info->buf2) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"dropping data for %s len %d\n",
			rpmsg_info->name, len);
		return 0;
	}

	fwd_info = rpmsg_info->fwd_ctxt;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: received data of length: %d for p:%d, t:%d\n",
		len, rpmsg_info->peripheral, rpmsg_info->type);

	if (rpmsg_info->buf1 && !fwd_info->buffer_status[BUF_1_INDEX] &&
			atomic_read(&fwd_info->buf_1->in_busy)) {
		buf = rpmsg_info->buf1;
		fwd_info->buffer_status[BUF_1_INDEX] = 1;
	} else if (rpmsg_info->buf2 && !fwd_info->buffer_status[BUF_2_INDEX] &&
			atomic_read(&fwd_info->buf_2->in_busy) &&
			(fwd_info->type == TYPE_DATA)) {
		buf = rpmsg_info->buf2;
		fwd_info->buffer_status[BUF_2_INDEX] = 1;
	} else {
		buf = NULL;
	}

	if (!buf)
		return 0;

	memcpy(buf, data, len);

	read_work = kmalloc(sizeof(*read_work), GFP_ATOMIC);
	if (!read_work) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: Could not allocate read_work\n");
		return 0;
	}
	read_work->rpmsg_info = rpmsg_info;
	read_work->ptr_read_done = buf;
	read_work->ptr_read_size = len;
	INIT_WORK(&read_work->work, diag_rpmsg_notify_rx_work_fn);
	queue_work(rpmsg_info->wq, &read_work->work);
	return 0;
}

static void diag_rpmsg_notify_rx_work_fn(struct work_struct *work)
{
	struct diag_rpmsg_read_work *read_work = container_of(work,
				struct diag_rpmsg_read_work, work);
	struct diag_rpmsg_info *rpmsg_info = read_work->rpmsg_info;

	if (!rpmsg_info || !rpmsg_info->hdl) {
		kfree(read_work);
		read_work = NULL;
		return;
	}

	mutex_lock(&driver->diagfwd_channel_mutex[rpmsg_info->peripheral]);
	diagfwd_channel_read_done(rpmsg_info->fwd_ctxt,
			(unsigned char *)(read_work->ptr_read_done),
			read_work->ptr_read_size);

	if (read_work->ptr_read_done == rpmsg_info->buf1)
		rpmsg_info->buf1 = NULL;
	else if (read_work->ptr_read_done == rpmsg_info->buf2)
		rpmsg_info->buf2 = NULL;
	kfree(read_work);
	read_work = NULL;
	mutex_unlock(&driver->diagfwd_channel_mutex[rpmsg_info->peripheral]);
}

static void rpmsg_late_init(struct diag_rpmsg_info *rpmsg_info)
{
	struct diagfwd_info *fwd_info = NULL;

	if (!rpmsg_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n", rpmsg_info->name);
	diagfwd_register(TRANSPORT_RPMSG, rpmsg_info->peripheral,
			rpmsg_info->type, (void *)rpmsg_info,
			&rpmsg_ops, &rpmsg_info->fwd_ctxt);
	fwd_info = rpmsg_info->fwd_ctxt;
	if (!fwd_info)
		return;

	rpmsg_info->inited = 1;
	if (atomic_read(&rpmsg_info->opened))
		queue_work(rpmsg_info->wq, &(rpmsg_info->late_init_work));

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n", rpmsg_info->name);
}

int diag_rpmsg_init_peripheral(uint8_t peripheral)
{
	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__,
			peripheral);
		return -EINVAL;
	}

	mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
	rpmsg_late_init(&rpmsg_data[peripheral]);
	rpmsg_late_init(&rpmsg_dci[peripheral]);
	rpmsg_late_init(&rpmsg_cmd[peripheral]);
	rpmsg_late_init(&rpmsg_dci_cmd[peripheral]);
	mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);

	return 0;
}

static void __diag_rpmsg_init(struct diag_rpmsg_info *rpmsg_info)
{
	char wq_name[DIAG_RPMSG_NAME_SZ + 12];

	if (!rpmsg_info)
		return;

	init_waitqueue_head(&rpmsg_info->wait_q);
	init_waitqueue_head(&rpmsg_info->read_wait_q);
	mutex_init(&rpmsg_info->lock);
	strlcpy(wq_name, "DIAG_RPMSG_", sizeof(wq_name));
	strlcat(wq_name, rpmsg_info->name, sizeof(wq_name));
	rpmsg_info->wq = create_singlethread_workqueue(wq_name);
	if (!rpmsg_info->wq) {
		pr_err("diag: In %s, unable to create workqueue for rpmsg ch:%s\n",
			   __func__, rpmsg_info->name);
		return;
	}
	INIT_WORK(&(rpmsg_info->open_work), diag_rpmsg_open_work_fn);
	INIT_WORK(&(rpmsg_info->close_work), diag_rpmsg_close_work_fn);
	INIT_WORK(&(rpmsg_info->read_work), diag_rpmsg_read_work_fn);
	INIT_WORK(&(rpmsg_info->late_init_work), diag_rpmsg_late_init_work_fn);
	rpmsg_info->hdl = NULL;
	rpmsg_info->fwd_ctxt = NULL;
	atomic_set(&rpmsg_info->opened, 0);
	atomic_set(&rpmsg_info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"%s initialized fwd_ctxt: %pK hdl: %pK\n",
		rpmsg_info->name, rpmsg_info->fwd_ctxt,
		rpmsg_info->hdl);
}

void diag_rpmsg_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt)
{
	struct diag_rpmsg_info *info = NULL;

	if (!ctxt || !fwd_ctxt)
		return;

	info = (struct diag_rpmsg_info *)ctxt;
	info->fwd_ctxt = fwd_ctxt;
}

int diag_rpmsg_init(void)
{
	uint8_t peripheral;
	struct diag_rpmsg_info *rpmsg_info = NULL;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		if (peripheral != PERIPHERAL_WDSP)
			continue;
		rpmsg_info = &rpmsg_cntl[peripheral];
		mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
		__diag_rpmsg_init(rpmsg_info);
		mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);
		diagfwd_cntl_register(TRANSPORT_RPMSG, rpmsg_info->peripheral,
					(void *)rpmsg_info, &rpmsg_ops,
					&(rpmsg_info->fwd_ctxt));
		mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
		rpmsg_info->inited = 1;
		mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);
		diagfwd_channel_open(rpmsg_info->fwd_ctxt);
		diagfwd_late_open(rpmsg_info->fwd_ctxt);
		mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
		__diag_rpmsg_init(&rpmsg_data[peripheral]);
		__diag_rpmsg_init(&rpmsg_cmd[peripheral]);
		__diag_rpmsg_init(&rpmsg_dci[peripheral]);
		__diag_rpmsg_init(&rpmsg_dci_cmd[peripheral]);
		mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);
	}
	return 0;
}

static void __diag_rpmsg_exit(struct diag_rpmsg_info *rpmsg_info)
{
	if (!rpmsg_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n",
			 rpmsg_info->name);

	diagfwd_deregister(rpmsg_info->peripheral, rpmsg_info->type,
					   (void *)rpmsg_info);
	rpmsg_info->fwd_ctxt = NULL;
	rpmsg_info->hdl = NULL;
	if (rpmsg_info->wq)
		destroy_workqueue(rpmsg_info->wq);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
			 rpmsg_info->name);
}

void diag_rpmsg_early_exit(void)
{
	int peripheral = 0;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		if (peripheral != PERIPHERAL_WDSP)
			continue;
		mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
		__diag_rpmsg_exit(&rpmsg_cntl[peripheral]);
		mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);
	}
}

void diag_rpmsg_exit(void)
{
	int peripheral = 0;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		mutex_lock(&driver->rpmsginfo_mutex[peripheral]);
		__diag_rpmsg_exit(&rpmsg_data[peripheral]);
		__diag_rpmsg_exit(&rpmsg_cmd[peripheral]);
		__diag_rpmsg_exit(&rpmsg_dci[peripheral]);
		__diag_rpmsg_exit(&rpmsg_dci_cmd[peripheral]);
		mutex_unlock(&driver->rpmsginfo_mutex[peripheral]);
	}
}

static struct diag_rpmsg_info *diag_get_rpmsg_ptr(char *name)
{

	if (!name)
		return NULL;
	if (!strcmp(name, "DIAG_CMD"))
		return &rpmsg_cmd[PERIPHERAL_WDSP];
	else if (!strcmp(name, "DIAG_CTRL"))
		return &rpmsg_cntl[PERIPHERAL_WDSP];
	else if (!strcmp(name, "DIAG_DATA"))
		return &rpmsg_data[PERIPHERAL_WDSP];
	else if (!strcmp(name, "DIAG_DCI_CMD"))
		return &rpmsg_dci_cmd[PERIPHERAL_WDSP];
	else if (!strcmp(name, "DIAG_DCI_DATA"))
		return &rpmsg_dci[PERIPHERAL_WDSP];
	else
		return NULL;
}

static int diag_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;

	if (!rpdev)
		return 0;
	if (strcmp(rpdev->dev.parent->of_node->name, "wdsp"))
		return 0;

	rpmsg_info = diag_get_rpmsg_ptr(rpdev->id.name);
	if (rpmsg_info) {

		mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		rpmsg_info->hdl = rpdev;
		atomic_set(&rpmsg_info->opened, 1);
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);

		dev_set_drvdata(&rpdev->dev, rpmsg_info);
		diagfwd_channel_read(rpmsg_info->fwd_ctxt);
		queue_work(rpmsg_info->wq, &rpmsg_info->open_work);
	}

	return 0;
}

static void diag_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct diag_rpmsg_info *rpmsg_info = NULL;

	if (!rpdev)
		return;

	rpmsg_info = diag_get_rpmsg_ptr(rpdev->id.name);
	if (rpmsg_info) {
		mutex_lock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		atomic_set(&rpmsg_info->opened, 0);
		mutex_unlock(&driver->rpmsginfo_mutex[rpmsg_info->peripheral]);
		queue_work(rpmsg_info->wq, &rpmsg_info->close_work);
	}
}

static struct rpmsg_device_id rpmsg_diag_table[] = {
	{ .name	= "DIAG_CMD" },
	{ .name	= "DIAG_CTRL" },
	{ .name	= "DIAG_DATA" },
	{ .name	= "DIAG_DCI_CMD" },
	{ .name	= "DIAG_DCI_DATA" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_diag_table);

static struct rpmsg_driver diag_rpmsg_drv = {
	.drv.name	= KBUILD_MODNAME,
	.id_table	= rpmsg_diag_table,
	.probe		= diag_rpmsg_probe,
	.callback	= diag_rpmsg_notify_cb,
	.remove		= diag_rpmsg_remove,
};
module_rpmsg_driver(diag_rpmsg_drv);

