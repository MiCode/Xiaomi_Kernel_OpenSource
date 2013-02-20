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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <mach/sdio_al.h>
#include <mach/sdio_smem.h>

static void sdio_smem_read(struct work_struct *work);

static struct sdio_channel *channel;
static struct workqueue_struct *workq;
static DECLARE_WORK(work_read, sdio_smem_read);
static DECLARE_WAIT_QUEUE_HEAD(waitq);
static int bytes_avail;
static int sdio_ch_opened;

static void sdio_smem_release(struct device *dev)
{
	pr_debug("sdio smem released\n");
}

static struct sdio_smem_client client;

static void sdio_smem_read(struct work_struct *work)
{
	int err;
	int read_avail;
	char *data = client.buf;

	if (!sdio_ch_opened)
		return;

	read_avail = sdio_read_avail(channel);
	if (read_avail > bytes_avail ||
		read_avail < 0) {
		pr_err("Error: read_avail=%d bytes_avail=%d\n",
			read_avail, bytes_avail);
		goto read_err;
	}

	if (read_avail == 0)
		return;

	err = sdio_read(channel,
			&data[client.size - bytes_avail],
			read_avail);
	if (err) {
		pr_err("sdio_read error (%d)", err);
		goto read_err;
	}

	bytes_avail -= read_avail;
	pr_debug("read %d bytes (bytes_avail = %d)\n",
			read_avail, bytes_avail);

	if (!bytes_avail) {
		bytes_avail = client.size;
		err = client.cb_func(SDIO_SMEM_EVENT_READ_DONE);
	}
	if (err)
		pr_err("error (%d) on callback\n", err);

	return;

read_err:
	if (sdio_ch_opened)
		client.cb_func(SDIO_SMEM_EVENT_READ_ERR);
	return;
}

static void sdio_smem_notify(void *priv, unsigned event)
{
	pr_debug("%d event received\n", event);

	if (event == SDIO_EVENT_DATA_READ_AVAIL ||
	    event == SDIO_EVENT_DATA_WRITE_AVAIL)
		queue_work(workq, &work_read);
}

int sdio_smem_register_client(void)
{
	int err = 0;

	if (!client.buf || !client.size || !client.cb_func)
		return -EINVAL;

	pr_debug("buf = %p\n", client.buf);
	pr_debug("size = 0x%x\n", client.size);

	bytes_avail = client.size;
	workq = create_singlethread_workqueue("sdio_smem");
	if (!workq)
		return -ENOMEM;

	sdio_ch_opened = 1;
	err = sdio_open("SDIO_SMEM", &channel, NULL, sdio_smem_notify);
	if (err) {
		sdio_ch_opened = 0;
		pr_err("sdio_open error (%d)\n", err);
		destroy_workqueue(workq);
		return err;
	}
	pr_debug("SDIO SMEM channel opened\n");
	return err;
}

int sdio_smem_unregister_client(void)
{
	int err = 0;

	sdio_ch_opened = 0;
	err = sdio_close(channel);
	if (err) {
		pr_err("sdio_close error (%d)\n", err);
		return err;
	}
	pr_debug("SDIO SMEM channel closed\n");
	flush_workqueue(workq);
	destroy_workqueue(workq);
	bytes_avail = 0;
	client.buf = NULL;
	client.cb_func = NULL;
	client.size = 0;

	return 0;
}

static int sdio_smem_probe(struct platform_device *pdev)
{
	client.plat_dev.name = "SDIO_SMEM_CLIENT";
	client.plat_dev.id = -1;
	client.plat_dev.dev.release = sdio_smem_release;

	return platform_device_register(&client.plat_dev);
}

static int sdio_smem_remove(struct platform_device *pdev)
{
	platform_device_unregister(&client.plat_dev);
	memset(&client, 0, sizeof(client));
	sdio_ch_opened = 0;
	return 0;
}
static struct platform_driver sdio_smem_drv = {
	.probe		= sdio_smem_probe,
	.remove		= sdio_smem_remove,
	.driver		= {
		.name	= "SDIO_SMEM",
		.owner	= THIS_MODULE,
	},
};

static int __init sdio_smem_init(void)
{
	return platform_driver_register(&sdio_smem_drv);
};

module_init(sdio_smem_init);

MODULE_DESCRIPTION("SDIO SMEM");
MODULE_LICENSE("GPL v2");
