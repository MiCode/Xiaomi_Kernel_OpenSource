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
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/smux.h>
#include <asm/current.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_hsic.h"
#include "diagfwd_smux.h"

#define READ_HSIC_BUF_SIZE 2048

static void diag_read_hsic_work_fn(struct work_struct *work)
{
	unsigned char *buf_in_hsic = NULL;
	int num_reads_submitted = 0;
	int err = 0;
	int write_ptrs_available;

	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	/*
	 * Determine the current number of available buffers for writing after
	 * reading from the HSIC has completed.
	 */
	if (driver->logging_mode == MEMORY_DEVICE_MODE)
		write_ptrs_available = driver->poolsize_hsic_write -
					driver->num_hsic_buf_tbl_entries;
	else
		write_ptrs_available = driver->poolsize_hsic_write -
					driver->count_hsic_write_pool;

	/*
	 * Queue up a read on the HSIC for all available buffers in the
	 * pool, exhausting the pool.
	 */
	do {
		/*
		 * If no more write buffers are available,
		 * stop queuing reads
		 */
		if (write_ptrs_available <= 0)
			break;

		write_ptrs_available--;

		buf_in_hsic = diagmem_alloc(driver, READ_HSIC_BUF_SIZE,
							POOL_TYPE_HSIC);
		if (buf_in_hsic) {
			/*
			 * Initiate the read from the hsic.  The hsic read is
			 * asynchronous.  Once the read is complete the read
			 * callback function will be called.
			 */
			pr_debug("diag: read from HSIC\n");
			num_reads_submitted++;
			err = diag_bridge_read((char *)buf_in_hsic,
							READ_HSIC_BUF_SIZE);
			if (err) {
				num_reads_submitted--;

				/* Return the buffer to the pool */
				diagmem_free(driver, buf_in_hsic,
						POOL_TYPE_HSIC);

				pr_err_ratelimited("diag: Error initiating HSIC read, err: %d\n",
					err);
				/*
				 * An error occurred, discontinue queuing
				 * reads
				 */
				break;
			}
		}
	} while (buf_in_hsic);

	/*
	 * If there are read buffers available and for some reason the
	 * read was not queued, and if no unrecoverable error occurred
	 * (-ENODEV is an unrecoverable error), then set up the next read
	 */
	if ((driver->count_hsic_pool < driver->poolsize_hsic) &&
		(num_reads_submitted == 0) && (err != -ENODEV))
		queue_work(driver->diag_bridge_wq,
				 &driver->diag_read_hsic_work);
}

static void diag_hsic_read_complete_callback(void *ctxt, char *buf,
					int buf_size, int actual_size)
{
	int err = -2;

	if (!driver->hsic_ch) {
		/*
		 * The hsic channel is closed. Return the buffer to
		 * the pool.  Do not send it on.
		 */
		diagmem_free(driver, buf, POOL_TYPE_HSIC);
		pr_debug("diag: In %s: driver->hsic_ch == 0, actual_size: %d\n",
			__func__, actual_size);
		return;
	}

	/*
	 * Note that zero length is valid and still needs to be sent to
	 * the USB only when we are logging data to the USB
	 */
	if ((actual_size > 0) ||
		((actual_size == 0) && (driver->logging_mode == USB_MODE))) {
		if (!buf) {
			pr_err("diag: Out of diagmem for HSIC\n");
		} else {
			/*
			 * Send data in buf to be written on the
			 * appropriate device, e.g. USB MDM channel
			 */
			driver->write_len_mdm = actual_size;
			err = diag_device_write((void *)buf, HSIC_DATA, NULL);
			/* If an error, return buffer to the pool */
			if (err) {
				diagmem_free(driver, buf, POOL_TYPE_HSIC);
				pr_err("diag: In %s, error calling diag_device_write, err: %d\n",
					__func__, err);
			}
		}
	} else {
		/*
		 * The buffer has an error status associated with it. Do not
		 * pass it on. Note that -ENOENT is sent when the diag bridge
		 * is closed.
		 */
		diagmem_free(driver, buf, POOL_TYPE_HSIC);
		pr_debug("diag: In %s: error status: %d\n", __func__,
			actual_size);
	}

	/*
	 * If for some reason there was no hsic data to write to the
	 * mdm channel, set up another read
	 */
	if (err &&
		((driver->logging_mode == MEMORY_DEVICE_MODE) ||
		(driver->usb_mdm_connected && !driver->hsic_suspend))) {
		queue_work(driver->diag_bridge_wq,
				 &driver->diag_read_hsic_work);
	}
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

	if (driver->usb_mdm_connected)
		queue_work(driver->diag_bridge_wq, &driver->diag_read_mdm_work);
}

static int diag_hsic_suspend(void *ctxt)
{
	pr_debug("diag: hsic_suspend\n");

	/* Don't allow suspend if a write in the HSIC is in progress */
	if (driver->in_busy_hsic_write)
		return -EBUSY;

	/* Don't allow suspend if in MEMORY_DEVICE_MODE */
	if (driver->logging_mode == MEMORY_DEVICE_MODE)
		return -EBUSY;

	driver->hsic_suspend = 1;

	return 0;
}

static void diag_hsic_resume(void *ctxt)
{
	pr_debug("diag: hsic_resume\n");
	driver->hsic_suspend = 0;

	if ((driver->count_hsic_pool < driver->poolsize_hsic) &&
		((driver->logging_mode == MEMORY_DEVICE_MODE) ||
				(driver->usb_mdm_connected)))
		queue_work(driver->diag_bridge_wq,
			 &driver->diag_read_hsic_work);
}

static struct diag_bridge_ops hsic_diag_bridge_ops = {
	.ctxt = NULL,
	.read_complete_cb = diag_hsic_read_complete_callback,
	.write_complete_cb = diag_hsic_write_complete_callback,
	.suspend = diag_hsic_suspend,
	.resume = diag_hsic_resume,
};

static void diag_hsic_close(void)
{
	if (driver->hsic_device_enabled) {
		driver->hsic_ch = 0;
		if (driver->hsic_device_opened) {
			driver->hsic_device_opened = 0;
			diag_bridge_close();
			pr_debug("diag: %s: closed successfully\n", __func__);
		} else {
			pr_debug("diag: %s: already closed\n", __func__);
		}
	} else {
		pr_debug("diag: %s: HSIC device already removed\n", __func__);
	}
}

/* diagfwd_cancel_hsic is called to cancel outstanding read/writes */
int diagfwd_cancel_hsic(void)
{
	int err;

	mutex_lock(&driver->bridge_mutex);
	if (driver->hsic_device_enabled) {
		if (driver->hsic_device_opened) {
			driver->hsic_ch = 0;
			driver->hsic_device_opened = 0;
			diag_bridge_close();
			err = diag_bridge_open(&hsic_diag_bridge_ops);
			if (err) {
				pr_err("diag: HSIC channel open error: %d\n",
					err);
			} else {
				pr_debug("diag: opened HSIC channel\n");
				driver->hsic_device_opened = 1;
				driver->hsic_ch = 1;
			}
		}
	}

	mutex_unlock(&driver->bridge_mutex);
	return 0;
}

/* diagfwd_connect_bridge is called when the USB mdm channel is connected */
int diagfwd_connect_bridge(int process_cable)
{
	int err;

	pr_debug("diag: in %s\n", __func__);

	mutex_lock(&driver->bridge_mutex);
	/* If the usb cable is being connected */
	if (process_cable) {
		err = usb_diag_alloc_req(driver->mdm_ch, N_MDM_WRITE,
			N_MDM_READ);
		if (err)
			pr_err("diag: unable to alloc USB req on mdm"
				" ch err:%d\n", err);

		driver->usb_mdm_connected = 1;
	}

	if (driver->hsic_device_enabled) {
		driver->in_busy_hsic_read_on_device = 0;
		driver->in_busy_hsic_write = 0;
	} else if (driver->diag_smux_enabled) {
		driver->in_busy_smux = 0;
		diagfwd_connect_smux();
		mutex_unlock(&driver->bridge_mutex);
		return 0;
	}

	/* If the hsic (diag_bridge) platform device is not open */
	if (driver->hsic_device_enabled) {
		if (!driver->hsic_device_opened) {
			err = diag_bridge_open(&hsic_diag_bridge_ops);
			if (err) {
				pr_err("diag: HSIC channel open error: %d\n",
					err);
			} else {
				pr_debug("diag: opened HSIC channel\n");
				driver->hsic_device_opened = 1;
			}
		} else {
			pr_debug("diag: HSIC channel already open\n");
		}

		/*
		 * Turn on communication over usb mdm and hsic, if the hsic
		 * device driver is enabled and opened
		 */
		if (driver->hsic_device_opened) {
			driver->hsic_ch = 1;

			/* Poll USB mdm channel to check for data */
			if (driver->logging_mode == USB_MODE)
				queue_work(driver->diag_bridge_wq,
						&driver->diag_read_mdm_work);

			/* Poll HSIC channel to check for data */
			queue_work(driver->diag_bridge_wq,
					 &driver->diag_read_hsic_work);
		}
	} else {
		/* The hsic device driver has not yet been enabled */
		pr_info("diag: HSIC channel not yet enabled\n");
	}

	mutex_unlock(&driver->bridge_mutex);
	return 0;
}

/*
 * diagfwd_disconnect_bridge is called when the USB mdm channel
 * is disconnected
 */
int diagfwd_disconnect_bridge(int process_cable)
{
	pr_debug("diag: In %s, process_cable: %d\n", __func__, process_cable);

	mutex_lock(&driver->bridge_mutex);

	/* If the usb cable is being disconnected */
	if (process_cable) {
		driver->usb_mdm_connected = 0;
		usb_diag_free_req(driver->mdm_ch);
	}

	if (driver->hsic_device_enabled &&
		driver->logging_mode != MEMORY_DEVICE_MODE) {
		driver->in_busy_hsic_read_on_device = 1;
		driver->in_busy_hsic_write = 1;
		/* Turn off communication over usb mdm and hsic */
		diag_hsic_close();
	} else if (driver->diag_smux_enabled &&
		driver->logging_mode == USB_MODE) {
		driver->in_busy_smux = 1;
		driver->lcid = LCID_INVALID;
		driver->smux_connected = 0;
		/* Turn off communication over usb mdm and smux */
		msm_smux_close(LCID_VALID);
	}

	mutex_unlock(&driver->bridge_mutex);
	return 0;
}

/*
 * diagfwd_write_complete_hsic is called after the asynchronous
 * usb_diag_write() on mdm channel is complete
 */
int diagfwd_write_complete_hsic(struct diag_request *diag_write_ptr)
{
	unsigned char *buf = (diag_write_ptr) ? diag_write_ptr->buf : NULL;

	if (buf) {
		/* Return buffers to their pools */
		diagmem_free(driver, (unsigned char *)buf, POOL_TYPE_HSIC);
		diagmem_free(driver, (unsigned char *)diag_write_ptr,
							POOL_TYPE_HSIC_WRITE);
	}

	if (!driver->hsic_ch) {
		pr_err("diag: In %s: driver->hsic_ch == 0\n", __func__);
		return 0;
	}

	/* Read data from the hsic */
	queue_work(driver->diag_bridge_wq, &driver->diag_read_hsic_work);

	return 0;
}

/* Called after the asychronous usb_diag_read() on mdm channel is complete */
static int diagfwd_read_complete_bridge(struct diag_request *diag_read_ptr)
{
	/* The read of the usb driver on the mdm (not hsic) has completed */
	driver->in_busy_hsic_read_on_device = 0;
	driver->read_len_mdm = diag_read_ptr->actual;

	if (driver->diag_smux_enabled) {
		diagfwd_read_complete_smux();
		return 0;
	}
	/* If SMUX not enabled, check for HSIC */
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
			pr_err_ratelimited("diag: mdm data on hsic write err: %d\n",
					err);
			/*
			 * If the error is recoverable, then clear
			 * the write flag, so we will resubmit a
			 * write on the next frame.  Otherwise, don't
			 * resubmit a write on the next frame.
			 */
			if ((-ENODEV) != err)
				driver->in_busy_hsic_write = 0;
		}
	}

	/*
	 * If there is no write of the usb mdm data on the
	 * hsic channel
	 */
	if (!driver->in_busy_hsic_write)
		queue_work(driver->diag_bridge_wq, &driver->diag_read_mdm_work);

	return 0;
}

static void diagfwd_bridge_notifier(void *priv, unsigned event,
					struct diag_request *d_req)
{
	switch (event) {
	case USB_DIAG_CONNECT:
		diagfwd_connect_bridge(1);
		break;
	case USB_DIAG_DISCONNECT:
		queue_work(driver->diag_bridge_wq,
			 &driver->diag_disconnect_work);
		break;
	case USB_DIAG_READ_DONE:
		queue_work(driver->diag_bridge_wq,
				&driver->diag_usb_read_complete_work);
		break;
	case USB_DIAG_WRITE_DONE:
		if (driver->hsic_device_enabled)
			diagfwd_write_complete_hsic(d_req);
		else if (driver->diag_smux_enabled)
			diagfwd_write_complete_smux();
		break;
	default:
		pr_err("diag: in %s: Unknown event from USB diag:%u\n",
			__func__, event);
		break;
	}
}

static void diag_usb_read_complete_fn(struct work_struct *w)
{
	diagfwd_read_complete_bridge(driver->usb_read_mdm_ptr);
}

static void diag_disconnect_work_fn(struct work_struct *w)
{
	diagfwd_disconnect_bridge(1);
}

static void diag_read_mdm_work_fn(struct work_struct *work)
{
	int ret;
	if (driver->diag_smux_enabled) {
		if (driver->lcid && driver->usb_buf_mdm_out &&
				(driver->read_len_mdm > 0) &&
				driver->smux_connected) {
			ret = msm_smux_write(driver->lcid,  NULL,
		 driver->usb_buf_mdm_out, driver->read_len_mdm);
			if (ret)
				pr_err("diag: writing to SMUX ch, r = %d,"
					"lcid = %d\n", ret, driver->lcid);
		}
		driver->usb_read_mdm_ptr->buf = driver->usb_buf_mdm_out;
		driver->usb_read_mdm_ptr->length = USB_MAX_OUT_BUF;
		usb_diag_read(driver->mdm_ch, driver->usb_read_mdm_ptr);
		return;
	}

	/* if SMUX not enabled, check for HSIC */
	if (!driver->hsic_ch) {
		pr_err("DIAG in %s: driver->hsic_ch == 0\n", __func__);
		return;
	}

	/*
	 * If there is no data being read from the usb mdm channel
	 * and there is no mdm channel data currently being written
	 * to the hsic
	 */
	if (!driver->in_busy_hsic_read_on_device &&
				 !driver->in_busy_hsic_write) {
		APPEND_DEBUG('x');

		/* Setup the next read from usb mdm channel */
		driver->in_busy_hsic_read_on_device = 1;
		driver->usb_read_mdm_ptr->buf = driver->usb_buf_mdm_out;
		driver->usb_read_mdm_ptr->length = USB_MAX_OUT_BUF;
		usb_diag_read(driver->mdm_ch, driver->usb_read_mdm_ptr);
		APPEND_DEBUG('y');
	}

	/*
	 * If for some reason there was no mdm channel read initiated,
	 * queue up the reading of data from the mdm channel
	 */
	if (!driver->in_busy_hsic_read_on_device)
		queue_work(driver->diag_bridge_wq,
			 &driver->diag_read_mdm_work);
}

static int diag_hsic_probe(struct platform_device *pdev)
{
	int err = 0;
	pr_debug("diag: in %s\n", __func__);
	if (!driver->hsic_inited) {
		diagmem_hsic_init(driver);
		INIT_WORK(&(driver->diag_read_hsic_work),
					 diag_read_hsic_work_fn);
		driver->hsic_inited = 1;
	}

	mutex_lock(&driver->bridge_mutex);

	/*
	 * The probe function was called after the usb was connected
	 * on the legacy channel OR ODL is turned on. Communication over usb
	 * mdm and hsic needs to be turned on.
	 */
	if (driver->usb_mdm_connected || (driver->logging_mode ==
							 MEMORY_DEVICE_MODE)) {
		if (driver->hsic_device_opened) {
			/* should not happen. close it before re-opening */
			pr_warn("diag: HSIC channel already opened in probe\n");
			diag_bridge_close();
		}

		err = diag_bridge_open(&hsic_diag_bridge_ops);
		if (err) {
			pr_err("diag: could not open HSIC, err: %d\n", err);
			driver->hsic_device_opened = 0;
			mutex_unlock(&driver->bridge_mutex);
			return err;
		}

		pr_info("diag: opened HSIC channel\n");
		driver->hsic_device_opened = 1;
		driver->hsic_ch = 1;

		driver->in_busy_hsic_read_on_device = 0;
		driver->in_busy_hsic_write = 0;

		if (driver->usb_mdm_connected) {
			/* Poll USB mdm channel to check for data */
			queue_work(driver->diag_bridge_wq,
					 &driver->diag_read_mdm_work);
		}

		/* Poll HSIC channel to check for data */
		queue_work(driver->diag_bridge_wq,
				 &driver->diag_read_hsic_work);
	}

	/* The hsic (diag_bridge) platform device driver is enabled */
	driver->hsic_device_enabled = 1;
	mutex_unlock(&driver->bridge_mutex);
	return err;
}

static int diag_hsic_remove(struct platform_device *pdev)
{
	pr_debug("diag: %s called\n", __func__);
	mutex_lock(&driver->bridge_mutex);
	diag_hsic_close();
	driver->hsic_device_enabled = 0;
	mutex_unlock(&driver->bridge_mutex);
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

void diagfwd_bridge_init(void)
{
	int ret;

	pr_debug("diag: in %s\n", __func__);
	driver->diag_bridge_wq = create_singlethread_workqueue(
							"diag_bridge_wq");
	driver->read_len_mdm = 0;
	driver->write_len_mdm = 0;
	driver->num_hsic_buf_tbl_entries = 0;
	spin_lock_init(&driver->hsic_spinlock);
	if (driver->usb_buf_mdm_out  == NULL)
		driver->usb_buf_mdm_out = kzalloc(USB_MAX_OUT_BUF,
							 GFP_KERNEL);
	if (driver->usb_buf_mdm_out == NULL)
		goto err;
	/* Only used by smux move to smux probe function */
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

	if (driver->hsic_buf_tbl == NULL)
		driver->hsic_buf_tbl = kzalloc(NUM_HSIC_BUF_TBL_ENTRIES *
				sizeof(struct diag_write_device), GFP_KERNEL);
	if (driver->hsic_buf_tbl == NULL)
		goto err;

	driver->count_hsic_pool = 0;
	driver->count_hsic_write_pool = 0;

	driver->itemsize_hsic = READ_HSIC_BUF_SIZE;
	driver->poolsize_hsic = N_MDM_WRITE;
	driver->itemsize_hsic_write = sizeof(struct diag_request);
	driver->poolsize_hsic_write = N_MDM_WRITE;

	mutex_init(&driver->bridge_mutex);
#ifdef CONFIG_DIAG_OVER_USB
	INIT_WORK(&(driver->diag_read_mdm_work), diag_read_mdm_work_fn);
#endif
	INIT_WORK(&(driver->diag_disconnect_work), diag_disconnect_work_fn);
	INIT_WORK(&(driver->diag_usb_read_complete_work),
			diag_usb_read_complete_fn);
#ifdef CONFIG_DIAG_OVER_USB
	driver->mdm_ch = usb_diag_open(DIAG_MDM, driver,
						 diagfwd_bridge_notifier);
	if (IS_ERR(driver->mdm_ch)) {
		pr_err("diag: Unable to open USB diag MDM channel\n");
		goto err;
	}
#endif
	/* register HSIC device */
	ret = platform_driver_register(&msm_hsic_ch_driver);
	if (ret)
		pr_err("diag: could not register HSIC device, ret: %d\n", ret);
	/* register SMUX device */
	ret = platform_driver_register(&msm_diagfwd_smux_driver);
	if (ret)
		pr_err("diag: could not register SMUX device, ret: %d\n", ret);

	return;
err:
	pr_err("diag: Could not initialize for bridge forwarding\n");
	kfree(driver->usb_buf_mdm_out);
	kfree(driver->hsic_buf_tbl);
	kfree(driver->write_ptr_mdm);
	kfree(driver->usb_read_mdm_ptr);
	if (driver->diag_bridge_wq)
		destroy_workqueue(driver->diag_bridge_wq);

	return;
}

void diagfwd_bridge_exit(void)
{
	pr_debug("diag: in %s\n", __func__);

	if (driver->hsic_device_enabled) {
		diag_hsic_close();
		driver->hsic_device_enabled = 0;
	}
	driver->hsic_inited = 0;
	diagmem_exit(driver, POOL_TYPE_ALL);
	if (driver->diag_smux_enabled) {
		driver->lcid = LCID_INVALID;
		kfree(driver->buf_in_smux);
		driver->diag_smux_enabled = 0;
	}
	platform_driver_unregister(&msm_hsic_ch_driver);
	platform_driver_unregister(&msm_diagfwd_smux_driver);
	/* destroy USB MDM specific variables */
#ifdef CONFIG_DIAG_OVER_USB
	if (driver->usb_mdm_connected)
		usb_diag_free_req(driver->mdm_ch);
	usb_diag_close(driver->mdm_ch);
#endif
	kfree(driver->usb_buf_mdm_out);
	kfree(driver->hsic_buf_tbl);
	kfree(driver->write_ptr_mdm);
	kfree(driver->usb_read_mdm_ptr);
	destroy_workqueue(driver->diag_bridge_wq);
}
