/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/diagchar.h>
#include <linux/kmemleak.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/ratelimit.h>
#include <linux/platform_device.h>
#include <linux/smux.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include "diagchar.h"
#include "diagmem.h"
#include "diagfwd_cntl.h"
#include "diagfwd_smux.h"
#include "diagfwd_hsic.h"
#include "diag_masks.h"
#include "diagfwd_bridge.h"
#include "diag_usb.h"

struct diag_bridge_dev *diag_bridge;
struct diag_bridge_dci_dev *diag_bridge_dci;

static int diagfwd_bridge_usb_connect(int index, int mode)
{
	int err = 0;

	if (index < 0 || index >= MAX_BRIDGES_DATA) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return -EINVAL;
	}

	if (index == SMUX) {
		mutex_lock(&diag_bridge[index].bridge_mutex);
		diag_bridge[index].usb_connected = 1;
		if (diag_smux->enabled) {
			diag_smux->in_busy = 0;
			diagfwd_connect_smux();
		}
		mutex_unlock(&diag_bridge[index].bridge_mutex);
		return 0;
	}

	if (!diag_hsic[index].hsic_device_enabled)
		return 0;

	mutex_lock(&diag_bridge[index].bridge_mutex);
	diag_bridge[index].usb_connected = 1;
	if ((driver->logging_mode != MEMORY_DEVICE_MODE ||
	     diag_hsic[index].hsic_data_requested)) {
		diag_hsic[index].in_busy_hsic_read_on_device = 0;
		diag_hsic[index].in_busy_hsic_write = 0;
		/*
		 * If the HSIC (diag_bridge) platform
		 * device is not open
		 */
		if (!diag_hsic[index].hsic_device_opened) {
			hsic_diag_bridge_ops[index].ctxt =
						(void *)(int)(index);
			err = diag_bridge_open(hsic_data_bridge_map[index],
					       &hsic_diag_bridge_ops[index]);
			if (err) {
				pr_err("diag: HSIC channel open error: %d\n",
				       err);
			} else {
				diag_hsic[index].hsic_device_opened = 1;
			}
		} else {
			pr_debug("diag: HSIC channel already open\n");
		}
		/*
		 * Turn on communication over usb mdm and HSIC,
		 * if the HSIC device driver is enabled
		 * and opened
		 */
		if (diag_hsic[index].hsic_device_opened) {
			diag_hsic[index].hsic_ch = 1;
			/* Poll HSIC channel to check for data */
			queue_work(diag_bridge[index].wq,
				   &diag_hsic[index].diag_read_hsic_work);
		}
	}
	mutex_unlock(&diag_bridge[index].bridge_mutex);

	return 0;
}

static int diagfwd_bridge_usb_disconnect(int index, int mode)
{
	if (index < 0 || index >= MAX_BRIDGES_DATA) {
		pr_err("diag: Invalid index %d in %s\n", index, __func__);
		return -EINVAL;
	}

	if (index == SMUX) {
		mutex_lock(&diag_bridge[index].bridge_mutex);
		diag_bridge[index].usb_connected = 0;
		if (diag_smux->enabled && driver->logging_mode == USB_MODE) {
			diag_smux->in_busy = 1;
			diag_smux->lcid = LCID_INVALID;
			diag_smux->connected = 0;
			/*
			 * Turn off communication over usb and smux
			 */
			msm_smux_close(LCID_VALID);
		}
		mutex_unlock(&diag_bridge[index].bridge_mutex);
		return 0;
	}

	mutex_lock(&diag_bridge[index].bridge_mutex);
	if (diag_hsic[index].hsic_device_enabled &&
	    (driver->logging_mode != MEMORY_DEVICE_MODE ||
	     !diag_hsic[index].hsic_data_requested)) {
		diag_hsic[index].in_busy_hsic_read_on_device = 1;
		diag_hsic[index].in_busy_hsic_write = 1;
		/* Turn off communication over usb and HSIC */
		diag_hsic_close(index);
	}
	mutex_unlock(&diag_bridge[index].bridge_mutex);

	return 0;
}

static int diagfwd_bridge_usb_read_complete(unsigned char *buf, int len,
					    int index)
{
	int bridge_index = 0;
	int err = 0;

	if (index < 0 || index >= MAX_BRIDGES_DATA) {
		pr_err("diag: In %s, invalid bridge index %d\n", __func__,
		       index);
		return -EINVAL;
	}

	/*
	 * If there is an error in the USB read, len will be a negative error
	 * code. Do not forward the packet to remote device
	 */
	if (!buf || len < 0)
		return -EIO;

	if (index == SMUX) {
		if (!diag_smux->connected)
			return 0;

		err = msm_smux_write(diag_smux->lcid, NULL, buf, len);
		if (err) {
			pr_err_ratelimited("diag: error writing to SMUX, err: %d\n",
					   err);
		}
		return err;
	}

	bridge_index = hsic_data_bridge_map[index];
	if (!diag_hsic[index].hsic_device_opened)
		return 0;

	diag_hsic[index].in_busy_hsic_write = 1;
	err = diag_bridge_write(bridge_index, buf, len);
	if (err) {
		pr_err_ratelimited("diag: unable to write usb data to remote device, err: %d\n",
				   err);
		diag_hsic[index].in_busy_hsic_write = 0;
	}

	return 0;
}

static int diagfwd_bridge_usb_write_complete(unsigned char *buf, int len,
					     int buf_ctx, int index)
{
	int usb_index = 0;

	if (index < 0 || index >= MAX_BRIDGES_DATA) {
		pr_err("diag: In %s, invalid bridge index %d\n", __func__,
		       index);
		return -EINVAL;
	}

	if (index == HSIC_DATA_CH || index == HSIC_DATA_CH_2) {
		usb_index = index - HSIC_DATA_CH;
		if (buf) {
			diagmem_free(driver, (unsigned char *)buf,
				     usb_index + POOL_TYPE_MDM);
		}
		if (!diag_hsic[index].hsic_ch) {
			pr_err("diag: In %s: hsic_ch == 0\n", __func__);
			return 0;
		}
		/* Read data from the HSIC */
		queue_work(diag_bridge[index].wq,
			   &diag_hsic[index].diag_read_hsic_work);
	} else if (index == SMUX) {
		diagfwd_write_complete_smux();
	}

	return 0;
}

static struct diag_mux_ops diagfwd_bridge_mux_ops = {
	.open = diagfwd_bridge_usb_connect,
	.close = diagfwd_bridge_usb_disconnect,
	.read_done = diagfwd_bridge_usb_read_complete,
	.write_done = diagfwd_bridge_usb_write_complete
};

int diagfwd_bridge_init(int index)
{
	int err = 0;
	unsigned char name[20];

	if (index == HSIC_DATA_CH) {
		strlcpy(name, "hsic", sizeof(name));
		err = diag_usb_register(DIAG_USB_MDM, HSIC_DATA_CH,
					&diagfwd_bridge_mux_ops);
		if (err)
			goto fail;
	} else if (index == HSIC_DATA_CH_2) {
		strlcpy(name, "hsic2", sizeof(name));
		err = diag_usb_register(DIAG_USB_MDM2, HSIC_DATA_CH_2,
					&diagfwd_bridge_mux_ops);
		if (err)
			goto fail;
	} else if (index == SMUX) {
		strlcpy(name, "smux", sizeof(name));
		err = diag_usb_register(DIAG_USB_QSC, SMUX,
					&diagfwd_bridge_mux_ops);
		if (err)
			goto fail;
		err = platform_driver_register(&msm_diagfwd_smux_driver);
		if (err) {
			pr_err("diag: could not register SMUX device, ret: %d\n",
									 err);
			goto fail;
		}
	} else {
		pr_debug("diag: incorrect bridge instance: %d\n", index);
		return 0;
	}
	diag_bridge[index].enabled = 1;
	strlcpy(diag_bridge[index].name, name,
		sizeof(diag_bridge[index].name));
	strlcat(name, "_diag_wq", sizeof(diag_bridge[index].name));
	diag_bridge[index].id = index;
	diag_bridge[index].wq = create_singlethread_workqueue(name);
	if (!diag_bridge[index].wq)
		goto fail;
	diag_bridge[index].read_len = 0;
	mutex_init(&diag_bridge[index].bridge_mutex);
	return 0;

fail:
	if (diag_bridge[index].wq)
		destroy_workqueue(diag_bridge[index].wq);
	return err;
}

void diagfwd_bridge_exit(void)
{
	int i;
	pr_debug("diag: in %s\n", __func__);

	for (i = 0; i < MAX_HSIC_DATA_CH; i++) {
		if (diag_hsic[i].hsic_device_enabled) {
			diag_hsic_close(i);
			diag_hsic[i].hsic_device_enabled = 0;
			diag_bridge[i].enabled = 0;
		}
		diag_hsic[i].hsic_inited = 0;
		kfree(diag_hsic[i].hsic_buf_tbl);
	}

	if (diag_smux->enabled) {
		diag_smux->lcid = LCID_INVALID;
		kfree(diag_smux->read_buf);
		diag_smux->enabled = 0;
		diag_bridge[SMUX].enabled = 0;
	}
	platform_driver_unregister(&msm_hsic_ch_driver);
	platform_driver_unregister(&msm_diagfwd_smux_driver);
	for (i = 0; i < MAX_BRIDGES_DATA; i++) {
		if (diag_bridge[i].enabled) {
			destroy_workqueue(diag_bridge[i].wq);
			diag_bridge[i].enabled = 0;
			diagmem_exit(driver, POOL_TYPE_MDM + i);
			diagmem_exit(driver, POOL_TYPE_MDM_USB + i);
		}
	}
}

int diagfwd_bridge_dci_init(int index)
{
	unsigned char name[20];

	if (!diag_bridge_dci)
		return -EIO;

	/*
	 * Don't return an error code if the channel is not supported. The rest
	 * of the driver initialization should proceed.
	 * diag_bridge_dci[index].enabled is used to check if a particular
	 * bridge instance is initialized.
	 */
	if (index == HSIC_DCI_CH) {
		strlcpy(name, "hsic_dci", sizeof(name));
	} else {
		pr_debug("diag: incorrect dci bridge instance: %d\n", index);
		return 0;
	}

	strlcpy(diag_bridge_dci[index].name, name,
		sizeof(diag_bridge_dci[index].name));
	strlcat(name, "_diag_wq", sizeof(diag_bridge_dci[index].name));
	diag_bridge_dci[index].id = index;
	diag_bridge_dci[index].wq = create_singlethread_workqueue(name);
	if (!diag_bridge_dci[index].wq)
		return -ENOMEM;
	diag_bridge_dci[index].read_len = 0;
	diag_bridge_dci[index].write_len = 0;
	diag_bridge_dci[index].enabled = 1;
	mutex_init(&diag_bridge_dci[index].bridge_mutex);

	return 0;
}

void diagfwd_bridge_dci_exit(void)
{
	int i;
	pr_debug("diag: in %s\n", __func__);

	for (i = 0; i < MAX_HSIC_DCI_CH; i++) {
		if (diag_hsic_dci[i].hsic_device_enabled) {
			diag_hsic_dci_close(i);
			diag_hsic_dci[i].hsic_device_enabled = 0;
			diag_bridge_dci[i].enabled = 0;
		}
		diag_hsic_dci[i].hsic_inited = 0;
	}

	for (i = 0; i < MAX_BRIDGES_DCI; i++) {
		if (diag_bridge_dci[i].enabled) {
			destroy_workqueue(diag_bridge_dci[i].wq);
			diag_bridge_dci[i].enabled = 0;
			diagmem_exit(driver, POOL_TYPE_MDM_DCI + i);
			diagmem_exit(driver, POOL_TYPE_MDM_DCI_WRITE + i);
		}
	}
}
