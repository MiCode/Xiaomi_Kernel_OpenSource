/* Copyright (c) 2012-2014, 2016-2018 The Linux Foundation. All rights reserved.
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
#include "diagfwd_hsic.h"

#define DIAG_HSIC_STRING_SZ	11

struct diag_hsic_info diag_hsic[NUM_HSIC_DEV] = {
	{
		.id = HSIC_1,
		.dev_id = DIAGFWD_MDM,
		.name = "MDM",
		.mempool = POOL_TYPE_MDM,
		.opened = 0,
		.enabled = 0,
		.suspended = 0,
		.hsic_wq = NULL
	},
	{
		.id = HSIC_2,
		.dev_id = DIAGFWD_MDM_DCI,
		.name = "MDM_DCI",
		.mempool = POOL_TYPE_MDM_DCI,
		.opened = 0,
		.enabled = 0,
		.suspended = 0,
		.hsic_wq = NULL
	}
};

static void diag_hsic_read_complete(void *ctxt, char *buf, int len,
				    int actual_size)
{
	int err = 0;
	int index = (int)(uintptr_t)ctxt;
	struct diag_hsic_info *ch = NULL;

	if (index < 0 || index >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid HSIC index %d\n",
				   __func__, index);
		return;
	}
	ch = &diag_hsic[index];

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
	queue_work(ch->hsic_wq, &ch->read_work);
}

static void diag_hsic_write_complete(void *ctxt, char *buf, int len,
				     int actual_size)
{
	int index = (int)(uintptr_t)ctxt;
	struct diag_hsic_info *ch = NULL;

	if (index < 0 || index >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid HSIC index %d\n",
				   __func__, index);
		return;
	}

	ch = &diag_hsic[index];
	diag_remote_dev_write_done(ch->dev_id, buf, actual_size, ch->id);
}

static int diag_hsic_suspend(void *ctxt)
{
	int index = (int)(uintptr_t)ctxt;
	unsigned long flags;
	struct diag_hsic_info *ch = NULL;

	if (index < 0 || index >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid HSIC index %d\n",
				   __func__, index);
		return -EINVAL;
	}

	ch = &diag_hsic[index];
	spin_lock_irqsave(&ch->lock, flags);
	ch->suspended = 1;
	spin_unlock_irqrestore(&ch->lock, flags);
	return 0;
}

static void diag_hsic_resume(void *ctxt)
{
	int index = (int)(uintptr_t)ctxt;
	unsigned long flags;
	struct diag_hsic_info *ch = NULL;

	if (index < 0 || index >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid HSIC index %d\n",
				   __func__, index);
		return;
	}
	ch = &diag_hsic[index];
	spin_lock_irqsave(&ch->lock, flags);
	ch->suspended = 0;
	spin_unlock_irqrestore(&ch->lock, flags);
	queue_work(ch->hsic_wq, &(ch->read_work));
}

static struct diag_bridge_ops diag_hsic_ops[NUM_HSIC_DEV] = {
	{
		.ctxt = (void *)HSIC_1,
		.read_complete_cb = diag_hsic_read_complete,
		.write_complete_cb = diag_hsic_write_complete,
		.suspend = diag_hsic_suspend,
		.resume = diag_hsic_resume,
	},
	{
		.ctxt = (void *)HSIC_2,
		.read_complete_cb = diag_hsic_read_complete,
		.write_complete_cb = diag_hsic_write_complete,
		.suspend = diag_hsic_suspend,
		.resume = diag_hsic_resume,
	}
};

static int hsic_open(int id)
{
	int err = 0;
	unsigned long flags;
	struct diag_hsic_info *ch = NULL;

	if (id < 0 || id >= NUM_HSIC_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}

	ch = &diag_hsic[id];
	if (!ch->enabled)
		return -ENODEV;

	if (ch->opened) {
		pr_debug("diag: HSIC channel %d is already opened\n", ch->id);
		return -ENODEV;
	}

	err = diag_bridge_open(ch->id, &diag_hsic_ops[ch->id]);
	if (err) {
		pr_err("diag: Unable to open HSIC channel %d, err: %d",
		       ch->id, err);
		return err;
	}
	spin_lock_irqsave(&ch->lock, flags);
	ch->opened = 1;
	spin_unlock_irqrestore(&ch->lock, flags);
	diagmem_init(driver, ch->mempool);
	/* Notify the bridge that the channel is open */
	diag_remote_dev_open(ch->dev_id);
	queue_work(ch->hsic_wq, &(ch->read_work));
	return 0;
}

static void hsic_open_work_fn(struct work_struct *work)
{
	struct diag_hsic_info *ch = container_of(work, struct diag_hsic_info,
						 open_work);
	if (ch)
		hsic_open(ch->id);
}

static int hsic_close(int id)
{
	unsigned long flags;
	struct diag_hsic_info *ch = NULL;

	if (id < 0 || id >= NUM_HSIC_DEV) {
		pr_err("diag: Invalid index %d in %s\n", id, __func__);
		return -EINVAL;
	}

	ch = &diag_hsic[id];
	if (!ch->enabled)
		return -ENODEV;

	if (!ch->opened) {
		pr_debug("diag: HSIC channel %d is already closed\n", ch->id);
		return -ENODEV;
	}

	spin_lock_irqsave(&ch->lock, flags);
	ch->opened = 0;
	spin_unlock_irqrestore(&ch->lock, flags);
	diag_bridge_close(ch->id);
	diagmem_exit(driver, ch->mempool);
	diag_remote_dev_close(ch->dev_id);
	return 0;
}

static void hsic_close_work_fn(struct work_struct *work)
{
	struct diag_hsic_info *ch = container_of(work, struct diag_hsic_info,
						 close_work);
	if (ch)
		hsic_close(ch->id);
}

static void hsic_read_work_fn(struct work_struct *work)
{
	int err = 0;
	unsigned char *buf = NULL;
	struct diag_hsic_info *ch = container_of(work, struct diag_hsic_info,
						 read_work);
	if (!ch || !ch->enabled || !ch->opened)
		return;

	do {
		buf = diagmem_alloc(driver, DIAG_MDM_BUF_SIZE, ch->mempool);
		if (!buf) {
			err = -ENOMEM;
			break;
		}

		err = diag_bridge_read(ch->id, buf, DIAG_MDM_BUF_SIZE);
		if (err) {
			diagmem_free(driver, buf, ch->mempool);
			pr_err_ratelimited("diag: Unable to read from HSIC channel %d, err: %d\n",
					   ch->id, err);
			break;
		}
	} while (buf);

	/* Read from the HSIC channel continuously if the channel is present */
	if (!err)
		queue_work(ch->hsic_wq, &ch->read_work);
}

static int diag_hsic_probe(struct platform_device *pdev)
{
	unsigned long flags;
	struct diag_hsic_info *ch = NULL;

	if (!pdev)
		return -EIO;

	pr_debug("diag: hsic probe pdev: %d\n", pdev->id);
	if (pdev->id >= NUM_HSIC_DEV) {
		pr_err("diag: No support for HSIC device %d\n", pdev->id);
		return -EIO;
	}

	ch = &diag_hsic[pdev->id];
	if (!ch->enabled) {
		spin_lock_irqsave(&ch->lock, flags);
		ch->enabled = 1;
		spin_unlock_irqrestore(&ch->lock, flags);
	}
	queue_work(ch->hsic_wq, &(ch->open_work));
	return 0;
}

static int diag_hsic_remove(struct platform_device *pdev)
{
	struct diag_hsic_info *ch = NULL;

	if (!pdev)
		return -EIO;

	pr_debug("diag: hsic close pdev: %d\n", pdev->id);
	if (pdev->id >= NUM_HSIC_DEV) {
		pr_err("diag: No support for HSIC device %d\n", pdev->id);
		return -EIO;
	}

	ch = &diag_hsic[pdev->id];
	queue_work(ch->hsic_wq, &(ch->close_work));
	return 0;
}

static int diagfwd_hsic_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_hsic_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_hsic_dev_pm_ops = {
	.runtime_suspend = diagfwd_hsic_runtime_suspend,
	.runtime_resume = diagfwd_hsic_runtime_resume,
};

static struct platform_driver msm_hsic_ch_driver = {
	.probe = diag_hsic_probe,
	.remove = diag_hsic_remove,
	.driver = {
		   .name = "diag_bridge",
		   .owner = THIS_MODULE,
		   .pm   = &diagfwd_hsic_dev_pm_ops,
		   },
};

static int hsic_queue_read(int id)
{
	if (id < 0 || id >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n",
				   __func__, id);
		return -EINVAL;
	}
	queue_work(diag_hsic[id].hsic_wq, &(diag_hsic[id].read_work));
	return 0;
}

static int hsic_write(int id, unsigned char *buf, int len, int ctxt)
{
	int err = 0;
	struct diag_hsic_info *ch = NULL;

	if (id < 0 || id >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n",
				   __func__, id);
		return -EINVAL;
	}
	if (!buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, ch %d, invalid buf %pK len %d\n",
				   __func__, id, buf, len);
		return -EINVAL;
	}

	ch = &diag_hsic[id];
	if (!ch->opened || !ch->enabled) {
		pr_debug_ratelimited("diag: In %s, ch %d is disabled. opened %d enabled: %d\n",
				     __func__, id, ch->opened, ch->enabled);
		return -EIO;
	}

	err = diag_bridge_write(ch->id, buf, len);
	if (err) {
		pr_err_ratelimited("diag: cannot write to HSIC ch %d, err: %d\n",
				   ch->id, err);
	}
	return err;
}

static int hsic_fwd_complete(int id, unsigned char *buf, int len, int ctxt)
{
	if (id < 0 || id >= NUM_HSIC_DEV) {
		pr_err_ratelimited("diag: In %s, invalid index %d\n",
				   __func__, id);
		return -EINVAL;
	}
	if (!buf)
		return -EIO;
	diagmem_free(driver, buf, diag_hsic[id].mempool);
	queue_work(diag_hsic[id].hsic_wq, &(diag_hsic[id].read_work));
	return 0;
}

static struct diag_remote_dev_ops diag_hsic_fwd_ops = {
	.open = hsic_open,
	.close = hsic_close,
	.queue_read = hsic_queue_read,
	.write = hsic_write,
	.fwd_complete = hsic_fwd_complete,
	.remote_proc_check = NULL,
};

int diag_hsic_init(void)
{
	int i;
	int err = 0;
	struct diag_hsic_info *ch = NULL;
	char wq_name[DIAG_HSIC_NAME_SZ + DIAG_HSIC_STRING_SZ];

	for (i = 0; i < NUM_HSIC_DEV; i++) {
		ch = &diag_hsic[i];
		spin_lock_init(&ch->lock);
		INIT_WORK(&(ch->read_work), hsic_read_work_fn);
		INIT_WORK(&(ch->open_work), hsic_open_work_fn);
		INIT_WORK(&(ch->close_work), hsic_close_work_fn);
		strlcpy(wq_name, "DIAG_HSIC_", sizeof(wq_name));
		strlcat(wq_name, ch->name, sizeof(wq_name));
		ch->hsic_wq = create_singlethread_workqueue(wq_name);
		if (!ch->hsic_wq)
			goto fail;
		err = diagfwd_bridge_register(ch->dev_id, ch->id,
					      &diag_hsic_fwd_ops);
		if (err) {
			pr_err("diag: Unable to register HSIC channel %d with bridge, err: %d\n",
			       i, err);
			goto fail;
		}
	}

	err = platform_driver_register(&msm_hsic_ch_driver);
	if (err) {
		pr_err("diag: could not register HSIC device, err: %d\n", err);
		goto fail;
	}

	return 0;
fail:
	diag_hsic_exit();
	return -ENOMEM;
}

void diag_hsic_exit(void)
{
	int i;
	struct diag_hsic_info *ch = NULL;

	for (i = 0; i < NUM_HSIC_DEV; i++) {
		ch = &diag_hsic[i];
		ch->enabled = 0;
		ch->opened = 0;
		ch->suspended = 0;
		if (ch->hsic_wq)
			destroy_workqueue(ch->hsic_wq);
	}
	platform_driver_unregister(&msm_hsic_ch_driver);
}

