/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

#include <linux/init.h>
#include <linux/io.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_platform.h>
#endif

#include "reviser_drv.h"
#include "reviser_ioctl.h"
#include "reviser_cmn.h"
#include "reviser_hw.h"
#include "reviser_dbg.h"
#include "reviser_mem_mgt.h"
#include "apusys_power.h"

/* define */
#define APUSYS_DRV_NAME "apusys_drv_reviser"
#define APUSYS_DEV_NAME "apusys_reviser"

/* global variable */
static struct class *reviser_class;
struct reviser_dev_info *g_reviser_device;
static struct task_struct *mem_task;
static int g_ioctl_enable;


/* function declaration */
static int reviser_open(struct inode *, struct file *);
static int reviser_release(struct inode *, struct file *);
static long reviser_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg);
static long reviser_compat_ioctl(struct file *, unsigned int, unsigned long);

static void reviser_power_on_cb(void *para);
static void reviser_power_off_cb(void *para);


irqreturn_t reviser_interrupt(int irq, void *private_data)
{

	struct reviser_dev_info *reviser_device;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;


	DEBUG_TAG;


	reviser_device = (struct reviser_dev_info *)private_data;

	if (!reviser_is_power(reviser_device)) {
		//LOG_ERR("Can Not Read when power disable\n");
		return IRQ_NONE;
	}

	// Check if INT is for reviser
	if (reviser_check_int_valid(reviser_device)) {
		//LOG_ERR("INT NOT triggered by reviser\n");
		return IRQ_NONE;
	}

	if (!reviser_get_interrupt_offset(private_data)) {
		//reviser_print_remap_table(private_data, NULL);
		//reviser_print_context_ID(private_data, NULL);
		spin_lock_irqsave(&g_reviser_device->lock_dump, flags);
		reviser_device->dump.err_count++;
		spin_unlock_irqrestore(&g_reviser_device->lock_dump, flags);
		ret = IRQ_HANDLED;
	} else {
		//LOG_ERR("INT NOT triggered by reviser\n");
		ret = IRQ_NONE;
	}

	return ret;

}

static int reviser_memory_func(void *arg)
{
	struct reviser_dev_info *reviser_device;

	reviser_device = (struct reviser_dev_info *) arg;

	if (reviser_dram_remap_init(reviser_device)) {
		LOG_ERR("Could not set memory for reviser\n");
		return -ENOMEM;
	}
	LOG_INFO("reviser memory init\n");

	return 0;
}


static const struct file_operations reviser_fops = {
	.open = reviser_open,
	.unlocked_ioctl = reviser_ioctl,
	.release = reviser_release,
	.compat_ioctl = reviser_compat_ioctl,
};


static void reviser_power_on_cb(void *para)
{
	unsigned long flags;

	if (g_reviser_device == NULL) {
		LOG_ERR("Not Found reviser_device\n");
		return;
	}
	spin_lock_irqsave(&g_reviser_device->lock_power, flags);
	g_reviser_device->power = true;
	spin_unlock_irqrestore(&g_reviser_device->lock_power, flags);


	reviser_enable_interrupt(g_reviser_device, 1);

	if (reviser_boundary_init(g_reviser_device, BOUNDARY_APUSYS)) {
		LOG_ERR("Set Boundary Fail\n");
		return;
	}
	if (reviser_set_default_iova(g_reviser_device)) {
		LOG_ERR("Set Default IOVA Fail\n");
		return;
	}

}

static void reviser_power_off_cb(void *para)
{
	unsigned long flags;

	if (g_reviser_device == NULL) {
		LOG_ERR("Not Found reviser_device\n");
		return;
	}
	reviser_enable_interrupt(g_reviser_device, 0);

	spin_lock_irqsave(&g_reviser_device->lock_power, flags);
	g_reviser_device->power = false;
	spin_unlock_irqrestore(&g_reviser_device->lock_power, flags);
}

static int reviser_open(struct inode *inode, struct file *filp)
{
	struct reviser_dev_info *reviser_device;

	DEBUG_TAG;
	reviser_device = container_of(inode->i_cdev,
			struct reviser_dev_info, reviser_cdev);

	filp->private_data = reviser_device;
	LOG_DEBUG("reviser_device  %p\n", reviser_device);
	LOG_DEBUG("filp->private_data  %p\n", filp->private_data);
	return 0;
}

static int reviser_release(struct inode *inode, struct file *filp)
{
	DEBUG_TAG;
	return 0;
}

static int reviser_probe(struct platform_device *pdev)
{
	//reviser_device_init();
	int ret = 0;
	int irq;

	struct resource *apusys_reviser_ctl; /* IO mem resources */
	struct resource *apusys_reviser_tcm; /* IO mem resources */
	struct resource *apusys_reviser_vlm; /* IO mem resources */
	struct resource *apusys_reviser_int; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct reviser_dev_info *reviser_device;

	struct device_node *power_node;
	struct platform_device *power_pdev;

	DEBUG_TAG;

	g_reviser_device = NULL;

	/* make sure apusys_power driver initiallized before
	 * calling apu_power_callback_device_register
	 */
	power_node = of_find_compatible_node(
			NULL, NULL, "mediatek,apusys_power");
	if (!power_node) {
		LOG_ERR("DT,mediatek,apusys_power not found\n");
		return -EINVAL;
	}

	power_pdev = of_find_device_by_node(power_node);

	if (!power_pdev || !power_pdev->dev.driver) {
		LOG_DEBUG("Waiting for %s\n",
				power_node->full_name);
		return -EPROBE_DEFER;
	}


	reviser_device = devm_kzalloc(dev, sizeof(*reviser_device), GFP_KERNEL);
	if (!reviser_device)
		return -ENOMEM;

	reviser_device->init_done = false;

	mutex_init(&reviser_device->mutex_ctxid);
	mutex_init(&reviser_device->mutex_tcm);
	mutex_init(&reviser_device->mutex_vlm_pgtable);
	mutex_init(&reviser_device->mutex_remap);
	mutex_init(&reviser_device->mutex_power);
	init_waitqueue_head(&reviser_device->wait_ctxid);
	init_waitqueue_head(&reviser_device->wait_tcm);
	spin_lock_init(&reviser_device->lock_power);
	spin_lock_init(&reviser_device->lock_dump);

	g_ioctl_enable = 0;
	reviser_device->dev = &pdev->dev;

	//memset(&g_reviser_info, 0, sizeof(struct reviser_dev_info));
	/* get major */
	ret = alloc_chrdev_region(&reviser_device->reviser_devt,
			0, 1, APUSYS_DRV_NAME);
	if (ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		goto out;
	}

	/* Attatch file operation. */
	cdev_init(&reviser_device->reviser_cdev, &reviser_fops);
	reviser_device->reviser_cdev.owner = THIS_MODULE;
	DEBUG_TAG;

	/* Add to system */
	ret = cdev_add(&reviser_device->reviser_cdev,
			reviser_device->reviser_devt, 1);
	if (ret < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto free_chrdev_region;
	}

	/* Create class register */
	reviser_class = class_create(THIS_MODULE, APUSYS_DRV_NAME);
	if (IS_ERR(reviser_class)) {
		ret = PTR_ERR(reviser_class);
		LOG_ERR("Unable to create class, err = %d\n", ret);
		goto free_cdev_add;
	}

	dev = device_create(reviser_class, NULL, reviser_device->reviser_devt,
				NULL, APUSYS_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		LOG_ERR("Failed to create device: /dev/%s, err = %d",
			APUSYS_DEV_NAME, ret);
		goto free_class;
	}
	DEBUG_TAG;

	apusys_reviser_ctl = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!apusys_reviser_ctl) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_device;
	}
	apusys_reviser_vlm = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!apusys_reviser_vlm) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_device;
	}
	apusys_reviser_tcm = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!apusys_reviser_tcm) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_device;
	}

	apusys_reviser_int = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!apusys_reviser_int) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_device;
	}

	LOG_DEBUG("apusys_reviser_ctl->start = %pa\n",
			&apusys_reviser_ctl->start);
	reviser_device->pctrl_top = ioremap_nocache(apusys_reviser_ctl->start,
		apusys_reviser_ctl->end - apusys_reviser_ctl->start + 1);
	if (!reviser_device->pctrl_top) {
		LOG_ERR("Could not allocate iomem\n");
		ret = -EIO;
		goto free_device;
	}

	LOG_DEBUG("apusys_reviser_vlm->start = %pa\n",
			&apusys_reviser_vlm->start);
	reviser_device->vlm_base =
		ioremap_nocache(apusys_reviser_vlm->start,
		apusys_reviser_vlm->end - apusys_reviser_vlm->start + 1);
	if (!reviser_device->vlm_base) {
		LOG_ERR("Could not allocate iomem\n");
		ret = -EIO;
		goto free_device;
	}
	reviser_device->vlm.iova = apusys_reviser_vlm->start;
	reviser_device->vlm.size = apusys_reviser_vlm->end - apusys_reviser_vlm->start + 1;

	LOG_DEBUG("apusys_reviser_tcm->start = %pa\n",
			&apusys_reviser_tcm->start);
	LOG_DEBUG("apusys_reviser_tcm->end = %pa\n",
			&apusys_reviser_tcm->end);
	if (apusys_reviser_tcm->end > apusys_reviser_tcm->start) {
		reviser_device->tcm_base =
				ioremap_nocache(apusys_reviser_tcm->start,
				apusys_reviser_tcm->end - apusys_reviser_tcm->start + 1);
		if (!reviser_device->tcm_base) {
			LOG_ERR("Could not allocate iomem\n");
			ret = -EIO;
			goto free_device;
		}
		reviser_device->tcm.size = apusys_reviser_tcm->end - apusys_reviser_tcm->start + 1;
	} else {
		reviser_device->tcm_base = NULL;
		reviser_device->tcm.size = 0;
	}
	reviser_device->tcm.iova = apusys_reviser_tcm->start;

	LOG_DEBUG("apusys_reviser_int->start = %pa\n",
			&apusys_reviser_int->start);
	reviser_device->int_base =
		ioremap_nocache(apusys_reviser_int->start,
		apusys_reviser_int->end - apusys_reviser_int->start + 1);
	if (!reviser_device->int_base) {
		LOG_ERR("Could not allocate iomem\n");
		ret = -EIO;
		goto free_device;
	}

	mem_task = kthread_run(reviser_memory_func, reviser_device, "reviser");
	if (mem_task == NULL) {
		LOG_ERR("create kthread(mem) fail\n");
		ret = -ENOMEM;
		goto free_device;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = -ENODEV;
		LOG_ERR("platform_get_irq Failed to request irq %d: %d\n",
				irq, ret);
		goto free_map;
	}

	ret = devm_request_irq(dev, irq, reviser_interrupt,
			IRQF_TRIGGER_HIGH | IRQF_SHARED,
			dev_name(dev),
			reviser_device);
	if (ret < 0) {
		LOG_ERR("devm_request_irq Failed to request irq %d: %d\n",
				irq, ret);
		ret = -ENODEV;
		goto free_map;
	}



	if (reviser_table_init_ctxID(reviser_device)) {
		ret = -EINVAL;
		goto free_map;
	}
	if (reviser_table_init_tcm(reviser_device)) {
		ret = -EINVAL;
		goto free_map;
	}
	if (reviser_table_init_vlm(reviser_device)) {
		ret = -EINVAL;
		goto free_map;
	}
	if (reviser_table_init_remap(reviser_device)) {
		ret = -EINVAL;
		goto free_map;
	}
	reviser_dbg_init(reviser_device);

	g_reviser_device = reviser_device;

	ret = apu_power_callback_device_register(REVISOR,
			reviser_power_on_cb, reviser_power_off_cb);
	if (ret) {
		LOG_ERR("apu_power_callback_device_register return error(%d)\n",
			ret);
		ret = -EINVAL;
		goto free_map;
	}

	apu_power_device_register(REVISER, pdev);
	/* Workaround for power all on mode*/
	//reviser_power_on(NULL);


	reviser_device->init_done = true;
	platform_set_drvdata(pdev, reviser_device);
	dev_set_drvdata(dev, reviser_device);


	return ret;

free_map:
	iounmap(reviser_device->pctrl_top);
	iounmap(reviser_device->vlm_base);
	if (reviser_device->tcm_base)
		iounmap(reviser_device->tcm_base);
free_device:
	/* Release device */
	device_destroy(reviser_class, reviser_device->reviser_devt);

free_class:
	/* Release class */
	class_destroy(reviser_class);
free_cdev_add:
	/* Release char driver */
	cdev_del(&reviser_device->reviser_cdev);

free_chrdev_region:
	unregister_chrdev_region(reviser_device->reviser_devt, 1);

out:
	return ret;
}

static int reviser_remove(struct platform_device *pdev)
{
	struct reviser_dev_info *reviser_device = platform_get_drvdata(pdev);

	DEBUG_TAG;

	apu_power_device_unregister(REVISER);
	apu_power_callback_device_unregister(REVISOR);

	g_reviser_device = NULL;

	reviser_dbg_destroy(reviser_device);
	reviser_dram_remap_destroy(reviser_device);
	iounmap(reviser_device->pctrl_top);

	/* Release device */
	device_destroy(reviser_class, reviser_device->reviser_devt);

	/* Release class */
	if (reviser_class != NULL)
		class_destroy(reviser_class);

	/* Release char driver */
	cdev_del(&reviser_device->reviser_cdev);

	unregister_chrdev_region(reviser_device->reviser_devt, 1);


	return 0;
}

static int reviser_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int reviser_resume(struct platform_device *pdev)
{
	return 0;
}

static long reviser_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct reviser_dev_info *reviser_device = filp->private_data;
	struct reviser_ioctl_info info;
	unsigned long ctxID = 0;
	struct table_tcm pg_table;
	uint32_t tcm_page_num = 0, tcm_size = 0;

	if (!g_ioctl_enable)
		return -EINVAL;

	switch (cmd) {
	case REVISER_IOCTL_SET_BOUNDARY:

		if (copy_from_user(&info,
				(void *)arg,
				sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		ret = reviser_set_boundary(
				reviser_device,
				info.bound.type,
				info.bound.index,
				info.bound.boundary);
		if (ret == 0) {
			if (copy_to_user((void *)arg,
				&info,
				sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SET_CONTEXT_ID:

		if (copy_from_user(&info, (void *)arg,
				sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		ret = reviser_set_context_ID(
				reviser_device,
			(enum REVISER_DEVICE_E) info.contex.type,
			info.contex.index,
			info.contex.ID);
		if (ret == 0) {
			if (copy_to_user((void *)arg, &info,
				sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SET_REMAP_TABLE:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		ret = reviser_set_remap_table(
			reviser_device, info.table.index,
			info.table.valid, info.table.ID,
			info.table.src_page, info.table.dst_page);
		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_GET_CTXID:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		//if (!reviser_table_get_ctxID(reviser_device, &ctxID)) {
		if (!reviser_table_get_ctxID_sync(reviser_device, &ctxID)) {
			LOG_DEBUG("ctxID: %lu\n", ctxID);
			info.contex.ID = ctxID;
		} else {
			ret = -EINVAL;
		}


		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_FREE_CTXID:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}
		ctxID = info.contex.ID;
		if (reviser_table_free_ctxID(reviser_device, ctxID)) {
			LOG_DEBUG("ctxID: %lu Fail\n", ctxID);
			ret = -EINVAL;
		}

		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_GET_TCM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}
		memset(&pg_table, 0, sizeof(struct table_tcm));

		tcm_page_num = DIV_ROUND_UP(info.page.tcm_size, VLM_BANK_SIZE);
		LOG_DEBUG("tcm_size: %x tcm_page_num %d\n",
				info.page.tcm_size, tcm_page_num);
		if (!reviser_table_get_tcm_sync(reviser_device,
				tcm_page_num, &pg_table)) {
			LOG_DEBUG("page_num: %u\n", pg_table.page_num);
			LOG_DEBUG("table_tcm: %lx\n", pg_table.table_tcm[0]);

			info.page.tcm_num = pg_table.page_num;
			memcpy(info.page.table_tcm,
					pg_table.table_tcm,
					sizeof(unsigned long) *
					BITS_TO_LONGS(TABLE_TCM_MAX));

		} else {
			LOG_DEBUG("Get TCM Fail\n");
			ret = -EINVAL;
		}


		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_FREE_TCM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}
		if (info.page.tcm_num > VLM_TCM_BANK_MAX) {
			LOG_ERR("tcm_num out of range %d\n", info.page.tcm_num);
			ret = -EINVAL;
		}
		if (info.page.tcm_num !=
				bitmap_weight(info.page.table_tcm,
						VLM_TCM_BANK_MAX)) {
			LOG_ERR("tcm_num %d is unequal to table tcm %lx\n",
					info.page.tcm_num,
					info.page.table_tcm[0]);
			ret = -EINVAL;
		}

		memset(&pg_table, 0, sizeof(struct table_tcm));
		pg_table.page_num = info.page.tcm_num;

		LOG_DEBUG("info.page.table_tcm: %lx\n", info.page.table_tcm[0]);
		memcpy(pg_table.table_tcm, info.page.table_tcm,
				sizeof(unsigned long) * BITS_TO_LONGS(4));
		LOG_DEBUG("page_num: %u\n", pg_table.page_num);
		LOG_DEBUG("table_tcm: %lx\n", pg_table.table_tcm[0]);
		if (reviser_table_free_tcm(reviser_device, &pg_table)) {
			LOG_DEBUG("Free TCM Fail\n");
			ret = -EINVAL;
		}
		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_GET_VLM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		if (!reviser_table_get_vlm(reviser_device,
				info.page.tcm_size, info.page.force,
				&ctxID, &tcm_size)) {
			LOG_DEBUG("GET VLM : tcm_size: %x\n", tcm_size);
			LOG_DEBUG("GET VLM : ctxID: %lu\n", ctxID);
			info.page.tcm_size = tcm_size;
			info.page.ID = ctxID;
		} else {
			ret = -EINVAL;
		}

		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_FREE_VLM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		if (reviser_table_free_vlm(reviser_device, info.page.ID)) {

			LOG_DEBUG("Free VLM : ctxID: %lu\n", ctxID);
			ret = -EINVAL;
		}

		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SWAPIN_VLM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}
		if (reviser_table_swapin_vlm(reviser_device, info.page.ID)) {
			LOG_DEBUG("Swapout ctxID Fail\n");
			ret = -EINVAL;
		}


		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SWAPOUT_VLM:

		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}
		if (reviser_table_swapout_vlm(reviser_device, info.page.ID)) {
			ret = -EINVAL;
			LOG_DEBUG("Swapout ctxID Fail\n");
		}

		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static long reviser_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	DEBUG_TAG;
	switch (cmd) {
	case REVISER_IOCTL_SET_BOUNDARY:
	case REVISER_IOCTL_SET_CONTEXT_ID:
	case REVISER_IOCTL_SET_REMAP_TABLE:
	case REVISER_IOCTL_GET_CTXID:
	case REVISER_IOCTL_FREE_CTXID:
	case REVISER_IOCTL_GET_TCM:
	case REVISER_IOCTL_FREE_TCM:
	case REVISER_IOCTL_GET_VLM:
	case REVISER_IOCTL_FREE_VLM:
	case REVISER_IOCTL_SWAPIN_VLM:
	case REVISER_IOCTL_SWAPOUT_VLM:
	{
		return flip->f_op->unlocked_ioctl(flip, cmd,
					(unsigned long)compat_ptr(arg));
	}
	default:
		return -ENOIOCTLCMD;
		/*return vpu_ioctl(flip, cmd, arg);*/
	}
	return 0;
}

static const struct of_device_id reviser_of_match[] = {
	{.compatible = "mediatek,apusys_reviser",},
	{/* end of list */},
};

static struct platform_driver reviser_driver = {
	.probe = reviser_probe,
	.remove = reviser_remove,
	.suspend = reviser_suspend,
	.resume  = reviser_resume,
	//.pm = apusys_pm_qos,
	.driver = {
		.name = APUSYS_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = reviser_of_match,
	},
};

static int __init reviser_init(void)
{
	int ret = 0;
	//struct device *dev = NULL;


	DEBUG_TAG;

	if (!apusys_power_check()) {
		LOG_ERR("reviser is disabled by apusys\n");
		return -ENODEV;
	}

	if (platform_driver_register(&reviser_driver)) {
		LOG_ERR("failed to register APUSYS driver");
		return -ENODEV;
	}


	return ret;
}

static void __exit reviser_destroy(void)
{
	platform_driver_unregister(&reviser_driver);
}

module_init(reviser_init);
module_exit(reviser_destroy);
MODULE_DESCRIPTION("MTK APUSYS REVISER Driver");
MODULE_AUTHOR("Yu-Ren Wang");
MODULE_LICENSE("GPL");
