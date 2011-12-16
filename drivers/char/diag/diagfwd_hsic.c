/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <asm/current.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_hsic.h"

static void diag_read_hsic_work_fn(struct work_struct *work)
{
	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	/*
	 * If there is no hsic data being read from the hsic and there
	 * is no hsic data being written to the usb mdm channel
	 */
	if (!driver->in_busy_hsic_read && !driver->in_busy_hsic_write_on_mdm) {
		/*
		 * Initiate the read from the hsic.  The hsic read is
		 * asynchronous.  Once the read is complete the read
		 * callback function will be called.
		 */
		int err;
		driver->in_busy_hsic_read = 1;
		APPEND_DEBUG('i');
		err = diag_bridge_read((char *)driver->buf_in_hsic,
					IN_BUF_SIZE);
		if (err) {
			pr_err("DIAG: Error initiating HSIC read, err: %d\n",
				err);
			/*
			 * If the error is recoverable, then clear
			 * the read flag, so we will resubmit a
			 * read on the next frame.  Otherwise, don't
			 * resubmit a read on the next frame.
			 */
			if ((-ESHUTDOWN) != err)
				driver->in_busy_hsic_read = 0;
		}
	}

	/*
	 * If for some reason there was no hsic data, set up
	 * the next read
	 */
	if (!driver->in_busy_hsic_read)
		queue_work(driver->diag_hsic_wq, &driver->diag_read_hsic_work);
}

static void diag_hsic_read_complete_callback(void *ctxt, char *buf,
					int buf_size, int actual_size)
{
	/* The read of the data from the HSIC bridge is complete */
	driver->in_busy_hsic_read = 0;

	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	APPEND_DEBUG('j');
	if (actual_size > 0) {
		if (!buf) {
			pr_err("Out of diagmem for HSIC\n");
		} else {
			driver->write_ptr_mdm->length = actual_size;
			/*
			 * Set flag to denote hsic data is currently
			 * being written to the usb mdm channel.
			 * driver->buf_in_hsic was given to
			 * diag_bridge_read(), so buf here should be
			 * driver->buf_in_hsic
			 */
			driver->in_busy_hsic_write_on_mdm = 1;
			diag_device_write((void *)buf, HSIC_DATA,
						driver->write_ptr_mdm);
		}
	} else {
		pr_err("DIAG in %s: actual_size: %d\n", __func__, actual_size);
	}

	/*
	 * If for some reason there was no hsic data to write to the
	 * mdm channel, set up another read
	 */
	if (!driver->in_busy_hsic_write_on_mdm)
		queue_work(driver->diag_hsic_wq, &driver->diag_read_hsic_work);
}

static void diag_hsic_write_complete_callback(void *ctxt, char *buf,
					int buf_size, int actual_size)
{
	/* The write of the data to the HSIC bridge is complete */
	driver->in_busy_hsic_write = 0;

	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	if (actual_size < 0)
		pr_err("DIAG in %s: actual_size: %d\n", __func__, actual_size);

	queue_work(driver->diag_hsic_wq, &driver->diag_read_mdm_work);
}

static struct diag_bridge_ops hsic_diag_bridge_ops = {
	.ctxt = NULL,
	.read_complete_cb = diag_hsic_read_complete_callback,
	.write_complete_cb = diag_hsic_write_complete_callback,
};

static int diag_hsic_close(void)
{
	if (driver->hsic_device_enabled) {
		driver->hsic_ch = 0;
		if (driver->hsic_device_opened) {
			driver->hsic_device_opened = 0;
			diag_bridge_close();
		}
		pr_debug("DIAG in %s: closed successfully\n", __func__);
	} else {
		pr_debug("DIAG in %s: already closed\n", __func__);
	}

	return 0;
}

/* diagfwd_connect_hsic is called when the USB mdm channel is connected */
static int diagfwd_connect_hsic(void)
{
	int err;

	pr_debug("DIAG in %s\n", __func__);

	err = usb_diag_alloc_req(driver->mdm_ch, N_MDM_WRITE, N_MDM_READ);
	if (err)
		pr_err("DIAG: unable to alloc USB req on mdm ch err:%d\n", err);

	driver->usb_mdm_connected = 1;
	driver->in_busy_hsic_write_on_mdm = 0;
	driver->in_busy_hsic_read_on_mdm = 0;
	driver->in_busy_hsic_write = 0;
	driver->in_busy_hsic_read = 0;

	/* If the hsic (diag_bridge) platform device is not open */
	if (driver->hsic_device_enabled) {
		if (!driver->hsic_device_opened) {
			err = diag_bridge_open(&hsic_diag_bridge_ops);
			if (err) {
				pr_err("DIAG: HSIC channel open error: %d\n",
					err);
			} else {
				pr_info("DIAG: opened HSIC channel\n");
				driver->hsic_device_opened = 1;
			}
		} else {
			pr_info("DIAG: HSIC channel already open\n");
		}

		/*
		 * Turn on communication over usb mdm and hsic, if the hsic
		 * device driver is enabled and opened
		 */
		if (driver->hsic_device_opened)
			driver->hsic_ch = 1;

		/* Poll USB mdm channel to check for data */
		queue_work(driver->diag_hsic_wq, &driver->diag_read_mdm_work);

		/* Poll HSIC channel to check for data */
		queue_work(driver->diag_hsic_wq, &driver->diag_read_hsic_work);
	} else {
		/* The hsic device driver has not yet been enabled */
		pr_info("DIAG: HSIC channel not yet enabled\n");
	}

	return 0;
}

/*
 * diagfwd_disconnect_hsic is called when the USB mdm channel
 * is disconnected
 */
static int diagfwd_disconnect_hsic(void)
{
	pr_debug("DIAG in %s\n", __func__);

	driver->usb_mdm_connected = 0;
	usb_diag_free_req(driver->mdm_ch);
	driver->in_busy_hsic_write_on_mdm = 1;
	driver->in_busy_hsic_read_on_mdm = 1;
	driver->in_busy_hsic_write = 1;
	driver->in_busy_hsic_read = 1;

	/* Turn off communication over usb mdm and hsic */
	driver->hsic_ch = 0;

	return 0;
}

/*
 * diagfwd_write_complete_hsic is called after the asynchronous
 * usb_diag_write() on mdm channel is complete
 */
static int diagfwd_write_complete_hsic(void)
{
	/*
	 * Clear flag to denote that the write of the hsic data on the
	 * usb mdm channel is complete
	 */
	driver->in_busy_hsic_write_on_mdm = 0;

	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return 0;
	}

	APPEND_DEBUG('q');

	/* Read data from the hsic */
	queue_work(driver->diag_hsic_wq, &driver->diag_read_hsic_work);

	return 0;
}

/* Called after the asychronous usb_diag_read() on mdm channel is complete */
static int diagfwd_read_complete_hsic(struct diag_request *diag_read_ptr)
{
	/* The read of the usb driver on the mdm (not hsic) has completed */
	driver->in_busy_hsic_read_on_mdm = 0;
	driver->read_len_mdm = diag_read_ptr->actual;

	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return 0;
	}

	/*
	 * The read of the usb driver on the mdm channel has completed.
	 * If there is no write on the hsic in progress, check if the
	 * read has data to pass on to the hsic. If so, pass the usb
	 * mdm data on to the hsic.
	 */
	if (!driver->in_busy_hsic_write && driver->usb_buf_mdm_out &&
		(driver->read_len_mdm > 0)) {

		/*
		 * Initiate the hsic write. The hsic write is
		 * asynchronous. When complete the write
		 * complete callback function will be called
		 */
		int err;
		driver->in_busy_hsic_write = 1;
		err = diag_bridge_write(driver->usb_buf_mdm_out,
					driver->read_len_mdm);
		if (err) {
			pr_err("DIAG: mdm data on hsic write err: %d\n", err);
			/*
			 * If the error is recoverable, then clear
			 * the write flag, so we will resubmit a
			 * write on the next frame.  Otherwise, don't
			 * resubmit a write on the next frame.
			 */
			if ((-ESHUTDOWN) != err)
				driver->in_busy_hsic_write = 0;
		}
	}

	/*
	 * If there is no write of the usb mdm data on the
	 * hsic channel
	 */
	if (!driver->in_busy_hsic_write)
		queue_work(driver->diag_hsic_wq, &driver->diag_read_mdm_work);

	return 0;
}

static void diagfwd_hsic_notifier(void *priv, unsigned event,
					struct diag_request *d_req)
{
	switch (event) {
	case USB_DIAG_CONNECT:
		diagfwd_connect_hsic();
		break;
	case USB_DIAG_DISCONNECT:
		diagfwd_disconnect_hsic();
		break;
	case USB_DIAG_READ_DONE:
		diagfwd_read_complete_hsic(d_req);
		break;
	case USB_DIAG_WRITE_DONE:
		diagfwd_write_complete_hsic();
		break;
	default:
		pr_err("DIAG in %s: Unknown event from USB diag:%u\n",
			__func__, event);
		break;
	}
}

static void diag_read_mdm_work_fn(struct work_struct *work)
{
	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	/*
	 * If there is no data being read from the usb mdm channel
	 * and there is no mdm channel data currently being written
	 * to the hsic
	 */
	if (!driver->in_busy_hsic_read_on_mdm && !driver->in_busy_hsic_write) {
		APPEND_DEBUG('x');

		/* Setup the next read from usb mdm channel */
		driver->in_busy_hsic_read_on_mdm = 1;
		driver->usb_read_mdm_ptr->buf = driver->usb_buf_mdm_out;
		driver->usb_read_mdm_ptr->length = USB_MAX_OUT_BUF;
		usb_diag_read(driver->mdm_ch, driver->usb_read_mdm_ptr);
		APPEND_DEBUG('y');
	}

	/*
	 * If for some reason there was no mdm channel read initiated,
	 * queue up the reading of data from the mdm channel
	 */
	if (!driver->in_busy_hsic_read_on_mdm)
		queue_work(driver->diag_hsic_wq, &driver->diag_read_mdm_work);
}

int diag_hsic_enable(void)
{
	pr_debug("DIAG in %s\n", __func__);

	driver->read_len_mdm = 0;
	if (driver->buf_in_hsic == NULL)
		driver->buf_in_hsic = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
	if (driver->buf_in_hsic == NULL)
		goto err;
	if (driver->usb_buf_mdm_out  == NULL)
		driver->usb_buf_mdm_out = kzalloc(USB_MAX_OUT_BUF, GFP_KERNEL);
	if (driver->usb_buf_mdm_out == NULL)
		goto err;
	if (driver->write_ptr_mdm == NULL)
		driver->write_ptr_mdm = kzalloc(
		sizeof(struct diag_request), GFP_KERNEL);
	if (driver->write_ptr_mdm == NULL)
		goto err;
	if (driver->usb_read_mdm_ptr == NULL)
		driver->usb_read_mdm_ptr = kzalloc(
		sizeof(struct diag_request), GFP_KERNEL);
	if (driver->usb_read_mdm_ptr == NULL)
		goto err;
	driver->diag_hsic_wq = create_singlethread_workqueue("diag_hsic_wq");
#ifdef CONFIG_DIAG_OVER_USB
	INIT_WORK(&(driver->diag_read_mdm_work), diag_read_mdm_work_fn);
#endif
	INIT_WORK(&(driver->diag_read_hsic_work), diag_read_hsic_work_fn);

	driver->hsic_device_enabled = 1;

	return 0;
err:
	pr_err("DIAG could not initialize buf for HSIC\n");
	kfree(driver->buf_in_hsic);
	kfree(driver->usb_buf_mdm_out);
	kfree(driver->write_ptr_mdm);
	kfree(driver->usb_read_mdm_ptr);
	if (driver->diag_hsic_wq)
		destroy_workqueue(driver->diag_hsic_wq);

	return -ENOMEM;
}

static int diag_hsic_probe(struct platform_device *pdev)
{
	int err;

	if (!driver->hsic_device_enabled) {
		err = diag_hsic_enable();
		if (err) {
			pr_err("DIAG could not enable HSIC, err: %d\n", err);
			return err;
		}
	}

	/* The hsic (diag_bridge) platform device driver is enabled */
	err = diag_bridge_open(&hsic_diag_bridge_ops);
	if (err) {
		pr_err("DIAG could not open HSIC channel, err: %d\n", err);
		driver->hsic_device_opened = 0;
		return err;
	}

	pr_info("DIAG opened HSIC channel\n");
	driver->hsic_device_opened = 1;

	/*
	 * The probe function was called after the usb was connected
	 * on the legacy channel. Communication over usb mdm and hsic
	 * needs to be turned on.
	 */
	if (driver->usb_connected) {
		driver->hsic_ch = 1;
		driver->in_busy_hsic_write_on_mdm = 0;
		driver->in_busy_hsic_read_on_mdm = 0;
		driver->in_busy_hsic_write = 0;
		driver->in_busy_hsic_read = 0;

		/* Poll USB mdm channel to check for data */
		queue_work(driver->diag_hsic_wq, &driver->diag_read_mdm_work);

		/* Poll HSIC channel to check for data */
		queue_work(driver->diag_hsic_wq, &driver->diag_read_hsic_work);
	}

	return err;
}

static int diag_hsic_remove(struct platform_device *pdev)
{
	pr_info("DIAG: %s called\n", __func__);
	diag_hsic_close();
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


void __init diagfwd_hsic_init(void)
{
	int ret;

	pr_debug("DIAG in %s\n", __func__);

#ifdef CONFIG_DIAG_OVER_USB
	driver->mdm_ch = usb_diag_open(DIAG_MDM, driver, diagfwd_hsic_notifier);
	if (IS_ERR(driver->mdm_ch)) {
		pr_err("DIAG Unable to open USB diag MDM channel\n");
		goto err;
	}
#endif
	ret = platform_driver_register(&msm_hsic_ch_driver);
	if (ret)
		pr_err("DIAG could not register HSIC device, ret: %d\n", ret);
	else
		driver->hsic_initialized = 1;

	return;
err:
	pr_err("DIAG could not initialize for HSIC execution\n");
}

void __exit diagfwd_hsic_exit(void)
{
	pr_debug("DIAG in %s\n", __func__);

	if (driver->hsic_initialized)
		diag_hsic_close();

#ifdef CONFIG_DIAG_OVER_USB
	if (driver->usb_mdm_connected)
		usb_diag_free_req(driver->mdm_ch);
#endif
	platform_driver_unregister(&msm_hsic_ch_driver);
#ifdef CONFIG_DIAG_OVER_USB
	usb_diag_close(driver->mdm_ch);
#endif
	kfree(driver->buf_in_hsic);
	kfree(driver->usb_buf_mdm_out);
	kfree(driver->write_ptr_mdm);
	kfree(driver->usb_read_mdm_ptr);
	destroy_workqueue(driver->diag_hsic_wq);

	driver->hsic_device_enabled = 0;
}
