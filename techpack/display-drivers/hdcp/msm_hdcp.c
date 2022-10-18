// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[msm-hdcp] %s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/msm_hdcp.h>
#include <linux/of.h>

#define CLASS_NAME "hdcp"
#define DRIVER_NAME "msm_hdcp"

struct msm_hdcp {
	struct platform_device *pdev;
	dev_t dev_num;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct HDCP_V2V1_MSG_TOPOLOGY cached_tp;
	u32 tp_msgid;
	void *client_ctx;
	void (*cb)(void *ctx, u8 data);
};

void msm_hdcp_register_cb(struct device *dev, void *ctx,
		void (*cb)(void *ctx, u8 data))
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	hdcp->cb = cb;
	hdcp->client_ctx = ctx;
}
EXPORT_SYMBOL(msm_hdcp_register_cb);

void msm_hdcp_notify_topology(struct device *dev)
{
	char *envp[4];
	char tp[SZ_16];
	char ver[SZ_16];
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	snprintf(tp, SZ_16, "%d", DOWN_CHECK_TOPOLOGY);
	snprintf(ver, SZ_16, "%d", HDCP_V1_TX);

	envp[0] = "HDCP_MGR_EVENT=MSG_READY";
	envp[1] = tp;
	envp[2] = ver;
	envp[3] = NULL;

	kobject_uevent_env(&hdcp->device->kobj, KOBJ_CHANGE, envp);
}
EXPORT_SYMBOL(msm_hdcp_notify_topology);

void msm_hdcp_cache_repeater_topology(struct device *dev,
			struct HDCP_V2V1_MSG_TOPOLOGY *tp)
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev || !tp) {
		pr_err("invalid input\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	memcpy(&hdcp->cached_tp, tp,
		   sizeof(struct HDCP_V2V1_MSG_TOPOLOGY));
}
EXPORT_SYMBOL(msm_hdcp_cache_repeater_topology);

static ssize_t tp_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t ret = 0;
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	switch (hdcp->tp_msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		buf[MSG_ID_IDX]   = hdcp->tp_msgid;
		buf[RET_CODE_IDX] = HDCP_AUTHED;
		ret = HEADER_LEN;

		memcpy(buf + HEADER_LEN, &hdcp->cached_tp,
			   sizeof(struct HDCP_V2V1_MSG_TOPOLOGY));

		ret += sizeof(struct HDCP_V2V1_MSG_TOPOLOGY);

		/* reset the flag once the data is written back to user space */
		hdcp->tp_msgid = DOWN_REQUEST_TOPOLOGY;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t tp_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int msgid = 0;
	ssize_t ret = count;
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	msgid = buf[0];

	switch (msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		hdcp->tp_msgid = msgid;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t min_level_change_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	int min_enc_lvl;
	ssize_t ret = count;
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &min_enc_lvl);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return -EINVAL;
	}

	if (hdcp->cb && hdcp->client_ctx)
		hdcp->cb(hdcp->client_ctx, min_enc_lvl);

	return ret;
}

static DEVICE_ATTR_RW(tp);

static DEVICE_ATTR_WO(min_level_change);

static struct attribute *msm_hdcp_fs_attrs[] = {
	&dev_attr_tp.attr,
	&dev_attr_min_level_change.attr,
	NULL
};

static struct attribute_group msm_hdcp_fs_attr_group = {
	.attrs = msm_hdcp_fs_attrs
};

static int msm_hdcp_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int msm_hdcp_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations msm_hdcp_fops = {
	.owner = THIS_MODULE,
	.open = msm_hdcp_open,
	.release = msm_hdcp_close,
};

static const struct of_device_id msm_hdcp_dt_match[] = {
	{ .compatible = "qcom,msm-hdcp",},
	{}
};

MODULE_DEVICE_TABLE(of, msm_hdcp_dt_match);

static int msm_hdcp_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_hdcp *hdcp;

	hdcp = devm_kzalloc(&pdev->dev, sizeof(struct msm_hdcp), GFP_KERNEL);
	if (!hdcp)
		return -ENOMEM;

	hdcp->pdev = pdev;

	platform_set_drvdata(pdev, hdcp);

	ret = alloc_chrdev_region(&hdcp->dev_num, 0, 1, DRIVER_NAME);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		return ret;
	}

	hdcp->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(hdcp->class)) {
		ret = PTR_ERR(hdcp->class);
		pr_err("couldn't create class rc = %d\n", ret);
		goto error_class_create;
	}

	hdcp->device = device_create(hdcp->class, NULL,
		hdcp->dev_num, hdcp, DRIVER_NAME);
	if (IS_ERR(hdcp->device)) {
		ret = PTR_ERR(hdcp->device);
		pr_err("device_create failed %d\n", ret);
		goto error_class_device_create;
	}

	cdev_init(&hdcp->cdev, &msm_hdcp_fops);
	ret = cdev_add(&hdcp->cdev, MKDEV(MAJOR(hdcp->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto error_cdev_add;
	}

	ret = sysfs_create_group(&hdcp->device->kobj, &msm_hdcp_fs_attr_group);
	if (ret)
		pr_err("unable to register msm_hdcp sysfs nodes\n");

	hdcp->tp_msgid = DOWN_REQUEST_TOPOLOGY;

	return 0;
error_cdev_add:
	device_destroy(hdcp->class, hdcp->dev_num);
error_class_device_create:
	class_destroy(hdcp->class);
error_class_create:
	unregister_chrdev_region(hdcp->dev_num, 1);
	return ret;
}

static int msm_hdcp_remove(struct platform_device *pdev)
{
	struct msm_hdcp *hdcp;

	hdcp = platform_get_drvdata(pdev);
	if (!hdcp)
		return -ENODEV;

	sysfs_remove_group(&hdcp->device->kobj,
	&msm_hdcp_fs_attr_group);
	cdev_del(&hdcp->cdev);
	device_destroy(hdcp->class, hdcp->dev_num);
	class_destroy(hdcp->class);
	unregister_chrdev_region(hdcp->dev_num, 1);

	return 0;
}

static struct platform_driver msm_hdcp_driver = {
	.probe = msm_hdcp_probe,
	.remove = msm_hdcp_remove,
	.driver = {
		.name = "msm_hdcp",
		.of_match_table = msm_hdcp_dt_match,
		.pm = NULL,
	}
};

void __init msm_hdcp_register(void)
{
	platform_driver_register(&msm_hdcp_driver);
}

void __exit msm_hdcp_unregister(void)
{
	platform_driver_unregister(&msm_hdcp_driver);
}
