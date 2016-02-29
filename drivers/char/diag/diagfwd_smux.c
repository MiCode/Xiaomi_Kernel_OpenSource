/* Copyright (c) 2012,2014 The Linux Foundation. All rights reserved.
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
#include <linux/termios.h>
#include <linux/slab.h>
#include <linux/diagchar.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/usb/usbdiag.h>

#include "diagchar.h"
#include "diagfwd_bridge.h"
#include "diagfwd_smux.h"

struct diag_smux_info diag_smux[NUM_SMUX_DEV] = {
	{
		.id = SMUX_1,
		.lcid = SMUX_USB_DIAG_0,
		.dev_id = DIAGFWD_SMUX,
		.name = "SMUX_1",
		.read_buf = NULL,
		.read_len = 0,
		.in_busy = 0,
		.enabled = 0,
		.opened = 0,
	},
};

static void diag_smux_event(void *priv, int event_type, const void *metadata)
{
	int len = 0;
	int id = (int)priv;
	unsigned char *rx_buf = NULL;
	struct diag_smux_info *ch = NULL;

	if (id < 0 || id >= NUM_SMUX_DEV)
		return;

	ch = &diag_smux[id];
	if (metadata) {
		len = ((struct smux_meta_read *)metadata)->len;
		rx_buf = ((struct smux_meta_read *)metadata)->buffer;
	}

	switch (event_type) {
	case SMUX_CONNECTED:
		pr_info("diag: SMUX_CONNECTED received, ch: %d\n", ch->id);
		ch->opened = 1;
		ch->in_busy = 0;
		break;
	case SMUX_DISCONNECTED:
		ch->opened = 0;
		msm_smux_close(ch->lcid);
		pr_info("diag: SMUX_DISCONNECTED received, ch: %d\n", ch->id);
		break;
	case SMUX_WRITE_DONE:
		pr_debug("diag: SMUX Write done, ch: %d\n", ch->id);
		diag_remote_dev_write_done(ch->dev_id, rx_buf, len, ch->id);
		break;
	case SMUX_WRITE_FAIL:
		pr_info("diag: SMUX Write Failed, ch: %d\n", ch->id);
		break;
	case SMUX_READ_FAIL:
		pr_info("diag: SMUX Read Failed, ch: %d\n", ch->id);
		break;
	case SMUX_READ_DONE:
		ch->read_buf = rx_buf;
		ch->read_len = len;
		ch->in_busy = 1;
		diag_remote_dev_read_done(ch->dev_id, ch->read_buf,
					  ch->read_len);
		break;
	};
}

static int diag_smux_init_ch(struct diag_smux_info *ch)
{
	if (!ch)
		return -EINVAL;

	if (!ch->enabled) {
		pr_debug("diag: SMUX channel is not enabled id: %d\n", ch->id);
		return -ENODEV;
	}

	if (ch->inited) {
		pr_debug("diag: SMUX channel %d is already initialize\n",
			 ch->id);
		return 0;
	}

	ch->read_buf = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
	if (!ch->read_buf)
		return -ENOMEM;

	ch->inited = 1;

	return 0;
}

static int smux_get_rx_buffer(void *priv, void **pkt_priv, void **buf,
			      int size)
{
	int id = (int)priv;
	struct diag_smux_info *ch = NULL;

	if (id < 0 || id >= NUM_SMUX_DEV)
		return -EINVAL;

	ch = &diag_smux[id];

	if (ch->in_busy) {
		pr_debug("diag: read buffer for SMUX is BUSY\n");
		return -EAGAIN;
	}

	*pkt_priv = (void *)0x1234;
	*buf = ch->read_buf;
	ch->in_busy = 1;
	return 0;
}

static int smux_open(int id)
{
	int err = 0;
	struct diag_smux_info *ch = NULL;

	if (id < 0 || id >= NUM_SMUX_DEV)
		return -EINVAL;

	ch = &diag_smux[id];
	if (ch->opened) {
		pr_debug("diag: SMUX channel %d is already connected\n",
			 ch->id);
		return 0;
	}

	err = diag_smux_init_ch(ch);
	if (err) {
		pr_err("diag: Unable to initialize SMUX channel %d, err: %d\n",
		       ch->id, err);
		return err;
	}

	err = msm_smux_open(ch->lcid, (void *)ch->id, diag_smux_event,
			    smux_get_rx_buffer);
	if (err) {
		pr_err("diag: failed to open SMUX ch %d, err: %d\n",
		       ch->id, err);
		return err;
	}
	msm_smux_tiocm_set(ch->lcid, TIOCM_DTR, 0);
	ch->opened = 1;
	pr_info("diag: SMUX ch %d is connected\n", ch->id);
	return 0;
}

static int smux_close(int id)
{
	struct diag_smux_info *ch = NULL;

	if (id < 0 || id >= NUM_SMUX_DEV)
		return -EINVAL;

	ch = &diag_smux[id];
	if (!ch->enabled) {
		pr_debug("diag: SMUX channel is not enabled id: %d\n", ch->id);
		return -ENODEV;
	}

	msm_smux_close(ch->lcid);
	ch->opened = 0;
	ch->in_busy = 1;
	kfree(ch->read_buf);
	ch->read_buf = NULL;
	return 0;
}

static int smux_queue_read(int id)
{
	return 0;
}

static int smux_write(int id, unsigned char *buf, int len, int ctxt)
{
	struct diag_smux_info *ch = NULL;

	if (id < 0 || id >= NUM_SMUX_DEV)
		return -EINVAL;

	ch = &diag_smux[id];
	return  msm_smux_write(ch->lcid, NULL, buf, len);
}

static int smux_fwd_complete(int id, unsigned char *buf, int len, int ctxt)
{
	if (id < 0 || id >= NUM_SMUX_DEV)
		return -EINVAL;

	diag_smux[id].in_busy = 0;
	return 0;
}

static int diagfwd_smux_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_smux_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_smux_dev_pm_ops = {
	.runtime_suspend = diagfwd_smux_runtime_suspend,
	.runtime_resume = diagfwd_smux_runtime_resume,
};

static int diagfwd_smux_probe(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;

	pr_debug("diag: SMUX probe called, pdev->id: %d\n", pdev->id);
	if (pdev->id < 0 || pdev->id >= NUM_SMUX_DEV) {
		pr_err("diag: No support for SMUX device %d\n", pdev->id);
		return -EINVAL;
	}

	diag_smux[pdev->id].enabled = 1;
	return smux_open(pdev->id);
}

static int diagfwd_smux_remove(struct platform_device *pdev)
{
	if (!pdev)
		return -EINVAL;

	pr_debug("diag: SMUX probe called, pdev->id: %d\n", pdev->id);
	if (pdev->id < 0 || pdev->id >= NUM_SMUX_DEV) {
		pr_err("diag: No support for SMUX device %d\n", pdev->id);
		return -EINVAL;
	}
	if (!diag_smux[pdev->id].enabled) {
		pr_err("diag: SMUX channel %d is not enabled\n",
		       diag_smux[pdev->id].id);
		return -ENODEV;
	}
	return smux_close(pdev->id);
}

static struct platform_driver msm_diagfwd_smux_driver = {
	.probe = diagfwd_smux_probe,
	.remove = diagfwd_smux_remove,
	.driver = {
		   .name = "SMUX_DIAG",
		   .owner = THIS_MODULE,
		   .pm   = &diagfwd_smux_dev_pm_ops,
		   },
};

static struct diag_remote_dev_ops diag_smux_fwd_ops = {
	.open = smux_open,
	.close = smux_close,
	.queue_read = smux_queue_read,
	.write = smux_write,
	.fwd_complete = smux_fwd_complete,
};

int diag_smux_init()
{
	int i;
	int err = 0;
	struct diag_smux_info *ch = NULL;
	char wq_name[DIAG_SMUX_NAME_SZ + 11];

	for (i = 0; i < NUM_SMUX_DEV; i++) {
		ch = &diag_smux[i];
		strlcpy(wq_name, "DIAG_SMUX_", 11);
		strlcat(wq_name, ch->name, sizeof(ch->name));
		ch->smux_wq = create_singlethread_workqueue(wq_name);
		if (!ch->smux_wq) {
			err = -ENOMEM;
			goto fail;
		}
		err = diagfwd_bridge_register(ch->dev_id, ch->id,
					      &diag_smux_fwd_ops);
		if (err) {
			pr_err("diag: Unable to register SMUX ch %d with bridge\n",
			       ch->id);
			goto fail;
		}
	}

	err = platform_driver_register(&msm_diagfwd_smux_driver);
	if (err) {
		pr_err("diag: Unable to register SMUX device, err: %d\n", err);
		goto fail;
	}

	return 0;
fail:
	diag_smux_exit();
	return err;
}

void diag_smux_exit()
{
	int i;
	struct diag_smux_info *ch = NULL;
	for (i = 0; i < NUM_SMUX_DEV; i++) {
		ch = &diag_smux[i];
		kfree(ch->read_buf);
		ch->read_buf = NULL;
		ch->enabled = 0;
		ch->opened = 0;
		ch->read_len = 0;
	}
	platform_driver_unregister(&msm_diagfwd_smux_driver);
}
