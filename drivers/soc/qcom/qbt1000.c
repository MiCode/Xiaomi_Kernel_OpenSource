/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "qbt1000:%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <uapi/linux/qbt1000.h>
#include <soc/qcom/scm.h>
#include "qseecom_kernel.h"

#define QBT1000_DEV "qbt1000"
#define QBT1000_IN_DEV_NAME "qbt1000_key_input"
#define QBT1000_IN_DEV_VERSION 0x0100
#define MAX_FW_EVENTS 128
#define FP_APP_CMD_RX_IPC 132
#define FW_MAX_IPC_MSG_DATA_SIZE 0x500
#define IPC_MSG_ID_CBGE_REQUIRED 29

/*
 * shared buffer size - init with max value,
 * user space will provide new value upon tz app load
 */
static uint32_t g_app_buf_size = SZ_256K;
static char const *const FP_APP_NAME = "fingerpr";

struct finger_detect_gpio {
	int gpio;
	int active_low;
	int irq;
	struct work_struct work;
	unsigned int key_code;
	int power_key_enabled;
	int last_gpio_state;
	int event_reported;
};

struct fw_event_desc {
	enum qbt1000_fw_event ev;
};

struct fw_ipc_info {
	int gpio;
	int irq;
};

struct qbt1000_drvdata {
	struct class	*qbt1000_class;
	struct cdev	qbt1000_cdev;
	struct device	*dev;
	char		*qbt1000_node;
	struct clk	**clocks;
	unsigned	clock_count;
	uint8_t		clock_state;
	unsigned	root_clk_idx;
	unsigned	frequency;
	atomic_t	available;
	struct mutex	mutex;
	struct mutex	fw_events_mutex;
	struct input_dev	*in_dev;
	struct fw_ipc_info	fw_ipc;
	struct finger_detect_gpio	fd_gpio;
	DECLARE_KFIFO(fw_events, struct fw_event_desc, MAX_FW_EVENTS);
	wait_queue_head_t read_wait_queue;
	struct qseecom_handle *app_handle;
	struct qseecom_handle *fp_app_handle;
};

/*
* struct fw_ipc_cmd -
*      used to store IPC commands to/from firmware
* @status - indicates whether sending/getting the IPC message was successful
* @msg_type - the type of IPC message
* @msg_len - the length of the message data
* @resp_needed - whether a response is needed for this message
* @msg_data - any extra data associated with the message
*/
struct fw_ipc_cmd {
	uint32_t status;
	uint32_t numMsgs;
	uint8_t msg_data[FW_MAX_IPC_MSG_DATA_SIZE];
};

struct fw_ipc_header {
	uint32_t msg_type;
	uint32_t msg_len;
	uint32_t resp_needed;
};

/*
* struct ipc_msg_type_to_fw_event -
*      entry in mapping between an IPC message type to a firmware event
* @msg_type - IPC message type, as reported by firmware
* @fw_event - corresponding firmware event code to report to driver client
*/
struct ipc_msg_type_to_fw_event {
	uint32_t msg_type;
	enum qbt1000_fw_event fw_event;
};

/* mapping between firmware IPC message types to HLOS firmware events */
struct ipc_msg_type_to_fw_event g_msg_to_event[] = {
		{IPC_MSG_ID_CBGE_REQUIRED, FW_EVENT_CBGE_REQUIRED}
};

/**
 * get_cmd_rsp_buffers() - Function sets cmd & rsp buffer pointers and
 *                         aligns buffer lengths
 * @hdl:	index of qseecom_handle
 * @cmd:	req buffer - set to qseecom_handle.sbuf
 * @cmd_len:	ptr to req buffer len
 * @rsp:	rsp buffer - set to qseecom_handle.sbuf + offset
 * @rsp_len:	ptr to rsp buffer len
 *
 * Return: 0 on success. Error code on failure.
 */
static int get_cmd_rsp_buffers(struct qseecom_handle *hdl,
	void **cmd,
	uint32_t *cmd_len,
	void **rsp,
	uint32_t *rsp_len)
{
	/* 64 bytes alignment for QSEECOM */
	uint64_t aligned_cmd_len = ALIGN((uint64_t)*cmd_len, 64);
	uint64_t aligned_rsp_len = ALIGN((uint64_t)*rsp_len, 64);

	if ((aligned_rsp_len + aligned_cmd_len) > (uint64_t)g_app_buf_size)
		return -ENOMEM;

	*cmd = hdl->sbuf;
	*cmd_len = aligned_cmd_len;
	*rsp = hdl->sbuf + *cmd_len;
	*rsp_len = aligned_rsp_len;

	return 0;
}

/**
 * clocks_on() - Function votes for SPI and AHB clocks to be on and sets
 *               the clk rate to predetermined value for SPI.
 * @drvdata:	ptr to driver data
 *
 * Return: 0 on success. Error code on failure.
 */
static int clocks_on(struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	int index;

	if (!drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++) {
			pr_debug("set clock rate at idx:%d, freq: %u\n",
				index, drvdata->frequency);
			if (index == drvdata->root_clk_idx) {
				rc = clk_set_rate(drvdata->clocks[index],
					drvdata->frequency);
				if (rc) {
					pr_err("failure set clock rate at idx:%d\n",
						index);
					goto unprepare;
				}
			}

	rc = clk_prepare_enable(drvdata->clocks[index]);
			if (rc) {
				pr_err("failure to prepare clk at idx:%d\n",
					index);
				goto unprepare;
			}


		}
		drvdata->clock_state = 1;
	}
	goto end;

unprepare:
	for (--index; index >= 0; index--)
		clk_disable_unprepare(drvdata->clocks[index]);

end:
	return rc;
}

/**
 * clocks_off() - Function votes for SPI and AHB clocks to be off
 * @drvdata:	ptr to driver data
 *
 * Return: None
 */
static void clocks_off(struct qbt1000_drvdata *drvdata)
{
	int index;

	if (drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++)
			clk_disable_unprepare(drvdata->clocks[index]);
		drvdata->clock_state = 0;
	}
}

/**
 * send_tz_cmd() - Function sends a command to TZ
 *
 * @drvdata: pointer to driver data
 * @app_handle: handle to tz app
 * @is_user_space: 1 if the cmd buffer is in user space, 0
 *          otherwise
 * @cmd: command buffer to send
 * @cmd_len: length of the command buffer
 * @rsp: output, will be set to location of response buffer
 * @rsp_len: max size of response
 *
 * Return: 0 on success.
 */
static int send_tz_cmd(struct qbt1000_drvdata *drvdata,
	struct qseecom_handle *app_handle,
	int is_user_space,
	void *cmd, uint32_t cmd_len,
	void **rsp, uint32_t rsp_len)
{
	int rc = 0;
	void *aligned_cmd;
	void *aligned_rsp;
	uint32_t aligned_cmd_len;
	uint32_t aligned_rsp_len;

	/* init command and response buffers and align lengths */
	aligned_cmd_len = cmd_len;
	aligned_rsp_len = rsp_len;

	rc = get_cmd_rsp_buffers(app_handle,
		(void **)&aligned_cmd,
		&aligned_cmd_len,
		(void **)&aligned_rsp,
		&aligned_rsp_len);

	if (rc != 0)
		goto end;

	if (!aligned_cmd) {
		dev_err(drvdata->dev, "%s: Null command buffer\n",
			__func__);
		rc = -EINVAL;
		goto end;
	}

	if (aligned_cmd - cmd + cmd_len > g_app_buf_size) {
		rc = -ENOMEM;
		goto end;
	}

	if (is_user_space) {
		rc = copy_from_user(aligned_cmd, (void __user *)cmd,
				cmd_len);
		if (rc != 0) {
			pr_err("failure to copy user space buf %d\n", rc);
			rc = -EFAULT;
			goto end;
		}
	} else
		memcpy(aligned_cmd, cmd, cmd_len);


	/* vote for clocks before sending TZ command */
	rc = clocks_on(drvdata);
	if (rc != 0) {
		pr_err("failure to enable clocks %d\n", rc);
		goto end;
	}

	/* send cmd to TZ */
	rc = qseecom_send_command(app_handle,
		aligned_cmd,
		aligned_cmd_len,
		aligned_rsp,
		aligned_rsp_len);

	/* un-vote for clocks */
	clocks_off(drvdata);

	if (rc != 0) {
		pr_err("failure to send tz cmd %d\n", rc);
		goto end;
	}

	*rsp = aligned_rsp;

end:
	return rc;
}

/**
 * qbt1000_open() - Function called when user space opens device.
 * Successful if driver not currently open.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	struct qbt1000_drvdata *drvdata = container_of(inode->i_cdev,
						   struct qbt1000_drvdata,
						   qbt1000_cdev);
	file->private_data = drvdata;

	pr_debug("qbt1000_open begin\n");
	/* disallowing concurrent opens */
	if (!atomic_dec_and_test(&drvdata->available)) {
		atomic_inc(&drvdata->available);
		rc = -EBUSY;
	}

	pr_debug("qbt1000_open end : %d\n", rc);
	return rc;
}

/**
 * qbt1000_release() - Function called when user space closes device.

 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_release(struct inode *inode, struct file *file)
{
	struct qbt1000_drvdata *drvdata;

	if (!file->private_data) {
		pr_err("Null pointer passed in file->private_data");
		return -EINVAL;
	}

	drvdata = file->private_data;

	atomic_inc(&drvdata->available);
	return 0;
}

/**
 * qbt1000_ioctl() - Function called when user space calls ioctl.
 * @file:	struct file - not used
 * @cmd:	cmd identifier:QBT1000_LOAD_APP,QBT1000_UNLOAD_APP,
 *              QBT1000_SEND_TZCMD
 * @arg:	ptr to relevant structe: either qbt1000_app or
 *              qbt1000_send_tz_cmd depending on which cmd is passed
 *
 * Return: 0 on success. Error code on failure.
 */
static long qbt1000_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int rc = 0;
	void __user *priv_arg = (void __user *)arg;
	struct qbt1000_drvdata *drvdata;

	if (!file->private_data) {
		pr_err("Null pointer passed in file->private_data");
		return -EINVAL;
	}

	drvdata = file->private_data;

	if (IS_ERR(priv_arg)) {
		dev_err(drvdata->dev, "%s: invalid user space pointer %lu\n",
			__func__, arg);
		return -EINVAL;
	}

	mutex_lock(&drvdata->mutex);

	pr_debug("qbt1000_ioctl %d\n", cmd);

	switch (cmd) {
	case QBT1000_LOAD_APP:
	{
		struct qbt1000_app app;
		struct qseecom_handle *app_handle;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -EFAULT;
			pr_err("failed copy from user space-LOAD\n");
			goto end;
		}

		if (!app.app_handle) {
			dev_err(drvdata->dev, "%s: LOAD app_handle is null\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		if (strcmp(app.name, FP_APP_NAME)) {
			dev_err(drvdata->dev, "%s: Invalid app name\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		if (drvdata->app_handle) {
			dev_err(drvdata->dev, "%s: LOAD app already loaded, unloading first\n",
				__func__);
			drvdata->fp_app_handle = 0;
			rc = qseecom_shutdown_app(&drvdata->app_handle);
			if (rc != 0) {
				dev_err(drvdata->dev, "%s: LOAD current app failed to shutdown\n",
					  __func__);
				goto end;
			}
		}

		pr_debug("app %s load before\n", app.name);
		app.name[MAX_NAME_SIZE - 1] = '\0';

		/* start the TZ app */
		rc = qseecom_start_app(
				&drvdata->app_handle, app.name, app.size);
		if (rc == 0) {
			g_app_buf_size = app.size;
			rc = qseecom_set_bandwidth(drvdata->app_handle,
				app.high_band_width == 1 ? true : false);
			if (rc != 0) {
				/* log error, allow to continue */
				pr_err("App %s failed to set bw\n", app.name);
			}
		} else {
			dev_err(drvdata->dev, "%s: Fingerprint Trusted App failed to load\n",
				__func__);
			goto end;
		}

		/* copy a fake app handle to user */
		app_handle = drvdata->app_handle ?
				(struct qseecom_handle *)123456 : 0;
		rc = copy_to_user((void __user *)app.app_handle, &app_handle,
			sizeof(*app.app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy 2us LOAD rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		pr_debug("app %s load after\n", app.name);

		drvdata->fp_app_handle = drvdata->app_handle;
		break;
	}
	case QBT1000_UNLOAD_APP:
	{
		struct qbt1000_app app;
		struct qseecom_handle *app_handle = 0;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -ENOMEM;
			pr_err("failed copy from user space-UNLOAD\n");
			goto end;
		}

		if (!app.app_handle) {
			dev_err(drvdata->dev, "%s: UNLOAD app_handle is null\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		rc = copy_from_user(&app_handle, app.app_handle,
			sizeof(app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy from user space-UNLOAD handle rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!drvdata->app_handle) {
			pr_err("app not loaded\n");
			rc = -EINVAL;
			goto end;
		}

		if (drvdata->fp_app_handle == drvdata->app_handle)
			drvdata->fp_app_handle = 0;

		/* set bw & shutdown the TZ app */
		qseecom_set_bandwidth(drvdata->app_handle,
			app.high_band_width == 1 ? true : false);
		rc = qseecom_shutdown_app(&drvdata->app_handle);
		if (rc != 0) {
			pr_err("app failed to shutdown\n");
			goto end;
		}

		/* copy the app handle (should be null) to user */
		rc = copy_to_user((void __user *)app.app_handle, &app_handle,
			sizeof(*app.app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy 2us UNLOAD rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		break;
	}
	case QBT1000_SEND_TZCMD:
	{
		struct qbt1000_send_tz_cmd tzcmd;
		void *rsp_buf;

		if (copy_from_user(&tzcmd, priv_arg,
			sizeof(tzcmd))
				!= 0) {
			rc = -EFAULT;
			pr_err("failed copy from user space %d\n", rc);
			goto end;
		}

		if (tzcmd.req_buf_len > g_app_buf_size ||
			tzcmd.rsp_buf_len > g_app_buf_size) {
			rc = -ENOMEM;
			pr_err("invalid cmd buf len, req=%d, rsp=%d\n",
				tzcmd.req_buf_len, tzcmd.rsp_buf_len);
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!drvdata->app_handle) {
			pr_err("app not loaded\n");
			rc = -EINVAL;
			goto end;
		}

		rc = send_tz_cmd(drvdata,
			drvdata->app_handle, 1,
			tzcmd.req_buf, tzcmd.req_buf_len,
			&rsp_buf, tzcmd.rsp_buf_len);

		if (rc < 0) {
			pr_err("failure sending command to tz\n");
			goto end;
		}

		/* copy rsp buf back to user space buffer */
		rc = copy_to_user((void __user *)tzcmd.rsp_buf,
			 rsp_buf, tzcmd.rsp_buf_len);
		if (rc != 0) {
			pr_err("failed copy 2us rc:%d bytes %d:\n",
				rc, tzcmd.rsp_buf_len);
			rc = -EFAULT;
			goto end;
		}

		break;
	}
	case QBT1000_SET_FINGER_DETECT_KEY:
	{
		struct qbt1000_set_finger_detect_key set_fd_key;

		if (copy_from_user(&set_fd_key, priv_arg,
			sizeof(set_fd_key))
				!= 0) {
			rc = -EFAULT;
			pr_err("failed copy from user space %d\n", rc);
			goto end;
		}

		drvdata->fd_gpio.key_code = set_fd_key.key_code;

		break;
	}
	case QBT1000_CONFIGURE_POWER_KEY:
	{
		struct qbt1000_configure_power_key power_key;

		if (copy_from_user(&power_key, priv_arg,
			sizeof(power_key))
				!= 0) {
			rc = -EFAULT;
			pr_err("failed copy from user space %d\n", rc);
			goto end;
		}

		drvdata->fd_gpio.power_key_enabled = power_key.enable;

		break;
	}
	default:
		pr_err("invalid cmd %d\n", cmd);
		rc = -ENOIOCTLCMD;
		goto end;
	}

end:
	mutex_unlock(&drvdata->mutex);
	return rc;
}

static int get_events_fifo_len_locked(struct qbt1000_drvdata *drvdata)
{
	int len;

	mutex_lock(&drvdata->fw_events_mutex);
	len = kfifo_len(&drvdata->fw_events);
	mutex_unlock(&drvdata->fw_events_mutex);

	return len;
}

static ssize_t qbt1000_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	struct fw_event_desc fw_event;
	struct qbt1000_drvdata *drvdata = filp->private_data;

	if (cnt < sizeof(fw_event.ev))
		return -EINVAL;

	mutex_lock(&drvdata->fw_events_mutex);

	while (kfifo_len(&drvdata->fw_events) == 0) {
		mutex_unlock(&drvdata->fw_events_mutex);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_debug("fw_events fifo: empty, waiting\n");

		if (wait_event_interruptible(drvdata->read_wait_queue,
			  (get_events_fifo_len_locked(drvdata) > 0)))
			return -ERESTARTSYS;

		mutex_lock(&drvdata->fw_events_mutex);
	}

	if (!kfifo_get(&drvdata->fw_events, &fw_event)) {
		pr_debug("fw_events fifo: unexpectedly empty\n");

		mutex_unlock(&drvdata->fw_events_mutex);
		return -EINVAL;
	}

	mutex_unlock(&drvdata->fw_events_mutex);

	pr_debug("fw_event: %d\n", (int)fw_event.ev);
	return copy_to_user(ubuf, &fw_event.ev, sizeof(fw_event.ev));
}

static unsigned int qbt1000_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	struct qbt1000_drvdata *drvdata = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &drvdata->read_wait_queue, wait);

	if (kfifo_len(&drvdata->fw_events) > 0)
		mask |= (POLLIN | POLLRDNORM);

	return mask;
}

static const struct file_operations qbt1000_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qbt1000_ioctl,
	.open = qbt1000_open,
	.release = qbt1000_release,
	.read = qbt1000_read,
	.poll = qbt1000_poll
};

static int qbt1000_dev_register(struct qbt1000_drvdata *drvdata)
{
	dev_t dev_no;
	int ret = 0;
	size_t node_size;
	char *node_name = QBT1000_DEV;
	struct device *dev = drvdata->dev;
	struct device *device;

	node_size = strlen(node_name) + 1;

	drvdata->qbt1000_node = devm_kzalloc(dev, node_size, GFP_KERNEL);
	if (!drvdata->qbt1000_node) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	strlcpy(drvdata->qbt1000_node, node_name, node_size);

	ret = alloc_chrdev_region(&dev_no, 0, 1, drvdata->qbt1000_node);
	if (ret) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		goto err_alloc;
	}

	cdev_init(&drvdata->qbt1000_cdev, &qbt1000_fops);

	drvdata->qbt1000_cdev.owner = THIS_MODULE;
	ret = cdev_add(&drvdata->qbt1000_cdev, dev_no, 1);
	if (ret) {
		pr_err("cdev_add failed %d\n", ret);
		goto err_cdev_add;
	}

	drvdata->qbt1000_class = class_create(THIS_MODULE,
					   drvdata->qbt1000_node);
	if (IS_ERR(drvdata->qbt1000_class)) {
		ret = PTR_ERR(drvdata->qbt1000_class);
		pr_err("class_create failed %d\n", ret);
		goto err_class_create;
	}

	device = device_create(drvdata->qbt1000_class, NULL,
			       drvdata->qbt1000_cdev.dev, drvdata,
			       drvdata->qbt1000_node);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("device_create failed %d\n", ret);
		goto err_dev_create;
	}

	return 0;
err_dev_create:
	class_destroy(drvdata->qbt1000_class);
err_class_create:
	cdev_del(&drvdata->qbt1000_cdev);
err_cdev_add:
	unregister_chrdev_region(drvdata->qbt1000_cdev.dev, 1);
err_alloc:
	return ret;
}

/**
 * qbt1000_create_input_device() - Function allocates an input
 * device, configures it for key events and registers it
 *
 * @drvdata:	ptr to driver data
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_create_input_device(struct qbt1000_drvdata *drvdata)
{
	int rc = 0;

	drvdata->in_dev = input_allocate_device();
	if (drvdata->in_dev == NULL) {
		dev_err(drvdata->dev, "%s: input_allocate_device() failed\n",
			__func__);
		rc = -ENOMEM;
		goto end;
	}

	drvdata->in_dev->name = QBT1000_IN_DEV_NAME;
	drvdata->in_dev->phys = NULL;
	drvdata->in_dev->id.bustype = BUS_HOST;
	drvdata->in_dev->id.vendor  = 0x0001;
	drvdata->in_dev->id.product = 0x0001;
	drvdata->in_dev->id.version = QBT1000_IN_DEV_VERSION;

	drvdata->in_dev->evbit[0] = BIT_MASK(EV_KEY) |  BIT_MASK(EV_ABS);
	drvdata->in_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	drvdata->in_dev->keybit[BIT_WORD(KEY_HOMEPAGE)] |=
		BIT_MASK(KEY_HOMEPAGE);
	drvdata->in_dev->keybit[BIT_WORD(KEY_CAMERA)] |=
		BIT_MASK(KEY_CAMERA);
	drvdata->in_dev->keybit[BIT_WORD(KEY_VOLUMEDOWN)] |=
		BIT_MASK(KEY_VOLUMEDOWN);
	drvdata->in_dev->keybit[BIT_WORD(KEY_POWER)] |=
		BIT_MASK(KEY_POWER);

	input_set_abs_params(drvdata->in_dev, ABS_X,
			     0,
			     1000,
			     0, 0);
	input_set_abs_params(drvdata->in_dev, ABS_Y,
			     0,
			     1000,
			     0, 0);

	rc = input_register_device(drvdata->in_dev);
	if (rc) {
		dev_err(drvdata->dev, "%s: input_reg_dev() failed %d\n",
			__func__, rc);
		goto end;
	}

end:
	if (rc)
		input_free_device(drvdata->in_dev);
	return rc;
}

static void purge_finger_events(struct qbt1000_drvdata *drvdata)
{
	int i, fifo_len;
	struct fw_event_desc fw_event;

	fifo_len = kfifo_len(&drvdata->fw_events);

	for (i = 0; i < fifo_len; i++) {
		if (!kfifo_get(&drvdata->fw_events, &fw_event))
			pr_err("fw events fifo: could not remove oldest item\n");
		else if (fw_event.ev != FW_EVENT_FINGER_DOWN
					&& fw_event.ev != FW_EVENT_FINGER_UP)
			kfifo_put(&drvdata->fw_events, fw_event);
	}
}

static void qbt1000_gpio_report_event(struct qbt1000_drvdata *drvdata)
{
	int state;
	struct fw_event_desc fw_event;

	state = (__gpio_get_value(drvdata->fd_gpio.gpio) ? 1 : 0)
		^ drvdata->fd_gpio.active_low;

	if (drvdata->fd_gpio.event_reported
		  && state == drvdata->fd_gpio.last_gpio_state)
		return;

	pr_debug("gpio %d: report state %d\n", drvdata->fd_gpio.gpio, state);

	drvdata->fd_gpio.event_reported = 1;
	drvdata->fd_gpio.last_gpio_state = state;

	if (drvdata->fd_gpio.key_code) {
		input_event(drvdata->in_dev, EV_KEY,
			drvdata->fd_gpio.key_code, !!state);
		input_sync(drvdata->in_dev);
	}

	if (state && drvdata->fd_gpio.power_key_enabled) {
		input_event(drvdata->in_dev, EV_KEY, KEY_POWER, 1);
		input_sync(drvdata->in_dev);
		input_event(drvdata->in_dev, EV_KEY, KEY_POWER, 0);
		input_sync(drvdata->in_dev);
	}

	fw_event.ev = (state ? FW_EVENT_FINGER_DOWN : FW_EVENT_FINGER_UP);

	mutex_lock(&drvdata->fw_events_mutex);

	if (kfifo_is_full(&drvdata->fw_events)) {
		struct fw_event_desc dummy_fw_event;

		pr_warn("fw events fifo: full, dropping oldest item\n");
		if (!kfifo_get(&drvdata->fw_events, &dummy_fw_event))
			pr_err("fw events fifo: could not remove oldest item\n");
	}

	purge_finger_events(drvdata);

	if (!kfifo_put(&drvdata->fw_events, fw_event))
		pr_err("fw events fifo: error adding item\n");

	mutex_unlock(&drvdata->fw_events_mutex);
	wake_up_interruptible(&drvdata->read_wait_queue);
}

static void qbt1000_gpio_work_func(struct work_struct *work)
{
	struct qbt1000_drvdata *drvdata =
		container_of(work, struct qbt1000_drvdata, fd_gpio.work);

	qbt1000_gpio_report_event(drvdata);

	pm_relax(drvdata->dev);
}

static irqreturn_t qbt1000_gpio_isr(int irq, void *dev_id)
{
	struct qbt1000_drvdata *drvdata = dev_id;

	if (irq != drvdata->fd_gpio.irq) {
		pr_warn("invalid irq %d (expected %d)\n",
			irq, drvdata->fd_gpio.irq);
		return IRQ_HANDLED;
	}

	pm_stay_awake(drvdata->dev);
	schedule_work(&drvdata->fd_gpio.work);

	return IRQ_HANDLED;
}

/**
 * qbt1000_ipc_irq_handler() - function processes IPC
 * interrupts on its own thread
 * @irq:	the interrupt that occurred
 * @dev_id: pointer to the qbt1000_drvdata
 *
 * Return: IRQ_HANDLED when complete
 */
static irqreturn_t qbt1000_ipc_irq_handler(int irq, void *dev_id)
{
	uint8_t *msg_buffer;
	struct fw_ipc_cmd *rx_cmd;
	struct fw_ipc_header *header;
	int i, j;
	uint32_t rxipc = FP_APP_CMD_RX_IPC;
	struct qbt1000_drvdata *drvdata = (struct qbt1000_drvdata *)dev_id;
	int rc = 0;
	uint32_t retry_count = 10;

	pm_stay_awake(drvdata->dev);

	mutex_lock(&drvdata->mutex);

	if (irq != drvdata->fw_ipc.irq) {
		pr_warn("invalid irq %d (expected %d)\n",
			irq, drvdata->fw_ipc.irq);
		goto end;
	}

	if (!drvdata->fp_app_handle)
		goto end;

	while (retry_count > 0) {
		/*
		 * send the TZ command to fetch the message from firmware
		 * TZ will process the message if it can
		 */
		rc = send_tz_cmd(drvdata, drvdata->fp_app_handle, 0,
				&rxipc, sizeof(rxipc),
				(void *)&rx_cmd, sizeof(*rx_cmd));
		if (rc < 0) {
			msleep(50); /* sleep for 50ms before retry */
			retry_count -= 1;
			continue;
		} else {
			break;
		}
	}

	if (rc < 0) {
		pr_err("failure sending tz cmd %d\n", rxipc);
		goto end;
	}

	if (rx_cmd->status != 0) {
		pr_err("tz command failed to complete\n");
		goto end;
	}

	msg_buffer = rx_cmd->msg_data;

	for (j = 0; j < rx_cmd->numMsgs; j++) {
		header = (struct fw_ipc_header *) msg_buffer;
		/*
		 * given the IPC message type, search for a corresponding event
		 * for the driver client. If found, add to the events FIFO
		 */
		for (i = 0; i < ARRAY_SIZE(g_msg_to_event); i++) {
			if (g_msg_to_event[i].msg_type == header->msg_type) {
				enum qbt1000_fw_event ev =
						g_msg_to_event[i].fw_event;
				struct fw_event_desc fw_ev_desc;

				mutex_lock(&drvdata->fw_events_mutex);
				pr_debug("fw events: add %d\n", (int) ev);
				fw_ev_desc.ev = ev;

				if (!kfifo_put(&drvdata->fw_events, fw_ev_desc))
					pr_err("fw events: fifo full, drop event %d\n",
						(int) ev);

				mutex_unlock(&drvdata->fw_events_mutex);
				break;
			}
		}
		msg_buffer += sizeof(*header) + header->msg_len;
	}
	wake_up_interruptible(&drvdata->read_wait_queue);
end:
	mutex_unlock(&drvdata->mutex);
	pm_relax(drvdata->dev);
	return IRQ_HANDLED;
}

static int setup_fd_gpio_irq(struct platform_device *pdev,
	struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	int irq;
	const char *desc = "qbt_finger_detect";

	rc = devm_gpio_request_one(&pdev->dev, drvdata->fd_gpio.gpio,
		GPIOF_IN, desc);

	if (rc < 0) {
		pr_err("failed to request gpio %d, error %d\n",
			drvdata->fd_gpio.gpio, rc);
		goto end;
	}

	irq = gpio_to_irq(drvdata->fd_gpio.gpio);
	if (irq < 0) {
		rc = irq;
		pr_err("unable to get irq number for gpio %d, error %d\n",
			drvdata->fd_gpio.gpio, rc);
		goto end;
	}

	drvdata->fd_gpio.irq = irq;
	INIT_WORK(&drvdata->fd_gpio.work, qbt1000_gpio_work_func);

	rc = devm_request_any_context_irq(&pdev->dev, drvdata->fd_gpio.irq,
		qbt1000_gpio_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		desc, drvdata);

	if (rc < 0) {
		pr_err("unable to claim irq %d; error %d\n",
			drvdata->fd_gpio.irq, rc);
		goto end;
	}

end:
	return rc;
}

static int setup_ipc_irq(struct platform_device *pdev,
	struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	const char *desc = "qbt_ipc";

	drvdata->fw_ipc.irq = gpio_to_irq(drvdata->fw_ipc.gpio);
	if (drvdata->fw_ipc.irq < 0) {
		rc = drvdata->fw_ipc.irq;
		pr_err("no irq for gpio %d, error=%d\n",
		  drvdata->fw_ipc.gpio, rc);
		goto end;
	}

	rc = devm_gpio_request_one(&pdev->dev, drvdata->fw_ipc.gpio,
			GPIOF_IN, desc);

	if (rc < 0) {
		pr_err("failed to request gpio %d, error %d\n",
			drvdata->fw_ipc.gpio, rc);
		goto end;
	}

	rc = devm_request_threaded_irq(&pdev->dev,
		drvdata->fw_ipc.irq,
		NULL,
		qbt1000_ipc_irq_handler,
		IRQF_ONESHOT | IRQF_TRIGGER_RISING,
		desc,
		drvdata);

	if (rc < 0) {
		pr_err("failed to register for ipc irq %d, rc = %d\n",
			drvdata->fw_ipc.irq, rc);
		goto end;
	}

end:
	return rc;
}

/**
 * qbt1000_read_device_tree() - Function reads device tree
 * properties into driver data
 * @pdev:	ptr to platform device object
 * @drvdata:	ptr to driver data
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_read_device_tree(struct platform_device *pdev,
	struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	uint8_t clkcnt = 0;
	int index = 0;
	uint32_t rate;
	const char *clock_name;
	int gpio;
	enum of_gpio_flags flags;

	/* obtain number of clocks from hw config */
	clkcnt = of_property_count_strings(pdev->dev.of_node, "clock-names");
	if (IS_ERR_VALUE(drvdata->clock_count)) {
		pr_err("failed to get clock names\n");
		rc = -EINVAL;
		goto end;
	}

	/* sanity check for max clock count */
	if (clkcnt > 16) {
		pr_err("invalid clock count %d\n", clkcnt);
		rc = -EINVAL;
		goto end;
	}

	/* alloc mem for clock array - auto free if probe fails */
	drvdata->clock_count = clkcnt;
	pr_debug("clock count %d\n", clkcnt);
	drvdata->clocks = devm_kzalloc(&pdev->dev,
		sizeof(struct clk *) * drvdata->clock_count, GFP_KERNEL);
	if (!drvdata->clocks) {
		rc = -ENOMEM;
		goto end;
	}

	/* load clock names */
	for (index = 0; index < drvdata->clock_count; index++) {
		of_property_read_string_index(pdev->dev.of_node,
			"clock-names",
			index, &clock_name);
		pr_debug("getting clock %s\n", clock_name);
		drvdata->clocks[index] = devm_clk_get(&pdev->dev, clock_name);
		if (IS_ERR(drvdata->clocks[index])) {
			rc = PTR_ERR(drvdata->clocks[index]);
			if (rc != -EPROBE_DEFER)
				pr_err("failed to get %s\n", clock_name);
			goto end;
		}

		if (!strcmp(clock_name, "iface_clk")) {
			pr_debug("root index %d\n", index);
			drvdata->root_clk_idx = index;
		}
	}

	/* read clock frequency */
	if (of_property_read_u32(pdev->dev.of_node,
		"clock-frequency", &rate) == 0) {
		pr_debug("clk frequency %d\n", rate);
		drvdata->frequency = rate;
	}

	/* read IPC gpio */
	drvdata->fw_ipc.gpio = of_get_named_gpio(pdev->dev.of_node,
		"qcom,ipc-gpio", 0);
	if (drvdata->fw_ipc.gpio < 0) {
		rc = drvdata->fw_ipc.gpio;
		pr_err("ipc gpio not found, error=%d\n", rc);
		goto end;
	}

	/* read finger detect GPIO configuration */

	gpio = of_get_named_gpio_flags(pdev->dev.of_node,
				"qcom,finger-detect-gpio", 0, &flags);
	if (gpio < 0) {
		pr_err("failed to get gpio flags\n");
		rc = gpio;
		goto end;
	}

	drvdata->fd_gpio.gpio = gpio;
	drvdata->fd_gpio.active_low = flags & OF_GPIO_ACTIVE_LOW;

end:
	return rc;
}

/**
 * qbt1000_probe() - Function loads hardware config from device tree
 * @pdev:	ptr to platform device object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct qbt1000_drvdata *drvdata;
	int rc = 0;

	pr_debug("qbt1000_probe begin\n");
	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	rc = qbt1000_read_device_tree(pdev, drvdata);
	if (rc < 0)
		goto end;

	atomic_set(&drvdata->available, 1);

	mutex_init(&drvdata->mutex);
	mutex_init(&drvdata->fw_events_mutex);

	rc = qbt1000_dev_register(drvdata);
	if (rc < 0)
		goto end;

	INIT_KFIFO(drvdata->fw_events);
	init_waitqueue_head(&drvdata->read_wait_queue);

	rc = qbt1000_create_input_device(drvdata);
	if (rc < 0)
		goto end;

	rc = setup_fd_gpio_irq(pdev, drvdata);
	if (rc < 0)
		goto end;

	rc = setup_ipc_irq(pdev, drvdata);
	if (rc < 0)
		goto end;

	rc = device_init_wakeup(&pdev->dev, 1);
	if (rc < 0)
		goto end;

end:
	pr_debug("qbt1000_probe end : %d\n", rc);
	return rc;
}

static int qbt1000_remove(struct platform_device *pdev)
{
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	input_unregister_device(drvdata->in_dev);

	clocks_off(drvdata);
	mutex_destroy(&drvdata->mutex);
	mutex_destroy(&drvdata->fw_events_mutex);

	device_destroy(drvdata->qbt1000_class, drvdata->qbt1000_cdev.dev);
	class_destroy(drvdata->qbt1000_class);
	cdev_del(&drvdata->qbt1000_cdev);
	unregister_chrdev_region(drvdata->qbt1000_cdev.dev, 1);

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

static int qbt1000_suspend(struct platform_device *pdev, pm_message_t state)
{
	int rc = 0;
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	/*
	 * Returning an error code if driver currently making a TZ call.
	 * Note: The purpose of this driver is to ensure that the clocks are on
	 * while making a TZ call. Hence the clock check to determine if the
	 * driver will allow suspend to occur.
	 */
	if (!mutex_trylock(&drvdata->mutex))
		return -EBUSY;

	if (drvdata->clock_state)
		rc = -EBUSY;
	else {
		enable_irq_wake(drvdata->fd_gpio.irq);
		enable_irq_wake(drvdata->fw_ipc.irq);
	}

	mutex_unlock(&drvdata->mutex);

	return rc;
}

static int qbt1000_resume(struct platform_device *pdev)
{
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	disable_irq_wake(drvdata->fd_gpio.irq);
	disable_irq_wake(drvdata->fw_ipc.irq);

	return 0;
}

static const struct of_device_id qbt1000_match[] = {
	{ .compatible = "qcom,qbt1000" },
	{}
};

static struct platform_driver qbt1000_plat_driver = {
	.probe = qbt1000_probe,
	.remove = qbt1000_remove,
	.suspend = qbt1000_suspend,
	.resume = qbt1000_resume,
	.driver = {
		.name = "qbt1000",
		.owner = THIS_MODULE,
		.of_match_table = qbt1000_match,
	},
};

module_platform_driver(qbt1000_plat_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. QBT1000 driver");
