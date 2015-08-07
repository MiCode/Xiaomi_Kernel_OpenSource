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

#define pr_fmt(fmt) "qbt1000:%s: " fmt, __func__

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
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <uapi/linux/qbt1000.h>
#include "qseecom_kernel.h"

#define QBT1000_DEV "qbt1000"

/*
 * shared buffer size - init with max value,
 * user space will provide new value upon tz app load
 */
static uint32_t g_app_buf_size = SZ_256K;

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
	*cmd_len = ALIGN(*cmd_len, 64);
	*rsp_len = ALIGN(*rsp_len, 64);

	if ((*rsp_len + *cmd_len) > g_app_buf_size) {
		pr_err("buffer too small to hold cmd=%d and rsp=%d\n",
			*cmd_len, *rsp_len);
		return -ENOMEM;
	}

	*cmd = hdl->sbuf;
	*rsp = hdl->sbuf + *cmd_len;
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

	mutex_lock(&drvdata->mutex);

	if (!drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++) {
			rc = clk_prepare_enable(drvdata->clocks[index]);
			if (rc) {
				pr_err("failure to prepare clk at idx:%d\n",
					index);
				goto unprepare;
			}
			if (index == drvdata->root_clk_idx) {
				rc = clk_set_rate(drvdata->clocks[index],
					drvdata->frequency);
				if (rc) {
					pr_err("failure set clock rate at idx:%d\n",
						index);
					goto unprepare;
				}
			}
		}
		drvdata->clock_state = 1;
	}
	goto end;

unprepare:
	for (--index; index >= 0; index--)
		clk_disable_unprepare(drvdata->clocks[index]);

end:
	mutex_unlock(&drvdata->mutex);
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

	mutex_lock(&drvdata->mutex);
	if (drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++)
			clk_disable_unprepare(drvdata->clocks[index]);
		drvdata->clock_state = 0;
	}
	mutex_unlock(&drvdata->mutex);
}

/**
 * qbt1000_open() - Function called when user space opens device.
 * Successful if driver not currently open and clocks turned on.
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

	/* disallowing concurrent opens */
	if (!atomic_dec_and_test(&drvdata->available)) {
		atomic_inc(&drvdata->available);
		rc = -EBUSY;
	} else {
		/*
		 * return success/err of clock vote.
		 * and increment atomic counter on failure
		 */
		rc = clocks_on(drvdata);
		if (rc)
			atomic_inc(&drvdata->available);
	}

	return rc;
}

/**
 * qbt1000_release() - Function called when user space closes device.
 *                     SPI Clocks turn off.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_release(struct inode *inode, struct file *file)
{
	struct qbt1000_drvdata *drvdata = file->private_data;

	clocks_off(drvdata);
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

	if (IS_ERR(priv_arg)) {
		pr_err("invalid user space pointer %lu\n", arg);
		return -EINVAL;
	}

	drvdata = file->private_data;

	mutex_lock(&drvdata->mutex);
	if (!drvdata->clock_state) {
		rc = -EPERM;
		pr_err("IOCTL call made with clocks off\n");
		goto end;
	}

	switch (cmd) {
	case QBT1000_LOAD_APP:
	{
		struct qbt1000_app app;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -ENOMEM;
			pr_err("failed copy from user space-LOAD\n");
			goto end;
		}

		/* start the TZ app */
		rc = qseecom_start_app(app.app_handle, app.name, app.size);
		if (rc == 0) {
			g_app_buf_size = app.size;
			rc = qseecom_set_bandwidth(*app.app_handle,
				app.high_band_width == 1 ? true : false);
			if (rc != 0) {
				/* log error, allow to continue */
				pr_err("App %s failed to set bw\n", app.name);
			}
		} else {
			pr_err("app %s failed to load\n", app.name);
			goto end;
		}

		break;
	}
	case QBT1000_UNLOAD_APP:
	{
		struct qbt1000_app app;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -ENOMEM;
			pr_err("failed copy from user space-UNLOAD\n");
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!app.app_handle) {
			pr_err("app not loaded\n");
			rc = -EINVAL;
			goto end;
		}

		/* set bw & shutdown the TZ app */
		qseecom_set_bandwidth(*app.app_handle,
			app.high_band_width == 1 ? true : false);
		rc = qseecom_shutdown_app(app.app_handle);
		if (rc != 0) {
			pr_err("app failed to shutdown\n");
			goto end;
		}

		break;
	}
	case QBT1000_SEND_TZCMD:
	{
		void *aligned_cmd;
		void *aligned_rsp;
		uint32_t aligned_cmd_len;
		uint32_t aligned_rsp_len;

		struct qbt1000_send_tz_cmd tzcmd;

		if (copy_from_user(&tzcmd, priv_arg,
			sizeof(tzcmd))
				!= 0) {
			rc = -ENOMEM;
			pr_err("failed copy from user space %d\n", rc);
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!tzcmd.app_handle) {
			pr_err("app not loaded\n");
			rc = -EINVAL;
			goto end;
		}

		/* init command and response buffers and align lengths */
		aligned_cmd_len = tzcmd.req_buf_len;
		aligned_rsp_len = tzcmd.rsp_buf_len;
		rc = get_cmd_rsp_buffers(tzcmd.app_handle,
			(void **)&aligned_cmd,
			&aligned_cmd_len,
			(void **)&aligned_rsp,
			&aligned_rsp_len);
		if (rc != 0)
			goto end;

		rc = copy_from_user(aligned_cmd, (void __user *)tzcmd.req_buf,
				tzcmd.req_buf_len);
		if (rc != 0) {
			pr_err("failure to copy user space buf %d\n", rc);
			goto end;
		}

		/* send cmd to TZ */
		rc = qseecom_send_command(tzcmd.app_handle,
			aligned_cmd,
			aligned_cmd_len,
			aligned_rsp,
			aligned_rsp_len);
		if (rc == 0) {
			/* copy rsp buf back to user space unaligned buffer */
			rc = copy_to_user((void __user *)tzcmd.rsp_buf,
				 aligned_rsp, tzcmd.rsp_buf_len);
			if (rc != 0) {
				pr_err("failed copy 2us rc:%d bytes %d:\n",
					rc, tzcmd.rsp_buf_len);
				goto end;
			}
		} else {
			pr_err("failure to send tz cmd %d\n", rc);
			goto end;
		}

		break;
	}
	default:
		pr_err("invalid cmd %d\n", cmd);
		rc = -EINVAL;
		goto end;
	}

end:
	mutex_unlock(&drvdata->mutex);
	return rc;
}

static const struct file_operations qbt1000_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qbt1000_ioctl,
	.open = qbt1000_open,
	.release = qbt1000_release
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
	int index = 0;
	uint32_t rate;
	uint8_t clkcnt = 0;
	const char *clock_name;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

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

		if (!strcmp(clock_name, "spi_clk")) {
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

	atomic_set(&drvdata->available, 1);

	mutex_init(&drvdata->mutex);

	rc = qbt1000_dev_register(drvdata);

end:
	return rc;
}

static int qbt1000_remove(struct platform_device *pdev)
{
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	clocks_off(drvdata);
	mutex_destroy(&drvdata->mutex);

	device_destroy(drvdata->qbt1000_class, drvdata->qbt1000_cdev.dev);
	class_destroy(drvdata->qbt1000_class);
	cdev_del(&drvdata->qbt1000_cdev);
	unregister_chrdev_region(drvdata->qbt1000_cdev.dev, 1);
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
	mutex_lock(&drvdata->mutex);
	if (drvdata->clock_state)
		rc = -EBUSY;
	mutex_unlock(&drvdata->mutex);

	return rc;
}

static struct of_device_id qbt1000_match[] = {
	{ .compatible = "qcom,qbt1000" },
	{}
};

static struct platform_driver qbt1000_plat_driver = {
	.probe = qbt1000_probe,
	.remove = qbt1000_remove,
	.suspend = qbt1000_suspend,
	.driver = {
		.name = "qbt1000",
		.owner = THIS_MODULE,
		.of_match_table = qbt1000_match,
	},
};

static int qbt1000_init(void)
{
	return platform_driver_register(&qbt1000_plat_driver);
}
module_init(qbt1000_init);

static void qbt1000_exit(void)
{
	platform_driver_unregister(&qbt1000_plat_driver);
}
module_exit(qbt1000_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. QBT1000 driver");
