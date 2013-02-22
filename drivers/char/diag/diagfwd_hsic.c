/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include "diagfwd_bridge.h"

#define READ_HSIC_BUF_SIZE 2048
struct diag_hsic_dev *diag_hsic;

static void diag_read_hsic_work_fn(struct work_struct *work)
{
	unsigned char *buf_in_hsic = NULL;
	int num_reads_submitted = 0;
	int err = 0;
	int write_ptrs_available;
	struct diag_hsic_dev *hsic_struct = container_of(work,
				struct diag_hsic_dev, diag_read_hsic_work);
	int index = hsic_struct->id;
	static DEFINE_RATELIMIT_STATE(rl, 10*HZ, 1);

	if (!diag_hsic[index].hsic_ch) {
		pr_err("DIAG in %s: diag_hsic[index].hsic_ch == 0\n", __func__);
		return;
	}

	/*
	 * Determine the current number of available buffers for writing after
	 * reading from the HSIC has completed.
	 */
	if (driver->logging_mode == MEMORY_DEVICE_MODE)
		write_ptrs_available = diag_hsic[index].poolsize_hsic_write -
					diag_hsic[index].
						num_hsic_buf_tbl_entries;
	else
		write_ptrs_available = diag_hsic[index].poolsize_hsic_write -
					diag_hsic[index].count_hsic_write_pool;

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

		/*
		 * No sense queuing a read if the HSIC bridge was
		 * closed in another thread
		 */
		if (!diag_hsic[index].hsic_ch)
			break;

		buf_in_hsic = diagmem_alloc(driver, READ_HSIC_BUF_SIZE,
							index+POOL_TYPE_HSIC);
		if (buf_in_hsic) {
			/*
			 * Initiate the read from the HSIC.  The HSIC read is
			 * asynchronous.  Once the read is complete the read
			 * callback function will be called.
			 */
			pr_debug("diag: read from HSIC\n");
			num_reads_submitted++;
			err = diag_bridge_read(index, (char *)buf_in_hsic,
							READ_HSIC_BUF_SIZE);
			if (err) {
				num_reads_submitted--;

				/* Return the buffer to the pool */
				diagmem_free(driver, buf_in_hsic,
						index+POOL_TYPE_HSIC);

				if (__ratelimit(&rl))
					pr_err("diag: Error initiating HSIC read, err: %d\n",
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
	if ((diag_hsic[index].count_hsic_pool <
		diag_hsic[index].poolsize_hsic) &&
		(num_reads_submitted == 0) && (err != -ENODEV) &&
		(diag_hsic[index].hsic_ch != 0))
		queue_work(diag_bridge[index].wq,
				 &diag_hsic[index].diag_read_hsic_work);
}

static void diag_hsic_read_complete_callback(void *ctxt, char *buf,
					int buf_size, int actual_size)
{
	int err = -2;
	int index = (int)ctxt;
	static DEFINE_RATELIMIT_STATE(rl, 10*HZ, 1);

	if (!diag_hsic[index].hsic_ch) {
		/*
		 * The HSIC channel is closed. Return the buffer to
		 * the pool.  Do not send it on.
		 */
		diagmem_free(driver, buf, index+POOL_TYPE_HSIC);
		pr_debug("diag: In %s: hsic_ch == 0, actual_size: %d\n",
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
			diag_bridge[index].write_len = actual_size;
			err = diag_device_write((void *)buf, index+HSIC_DATA,
									NULL);
			/* If an error, return buffer to the pool */
			if (err) {
				diagmem_free(driver, buf, index +
							POOL_TYPE_HSIC);
				if (__ratelimit(&rl))
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
		diagmem_free(driver, buf, index+POOL_TYPE_HSIC);
		pr_debug("diag: In %s: error status: %d\n", __func__,
			actual_size);
	}

	/*
	 * If for some reason there was no HSIC data to write to the
	 * mdm channel, set up another read
	 */
	if (err &&
		((driver->logging_mode == MEMORY_DEVICE_MODE) ||
		(diag_bridge[index].usb_connected &&
		!diag_hsic[index].hsic_suspend))) {
		queue_work(diag_bridge[index].wq,
				 &diag_hsic[index].diag_read_hsic_work);
	}
}

static void diag_hsic_write_complete_callback(void *ctxt, char *buf,
					int buf_size, int actual_size)
{
	int index = (int)ctxt;

	/* The write of the data to the HSIC bridge is complete */
	diag_hsic[index].in_busy_hsic_write = 0;

	if (!diag_hsic[index].hsic_ch) {
		pr_err("DIAG in %s: hsic_ch == 0, ch = %d\n", __func__, index);
		return;
	}

	if (actual_size < 0)
		pr_err("DIAG in %s: actual_size: %d\n", __func__, actual_size);

	if (diag_bridge[index].usb_connected &&
				 (driver->logging_mode == USB_MODE))
		queue_work(diag_bridge[index].wq,
				 &diag_bridge[index].diag_read_work);
}

static int diag_hsic_suspend(void *ctxt)
{
	int index = (int)ctxt;
	pr_debug("diag: hsic_suspend\n");

	/* Don't allow suspend if a write in the HSIC is in progress */
	if (diag_hsic[index].in_busy_hsic_write)
		return -EBUSY;

	/* Don't allow suspend if in MEMORY_DEVICE_MODE */
	if (driver->logging_mode == MEMORY_DEVICE_MODE)
		return -EBUSY;

	diag_hsic[index].hsic_suspend = 1;

	return 0;
}

static void diag_hsic_resume(void *ctxt)
{
	 int index = (int)ctxt;

	pr_debug("diag: hsic_resume\n");
	diag_hsic[index].hsic_suspend = 0;

	if ((diag_hsic[index].count_hsic_pool <
		diag_hsic[index].poolsize_hsic) &&
		((driver->logging_mode == MEMORY_DEVICE_MODE) ||
				(diag_bridge[index].usb_connected)))
		queue_work(diag_bridge[index].wq,
			 &diag_hsic[index].diag_read_hsic_work);
}

struct diag_bridge_ops hsic_diag_bridge_ops[MAX_HSIC_CH] = {
	{
	.ctxt = NULL,
	.read_complete_cb = diag_hsic_read_complete_callback,
	.write_complete_cb = diag_hsic_write_complete_callback,
	.suspend = diag_hsic_suspend,
	.resume = diag_hsic_resume,
	},
	{
	.ctxt = NULL,
	.read_complete_cb = diag_hsic_read_complete_callback,
	.write_complete_cb = diag_hsic_write_complete_callback,
	.suspend = diag_hsic_suspend,
	.resume = diag_hsic_resume,
	}
};

void diag_hsic_close(int ch_id)
{
	if (diag_hsic[ch_id].hsic_device_enabled) {
		diag_hsic[ch_id].hsic_ch = 0;
		if (diag_hsic[ch_id].hsic_device_opened) {
			diag_hsic[ch_id].hsic_device_opened = 0;
			diag_bridge_close(ch_id);
			pr_debug("diag: %s: closed successfully ch %d\n",
							__func__, ch_id);
		} else {
			pr_debug("diag: %s: already closed ch %d\n",
							__func__, ch_id);
		}
	} else {
		pr_debug("diag: %s: HSIC device already removed ch %d\n",
							__func__, ch_id);
	}
}

/* diagfwd_cancel_hsic is called to cancel outstanding read/writes */
int diagfwd_cancel_hsic(void)
{
	int err, i;

	/* Cancel it for all active HSIC bridges */
	for (i = 0; i < MAX_HSIC_CH; i++) {
		if (!diag_bridge[i].enabled)
			continue;
		mutex_lock(&diag_bridge[i].bridge_mutex);
		if (diag_hsic[i].hsic_device_enabled) {
			if (diag_hsic[i].hsic_device_opened) {
				diag_hsic[i].hsic_ch = 0;
				diag_hsic[i].hsic_device_opened = 0;
				diag_bridge_close(i);
				hsic_diag_bridge_ops[i].ctxt = (void *)(i);
				err = diag_bridge_open(i,
						   &hsic_diag_bridge_ops[i]);
				if (err) {
					pr_err("diag: HSIC %d channel open error: %d\n",
						 i, err);
				} else {
					pr_debug("diag: opened HSIC channel: %d\n",
						i);
					diag_hsic[i].hsic_device_opened = 1;
					diag_hsic[i].hsic_ch = 1;
				}
			}
		}
		mutex_unlock(&diag_bridge[i].bridge_mutex);
	}
	return 0;
}

/*
 * diagfwd_write_complete_hsic is called after the asynchronous
 * usb_diag_write() on mdm channel is complete
 */
int diagfwd_write_complete_hsic(struct diag_request *diag_write_ptr, int index)
{
	unsigned char *buf = (diag_write_ptr) ? diag_write_ptr->buf : NULL;

	if (buf) {
		/* Return buffers to their pools */
		diagmem_free(driver, (unsigned char *)buf, index +
							POOL_TYPE_HSIC);
		diagmem_free(driver, (unsigned char *)diag_write_ptr,
							index +
							POOL_TYPE_HSIC_WRITE);
	}

	if (!diag_hsic[index].hsic_ch) {
		pr_err("diag: In %s: hsic_ch == 0\n", __func__);
		return 0;
	}

	/* Read data from the HSIC */
	queue_work(diag_bridge[index].wq,
				&diag_hsic[index].diag_read_hsic_work);

	return 0;
}

void diag_usb_read_complete_hsic_fn(struct work_struct *w)
{
	struct diag_bridge_dev *bridge_struct = container_of(w,
			struct diag_bridge_dev, usb_read_complete_work);

	diagfwd_read_complete_bridge(
			diag_bridge[bridge_struct->id].usb_read_ptr);
}

void diag_read_usb_hsic_work_fn(struct work_struct *work)
{
	struct diag_bridge_dev *bridge_struct = container_of(work,
				struct diag_bridge_dev, diag_read_work);
	int index = bridge_struct->id;

	if (!diag_hsic[index].hsic_ch) {
		pr_err("diag: in %s: hsic_ch == 0\n", __func__);
		return;
	}
	/*
	 * If there is no data being read from the usb mdm channel
	 * and there is no mdm channel data currently being written
	 * to the HSIC
	 */
	if (!diag_hsic[index].in_busy_hsic_read_on_device &&
	     !diag_hsic[index].in_busy_hsic_write) {
		APPEND_DEBUG('x');
		/* Setup the next read from usb mdm channel */
		diag_hsic[index].in_busy_hsic_read_on_device = 1;
		diag_bridge[index].usb_read_ptr->buf =
				 diag_bridge[index].usb_buf_out;
		diag_bridge[index].usb_read_ptr->length = USB_MAX_OUT_BUF;
		diag_bridge[index].usb_read_ptr->context = (void *)index;
		usb_diag_read(diag_bridge[index].ch,
				 diag_bridge[index].usb_read_ptr);
		APPEND_DEBUG('y');
	}
	/* If for some reason there was no mdm channel read initiated,
	 * queue up the reading of data from the mdm channel
	 */

	if (!diag_hsic[index].in_busy_hsic_read_on_device &&
		(driver->logging_mode == USB_MODE))
		queue_work(diag_bridge[index].wq,
			 &(diag_bridge[index].diag_read_work));
}

static int diag_hsic_probe(struct platform_device *pdev)
{
	int err = 0;

	/* pdev->Id will indicate which HSIC is working. 0 stands for HSIC
	 *  or CP1 1 indicates HS-USB or CP2
	 */
	pr_debug("diag: in %s, ch = %d\n", __func__, pdev->id);
	mutex_lock(&diag_bridge[pdev->id].bridge_mutex);
	if (!diag_hsic[pdev->id].hsic_inited) {
		spin_lock_init(&diag_hsic[pdev->id].hsic_spinlock);
		diag_hsic[pdev->id].num_hsic_buf_tbl_entries = 0;
		if (diag_hsic[pdev->id].hsic_buf_tbl == NULL)
			diag_hsic[pdev->id].hsic_buf_tbl =
				kzalloc(NUM_HSIC_BUF_TBL_ENTRIES *
				sizeof(struct diag_write_device), GFP_KERNEL);
		if (diag_hsic[pdev->id].hsic_buf_tbl == NULL) {
			mutex_unlock(&diag_bridge[pdev->id].bridge_mutex);
			return -ENOMEM;
		}
		diag_hsic[pdev->id].id = pdev->id;
		diag_hsic[pdev->id].count_hsic_pool = 0;
		diag_hsic[pdev->id].count_hsic_write_pool = 0;
		diag_hsic[pdev->id].itemsize_hsic = READ_HSIC_BUF_SIZE;
		diag_hsic[pdev->id].poolsize_hsic = N_MDM_WRITE;
		diag_hsic[pdev->id].itemsize_hsic_write =
					sizeof(struct diag_request);
		diag_hsic[pdev->id].poolsize_hsic_write = N_MDM_WRITE;
		diagmem_hsic_init(pdev->id);
		INIT_WORK(&(diag_hsic[pdev->id].diag_read_hsic_work),
			    diag_read_hsic_work_fn);
		diag_hsic[pdev->id].hsic_inited = 1;
	}
	/*
	 * The probe function was called after the usb was connected
	 * on the legacy channel OR ODL is turned on. Communication over usb
	 * mdm and HSIC needs to be turned on.
	 */
	if (diag_bridge[pdev->id].usb_connected || (driver->logging_mode ==
						   MEMORY_DEVICE_MODE)) {
		if (diag_hsic[pdev->id].hsic_device_opened) {
			/* should not happen. close it before re-opening */
			pr_warn("diag: HSIC channel already opened in probe\n");
			diag_bridge_close(pdev->id);
		}
		hsic_diag_bridge_ops[pdev->id].ctxt = (void *)(pdev->id);
		err = diag_bridge_open(pdev->id,
				       &hsic_diag_bridge_ops[pdev->id]);
		if (err) {
			pr_err("diag: could not open HSIC, err: %d\n", err);
			diag_hsic[pdev->id].hsic_device_opened = 0;
			mutex_unlock(&diag_bridge[pdev->id].bridge_mutex);
			return err;
		}

		pr_info("diag: opened HSIC bridge, ch = %d\n", pdev->id);
		diag_hsic[pdev->id].hsic_device_opened = 1;
		diag_hsic[pdev->id].hsic_ch = 1;
		diag_hsic[pdev->id].in_busy_hsic_read_on_device = 0;
		diag_hsic[pdev->id].in_busy_hsic_write = 0;

		if (diag_bridge[pdev->id].usb_connected) {
			/* Poll USB mdm channel to check for data */
			queue_work(diag_bridge[pdev->id].wq,
			     &diag_bridge[pdev->id].diag_read_work);
		}
		/* Poll HSIC channel to check for data */
		queue_work(diag_bridge[pdev->id].wq,
			  &diag_hsic[pdev->id].diag_read_hsic_work);
	}
	/* The HSIC (diag_bridge) platform device driver is enabled */
	diag_hsic[pdev->id].hsic_device_enabled = 1;
	mutex_unlock(&diag_bridge[pdev->id].bridge_mutex);
	return err;
}

static int diag_hsic_remove(struct platform_device *pdev)
{
	pr_debug("diag: %s called\n", __func__);
	if (diag_hsic[pdev->id].hsic_device_enabled) {
		mutex_lock(&diag_bridge[pdev->id].bridge_mutex);
		diag_hsic_close(pdev->id);
		diag_hsic[pdev->id].hsic_device_enabled = 0;
		mutex_unlock(&diag_bridge[pdev->id].bridge_mutex);
	}

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

struct platform_driver msm_hsic_ch_driver = {
	.probe = diag_hsic_probe,
	.remove = diag_hsic_remove,
	.driver = {
		   .name = "diag_bridge",
		   .owner = THIS_MODULE,
		   .pm   = &diagfwd_hsic_dev_pm_ops,
		   },
};
