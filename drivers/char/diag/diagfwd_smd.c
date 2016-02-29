/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
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
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_smd.h"
#include "diag_ipc_logging.h"

struct diag_smd_info smd_data[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DATA,
		.name = "MODEM_DATA"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DATA,
		.name = "LPASS_DATA"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DATA,
		.name = "WCNSS_DATA"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DATA,
		.name = "SENSORS_DATA"
	}
};

struct diag_smd_info smd_cntl[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CNTL,
		.name = "MODEM_CNTL"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CNTL,
		.name = "LPASS_CNTL"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CNTL,
		.name = "WCNSS_CNTL"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CNTL,
		.name = "SENSORS_CNTL"
	}
};

struct diag_smd_info smd_dci[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI,
		.name = "MODEM_DCI"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI,
		.name = "LPASS_DCI"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI,
		.name = "WCNSS_DCI"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI,
		.name = "SENSORS_DCI"
	}
};

struct diag_smd_info smd_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CMD,
		.name = "MODEM_CMD"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CMD,
		.name = "LPASS_CMD"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CMD,
		.name = "WCNSS_CMD"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CMD,
		.name = "SENSORS_CMD"
	}
};

struct diag_smd_info smd_dci_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI_CMD,
		.name = "MODEM_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI_CMD,
		.name = "LPASS_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI_CMD,
		.name = "WCNSS_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI_CMD,
		.name = "SENSORS_DCI_CMD"
	}
};

static void diag_state_open_smd(void *ctxt);
static void diag_state_close_smd(void *ctxt);
static void smd_notify(void *ctxt, unsigned event);
static int diag_smd_write(void *ctxt, unsigned char *buf, int len);
static int diag_smd_read(void *ctxt, unsigned char *buf, int buf_len);
static void diag_smd_queue_read(void *ctxt);

static struct diag_peripheral_ops smd_ops = {
	.open = diag_state_open_smd,
	.close = diag_state_close_smd,
	.write = diag_smd_write,
	.read = diag_smd_read,
	.queue_read = diag_smd_queue_read
};

static void diag_state_open_smd(void *ctxt)
{
	struct diag_smd_info *smd_info = NULL;

	if (!ctxt)
		return;

	smd_info = (struct diag_smd_info *)(ctxt);
	atomic_set(&smd_info->diag_state, 1);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		 "%s setting diag state to 1", smd_info->name);
}

static void diag_state_close_smd(void *ctxt)
{
	struct diag_smd_info *smd_info = NULL;

	if (!ctxt)
		return;

	smd_info = (struct diag_smd_info *)(ctxt);
	atomic_set(&smd_info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		 "%s setting diag state to 0", smd_info->name);
	wake_up_interruptible(&smd_info->read_wait_q);
	flush_workqueue(smd_info->wq);
}

static int smd_channel_probe(struct platform_device *pdev, uint8_t type)
{
	int r = 0;
	int index = -1;
	const char *channel_name = NULL;
	struct diag_smd_info *smd_info = NULL;

	switch (pdev->id) {
	case SMD_APPS_MODEM:
		index = PERIPHERAL_MODEM;
		break;
	case SMD_APPS_QDSP:
		index = PERIPHERAL_LPASS;
		break;
	case SMD_APPS_WCNSS:
		index = PERIPHERAL_WCNSS;
		break;
	case SMD_APPS_DSPS:
		index = PERIPHERAL_SENSORS;
		break;
	default:
		pr_debug("diag: In %s Received probe for invalid index %d",
			__func__, pdev->id);
		return -EINVAL;
	}

	switch (type) {
	case TYPE_DATA:
		smd_info = &smd_data[index];
		channel_name = "DIAG";
		break;
	case TYPE_CNTL:
		smd_info = &smd_cntl[index];
		channel_name = "DIAG_CNTL";
		break;
	case TYPE_CMD:
		smd_info = &smd_cmd[index];
		channel_name = "DIAG_CMD";
		break;
	case TYPE_DCI:
		smd_info = &smd_dci[index];
		channel_name = "DIAG_2";
		break;
	case TYPE_DCI_CMD:
		smd_info = &smd_dci_cmd[index];
		channel_name = "DIAG_2_CMD";
		break;
	default:
		return -EINVAL;
	}

	if (index == PERIPHERAL_WCNSS && type == TYPE_DATA)
		channel_name = "APPS_RIVA_DATA";
	else if (index == PERIPHERAL_WCNSS && type == TYPE_CNTL)
		channel_name = "APPS_RIVA_CTRL";

	if (!channel_name || !smd_info)
		return -EIO;

	r = smd_named_open_on_edge(channel_name, pdev->id, &smd_info->hdl,
				   smd_info, smd_notify);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pr_debug("diag: In %s, SMD port probed %s, id = %d, r = %d\n",
		 __func__, smd_info->name, pdev->id, r);

	return 0;
}

static int smd_data_probe(struct platform_device *pdev)
{
	return smd_channel_probe(pdev, TYPE_DATA);
}

static int smd_cntl_probe(struct platform_device *pdev)
{
	return smd_channel_probe(pdev, TYPE_CNTL);
}

static int smd_cmd_probe(struct platform_device *pdev)
{
	return smd_channel_probe(pdev, TYPE_CMD);
}

static int smd_dci_probe(struct platform_device *pdev)
{
	return smd_channel_probe(pdev, TYPE_DCI);
}

static int smd_dci_cmd_probe(struct platform_device *pdev)
{
	return smd_channel_probe(pdev, TYPE_DCI_CMD);
}

static int smd_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int smd_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops smd_dev_pm_ops = {
	.runtime_suspend = smd_runtime_suspend,
	.runtime_resume = smd_runtime_resume,
};

static struct platform_driver diag_smd_ch_driver = {
	.probe = smd_data_probe,
	.driver = {
		.name = "DIAG",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_driver = {
	.probe = smd_data_probe,
	.driver = {
		.name = "APPS_RIVA_DATA",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_cntl_driver = {
	.probe = smd_cntl_probe,
	.driver = {
		.name = "DIAG_CNTL",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_cntl_driver = {
	.probe = smd_cntl_probe,
	.driver = {
		.name = "APPS_RIVA_CTRL",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_cmd_driver = {
	.probe = smd_cmd_probe,
	.driver = {
		.name = "DIAG_CMD",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	}
};

static struct platform_driver diag_smd_dci_driver = {
	.probe = smd_dci_probe,
	.driver = {
		.name = "DIAG_2",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_dci_cmd_driver = {
	.probe = smd_dci_cmd_probe,
	.driver = {
		.name = "DIAG_2_CMD",
		.owner = THIS_MODULE,
		.pm   = &smd_dev_pm_ops,
	},
};

static void smd_open_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						      struct diag_smd_info,
						      open_work);
	if (!smd_info->inited)
		return;

	diagfwd_channel_open(smd_info->fwd_ctxt);
	diagfwd_late_open(smd_info->fwd_ctxt);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 smd_info->name);
}

static void smd_close_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						      struct diag_smd_info,
						      close_work);
	if (!smd_info->inited)
		return;

	diagfwd_channel_close(smd_info->fwd_ctxt);
	wake_up_interruptible(&smd_info->read_wait_q);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 smd_info->name);
}

static void smd_read_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						      struct diag_smd_info,
						      read_work);
	if (!smd_info->inited) {
		diag_ws_release();
		return;
	}

	diagfwd_channel_read(smd_info->fwd_ctxt);
}

static void diag_smd_queue_read(void *ctxt)
{
	struct diag_smd_info *smd_info = NULL;

	if (!ctxt)
		return;

	smd_info = (struct diag_smd_info *)ctxt;
	if (smd_info->inited && atomic_read(&smd_info->opened) &&
	    smd_info->hdl) {
		wake_up_interruptible(&smd_info->read_wait_q);
		queue_work(smd_info->wq, &(smd_info->read_work));
	}
}
int diag_smd_check_state(void *ctxt)
{
	struct diag_smd_info *info = NULL;

	if (!ctxt)
		return 0;

	info = (struct diag_smd_info *)ctxt;
	return (int)(atomic_read(&info->diag_state));
}
void diag_smd_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt)
{
	struct diag_smd_info *smd_info = NULL;
	void *prev = NULL;

	if (!ctxt || !fwd_ctxt)
		return;

	smd_info = (struct diag_smd_info *)ctxt;
	prev = smd_info->fwd_ctxt;
	smd_info->fwd_ctxt = fwd_ctxt;
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s prev: %p fwd_ctxt: %p\n",
		 smd_info->name, prev, smd_info->fwd_ctxt);
}

static void __diag_smd_init(struct diag_smd_info *smd_info)
{
	char wq_name[DIAG_SMD_NAME_SZ + 10];
	if (!smd_info)
		return;

	init_waitqueue_head(&smd_info->read_wait_q);
	mutex_init(&smd_info->lock);
	strlcpy(wq_name, "DIAG_SMD_", 10);
	strlcat(wq_name, smd_info->name, sizeof(smd_info->name));
	smd_info->wq = create_singlethread_workqueue(wq_name);
	if (!smd_info->wq) {
		pr_err("diag: In %s, unable to create workqueue for smd channel %s\n",
		       __func__, smd_info->name);
		return;
	}
	INIT_WORK(&(smd_info->open_work), smd_open_work_fn);
	INIT_WORK(&(smd_info->close_work), smd_close_work_fn);
	INIT_WORK(&(smd_info->read_work), smd_read_work_fn);
	smd_info->fifo_size = 0;
	smd_info->hdl = NULL;
	smd_info->fwd_ctxt = NULL;
	atomic_set(&smd_info->opened, 0);
	atomic_set(&smd_info->diag_state, 0);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s initialized fwd_ctxt: %p\n",
		 smd_info->name, smd_info->fwd_ctxt);
}

int diag_smd_init(void)
{
	uint8_t peripheral;
	struct diag_smd_info *smd_info = NULL;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		smd_info = &smd_cntl[peripheral];
		__diag_smd_init(smd_info);
		diagfwd_cntl_register(TRANSPORT_SMD, smd_info->peripheral,
				      (void *)smd_info, &smd_ops,
				      &smd_info->fwd_ctxt);
		smd_info->inited = 1;
		__diag_smd_init(&smd_data[peripheral]);
		__diag_smd_init(&smd_cmd[peripheral]);
		__diag_smd_init(&smd_dci[peripheral]);
		__diag_smd_init(&smd_dci_cmd[peripheral]);
	}

	platform_driver_register(&diag_smd_cntl_driver);
	platform_driver_register(&diag_smd_lite_cntl_driver);
	platform_driver_register(&diag_smd_ch_driver);
	platform_driver_register(&diag_smd_lite_driver);
	platform_driver_register(&diag_smd_lite_cmd_driver);
	platform_driver_register(&diag_smd_dci_driver);
	platform_driver_register(&diag_smd_dci_cmd_driver);

	return 0;
}

static void smd_late_init(struct diag_smd_info *smd_info)
{
	struct diagfwd_info *fwd_info = NULL;
	if (!smd_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n",
		 smd_info->name);

	diagfwd_register(TRANSPORT_SMD, smd_info->peripheral, smd_info->type,
			 (void *)smd_info, &smd_ops, &smd_info->fwd_ctxt);
	fwd_info = smd_info->fwd_ctxt;
	smd_info->inited = 1;
	/*
	 * The channel is already open by the probe call as a result of other
	 * peripheral. Inform the diag fwd layer that the channel is open.
	 */
	if (atomic_read(&smd_info->opened))
		diagfwd_channel_open(smd_info->fwd_ctxt);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 smd_info->name);
}

int diag_smd_init_peripheral(uint8_t peripheral)
{
	if (peripheral >= NUM_PERIPHERALS) {
		pr_err("diag: In %s, invalid peripheral %d\n",
		       __func__, peripheral);
		return -EINVAL;
	}

	smd_late_init(&smd_data[peripheral]);
	smd_late_init(&smd_dci[peripheral]);
	smd_late_init(&smd_cmd[peripheral]);
	smd_late_init(&smd_dci_cmd[peripheral]);

	return 0;
}

static void __diag_smd_exit(struct diag_smd_info *smd_info)
{
	if (!smd_info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s entering\n",
		 smd_info->name);

	diagfwd_deregister(smd_info->peripheral, smd_info->type,
			   (void *)smd_info);
	smd_info->fwd_ctxt = NULL;
	smd_info->hdl = NULL;
	if (smd_info->wq)
		destroy_workqueue(smd_info->wq);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n",
		 smd_info->name);
}

void diag_smd_early_exit(void)
{
	int i = 0;

	for (i = 0; i < NUM_PERIPHERALS; i++)
		__diag_smd_exit(&smd_cntl[i]);

	platform_driver_unregister(&diag_smd_cntl_driver);
	platform_driver_unregister(&diag_smd_lite_cntl_driver);
}

void diag_smd_exit(void)
{
	int i = 0;

	for (i = 0; i < NUM_PERIPHERALS; i++) {
		__diag_smd_exit(&smd_data[i]);
		__diag_smd_exit(&smd_cmd[i]);
		__diag_smd_exit(&smd_dci[i]);
		__diag_smd_exit(&smd_dci_cmd[i]);
	}

	platform_driver_unregister(&diag_smd_ch_driver);
	platform_driver_unregister(&diag_smd_lite_driver);
	platform_driver_unregister(&diag_smd_lite_cmd_driver);
	platform_driver_unregister(&diag_smd_dci_driver);
	platform_driver_unregister(&diag_smd_dci_cmd_driver);
}

static int diag_smd_write_ext(struct diag_smd_info *smd_info,
			      unsigned char *buf, int len)
{
	int err = 0;
	int offset = 0;
	int write_len = 0;
	int retry_count = 0;
	int max_retries = 3;
	uint8_t avail = 0;

	if (!smd_info || !buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, invalid params, smd_info: %p, buf: %p, len: %d\n",
				   __func__, smd_info, buf, len);
		return -EINVAL;
	}

	if (!smd_info->inited || !smd_info->hdl ||
	    !atomic_read(&smd_info->opened))
		return -ENODEV;

	mutex_lock(&smd_info->lock);
	err = smd_write_start(smd_info->hdl, len);
	if (err) {
		pr_err_ratelimited("diag: In %s, error calling smd_write_start, peripheral: %d, err: %d\n",
				   __func__, smd_info->peripheral, err);
		goto fail;
	}

	while (offset < len) {
		retry_count = 0;
		do {
			if (smd_write_segment_avail(smd_info->hdl)) {
				avail = 1;
				break;
			}
			/*
			 * The channel maybe busy - the FIFO can be full. Retry
			 * after sometime. The value of 10000 was chosen
			 * emprically as the optimal value for the peripherals
			 * to read data from the SMD channel.
			 */
			usleep_range(10000, 10100);
			retry_count++;
		} while (retry_count < max_retries);

		if (!avail) {
			err = -EAGAIN;
			goto fail;
		}

		write_len = smd_write_segment(smd_info->hdl, buf + offset,
					      (len - offset));
		offset += write_len;
		write_len = 0;
	}

	err = smd_write_end(smd_info->hdl);
	if (err) {
		pr_err_ratelimited("diag: In %s, error calling smd_write_end, peripheral: %d, err: %d\n",
				   __func__, smd_info->peripheral, err);
		goto fail;
	}

fail:
	mutex_unlock(&smd_info->lock);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		 "%s wrote to channel, write_len: %d, err: %d\n",
		 smd_info->name, offset, err);
	return err;
}

static int diag_smd_write(void *ctxt, unsigned char *buf, int len)
{
	int write_len = 0;
	int retry_count = 0;
	int max_retries = 3;
	struct diag_smd_info *smd_info = NULL;

	if (!ctxt || !buf)
		return -EIO;

	smd_info = (struct diag_smd_info *)ctxt;
	if (!smd_info || !buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, invalid params, smd_info: %p, buf: %p, len: %d\n",
				   __func__, smd_info, buf, len);
		return -EINVAL;
	}

	if (!smd_info->inited || !smd_info->hdl ||
	    !atomic_read(&smd_info->opened))
		return -ENODEV;

	if (len > smd_info->fifo_size)
		return diag_smd_write_ext(smd_info, buf, len);

	do {
		mutex_lock(&smd_info->lock);
		write_len = smd_write(smd_info->hdl, buf, len);
		mutex_unlock(&smd_info->lock);
		if (write_len == len)
			break;
		/*
		 * The channel maybe busy - the FIFO can be full. Retry after
		 * sometime. The value of 10000 was chosen emprically as the
		 * optimal value for the peripherals to read data from the SMD
		 * channel.
		 */
		usleep_range(10000, 10100);
		retry_count++;
	} while (retry_count < max_retries);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s wrote to channel, write_len: %d\n",
		 smd_info->name, write_len);

	if (write_len != len)
		return -ENOMEM;

	return 0;
}

static int diag_smd_read(void *ctxt, unsigned char *buf, int buf_len)
{
	int pkt_len = 0;
	int err = 0;
	int total_recd_partial = 0;
	int total_recd = 0;
	uint8_t buf_full = 0;
	unsigned char *temp_buf = NULL;
	uint32_t read_len = 0;
	struct diag_smd_info *smd_info = NULL;

	if (!ctxt || !buf || buf_len <= 0)
		return -EIO;

	smd_info = (struct diag_smd_info *)ctxt;
	if (!smd_info->hdl || !smd_info->inited ||
	    !atomic_read(&smd_info->opened))
		return -EIO;

	err = wait_event_interruptible(smd_info->read_wait_q,
				       (smd_info->hdl == NULL) ||
				       (atomic_read(&smd_info->opened) == 0) ||
				       (smd_cur_packet_size(smd_info->hdl)) ||
				       (!atomic_read(&smd_info->diag_state)));
	if (err) {
		diagfwd_channel_read_done(smd_info->fwd_ctxt, buf, 0);
		return -ERESTARTSYS;
	}

	/*
	 * In this case don't reset the buffers as there is no need to further
	 * read over peripherals. Also release the wake source hold earlier.
	 */
	if (atomic_read(&smd_info->diag_state) == 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s closing read thread. diag state is closed\n",
			 smd_info->name);
		diag_ws_release();
		return 0;
	}

	if (!smd_info->hdl || !atomic_read(&smd_info->opened)) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s stopping read, hdl: %p, opened: %d\n",
			 smd_info->name, smd_info->hdl,
			 atomic_read(&smd_info->opened));
		goto fail_return;
	}

	do {
		total_recd_partial = 0;
		temp_buf = buf + total_recd;
		pkt_len = smd_cur_packet_size(smd_info->hdl);
		if (pkt_len <= 0)
			break;

		if (total_recd + pkt_len > buf_len) {
			buf_full = 1;
			break;
		}

		while (total_recd_partial < pkt_len) {
			read_len = smd_read_avail(smd_info->hdl);
			if (!read_len) {
				wait_event_interruptible(smd_info->read_wait_q,
					   ((atomic_read(&smd_info->opened)) &&
					    smd_read_avail(smd_info->hdl)));

				if (!smd_info->hdl ||
				    !atomic_read(&smd_info->opened)) {
					DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
						"%s exiting from wait",
						smd_info->name);
					goto fail_return;
				}
			}

			if (pkt_len < read_len)
				goto fail_return;

			smd_read(smd_info->hdl, temp_buf, read_len);
			total_recd_partial += read_len;
			total_recd += read_len;
			temp_buf += read_len;
		}
	} while (pkt_len > 0);

	if ((smd_info->type == TYPE_DATA && pkt_len) || buf_full)
		err = queue_work(smd_info->wq, &(smd_info->read_work));

	if (total_recd > 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s read total bytes: %d\n",
			 smd_info->name, total_recd);
		diagfwd_channel_read_done(smd_info->fwd_ctxt, buf, total_recd);
	} else {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s error in read, err: %d\n",
			 smd_info->name, total_recd);
		goto fail_return;
	}
	return 0;

fail_return:
	diagfwd_channel_read_done(smd_info->fwd_ctxt, buf, 0);
	return -EINVAL;
}

static void smd_notify(void *ctxt, unsigned event)
{
	struct diag_smd_info *smd_info = NULL;

	smd_info = (struct diag_smd_info *)ctxt;
	if (!smd_info)
		return;

	switch (event) {
	case SMD_EVENT_OPEN:
		atomic_set(&smd_info->opened, 1);
		smd_info->fifo_size = smd_write_avail(smd_info->hdl);
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s channel opened\n",
			 smd_info->name);
		queue_work(smd_info->wq, &(smd_info->open_work));
		break;
	case SMD_EVENT_CLOSE:
		atomic_set(&smd_info->opened, 0);
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s channel closed\n",
			 smd_info->name);
		queue_work(smd_info->wq, &(smd_info->close_work));
		break;
	case SMD_EVENT_DATA:
		diag_ws_on_notify();
		queue_work(smd_info->wq, &(smd_info->read_work));
		break;
	}

	wake_up_interruptible(&smd_info->read_wait_q);
}

