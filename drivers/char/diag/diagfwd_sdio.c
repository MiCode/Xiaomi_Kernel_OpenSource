/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <asm/current.h>
#include "diagmem.h"
#include "diagfwd_bridge.h"
#include "diagfwd_sdio.h"

#define DIAG_SDIO_STRING_SZ	11

struct diag_sdio_info diag_sdio[NUM_SDIO_DEV] = {
	{
		.id = SDIO_1,
		.dev_id = DIAGFWD_MDM,
		.name = "MDM",
		.mempool = POOL_TYPE_MDM,
		.opened = 0,
		.enabled = 0,
		.suspended = 0,
		.sdio_wq = NULL
	},
};

static void diag_sdio_read_complete(void *ctxt, char *buf, int len,
				    int actual_size)
{
	int err = 0;
	int index = (int)(uintptr_t)ctxt;
	struct diag_sdio_info *ch = NULL;

	if (index < 0 || index >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return;
	}
	ch = &diag_sdio[index];

	/*
	 * Don't pass on the buffer if the channel is closed when a pending read
	 * completes. Also, actual size can be negative error codes - do not
	 * pass on the buffer.
	 */
	if (!ch->opened || actual_size <= 0)
		goto fail;
	err = diag_remote_dev_read_done(ch->dev_id, buf, actual_size);
	if (err)
		goto fail;
	return;

fail:
	diagmem_free(driver, buf, ch->mempool);
	queue_work(ch->sdio_wq, &ch->read_work);
	return;
}

static void diag_sdio_write_complete(void *ctxt, char *buf, int len,
				     int actual_size)
{
	int index = (int)(uintptr_t)ctxt;
	struct diag_sdio_info *ch = NULL;

	if (index < 0 || index >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return;
	}

	ch = &diag_sdio[index];
	diag_remote_dev_write_done(ch->dev_id, buf, actual_size, ch->id);
	return;
}

static int diag_sdio_suspend(void *ctxt)
{
	int index = (int)(uintptr_t)ctxt;
	unsigned long flags;
	struct diag_sdio_info *ch = NULL;

	if (index < 0 || index >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return -EINVAL;
	}

	ch = &diag_sdio[index];
	spin_lock_irqsave(&ch->lock, flags);
	ch->suspended = 1;
	spin_unlock_irqrestore(&ch->lock, flags);
	return 0;
}

static void diag_sdio_resume(void *ctxt)
{
	int index = (int)(uintptr_t)ctxt;
	unsigned long flags;
	struct diag_sdio_info *ch = NULL;

	if (index < 0 || index >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return;
	}
	ch = &diag_sdio[index];
	spin_lock_irqsave(&ch->lock, flags);
	ch->suspended = 0;
	spin_unlock_irqrestore(&ch->lock, flags);
	queue_work(ch->sdio_wq, &(ch->read_work));
}

static struct diag_bridge_ops diag_sdio_ops[NUM_SDIO_DEV] = {
	{
		.ctxt = (void *)SDIO_1,
		.read_complete_cb = diag_sdio_read_complete,
		.write_complete_cb = diag_sdio_write_complete,
		.suspend = diag_sdio_suspend,
		.resume = diag_sdio_resume,
	},
};

static int sdio_open(int id)
{
	int err = 0;
	unsigned long flags;
	struct diag_sdio_info *ch = NULL;

	if (id < 0 || id >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}

	ch = &diag_sdio[id];
	if (!ch->enabled)
		return -ENODEV;

	if (ch->opened) {
		pr_debug("diag: SDIO channel %d is already opened\n", ch->id);
		return -ENODEV;
	}

	err = qti_client_open(ch->ch_num, &diag_sdio_ops[ch->id]);
	if (err) {
		pr_err("diag: Unable to open SDIO channel %d, err: %d",
		       ch->id, err);
		return err;
	}
	spin_lock_irqsave(&ch->lock, flags);
	ch->opened = 1;
	spin_unlock_irqrestore(&ch->lock, flags);
	diagmem_init(driver, ch->mempool);
	/* Notify the bridge that the channel is open */
	diag_remote_dev_open(ch->dev_id);
	queue_work(ch->sdio_wq, &(ch->read_work));
	return 0;
}

static void sdio_open_work_fn(struct work_struct *work)
{
	struct diag_sdio_info *ch = container_of(work, struct diag_sdio_info,
						 open_work);
	if (ch)
		sdio_open(ch->id);
}

static int sdio_close(int id)
{
	unsigned long flags;
	struct diag_sdio_info *ch = NULL;

	if (id < 0 || id >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}

	ch = &diag_sdio[id];
	if (!ch->enabled)
		return -ENODEV;

	if (!ch->opened) {
		pr_debug("diag: SDIO channel %d is already closed\n", ch->id);
		return -ENODEV;
	}

	spin_lock_irqsave(&ch->lock, flags);
	ch->opened = 0;
	spin_unlock_irqrestore(&ch->lock, flags);
	qti_client_close(ch->ch_num);
	diagmem_exit(driver, ch->mempool);
	diag_remote_dev_close(ch->dev_id);
	return 0;
}

static void sdio_close_work_fn(struct work_struct *work)
{
	struct diag_sdio_info *ch = container_of(work, struct diag_sdio_info,
						 close_work);
	if (ch)
		sdio_close(ch->id);
}

static void sdio_read_work_fn(struct work_struct *work)
{
	int err = 0;
	unsigned char *buf = NULL;
	struct diag_sdio_info *ch = container_of(work, struct diag_sdio_info,
						 read_work);
	if (!ch || !ch->enabled || !ch->opened)
		return;

	do {
		buf = diagmem_alloc(driver, DIAG_MDM_BUF_SIZE, ch->mempool);
		if (!buf) {
			err = -ENOMEM;
			break;
		}

		err = qti_client_read(ch->ch_num, buf, DIAG_MDM_BUF_SIZE);
		if (err < 0) {
			diagmem_free(driver, buf, ch->mempool);
			break;
		}
	} while (buf);

	/* Read from the SDIO channel continuously if the channel is present */
	if (!err)
		queue_work(ch->sdio_wq, &ch->read_work);
}

static int diag_sdio_probe(struct platform_device *pdev)
{
	unsigned long flags;
	struct diag_sdio_info *ch = NULL;

	if (!pdev)
		return -EIO;

	pr_debug("diag: sdio probe pdev: %d\n", pdev->id);
	if (pdev->id >= NUM_SDIO_DEV) {
		pr_err("diag: No support for SDIO device %d\n", pdev->id);
		return -EIO;
	}

	ch = &diag_sdio[pdev->id];
	ch->ch_num = *((int *)pdev->dev.platform_data);

	if (!ch->enabled) {
		spin_lock_irqsave(&ch->lock, flags);
		ch->enabled = 1;
		spin_unlock_irqrestore(&ch->lock, flags);
	}
	queue_work(ch->sdio_wq, &(ch->open_work));
	return 0;
}

static int diag_sdio_remove(struct platform_device *pdev)
{
	struct diag_sdio_info *ch = NULL;

	if (!pdev)
		return -EIO;

	pr_debug("diag: sdio close pdev: %d\n", pdev->id);
	if (pdev->id >= NUM_SDIO_DEV) {
		pr_err("diag: No support for SDIO device %d\n", pdev->id);
		return -EIO;
	}

	ch = &diag_sdio[pdev->id];
	queue_work(ch->sdio_wq, &(ch->close_work));
	return 0;
}

static int diagfwd_sdio_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_sdio_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_sdio_dev_pm_ops = {
	.runtime_suspend = diagfwd_sdio_runtime_suspend,
	.runtime_resume = diagfwd_sdio_runtime_resume,
};

static struct platform_driver msm_sdio_ch_driver = {
	.probe = diag_sdio_probe,
	.remove = diag_sdio_remove,
	.driver = {
		   .name = "diag_bridge_sdio",
		   .owner = THIS_MODULE,
		   .pm   = &diagfwd_sdio_dev_pm_ops,
		   },
};

static int sdio_queue_read(int id)
{
	if (id < 0 || id >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}
	queue_work(diag_sdio[id].sdio_wq, &(diag_sdio[id].read_work));
	return 0;
}

static int sdio_write(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	struct diag_sdio_info *ch = NULL;


	if (id < 0 || id >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}
	if (!buf || len <= 0) {
		return -EINVAL;
	}

	ch = &diag_sdio[id];
	if (!ch->opened || !ch->enabled) {
		pr_debug("diag: %s, ch %d is disabled. opened %d enabled %d\n",
				     __func__, id, ch->opened, ch->enabled);
		return -EIO;
	}

	err = qti_client_write(ch->ch_num, buf, len);
	if (err < 0) {
		pr_err("diag: failed to write to ch[%d] in %s\n", ch->ch_num,
								__func__);
		return err;
	}
	return 0;
}

static int sdio_fwd_complete(int id, unsigned char *buf, int len, int ctxt)
{
	if (id < 0 || id >= NUM_SDIO_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}
	if (!buf)
		return -EIO;
	diagmem_free(driver, buf, diag_sdio[id].mempool);
	queue_work(diag_sdio[id].sdio_wq, &(diag_sdio[id].read_work));
	return 0;
}

static struct diag_remote_dev_ops diag_sdio_fwd_ops = {
	.open = sdio_open,
	.close = sdio_close,
	.queue_read = sdio_queue_read,
	.write = sdio_write,
	.fwd_complete = sdio_fwd_complete,
};

int diag_sdio_init(void)
{
	int i;
	int err = 0;
	struct diag_sdio_info *ch = NULL;
	char wq_name[DIAG_SDIO_NAME_SZ + DIAG_SDIO_STRING_SZ];

	for (i = 0; i < NUM_SDIO_DEV; i++) {
		ch = &diag_sdio[i];
		spin_lock_init(&ch->lock);
		INIT_WORK(&(ch->read_work), sdio_read_work_fn);
		INIT_WORK(&(ch->open_work), sdio_open_work_fn);
		INIT_WORK(&(ch->close_work), sdio_close_work_fn);
		strlcpy(wq_name, "DIAG_SDIO_", DIAG_SDIO_STRING_SZ);
		strlcat(wq_name, ch->name, sizeof(ch->name));
		ch->sdio_wq = create_singlethread_workqueue(wq_name);
		if (!ch->sdio_wq)
			goto fail;
		err = diagfwd_bridge_register(ch->dev_id, ch->id,
					      &diag_sdio_fwd_ops);
		if (err) {
			pr_err("diag: Unable to register SDIO channel %d with bridge, err: %d\n",
			       i, err);
			goto fail;
		}
	}

	err = platform_driver_register(&msm_sdio_ch_driver);
	if (err) {
		pr_err("diag: could not register SDIO device, err: %d\n", err);
		goto fail;
	}

	return 0;
fail:
	diag_sdio_exit();
	return -ENOMEM;
}

void diag_sdio_exit(void)
{
	int i;
	struct diag_sdio_info *ch = NULL;

	for (i = 0; i < NUM_SDIO_DEV; i++) {
		ch = &diag_sdio[i];
		ch->enabled = 0;
		ch->opened = 0;
		ch->suspended = 0;
		if (ch->sdio_wq)
			destroy_workqueue(ch->sdio_wq);
	}
	platform_driver_unregister(&msm_sdio_ch_driver);
}
