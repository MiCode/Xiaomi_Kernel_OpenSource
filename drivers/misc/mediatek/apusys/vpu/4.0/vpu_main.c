/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>

/* internal headers */
#include "vpu_drv.h"
#include "vpu_cmn.h"
#include "vpu_mem.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "apusys_power.h"
// #include "../mt6779/vpu_dvfs.h"  // TODO: for vpu_boost_value_to_opp()
#include "remoteproc_internal.h"  // TODO: move to drivers/remoteproc/../..

/* remote proc */
#define VPU_FIRMWARE_NAME "mtk_vpu"

/* interface to APUSYS */
int vpu_send_cmd(int op, void *hnd, struct apusys_device *adev)
{
	struct vpu_device *dev;
	struct apusys_cmd_hnd *cmd = (struct apusys_cmd_hnd *)hnd;
//	struct apusys_power_hnd *pw = (struct apusys_power_hnd *)hnd;
//	struct apusys_preempt_hnd *pmt = (struct apusys_preempt_hnd *)hnd;
//	struct apusys_firmware_hnd *fw = (struct apusys_firmware_hnd *)hnd;

	dev = (struct vpu_device *)adev->private;

	vpu_cmd_debug("%s: cmd: %d, hnd: %p\n", __func__, op, hnd);

	switch (op) {
	case APUSYS_CMD_POWERON:
		vpu_cmd_debug("%s: APUSYS_CMD_POWERON, boost: %d, opp: %d\n",
			__func__, pw->boost_val, pw->opp);
		break;
	case APUSYS_CMD_POWERDOWN:
		vpu_cmd_debug("%s: APUSYS_CMD_POWERDOWN\n", __func__);
		break;
	case APUSYS_CMD_RESUME:
		vpu_cmd_debug("%s: APUSYS_CMD_RESUME\n", __func__);
		break;
	case APUSYS_CMD_SUSPEND:
		vpu_cmd_debug("%s: APUSYS_CMD_SUSPEND\n", __func__);
		break;
	case APUSYS_CMD_EXECUTE:
		vpu_cmd_debug("%s: APUSYS_CMD_EXECUTE, kva: %lx\n",
			__func__, (unsigned long)cmd->kva);
		return vpu_execute(dev, (struct vpu_request *)cmd->kva);
	case APUSYS_CMD_PREEMPT:
		vpu_cmd_debug("%s: APUSYS_CMD_PREEMPT, new cmd kva: %lx\n",
			__func__, (unsigned long)pmt->new_cmd->kva);
		break;
	case APUSYS_CMD_FIRMWARE:
		vpu_cmd_debug("%s: APUSYS_CMD_FIRMWARE, kva: %p\n",
			__func__, fw->kva);
		break;
	default:
		vpu_cmd_debug("%s: unknown command: %d\n", __func__, cmd);
		break;
	}

	return -EINVAL;
}

static int vpu_load(struct rproc *rproc, const struct firmware *fw)
{
	return 0;
}

#if 1
// TODO: move to drivers/remoteproc/../..
static struct resource_table *
vpu_rsc_table(struct rproc *rproc, const struct firmware *fw, int *tablesz)
{
	static struct resource_table table = { .ver = 1, };

	*tablesz = sizeof(table);
	return &table;
}

static const struct rproc_fw_ops vpu_fw_ops = {
	.find_rsc_table = vpu_rsc_table,
	.load = vpu_load,
};
#endif

struct vpu_driver *vpu_drv;

void vpu_drv_release(struct kref *ref)
{
	class_destroy(vpu_drv->class);
	vpu_drv->class = NULL;
	vpu_drv_debug("%s:\n", __func__);
	kfree(vpu_drv);
	vpu_drv = NULL;
}

void vpu_drv_put(void)
{
	if (!vpu_drv)
		return;

	vpu_drv_debug("%s:\n", __func__);
	kref_put(&vpu_drv->ref, vpu_drv_release);
}

void vpu_drv_get(void)
{
	kref_get(&vpu_drv->ref);
}

static int vpu_start(struct rproc *rproc)
{
	/* enable power and clock */
	return 0;
}

static int vpu_stop(struct rproc *rproc)
{
	/* disable regulator and clock */
	return 0;
}

static void *vpu_da_to_va(struct rproc *rproc, u64 da, int len)
{
	/* convert device address to kernel virtual address */
	return 0;
}

static const struct rproc_ops vpu_ops = {
	.start = vpu_start,
	.stop = vpu_stop,
	.da_to_va = vpu_da_to_va,
};

/*---------------------------------------------------------------------------*/
/* File operations                                                           */
/*---------------------------------------------------------------------------*/
static int vpu_open(struct inode *inode, struct file *flip)
{
	struct vpu_device *dev;

	dev = container_of(inode->i_cdev, struct vpu_device, cdev);

	if (dev->state == VS_DISALBED || dev->state == VS_REMOVING) {
		pr_info("%s: %s is disabled or removed, state: %d.\n",
			__func__, dev->name, dev->state);
		return -ENODEV;
	}

	flip->private_data = dev;

	return 0;
}

#ifdef CONFIG_COMPAT
static long vpu_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case VPU_IOCTL_SET_POWER:
	case VPU_IOCTL_EARA_LOCK_POWER:
	case VPU_IOCTL_EARA_UNLOCK_POWER:
	case VPU_IOCTL_POWER_HAL_LOCK_POWER:
	case VPU_IOCTL_POWER_HAL_UNLOCK_POWER:
	case VPU_IOCTL_ENQUE_REQUEST:
	case VPU_IOCTL_DEQUE_REQUEST:
	case VPU_IOCTL_GET_ALGO_INFO:
	case VPU_IOCTL_REG_WRITE:
	case VPU_IOCTL_REG_READ:
	case VPU_IOCTL_LOAD_ALG_TO_POOL:
	case VPU_IOCTL_GET_CORE_STATUS:
	case VPU_IOCTL_CREATE_ALGO:
	case VPU_IOCTL_FREE_ALGO:
	case VPU_IOCTL_OPEN_DEV_NOTICE:
	case VPU_IOCTL_CLOSE_DEV_NOTICE:
	{
		return flip->f_op->unlocked_ioctl(flip, cmd,
					(unsigned long)compat_ptr(arg));
	}
	case VPU_IOCTL_LOCK:
	case VPU_IOCTL_UNLOCK:
	default:
		return -ENOIOCTLCMD;
		/*return vpu_ioctl(flip, cmd, arg);*/
	}
}
#endif

static long vpu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct vpu_device *dev = filp->private_data;

	switch (cmd) {
	case VPU_IOCTL_SET_POWER:
	case VPU_IOCTL_EARA_LOCK_POWER:
	case VPU_IOCTL_EARA_UNLOCK_POWER:
	case VPU_IOCTL_POWER_HAL_LOCK_POWER:
	case VPU_IOCTL_POWER_HAL_UNLOCK_POWER:
	case VPU_IOCTL_DEQUE_REQUEST:
	case VPU_IOCTL_FLUSH_REQUEST:
	case VPU_IOCTL_GET_ALGO_INFO:
	case VPU_IOCTL_REG_WRITE:
	case VPU_IOCTL_LOCK:
	case VPU_IOCTL_UNLOCK:
	case VPU_IOCTL_OPEN_DEV_NOTICE:
	case VPU_IOCTL_CLOSE_DEV_NOTICE:
	case VPU_IOCTL_SDSP_SEC_LOCK:
	case VPU_IOCTL_SDSP_SEC_UNLOCK:
		vpu_drv_debug("%s: function not implemented: %x\n",
			__func__, cmd);
			break;

	case VPU_IOCTL_ENQUE_REQUEST:
	{
		struct vpu_request *req;
//		struct vpu_power *pw;

		ret = vpu_alloc_request(&req);
		if (ret) {
			pr_info("%s: REQ: vpu_alloc_request: %d\n",
				__func__, ret);
			goto out;
		}

		ret = copy_from_user(req, (void *)arg,
			sizeof(struct vpu_request));

		if (ret) {
			pr_info("%s: REQ: copy_from_user: %d\n",
				__func__, ret);
			goto enqueue_err;
		}

		/* opp_step counted by vpu driver */
#if 0
		pw = &req->power_param;
		if (pw->boost_value != 0xff) {
			if (pw->boost_value >= 0 &&	pw->boost_value <= 100)
				pw->opp_step = pw->freq_step =
					vpu_boost_value_to_opp(pw->boost_value);
			else
				pw->opp_step = pw->freq_step = 0xFF;
		}
#endif

		if (req->priority >= VPU_REQ_MAX_NUM_PRIORITY) {
			pr_info("%s: REQ: invalid priority (%d)\n",
				__func__, req->priority);
			ret = -EINVAL;
			goto enqueue_err;
		}

		if (req->buffer_count > VPU_MAX_NUM_PORTS) {
			pr_info("%s: REQ: buffer count wrong: %d\n",
				__func__, req->buffer_count);
			ret = -EINVAL;
			goto enqueue_err;
		}

		/* run request */
		ret = vpu_send_cmd(APUSYS_CMD_EXECUTE, req, &dev->adev);

		if (ret) {
			pr_info("%s: vpu_send_cmd: %d\n", __func__, ret);
			goto enqueue_err;
		}

		/* update execution results */
		ret = copy_to_user((void *)arg, req,
			sizeof(struct vpu_request));

		if (ret) {
			pr_info("%s: REQ: copy_to_user: %d\n",
				__func__, ret);
			goto enqueue_err;
		}

enqueue_err:
		/* free the request, error happened here*/
		vpu_free_request(req);
		break;
	}

	case VPU_IOCTL_LOAD_ALG_TO_POOL:
	{
		vpu_drv_debug("%s: VPU_IOCTL_LOAD_ALG_TO_POOL is deprecated\n",
			__func__);
		break;
	}
	case VPU_IOCTL_CREATE_ALGO:
	{
		vpu_drv_debug("%s: VPU_IOCTL_CREATE_ALGO is deprecated\n",
			__func__);
		break;
	}
	case VPU_IOCTL_FREE_ALGO:
	{
		vpu_drv_debug("%s: VPU_IOCTL_FREE_ALGO is deprecated\n",
			__func__);
		break;
	}

	case VPU_IOCTL_GET_CORE_STATUS:
	{
		vpu_drv_debug("%s: VPU_IOCTL_GET_CORE_STATUS is deprecated\n",
			__func__);
		break;
	}

	default:
		vpu_drv_debug("%s: unknown command: %d\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

out:
	if (ret)
		pr_info("%s: ret=%d, cmd(%d)\n", __func__, ret, cmd);

	return ret;
}

static int vpu_release(struct inode *inode, struct file *flip)
{
	return 0;
}

static int vpu_mmap(struct file *flip, struct vm_area_struct *vma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = (vma->vm_end - vma->vm_start);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	switch (pfn) {
	default:
		pr_info("illegal hw addr for mmap!\n");
		return -EAGAIN;
	}
}

static const struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.release = vpu_release,
	.mmap = vpu_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpu_compat_ioctl,
#endif
	.unlocked_ioctl = vpu_ioctl
};

static int vpu_init_bin(void)
{
	struct device_node *node;
	uint32_t phy_addr;
	uint32_t phy_size;

	/* skip, if vpu firmware had ready been mapped */
	if (vpu_drv && vpu_drv->bin_va)
		return 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,vpu_core0");

	if (of_property_read_u32(node, "bin-phy-addr", &phy_addr) ||
		of_property_read_u32(node, "bin-size", &phy_size)) {
		pr_info("%s: unable to get vpu firmware.\n", __func__);
		return -ENODEV;
	}

	/* map vpu firmware to kernel virtual address */
	vpu_drv->bin_va = ioremap_wc(phy_addr, phy_size);
	vpu_drv->bin_pa = phy_addr;
	vpu_drv->bin_size = phy_size;

	pr_info("%s: mapped vpu firmware: pa: %ld, size: %u, kva: %p\n",
		__func__, vpu_drv->bin_pa, vpu_drv->bin_size,
		vpu_drv->bin_va);  // debug

	return 0;
}

static int vpu_init_dev_mem(struct platform_device *pdev,
	struct vpu_device *dev)
{
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "unable to get resource\n");
		return -ENODEV;
	}
	dev->reg_base = devm_ioremap_resource(&pdev->dev, res); /* IPU_BASE */

	return ret;
}

static int vpu_init_dev_irq(struct platform_device *pdev,
	struct vpu_device *dev)
{
	unsigned int irq_info[3];

	dev->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (dev->irq_num > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(pdev->dev.of_node,
				"interrupts", irq_info, ARRAY_SIZE(irq_info))) {
			dev_err(&pdev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}
		dev->irq_level = irq_info[2];
		dev_info(&pdev->dev, "irq_level (0x%x), %s(0x%x)\n",
			dev->irq_level,
			"IRQF_TRIGGER_NONE", IRQF_TRIGGER_NONE);
	} else {
		dev_err(&pdev->dev, "Invalid IRQ number: %d\n", dev->irq_num);
		return -ENODEV;
	}

	return 0;
}

static void vpu_unreg_chardev(struct platform_device *pdev,
	struct vpu_device *dev)
{
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devt, 1);
	// TODO: device_destroy(dev->ddev);
}

static int vpu_reg_chardev(struct platform_device *pdev, struct vpu_device *dev)
{
	int ret = 0;

	ret = alloc_chrdev_region(&dev->devt, 0, 1, dev->name);
	if ((ret) < 0) {
		dev_err(&pdev->dev, "alloc_chrdev_region, failed: %d\n", ret);
		return ret;
	}

	/* Attatch file operation. */
	cdev_init(&dev->cdev, &vpu_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.kobj.parent = &pdev->dev.kobj;

	/* Create character device */
	dev->ddev = device_create(vpu_drv->class, NULL,
		dev->devt, NULL, dev->name);

	if (IS_ERR(dev->ddev)) {
		ret = PTR_ERR(dev->ddev);
		dev_err(&pdev->dev,	"failed to create device: /dev/%s: %d",
			dev->name, ret);
		goto out;
	}

	/* Add to system */
	ret = cdev_add(&dev->cdev, dev->devt, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "attatch cdev failed: %d\n", ret);
		goto out;
	}

out:
	if (ret < 0)
		vpu_unreg_chardev(pdev, dev);

	return ret;
}

static int vpu_probe(struct platform_device *pdev)
{
	struct vpu_device *dev;
	struct rproc *rproc;
	int ret;

	vpu_drv_debug("%s:\n", __func__);

	rproc = rproc_alloc(&pdev->dev, pdev->name, &vpu_ops,
		VPU_FIRMWARE_NAME, sizeof(*dev));

	if (!rproc) {
		dev_err(&pdev->dev, "failed to allocate rproc\n");
		return -ENOMEM;
	}

	/* initialize device (core specific) data */
	rproc->fw_ops = &vpu_fw_ops;
	dev = (struct vpu_device *)rproc->priv;
	dev->dev = &pdev->dev;
	dev->rproc = rproc;
	platform_set_drvdata(pdev, dev);
	dev->id = vpu_drv->cores;
	snprintf(dev->name, sizeof(dev->name), "vpu%d", dev->id);

	/* allocate resources */
	ret = vpu_init_dev_mem(pdev, dev);
	if (ret)
		goto free_rproc;

	ret = vpu_init_dev_irq(pdev, dev);
	if (ret)
		goto free_rproc;

	/* device hw initialization */
	ret = vpu_init_dev_hw(pdev, dev);
	if (ret)
		goto free_rproc;

	/* power initialization */
	vpu_drv_debug("%s: apu_power_device_register call\n", __func__);
	if (dev->id != 0) { // we just need to take pdev of core0 to init power
		ret = apu_power_device_register(VPU0 + dev->id, NULL);
	} else {
		ret = apu_power_device_register(VPU0 + dev->id, pdev);
	}
	vpu_drv_debug("%s: apu_power_device_register = %d\n", __func__, ret);
	if (ret)
		goto free_rproc;

	vpu_drv_debug("%s: apu_device_power_on call\n", __func__);
	ret = apu_device_power_on((VPU0 + dev->id));
	vpu_drv_debug("%s: apu_device_power_on = %d\n", __func__, ret);
	if (ret)
		goto free_rproc;

	/* device algo initialization */
	INIT_LIST_HEAD(&dev->algo);
	ret = vpu_init_dev_algo(pdev, dev);
	if (ret)
		goto free_rproc;

	/* register device to APUSYS */
	dev->adev.dev_type = APUSYS_DEVICE_VPU;
	dev->adev.preempt_type = APUSYS_PREEMPT_WAITCOMPLETED;
	dev->adev.private = dev;
	dev->adev.send_cmd = vpu_send_cmd;
	ret = apusys_register_device(&dev->adev);
	if (ret)
		goto free_rproc;

	/* add to remoteproc */
	ret = rproc_add(rproc);
	if (ret)
		goto free_rproc;

	// TODO: remove legacy character device, we used them for UT only
	/* register character device */
	ret = vpu_reg_chardev(pdev, dev);
	if (ret)
		goto free_rproc;

	/* register debugfs nodes */
	ret = vpu_init_dev_debug(pdev, dev);
	if (ret)
		goto free_rproc;

	/* add to dev list, increment total VPU cores */
	mutex_lock(&vpu_drv->lock);
	vpu_drv_get();
	vpu_drv->cores++;
	list_add_tail(&dev->list, &vpu_drv->devs);
	mutex_unlock(&vpu_drv->lock);

	return 0;

	// TODO: add error handling free algo

free_rproc:
	rproc_free(rproc);
	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_device *dev = platform_get_drvdata(pdev);

	vpu_exit_dev_debug(pdev, dev);
	vpu_exit_dev_hw(pdev, dev);
	vpu_unreg_chardev(pdev, dev);
	vpu_exit_dev_algo(pdev, dev);
	disable_irq(dev->irq_num);
	free_irq(dev->irq_num, (void *) dev);
	apusys_unregister_device(&dev->adev);
	rproc_del(dev->rproc);
	rproc_free(dev->rproc);
	apu_device_power_off(VPU0 + dev->id);
	apu_power_device_unregister(VPU0 + dev->id);
	vpu_drv_put();

	return 0;
}

static int vpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	return 0;
}

/* device power management */
#ifdef CONFIG_PM
int vpu_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	return vpu_suspend(pdev, PMSG_SUSPEND);  // TODO: inplement vpu_suspend
}

int vpu_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	return vpu_resume(pdev);  // TODO: implement vpu resume
}

int vpu_pm_restore_noirq(struct device *device)
{
	return 0;
}

static const struct dev_pm_ops vpu_pm_ops = {
	.suspend = vpu_pm_suspend,
	.resume = vpu_pm_resume,
	.freeze = vpu_pm_suspend,
	.thaw = vpu_pm_resume,
	.poweroff = vpu_pm_suspend,
	.restore = vpu_pm_resume,
	.restore_noirq = vpu_pm_restore_noirq,
};
#endif

static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,vpu_core0",},
	{.compatible = "mediatek,vpu_core1",},
	{.compatible = "mediatek,vpu_core2",},
	{}
};

static struct platform_driver vpu_plat_drv = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.suspend = vpu_suspend,
	.resume  = vpu_resume,
	.driver  = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = vpu_of_ids,
#ifdef CONFIG_PM
		.pm = &vpu_pm_ops,
#endif
	}
};

static int __init vpu_init(void)
{
	int ret;

	vpu_drv_debug("%s: allocate vpu_drv\n", __func__);  // debug
	vpu_drv = kzalloc(sizeof(struct vpu_driver), GFP_KERNEL);

	if (!vpu_drv)
		return -ENOMEM;

	kref_init(&vpu_drv->ref);

	vpu_drv_debug("%s: vpu_init_bin\n", __func__);  // debug
	ret = vpu_init_bin();
	if (ret)
		goto error_out;

	vpu_drv_debug("%s: vpu_init_algo\n", __func__);  // debug
	ret = vpu_init_algo();
	if (ret)
		goto error_out;

	vpu_drv_debug("%s: vpu_init_debug\n", __func__);  // debug
	vpu_init_debug();

	vpu_drv_debug("%s: vpu_init_mem\n", __func__);  // debug
	vpu_init_mem();

	vpu_drv_debug("%s: class_create\n", __func__); // debug
	vpu_drv->class = class_create(THIS_MODULE, "vpudrv");
	if (IS_ERR(vpu_drv->class)) {
		ret = PTR_ERR(vpu_drv->class);
		pr_info("%s: class_create: %d\n", __func__, ret); // debug
		goto error_out;
	}
//	vpu_drv->class->shutdown = NULL;

	INIT_LIST_HEAD(&vpu_drv->devs);
	mutex_init(&vpu_drv->lock);

	vpu_drv_debug("%s: vpu_init_drv_hw\n", __func__);  // debug
	vpu_init_drv_hw();

	vpu_drv_debug("%s: platform_driver_register\n", __func__); // debug
	ret = platform_driver_register(&vpu_plat_drv);

	return ret;

error_out:
	kfree(vpu_drv);
	vpu_drv = NULL;
	return ret;
}

static void __exit vpu_exit(void)
{
	struct vpu_device *dev;
	struct list_head *ptr, *tmp;

	/* notify all devices that we are going to be removed
	 *  wait and stop all on-going requests
	 **/
	mutex_lock(&vpu_drv->lock);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		dev = list_entry(ptr, struct vpu_device, list);
		list_del(ptr);
		mutex_lock(&dev->cmd_lock);
		dev->state = VS_REMOVING;
		mutex_unlock(&dev->cmd_lock);
	}
	mutex_unlock(&vpu_drv->lock);

	vpu_exit_debug();
	vpu_exit_drv_hw();

	if (vpu_drv) {
		vpu_drv_debug("%s: iounmap\n", __func__);
		if (vpu_drv->bin_va) {
			iounmap(vpu_drv->bin_va);
			vpu_drv->bin_va = NULL;
		}

		vpu_exit_mem();
		vpu_drv_put();
	}

	vpu_drv_debug("%s: platform_driver_unregister\n", __func__);
	platform_driver_unregister(&vpu_plat_drv);
}

late_initcall(vpu_init);
module_exit(vpu_exit);
MODULE_DESCRIPTION("Mediatek VPU Driver");
MODULE_LICENSE("GPL");

