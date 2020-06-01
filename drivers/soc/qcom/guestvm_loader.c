// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/haven/hh_common.h>
#include <linux/haven/hh_rm_drv.h>

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

#define MAX_LEN 256

const static struct {
	enum hh_vm_names val;
	const char *str;
} conversion[] = {
	{HH_PRIMARY_VM, "pvm"},
	{HH_TRUSTED_VM, "trustedvm"},
};

static struct kobj_type guestvm_kobj_type = {
	.sysfs_ops = &kobj_sysfs_ops,
};

struct guestvm_loader_private {
	struct work_struct vm_loader_work;
	struct kobject vm_loader_kobj;
	struct device *dev;
	char vm_name[MAX_LEN];
	void *vm_loaded;
	int vmid;
};

static inline enum hh_vm_names get_hh_vm_name(const char *str)
{
	int vmid;

	for (vmid = 0; vmid < ARRAY_SIZE(conversion); ++vmid) {
		if (!strcmp(str, conversion[vmid].str))
			return conversion[vmid].val;
	}
	return HH_VM_MAX;
}

static void guestvm_loader_rm_notifier(struct work_struct *vm_loader_work)
{
	struct guestvm_loader_private *priv;
	int ret = 0;

	priv = container_of(vm_loader_work, struct guestvm_loader_private,
				vm_loader_work);
	priv->vmid = hh_rm_vm_alloc_vmid(get_hh_vm_name(priv->vm_name));
	if (priv->vmid == HH_VM_MAX) {
		dev_err(priv->dev, "Couldn't get vmid.\n");
		return;
	}
	ret = hh_rm_vm_start(priv->vmid);
	if (ret)
		dev_err(priv->dev, "VM start has failed with %d.\n", ret);
}

static ssize_t guestvm_load_start(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	struct guestvm_loader_private *priv;
	int ret = 0;
	bool boot = false;

	ret = kstrtobool(buf, &boot);
	if (ret)
		return -EINVAL;

	priv = container_of(kobj, struct guestvm_loader_private,
				vm_loader_kobj);
	if (priv->vm_loaded) {
		dev_err(priv->dev, "VM load has already been started\n");
		return -EINVAL;
	}

	if (boot) {
		priv->vm_loaded = subsystem_get(priv->vm_name);
		if (IS_ERR(priv->vm_loaded)) {
			ret = (int)(PTR_ERR(priv->vm_loaded));
			dev_err(priv->dev,
				"subsystem_get failed with error %d\n", ret);
			priv->vm_loaded = NULL;
			return ret;
		}
		schedule_work(&priv->vm_loader_work);
	}

	return count;
}
static struct kobj_attribute guestvm_loader_attribute =
__ATTR(boot_guestvm, 0220, NULL, guestvm_load_start);

static struct attribute *attrs[] = {
	&guestvm_loader_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int guestvm_loader_probe(struct platform_device *pdev)
{
	struct guestvm_loader_private *priv = NULL;
	const char *sub_sys;
	int ret = 0;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;

	ret = of_property_read_string(pdev->dev.of_node, "image_to_be_loaded",
		 &sub_sys);
	if (ret)
		return -EINVAL;
	strlcpy(priv->vm_name, sub_sys, sizeof(priv->vm_name));

	INIT_WORK(&priv->vm_loader_work, guestvm_loader_rm_notifier);

	ret = kobject_init_and_add(&priv->vm_loader_kobj, &guestvm_kobj_type,
				   kernel_kobj, "load_guestvm");
	if (ret) {
		dev_err(&pdev->dev, "sysfs create and add failed\n");
		ret = -ENOMEM;
		goto error_return;
	}

	ret = sysfs_create_group(&priv->vm_loader_kobj, &attr_group);
	if (ret) {
		dev_err(&pdev->dev, "sysfs create group failed %d\n", ret);
		goto error_return;
	}

	return 0;

error_return:
	if (kobject_name(&priv->vm_loader_kobj) != NULL) {
		kobject_del(&priv->vm_loader_kobj);
		kobject_put(&priv->vm_loader_kobj);

		memset(&priv->vm_loader_kobj, 0, sizeof(priv->vm_loader_kobj));
	}

	return ret;
}

static int guestvm_loader_remove(struct platform_device *pdev)
{
	struct guestvm_loader_private *priv = platform_get_drvdata(pdev);

	if (priv->vm_loaded)
		subsystem_put(priv->vm_loaded);

	if (kobject_name(&priv->vm_loader_kobj) != NULL) {
		kobject_del(&priv->vm_loader_kobj);
		kobject_put(&priv->vm_loader_kobj);

		memset(&priv->vm_loader_kobj, 0, sizeof(priv->vm_loader_kobj));
	}

	return 0;
}

static const struct of_device_id guestvm_loader_match_table[] = {
	{ .compatible = "qcom,guestvm-loader" },
	{},
};

static struct platform_driver guestvm_loader_driver = {
	.probe = guestvm_loader_probe,
	.remove = guestvm_loader_remove,
	.driver = {
		.name = "qcom_guestvm_loader",
		.of_match_table = guestvm_loader_match_table,
	},
};

module_platform_driver(guestvm_loader_driver);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. GuestVM loader");
MODULE_LICENSE("GPL v2");
