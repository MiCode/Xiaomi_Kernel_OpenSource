// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/completion.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/cpu.h>
#include <linux/of_address.h>
#include <linux/qcom_scm.h>
#include <linux/firmware.h>
#include <linux/soc/qcom/mdt_loader.h>

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

#define MAX_LEN 256
#define DEFAULT_UNISO_TIMEOUT_MS 12000
#define NUM_RESERVED_CPUS 2

const static struct {
	enum gh_vm_names val;
	const char *str;
} conversion[] = {
	{GH_PRIMARY_VM, "pvm"},
	{GH_TRUSTED_VM, "trustedvm"},
	{GH_CPUSYS_VM, "cpusys_vm"},
	{GH_OEM_VM, "oem_vm"},
};

static struct kobj_type guestvm_kobj_type = {
	.sysfs_ops = &kobj_sysfs_ops,
};

struct guestvm_loader_private {
	struct notifier_block guestvm_nb;
	struct completion vm_start;
	struct kobject vm_loader_kobj;
	struct device *dev;
	ktime_t vm_isol_start;
	char vm_name[MAX_LEN];
	bool vm_loaded;
	bool iso_needed;
	int pas_id;
	int vmid;
	u8 vm_status;
	u8 os_status;
	u16 app_status;
};

static struct timer_list guestvm_cpu_isolate_timer;
static struct completion isolation_done;
static struct work_struct unisolation_work;
static cpumask_t guestvm_isolated_cpus;
static cpumask_t guestvm_reserve_cpus;
static u32 guestvm_unisolate_timeout;

static void guestvm_isolate_cpu(void)
{
	int cpu, ret;

	for_each_cpu_and(cpu, &guestvm_reserve_cpus, cpu_online_mask) {
		ret = remove_cpu(cpu);
		if (ret) {
			pr_err("fail to offline CPU%d. ret=%d\n", cpu, ret);
			continue;
		}
		pr_info("%s: offlined cpu : %d\n", __func__, cpu);
		cpumask_set_cpu(cpu, &guestvm_isolated_cpus);
	}
}

static void guestvm_unisolate_cpu(void)
{
	int i, ret;

	for_each_cpu(i, &guestvm_isolated_cpus) {
		ret = add_cpu(i);
		if (ret) {
			pr_err("fail to online CPU%d. ret=%d\n", i, ret);
			continue;
		}
		pr_info("%s: onlined cpu : %d\n", __func__, i);

		cpumask_clear_cpu(i, &guestvm_isolated_cpus);
	}

	del_timer(&guestvm_cpu_isolate_timer);
}

static void guestvm_unisolate_work(struct work_struct *work)
{
	if (wait_for_completion_interruptible(&isolation_done))
		pr_err("%s: CPU unisolation is interrupted\n", __func__);

	guestvm_unisolate_cpu();
}

static void guestvm_timer_callback(struct timer_list *t)
{
	pr_err("%s: expired: VM app status not set\n", __func__);
	complete(&isolation_done);
}

static inline enum gh_vm_names get_gh_vm_name(const char *str)
{
	int vmid;

	for (vmid = 0; vmid < ARRAY_SIZE(conversion); ++vmid) {
		if (!strcmp(str, conversion[vmid].str))
			return conversion[vmid].val;
	}
	return GH_VM_MAX;
}

static int guestvm_loader_nb_handler(struct notifier_block *this,
					unsigned long cmd, void *data)
{
	struct guestvm_loader_private *priv;
	struct gh_rm_notif_vm_status_payload *vm_status_payload = data;
	u8 vm_status = vm_status_payload->vm_status;
	u8 os_status = vm_status_payload->os_status;
	u16 app_status = vm_status_payload->app_status;
	u64 delta;
	ktime_t now;
	int ret;

	priv = container_of(this, struct guestvm_loader_private, guestvm_nb);

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	if (priv->vmid != vm_status_payload->vmid) {
		dev_warn(priv->dev, "Expected a notification from vmid = %d, but received one from vmid = %d\n",
				priv->vmid, vm_status_payload->vmid);
		return NOTIFY_DONE;
	}

	/*
	 * Listen to STATUS_READY or STATUS_RUNNING notifications from RM.
	 * These notifications come from RM after PIL loading the VM images.
	 * Query GET_HYP_RESOURCES to populate other entities such as MessageQ
	 * and DBL.
	 */
	switch (vm_status) {
	case GH_RM_VM_STATUS_READY:
		priv->vm_status = GH_RM_VM_STATUS_READY;
		ret = gh_rm_populate_hyp_res(vm_status_payload->vmid, priv->vm_name);
		if (ret < 0) {
			dev_err(priv->dev, "Failed to get hyp resources for vmid = %d ret = %d\n",
				vm_status_payload->vmid, ret);
			complete_all(&priv->vm_start);
			return NOTIFY_DONE;
		}
		ret = gh_rm_get_vm_id_info(get_gh_vm_name(priv->vm_name),
							priv->vmid);
		if (ret < 0)
			dev_err(priv->dev, "Couldn't obtain VM ID info.\n");

		complete_all(&priv->vm_start);
		break;
	case GH_RM_VM_STATUS_RUNNING:
		dev_info(priv->dev, "vm(%d) started running\n", vm_status_payload->vmid);
		break;
	default:
		dev_err(priv->dev, "Unknown notification receieved for vmid = %d vm_status = %d\n",
				vm_status_payload->vmid, vm_status);
	}

	if (priv->os_status != os_status) {
		if (os_status == GH_RM_OS_STATUS_BOOT) {
			priv->os_status = os_status;
			now = ktime_get();
			delta = ktime_to_ns(ktime_sub(now, priv->vm_isol_start));
			dev_info(priv->dev, "VM(%d) booted in %lu ns\n",
					    priv->vmid, delta);
		}
	}

	if (priv->app_status != app_status) {
		if (app_status == GH_RM_APP_STATUS_TUI_SERVICE_BOOT) {
			priv->app_status = app_status;
			now = ktime_get();
			delta = ktime_to_ns(ktime_sub(now, priv->vm_isol_start));
			dev_info(priv->dev, "Unisolating VM(%d) cpus after %lu ns: VM app status = %d\n",
					    priv->vmid, delta, app_status);
			complete(&isolation_done);
		}
	}

	return NOTIFY_DONE;
}

static int vm_load(struct guestvm_loader_private *priv)
{
	const struct firmware *fw;
	char fw_name[32];
	struct device_node *node;
	struct resource res;
	phys_addr_t phys;
	ssize_t size;
	void *virt;
	int ret;
	struct device *dev = priv->dev;

	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(dev, "DT error getting \"memory-region\" property\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dev, "error %d getting \"memory-region\" resource\n",
			ret);
		return ret;
	}

	scnprintf(fw_name, ARRAY_SIZE(fw_name), "%s.mdt", priv->vm_name);

	ret = of_property_read_u32(dev->of_node, "qcom,pas-id", &priv->pas_id);
	if (ret) {
		dev_err(dev, "error %d getting pas-id\n", ret);
		return ret;
	}

	ret = request_firmware(&fw, fw_name, dev);
	if (ret) {
		dev_err(dev, "error %d requesting \"%s\"\n", ret, fw_name);
		return ret;
	}

	phys = res.start;
	size = (size_t)resource_size(&res);
	virt = memremap(phys, size, MEMREMAP_WC);
	if (!virt) {
		dev_err(dev, "unable to remap firmware memory\n");
		ret = -ENOMEM;
		goto release_firmware;
	}

	ret = qcom_mdt_load(dev, fw, fw_name, priv->pas_id, virt, phys, size, NULL);
	if (ret) {
		dev_err(dev, "error %d loading \"%s\"\n", ret, fw_name);
		goto mem_unmap;
	}

	ret = qcom_scm_pas_auth_and_reset(priv->pas_id);
	if (ret)
		dev_err(dev, "error %d authenticating \"%s\"\n", ret, fw_name);

mem_unmap:
	memunmap(virt);
release_firmware:
	release_firmware(fw);
	return ret;
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
		priv->vm_status = GH_RM_VM_STATUS_INIT;
		ret = gh_rm_vm_alloc_vmid(get_gh_vm_name(priv->vm_name),
							&priv->vmid);
		if (ret < 0) {
			dev_err(priv->dev, "Couldn't allocate VMID.\n");
			return count;
		}

		ret = vm_load(priv);
		if (ret) {
			dev_err(priv->dev,
				"vm_load failed with error %d\n", ret);
			return ret;
		}
		priv->vm_loaded = true;

		if (wait_for_completion_interruptible(&priv->vm_start)) {
			dev_err(priv->dev, "VM start completion interrupted\n");
			return count;
		}

		priv->vm_status = GH_RM_VM_STATUS_RUNNING;

		priv->vm_isol_start = ktime_get();
		if (priv->iso_needed) {
			INIT_WORK(&unisolation_work, guestvm_unisolate_work);
			schedule_work(&unisolation_work);
			guestvm_isolate_cpu();
			mod_timer(&guestvm_cpu_isolate_timer, jiffies +
					msecs_to_jiffies(guestvm_unisolate_timeout));
		}

		ret = gh_rm_vm_start(priv->vmid);
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
	int ret = 0, i, reserve_cpus_len;
	u32 reserve_cpus[NUM_RESERVED_CPUS] = {0};

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &sub_sys);
	if (ret)
		return -EINVAL;
	strlcpy(priv->vm_name, sub_sys, sizeof(priv->vm_name));

	ret = kobject_init_and_add(&priv->vm_loader_kobj, &guestvm_kobj_type,
			kernel_kobj, "%s_%s", "load_guestvm", priv->vm_name);

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
	init_completion(&isolation_done);
	priv->guestvm_nb.notifier_call = guestvm_loader_nb_handler;
	priv->guestvm_nb.priority = 1;
	ret = gh_rm_register_notifier(&priv->guestvm_nb);
	if (ret)
		return ret;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,vmid",
							&priv->vmid);
	if (ret)
		dev_err(&pdev->dev, "Unable to get vmid from DT, ret=%d\n", ret);

	priv->iso_needed = of_property_read_bool(pdev->dev.of_node,
							"qcom,isolate-cpus");
	if (!priv->iso_needed) {
		pr_err("%s: no isolation needed for %s\n", __func__, priv->vm_name);
		goto no_iso;
	}

	reserve_cpus_len = of_property_read_variable_u32_array(
					pdev->dev.of_node,
					"qcom,reserved-cpus",
					reserve_cpus, 0, NUM_RESERVED_CPUS);
	for (i = 0; i < reserve_cpus_len; i++)
		if (reserve_cpus[i] < num_possible_cpus())
			cpumask_set_cpu(reserve_cpus[i], &guestvm_reserve_cpus);

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,unisolate-timeout-ms",
				&guestvm_unisolate_timeout);
	if (ret) {
		pr_warn("%s: no unisolate timeout specified\n", __func__);
		guestvm_unisolate_timeout = DEFAULT_UNISO_TIMEOUT_MS;
	}
	timer_setup(&guestvm_cpu_isolate_timer, guestvm_timer_callback, 0);

no_iso:
	priv->vm_status = GH_RM_VM_STATUS_NO_STATE;
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
		qcom_scm_pas_shutdown(priv->pas_id);
		priv->vm_loaded = false;
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
