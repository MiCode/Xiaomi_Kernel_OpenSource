// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/qcom_scm.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/mdt_loader.h>

#include <linux/gunyah/gh_vm.h>
#include <linux/gunyah/gh_rm_drv.h>

#include "gh_vm_loader_private.h"

/* Structure per VM device node */
struct gh_sec_vm_dev {
	struct list_head list;
	const char *vm_name;
	struct device *dev;
	phys_addr_t fw_phys;
	void *fw_virt;
	ssize_t fw_size;
	int pas_id;
	int vmid;
};

/* Structure per VM: Binds struct gh_vm_struct and struct gh_sec_vm_dev */
struct gh_sec_vm_struct {
	struct gh_vm_struct *vm_struct;
	struct gh_sec_vm_dev *vm_dev;
	struct mutex vm_lock;
	int vmid;
	struct gh_vm_status vm_status;
	wait_queue_head_t vm_status_wait;
};

#define GH_VM_LOADER_SEC_STATUS_TIMEOUT_MS 5000

static DEFINE_SPINLOCK(gh_sec_vm_devs_lock);
static LIST_HEAD(gh_sec_vm_devs);

static struct gh_sec_vm_dev *get_sec_vm_dev_by_name(const char *vm_name)
{
	struct gh_sec_vm_dev *sec_vm_dev;

	spin_lock(&gh_sec_vm_devs_lock);

	list_for_each_entry(sec_vm_dev, &gh_sec_vm_devs, list) {
		if (!strcmp(sec_vm_dev->vm_name, vm_name)) {
			spin_unlock(&gh_sec_vm_devs_lock);
			return sec_vm_dev;
		}
	}

	spin_unlock(&gh_sec_vm_devs_lock);

	return NULL;
}

static int
gh_vm_loader_wait_for_vm_status(struct gh_sec_vm_struct *sec_vm_struct,
				int wait_status)
{
	struct device *dev = sec_vm_struct->vm_dev->dev;
	long timeleft;

	timeleft = wait_event_interruptible_timeout(
			sec_vm_struct->vm_status_wait,
			sec_vm_struct->vm_status.vm_status == wait_status,
			msecs_to_jiffies(GH_VM_LOADER_SEC_STATUS_TIMEOUT_MS));
	if (timeleft < 0) {
		dev_err(dev, "Wait for VM_STATUS %d interrupt\n", wait_status);
		return -ERESTARTSYS;
	} else if (timeleft == 0) {
		dev_err(dev, "Wait for VM_STATUS %d timed out\n", wait_status);
		return -ETIMEDOUT;
	}

	return 0;
}

static int
gh_vm_loader_wait_for_os_status(struct gh_sec_vm_struct *sec_vm_struct,
				int wait_status)
{
	struct device *dev = sec_vm_struct->vm_dev->dev;
	long timeleft;

	timeleft = wait_event_interruptible_timeout(
			sec_vm_struct->vm_status_wait,
			sec_vm_struct->vm_status.os_status == wait_status,
			msecs_to_jiffies(GH_VM_LOADER_SEC_STATUS_TIMEOUT_MS));
	if (timeleft < 0) {
		dev_err(dev, "Wait for OS_STATUS %d interrupt\n", wait_status);
		return -ERESTARTSYS;
	} else if (timeleft == 0) {
		dev_err(dev, "Wait for OS_STATUS %d timed out\n", wait_status);
		return -ETIMEDOUT;
	}

	return 0;
}

static void
gh_vm_loader_sec_notif_vm_status(struct gh_sec_vm_struct *sec_vm_struct,
				struct gh_rm_notif_vm_status_payload *vm_status)
{
	struct gh_sec_vm_dev *vm_dev = sec_vm_struct->vm_dev;
	struct device *dev = vm_dev->dev;
	int vmid = sec_vm_struct->vmid;

	if (vmid != vm_status->vmid) {
		dev_warn(dev, "VM_STATUS notification not for this vmid\n");
		return;
	}

	/* Wake up the waiters only if there's a change in any of the states */
	if (vm_status->vm_status != sec_vm_struct->vm_status.vm_status) {
		switch (vm_status->vm_status) {
		case GH_RM_VM_STATUS_RUNNING:
			dev_info(dev, "VM:%d started running\n", vmid);
			break;
		}

		sec_vm_struct->vm_status.vm_status = vm_status->vm_status;
		wake_up_interruptible(&sec_vm_struct->vm_status_wait);
	}

	if (vm_status->os_status != sec_vm_struct->vm_status.os_status) {
		switch (vm_status->os_status) {
		case GH_RM_OS_STATUS_BOOT:
			dev_info(dev, "VM:%d OS booted\n", vmid);
			break;
		}

		sec_vm_struct->vm_status.os_status = vm_status->os_status;
		wake_up_interruptible(&sec_vm_struct->vm_status_wait);
	}
}

static void gh_vm_loader_sec_rm_notifier(struct gh_vm_struct *vm_struct,
					unsigned long cmd, void *data)
{
	struct gh_sec_vm_struct *sec_vm_struct;

	sec_vm_struct = gh_vm_loader_get_loader_data(vm_struct);
	if (!sec_vm_struct)
		return;

	switch (cmd) {
	case GH_RM_NOTIF_VM_STATUS:
		gh_vm_loader_sec_notif_vm_status(sec_vm_struct, data);
		break;
	}
}

static int gh_vm_loader_sec_load(struct gh_sec_vm_struct *sec_vm_struct)
{
	struct gh_sec_vm_dev *vm_dev = sec_vm_struct->vm_dev;
	struct device *dev = vm_dev->dev;
	const struct firmware *fw;
	char fw_name[32];
	int ret;

	scnprintf(fw_name, ARRAY_SIZE(fw_name), "%s.mdt", vm_dev->vm_name);

	ret = request_firmware(&fw, fw_name, dev);
	if (ret) {
		dev_err(dev, "Error requesting fw \"%s\": %d\n", fw_name, ret);
		return ret;
	}

	ret = qcom_mdt_load(dev, fw, fw_name, vm_dev->pas_id, vm_dev->fw_virt,
				vm_dev->fw_phys, vm_dev->fw_size, NULL);
	if (ret) {
		dev_err(dev, "Failed to load fw \"%s\": %d\n", fw_name, ret);
		goto release_fw;
	}

	ret = qcom_scm_pas_auth_and_reset(vm_dev->pas_id);
	if (ret)
		dev_err(dev, "Failed to auth and reset: %d\n", ret);

release_fw:
	release_firmware(fw);
	return ret;
}

static int gh_vm_loader_sec_start(struct gh_vm_struct *vm_struct)
{
	struct gh_sec_vm_struct *sec_vm_struct;
	struct gh_sec_vm_dev *vm_dev;
	enum gh_vm_names vm_name_val;
	struct device *dev;
	int ret, vmid;

	sec_vm_struct = gh_vm_loader_get_loader_data(vm_struct);
	if (!sec_vm_struct)
		return -EINVAL;

	vm_dev = sec_vm_struct->vm_dev;
	dev = vm_dev->dev;

	mutex_lock(&sec_vm_struct->vm_lock);

	if (sec_vm_struct->vm_status.vm_status != GH_RM_VM_STATUS_INIT) {
		dev_err(dev, "VM already loaded\n");
		ret = -EBUSY;
		goto err_unlock;
	}

	vm_name_val = gh_vm_loader_get_name_val(vm_struct);
	ret = gh_rm_vm_alloc_vmid(vm_name_val, &vm_dev->vmid);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain the vmid: %d\n", ret);
		goto err_unlock;
	}
	vmid = vm_dev->vmid;
	sec_vm_struct->vmid = vmid;

	ret = gh_vm_loader_sec_load(sec_vm_struct);
	if (ret)
		goto err_dealloc_vmid;

	ret = gh_vm_loader_wait_for_vm_status(sec_vm_struct,
						GH_RM_VM_STATUS_READY);
	if (ret)
		goto err_unload;

	ret = gh_rm_populate_hyp_res(vmid, vm_dev->vm_name);
	if (ret < 0) {
		dev_err(dev, "Failed to populate hyp res: %d\n", ret);
		goto err_unload;
	}

	ret = gh_rm_vm_start(vmid);
	if (ret) {
		dev_err(dev, "Failed to start the VM: %d\n", ret);
		goto err_unpopulate_hyp_res;
	}

	ret = gh_vm_loader_wait_for_vm_status(sec_vm_struct,
						GH_RM_VM_STATUS_RUNNING);
	if (ret)
		goto err_reset_vm;

	ret = gh_vm_loader_wait_for_os_status(sec_vm_struct,
						GH_RM_OS_STATUS_BOOT);
	if (ret)
		goto err_reset_vm;

	mutex_unlock(&sec_vm_struct->vm_lock);

	return 0;

err_reset_vm:
	gh_rm_vm_reset(vmid);
err_unpopulate_hyp_res:
	gh_rm_unpopulate_hyp_res(vmid, vm_dev->vm_name);
err_unload:
	qcom_scm_pas_shutdown(vm_dev->pas_id);
err_dealloc_vmid:
	gh_rm_vm_dealloc_vmid(vmid);
err_unlock:
	mutex_unlock(&sec_vm_struct->vm_lock);
	return ret;
}

static long gh_vm_loader_sec_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	struct gh_vm_struct *vm_struct = file->private_data;

	switch (cmd) {
	case GH_VM_SEC_START:
		return gh_vm_loader_sec_start(vm_struct);
	default:
		pr_err("Invalid IOCTL for secure loader\n");
		break;
	}

	return -EINVAL;
}

static int gh_vm_loader_sec_release(struct inode *inode, struct file *file)
{
	struct gh_vm_struct *vm_struct = file->private_data;
	struct gh_sec_vm_struct *sec_vm_struct;

	sec_vm_struct = gh_vm_loader_get_loader_data(vm_struct);
	if (sec_vm_struct) {
		gh_vm_loader_set_loader_data(vm_struct, NULL);
		mutex_destroy(&sec_vm_struct->vm_lock);
		kfree(sec_vm_struct);
	}

	/* vm_struct should not be accessed after this */
	gh_vm_loader_destroy_vm(vm_struct);

	return 0;
}

static const struct file_operations gh_vm_loader_sec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_vm_loader_sec_ioctl,
	.release = gh_vm_loader_sec_release,
};

static int gh_vm_loader_sec_vm_init(struct gh_vm_struct *vm_struct)
{
	struct gh_sec_vm_struct *sec_vm_struct;
	struct gh_sec_vm_dev *vm_dev;
	const char *vm_name;

	vm_name = gh_vm_loader_get_name(vm_struct);
	vm_dev = get_sec_vm_dev_by_name(vm_name);
	if (!vm_dev) {
		pr_err("Unable to find a registered secure VM by name: %s\n",
			vm_name);
		return -ENODEV;
	}

	sec_vm_struct = kzalloc(sizeof(*sec_vm_struct), GFP_KERNEL);
	if (!sec_vm_struct)
		return -ENOMEM;

	mutex_init(&sec_vm_struct->vm_lock);
	init_waitqueue_head(&sec_vm_struct->vm_status_wait);

	sec_vm_struct->vm_struct = vm_struct;
	sec_vm_struct->vm_dev = vm_dev;
	sec_vm_struct->vm_status.vm_status = GH_RM_VM_STATUS_INIT;
	sec_vm_struct->vmid = -EINVAL;

	gh_vm_loader_set_loader_data(vm_struct, sec_vm_struct);

	return 0;
}

static int gh_vm_loader_mem_probe(struct gh_sec_vm_dev *sec_vm_dev)
{
	struct device *dev = sec_vm_dev->dev;
	struct device_node *node;
	struct resource res;
	phys_addr_t phys;
	ssize_t size;
	void *virt;
	int ret;

	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(dev, "DT error getting \"memory-region\"\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dev, "error %d getting \"memory-region\" resource\n",
			ret);
		goto err_of_node_put;
	}

	phys = res.start;
	size = (size_t)resource_size(&res);
	virt = memremap(phys, size, MEMREMAP_WC);
	if (!virt) {
		dev_err(dev, "Unable to remap firmware memory\n");
		ret = -ENOMEM;
		goto err_of_node_put;
	}

	sec_vm_dev->fw_phys = phys;
	sec_vm_dev->fw_virt = virt;
	sec_vm_dev->fw_size = size;

err_of_node_put:
	of_node_put(node);
	return ret;
}

static int gh_vm_loader_sec_probe(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	struct device *dev = &pdev->dev;
	int ret;

	sec_vm_dev = devm_kzalloc(dev, sizeof(*sec_vm_dev), GFP_KERNEL);
	if (!sec_vm_dev)
		return -ENOMEM;

	sec_vm_dev->dev = dev;
	platform_set_drvdata(pdev, sec_vm_dev);

	ret = of_property_read_u32(dev->of_node,
				"qcom,pas-id", &sec_vm_dev->pas_id);
	if (ret) {
		dev_err(dev, "DT error getting \"qcom,pas-id\": %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node,
				"qcom,vmid", &sec_vm_dev->vmid);
	if (ret) {
		dev_err(dev, "DT error getting \"qcom,vmid\": %d\n", ret);
		return ret;
	}

	ret = gh_vm_loader_mem_probe(sec_vm_dev);
	if (ret)
		return ret;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &sec_vm_dev->vm_name);
	if (ret)
		goto err_unmap_fw;

	spin_lock(&gh_sec_vm_devs_lock);
	list_add(&sec_vm_dev->list, &gh_sec_vm_devs);
	spin_unlock(&gh_sec_vm_devs_lock);

	return 0;

err_unmap_fw:
	memunmap(sec_vm_dev->fw_virt);
	return ret;
}

static int gh_vm_loader_sec_remove(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;

	sec_vm_dev = platform_get_drvdata(pdev);

	spin_lock(&gh_sec_vm_devs_lock);
	list_del(&sec_vm_dev->list);
	spin_unlock(&gh_sec_vm_devs_lock);

	memunmap(sec_vm_dev->fw_virt);

	return 0;
}

static const struct of_device_id gh_vm_loader_sec_match_table[] = {
	{ .compatible = "qcom,gh-vm-loader-sec" },
	{},
};

static struct platform_driver gh_vm_loader_sec_drv = {
	.probe = gh_vm_loader_sec_probe,
	.remove = gh_vm_loader_sec_remove,
	.driver = {
		.name = "gh_vm_loader_sec",
		.of_match_table = gh_vm_loader_sec_match_table,
	},
};

int gh_vm_loader_sec_init(void)
{
	return platform_driver_register(&gh_vm_loader_sec_drv);
}

void gh_vm_loader_sec_exit(void)
{
	platform_driver_unregister(&gh_vm_loader_sec_drv);
}

struct gh_vm_loader_info gh_vm_sec_loader_info = {
	.vm_fops = &gh_vm_loader_sec_fops,
	.gh_vm_loader_vm_init = gh_vm_loader_sec_vm_init,
	.gh_vm_loader_init = gh_vm_loader_sec_init,
	.gh_vm_loader_exit = gh_vm_loader_sec_exit,
	.gh_vm_loader_rm_notifier = gh_vm_loader_sec_rm_notifier,
};
