// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/completion.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
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
	struct notifier_block guestvm_nb;
	struct completion vm_start;
	struct kobject vm_loader_kobj;
	struct device *dev;
	char vm_name[MAX_LEN];
	void *vm_loaded;
	int vmid;
	u8 vm_status;
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

static int guestvm_loader_nb_handler(struct notifier_block *this,
					unsigned long cmd, void *data)
{
	struct guestvm_loader_private *priv;
	struct hh_rm_notif_vm_status_payload *vm_status_payload = data;
	u8 vm_status = vm_status_payload->vm_status;
	int ret;

	priv = container_of(this, struct guestvm_loader_private, guestvm_nb);

	if (cmd != HH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	if (priv->vmid != vm_status_payload->vmid)
		dev_warn(priv->dev, "Expected a notification from vmid = %d, but received one from vmid = %d\n",
				priv->vmid, vm_status_payload->vmid);

	/*
	 * Listen to STATUS_READY or STATUS_RUNNING notifications from RM.
	 * These notifications come from RM after PIL loading the VM images.
	 * Query GET_HYP_RESOURCES to populate other entities such as MessageQ
	 * and DBL.
	 */
	switch (vm_status) {
	case HH_RM_VM_STATUS_READY:
		priv->vm_status = HH_RM_VM_STATUS_READY;
		ret = hh_rm_populate_hyp_res(vm_status_payload->vmid);
		if (ret < 0) {
			dev_err(priv->dev, "Failed to get hyp resources for vmid = %d ret = %d\n",
				vm_status_payload->vmid, ret);
			return NOTIFY_DONE;
		}
		complete_all(&priv->vm_start);
		break;
	case HH_RM_VM_STATUS_RUNNING:
		dev_info(priv->dev, "vm(%d) started running\n", vm_status_payload->vmid);
		break;
	default:
		dev_err(priv->dev, "Unknown notification receieved for vmid = %d vm_status = %d\n",
				vm_status_payload->vmid, vm_status);
	}

	return NOTIFY_DONE;
}

static ssize_t guestvm_loader_start(struct kobject *kobj,
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
		priv->vm_status = HH_RM_VM_STATUS_INIT;
		priv->vmid = hh_rm_vm_alloc_vmid(get_hh_vm_name(priv->vm_name));
		if (priv->vmid < 0) {
			dev_err(priv->dev, "Couldn't allocate VMID.\n");
			return count;
		}

		priv->vm_loaded = subsystem_get(priv->vm_name);
		if (IS_ERR(priv->vm_loaded)) {
			ret = (int)(PTR_ERR(priv->vm_loaded));
			dev_err(priv->dev,
				"subsystem_get failed with error %d\n", ret);
			priv->vm_loaded = NULL;
			return ret;
		}
		if (wait_for_completion_interruptible(&priv->vm_start))
			dev_err(priv->dev, "VM start completion interrupted\n");

		priv->vm_status = HH_RM_VM_STATUS_RUNNING;
		ret = hh_rm_vm_start(priv->vmid);
		if (ret)
			dev_err(priv->dev, "VM start failed for vmid = %d ret = %d\n",
				priv->vmid, ret);
	}

	return count;
}
static struct kobj_attribute guestvm_loader_attribute =
__ATTR(boot_guestvm, 0220, NULL, guestvm_loader_start);

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

	init_completion(&priv->vm_start);
	priv->guestvm_nb.notifier_call = guestvm_loader_nb_handler;
	ret = hh_rm_register_notifier(&priv->guestvm_nb);
	if (ret)
		return ret;

	priv->vm_status = HH_RM_VM_STATUS_NO_STATE;
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

	if (priv->vm_loaded) {
		subsystem_put(priv->vm_loaded);
		init_completion(&priv->vm_start);
	}

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
