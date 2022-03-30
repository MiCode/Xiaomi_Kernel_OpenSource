// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/rpmsg.h>

#include "reviser_plat.h"
#include "reviser_device.h"

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_platform.h>
#endif
#include "apusys_core.h"
#include "apu.h"
#include "apu_config.h"

#include "reviser_drv.h"
#include "reviser_cmn.h"

#include "reviser_dbg.h"
#include "reviser_table_mgt.h"
#include "apusys_power.h"

#include "reviser_mem.h"
#include "reviser_power.h"
#include "reviser_hw_mgt.h"
#include "reviser_remote.h"
#include "reviser_remote_cmd.h"
/* define */
#define APUSYS_DRV_NAME "apusys_drv_reviser"
#define APUSYS_DEV_NAME "apusys_reviser"

/* global variable */
static struct class *reviser_class;
struct reviser_dev_info *g_rdv;
static struct task_struct *mem_task;
static struct apusys_core_info *g_apusys;

/* function declaration */
static int reviser_open(struct inode *, struct file *);
static int reviser_release(struct inode *, struct file *);
static int reviser_memory_func(void *arg);
static int reviser_init_para(struct reviser_dev_info *rdv);
static int reviser_get_addr(struct platform_device *pdev, void **reg, int num,
		unsigned int *base, unsigned int *size);
static int reviser_map_dts(struct platform_device *pdev);
static int reviser_unmap_dts(struct platform_device *pdev);
static int reviser_create_node(struct platform_device *pdev);
static int reviser_delete_node(void *drvinfo);
static void reviser_power_on_cb(void *para);
static void reviser_power_off_cb(void *para);

static int reviser_memory_func(void *arg)
{
	struct reviser_dev_info *rdv;

	rdv = (struct reviser_dev_info *) arg;

	if (reviser_dram_remap_init(rdv)) {
		LOG_ERR("Could not set memory for reviser\n");
		return -ENOMEM;
	}
	LOG_INFO("reviser memory init\n");

	return 0;
}

static int reviser_rprmsg_memory_func(void *arg)
{
	struct reviser_dev_info *rdv;
	int ret = 0;
	uint32_t i;

	rdv = (struct reviser_dev_info *) arg;

	ret = reviser_remote_handshake(rdv, NULL);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	ret = reviser_memory_func(arg);
	if (ret) {
		LOG_ERR("reviser memory fail\n");
		goto out;
	}

	for (i = 0; i < rdv->plat.dram_max; i++) {
		ret = reviser_remote_set_hw_default_iova(rdv, i, rdv->plat.dram[i]);
		if (ret) {
			LOG_ERR("reviser_remote_set_hw_default_iova fail %d\n", ret);
			goto out;
		}
	}

	LOG_INFO("reviser memory init\n");


out:
	return ret;
}


int reviser_set_init_info(struct mtk_apu *apu)
{
	struct reviser_dev_info *rdv;
	struct reviser_init_info *rv_info;
	int i = 0;

	rdv = g_rdv;

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -EINVAL;
	}

	rv_info = (struct reviser_init_info *)
		   get_apu_config_user_ptr(apu->conf_buf, eREVISER_INIT_INFO);

	memset((void *)rv_info, 0, sizeof(struct reviser_init_info));

//	if (reviser_dram_remap_init(rdv)) {
//		LOG_ERR("Could not set memory for reviser\n");
//		return -ENOMEM;
//	}

	rv_info->boundary = rdv->plat.boundary;
	for (i = 0; i < rdv->plat.dram_max; i++)
		rv_info->dram[i] = rdv->plat.dram[i];


	LOG_INFO("reviser info init\n");

	return 0;
}
EXPORT_SYMBOL(reviser_set_init_info);

static void reviser_power_on_cb(void *para)
{
	unsigned long flags;

	if (g_rdv == NULL) {
		LOG_ERR("Not Found rdv\n");
		return;
	}
	spin_lock_irqsave(&g_rdv->lock.lock_power, flags);
	g_rdv->power.power = true;
	spin_unlock_irqrestore(&g_rdv->lock.lock_power, flags);


	reviser_mgt_set_int(g_rdv, 1);

	if (reviser_mgt_set_boundary(g_rdv, g_rdv->plat.boundary)) {
		LOG_ERR("Set Boundary Fail\n");
		return;
	}
	reviser_mgt_dmp_boundary(g_rdv, NULL);
	if (reviser_mgt_set_default(g_rdv)) {
		LOG_ERR("Set Default IOVA Fail\n");
		return;
	}

	LOG_INFO("reviser power-on callback Done\n");
}

static void reviser_power_off_cb(void *para)
{
	unsigned long flags;

	if (g_rdv == NULL) {
		LOG_ERR("Not Found rdv\n");
		return;
	}

	reviser_mgt_set_int(g_rdv, 0);

	spin_lock_irqsave(&g_rdv->lock.lock_power, flags);
	g_rdv->power.power = false;
	spin_unlock_irqrestore(&g_rdv->lock.lock_power, flags);

	LOG_INFO("reviser power-off callback Done\n");
}


static const struct file_operations reviser_fops = {
	.open = reviser_open,
	.release = reviser_release,
};



static int reviser_open(struct inode *inode, struct file *filp)
{
	struct reviser_dev_info *rdv;

	DEBUG_TAG;
	rdv = container_of(inode->i_cdev,
			struct reviser_dev_info, reviser_cdev);

	filp->private_data = rdv;
	LOG_DBG_RVR_FLW("rdv  %p\n", rdv);
	LOG_DBG_RVR_FLW("filp->private_data  %p\n", filp->private_data);
	return 0;
}

static int reviser_release(struct inode *inode, struct file *filp)
{
	DEBUG_TAG;
	return 0;
}
static int reviser_init_para(struct reviser_dev_info *rdv)
{
	mutex_init(&rdv->lock.mutex_ctx);
	mutex_init(&rdv->lock.mutex_tcm);
	mutex_init(&rdv->lock.mutex_ctx_pgt);
	mutex_init(&rdv->lock.mutex_remap);
	mutex_init(&rdv->lock.mutex_power);


	init_waitqueue_head(&rdv->lock.wait_ctx);
	init_waitqueue_head(&rdv->lock.wait_tcm);
	spin_lock_init(&rdv->lock.lock_power);
	spin_lock_init(&rdv->lock.lock_dump);

	return 0;
}

static int reviser_get_addr(struct platform_device *pdev, void **reg, int num,
		unsigned int *base, unsigned int *size)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (!res) {
		dev_info(&pdev->dev, "invalid address (num = %d)\n", num);
		return -ENODEV;
	}

	if (res->start > res->end) {
		*base = res->start;
		*size = 0;
		*reg = NULL;
		dev_info(&pdev->dev,
			"(num = %d) at 0x%08lx map 0x%08lx base:0x%08lx size:0x%08lx\n",
			num, (unsigned long __force)res->start,
			(unsigned long __force)res->end, *base, *size);
		return -ENOMEM;
	}

	*reg = ioremap(res->start, res->end - res->start + 1);
	if (*reg == 0) {
		dev_info(&pdev->dev,
			"could not allocate iomem (num = %d)\n", num);
		return -EIO;
	}

	*base = res->start;
	*size = res->end - res->start + 1;
	dev_info(&pdev->dev,
		"(num = %d) at 0x%08lx map 0x%08lx base:0x%08lx size:0x%08lx\n",
		num, (unsigned long __force)res->start,
		(unsigned long __force)res->end, *base, *size);
	return 0;
}
static int reviser_map_dts(struct platform_device *pdev)
{
	int ret = 0;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);
	uint32_t dram_offset = 0;
	uint32_t slb_size = 0;
	struct device_node *slb_node;

	DEBUG_TAG;

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -EINVAL;
	}

	if (reviser_get_addr(pdev, &rdv->rsc.ctrl.base, 0,
			&rdv->rsc.ctrl.addr, &rdv->rsc.ctrl.size)) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto out;
	}
	if (reviser_get_addr(pdev, &rdv->rsc.vlm.base, 1,
			&rdv->rsc.vlm.addr, &rdv->rsc.vlm.size)) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_ctrl;
	}
	rdv->plat.vlm_addr = rdv->rsc.vlm.addr;
	rdv->plat.vlm_size = rdv->rsc.vlm.size;
	rdv->plat.vlm_bank_max = rdv->plat.vlm_size / rdv->plat.bank_size;


	ret = reviser_get_addr(pdev, &rdv->rsc.pool[REVSIER_POOL_TCM].base,
			2, &rdv->rsc.pool[REVSIER_POOL_TCM].addr,
			&rdv->rsc.pool[REVSIER_POOL_TCM].size);
	if (ret && (ret != -ENOMEM)) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_vlm;
	} else {
		ret = 0;
		LOG_INFO("TCM Empty\n");
	}

	rdv->plat.vlm_bank_max = rdv->plat.vlm_size / rdv->plat.bank_size;
	rdv->plat.pool_addr[REVSIER_POOL_TCM] = rdv->rsc.pool[REVSIER_POOL_TCM].addr;
	rdv->plat.pool_size[REVSIER_POOL_TCM] = rdv->rsc.pool[REVSIER_POOL_TCM].size;
	rdv->plat.pool_bank_max[REVSIER_POOL_TCM] =
			rdv->plat.pool_size[REVSIER_POOL_TCM] / rdv->plat.bank_size;
	rdv->plat.pool_max++;


	if (reviser_get_addr(pdev, &rdv->rsc.isr.base, 3,
			&rdv->rsc.isr.addr, &rdv->rsc.isr.size)) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_tcm;
	}

	rdv->rsc.dram.addr = rdv->plat.dram[0];
	LOG_DBG_RVR_FLW("rdv->rsc.dram.addr %x\n", rdv->rsc.dram.addr);

	if (of_property_read_u32(pdev->dev.of_node,
			"boundary", &rdv->plat.boundary)) {
		LOG_ERR("Invalid boundary %d\n", ret);

		goto free_int;
	}
	LOG_DBG_RVR_FLW("boundary: %08xh\n", rdv->plat.boundary);

	if (of_property_read_u32(pdev->dev.of_node,
			"default-dram", &dram_offset)) {
		LOG_ERR("Invalid dram_offset %d\n", ret);
		goto free_int;
	}
	LOG_DBG_RVR_FLW("dram_offset: %08xh\n", dram_offset);
	if (dram_offset > 0)
		rdv->plat.fix_dram = dram_offset;
	else
		rdv->plat.fix_dram = 0;

	slb_node = of_find_compatible_node(
			NULL, NULL, "mediatek,mtk-slbc");
	if (slb_node) {
		of_property_read_u32(slb_node,
					"apu", &slb_size);
		rdv->rsc.pool[REVSIER_POOL_SLBS].size = slb_size;
		rdv->plat.pool_type[REVSIER_POOL_SLBS] = REVISER_MEM_TYPE_SLBS;
		rdv->plat.pool_size[REVSIER_POOL_SLBS] = rdv->rsc.pool[REVSIER_POOL_SLBS].size;
		rdv->plat.pool_max++;
		LOG_INFO("APU-slb size: 0x%x\n", rdv->plat.pool_size[REVSIER_POOL_SLBS]);
	}

	return ret;

free_int:
	iounmap(rdv->rsc.isr.base);
free_tcm:
	if (!rdv->rsc.pool[0].base)
		iounmap(rdv->rsc.pool[0].base);
free_vlm:
	iounmap(rdv->rsc.vlm.base);
free_ctrl:
	iounmap(rdv->rsc.ctrl.base);
out:
	return ret;

}
static int reviser_unmap_dts(struct platform_device *pdev)
{
	int ret = 0;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	DEBUG_TAG;

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -EINVAL;
	}
	if (!rdv->rsc.pool[0].base)
		iounmap(rdv->rsc.pool[0].base);
	iounmap(rdv->rsc.vlm.base);
	iounmap(rdv->rsc.ctrl.base);

	return ret;

}

static int reviser_create_node(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -EINVAL;
	}

	/* get major */
	ret = alloc_chrdev_region(&rdv->reviser_devt,
			0, 1, APUSYS_DRV_NAME);
	if (ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		goto out;
	}

	/* Attach file operation. */
	cdev_init(&rdv->reviser_cdev, &reviser_fops);
	rdv->reviser_cdev.owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(&rdv->reviser_cdev,
			rdv->reviser_devt, 1);
	if (ret < 0) {
		LOG_ERR("Attach file operation failed, %d\n", ret);
		goto free_chrdev_region;
	}

	/* Create class register */
	reviser_class = class_create(THIS_MODULE, APUSYS_DRV_NAME);
	if (IS_ERR(reviser_class)) {
		ret = PTR_ERR(reviser_class);
		LOG_ERR("Unable to create class, err = %d\n", ret);
		goto free_cdev_add;
	}

	dev = device_create(reviser_class, NULL, rdv->reviser_devt,
				NULL, APUSYS_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		LOG_ERR("Failed to create device: /dev/%s, err = %d",
			APUSYS_DEV_NAME, ret);
		goto free_class;
	}

	return 0;

free_class:
	/* Release class */
	class_destroy(reviser_class);
free_cdev_add:
	/* Release char driver */
	cdev_del(&rdv->reviser_cdev);

free_chrdev_region:
	unregister_chrdev_region(rdv->reviser_devt, 1);
out:
	return ret;
}
static int reviser_delete_node(void *drvinfo)
{
	int ret = 0;
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	device_destroy(reviser_class, rdv->reviser_devt);
	class_destroy(reviser_class);
	cdev_del(&rdv->reviser_cdev);
	unregister_chrdev_region(rdv->reviser_devt, 1);

	return ret;
}

static int reviser_probe(struct platform_device *pdev)
{
	//reviser_device_init();
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct reviser_dev_info *rdv;

	DEBUG_TAG;

	g_rdv = NULL;
	mem_task = NULL;

	rdv = devm_kzalloc(dev, sizeof(*rdv), GFP_KERNEL);
	if (!rdv)
		return -ENOMEM;

	rdv->dev = &pdev->dev;

	platform_set_drvdata(pdev, rdv);
	dev_set_drvdata(dev, rdv);

	rdv->init_done = false;

	reviser_init_para(rdv);

	if (reviser_plat_init(pdev)) {
		dev_info(dev, "platform init failed\n");
		return -ENODEV;
	}

	//memset(&g_reviser_info, 0, sizeof(struct reviser_dev_info));

	if (reviser_create_node(pdev)) {
		LOG_ERR("reviser_create_node fail\n");
		return -ENODEV;
	}

	if (reviser_map_dts(pdev)) {
		LOG_ERR("reviser_map_dts fail\n");
		ret = -ENODEV;
		goto free_node;
	}
	if (rdv->plat.fix_dram) {
		mem_task = kthread_run(reviser_memory_func, rdv, "reviser");
		if (mem_task == NULL) {
			LOG_ERR("create kthread(mem) fail\n");
			ret = -ENOMEM;
			goto free_map;
		}

		ret = apu_power_callback_device_register(REVISOR,
				reviser_power_on_cb, reviser_power_off_cb);
		if (ret) {
			LOG_ERR("apu_power_callback_device_register return error(%d)\n",
				ret);
			ret = -EINVAL;
			goto free_dbg;
		}

		apu_power_device_register(REVISER, pdev);
	}


	if (reviser_table_init(rdv)) {
		LOG_ERR("table init fail\n");
		ret = -EINVAL;
		goto free_map;
	}

	reviser_dbg_init(rdv, g_apusys->dbg_root);


	g_rdv = rdv;

	/* Workaround for power all on mode*/
	//reviser_power_on(rdv);

	rdv->init_done = true;



	LOG_INFO("probe done\n");

	return ret;
free_dbg:
	reviser_dbg_destroy(rdv);
free_map:
	reviser_unmap_dts(pdev);
free_node:
	reviser_delete_node(rdv);

	return ret;
}

static int reviser_remove(struct platform_device *pdev)
{
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	DEBUG_TAG;

	if (rdv->plat.fix_dram) {
		apu_power_device_unregister(REVISER);
		apu_power_callback_device_unregister(REVISOR);
	}
	reviser_table_uninit(rdv);
	reviser_dbg_destroy(rdv);
	if (mem_task) {
		reviser_dram_remap_destroy(rdv);
		mem_task = NULL;
	}
	reviser_unmap_dts(pdev);
	reviser_delete_node(rdv);

	g_rdv = NULL;

	LOG_INFO("remove done\n");

	return 0;
}



static struct platform_driver reviser_driver = {
	.probe = reviser_probe,
	.remove = reviser_remove,
	.driver = {
		.name = APUSYS_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int reviser_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
				 int len, void *priv, u32 src)
{
	int ret = 0;

	//LOG_INFO("reviser len=%d, priv=%p, src=%d\n", len, priv, src);
	ret = reviser_remote_rx_cb(data, len);

	return ret;
}

static int reviser_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct reviser_dev_info *rdv;
	int ret = 0;

	LOG_INFO("name=%s, src=%d\n", rpdev->id.name, rpdev->src);

	if (!g_rdv) {
		LOG_ERR("No Reviser Driver Init\n");
		return -ENODEV;
	}

	rdv = g_rdv;
	rdv->rpdev = rpdev;
	rdv->power.power = true;
	reviser_remote_init();

	mem_task = kthread_run(reviser_rprmsg_memory_func, rdv, "reviser");
	if (mem_task == NULL) {
		LOG_ERR("create kthread(mem) fail\n");
		ret = -ENOMEM;
		goto out;
	}

	dev_set_drvdata(&rpdev->dev, rdv);



	/* Workaround for power all on mode*/
	//reviser_power_on(rdv);

	LOG_INFO("Done\n");
out:
	return ret;
}

static void reviser_rpmsg_remove(struct rpmsg_device *rpdev)
{
	reviser_remote_exit();

	LOG_INFO("Done\n");
}

static const struct of_device_id reviser_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-reviser-rpmsg", },
	{ },
};

static struct rpmsg_driver reviser_rpmsg_driver = {
	.drv	= {
		.name	= "apu-reviser-rpmsg",
		.of_match_table = reviser_rpmsg_of_match,
	},
	.probe	= reviser_rpmsg_probe,
	.remove	= reviser_rpmsg_remove,
	.callback = reviser_rpmsg_cb,
};

int reviser_init(struct apusys_core_info *info)
{
	int ret = 0;
	//struct device *dev = NULL;


	DEBUG_TAG;

	g_apusys = info;

	reviser_driver.driver.of_match_table = reviser_get_of_device_id();

	if (platform_driver_register(&reviser_driver)) {
		LOG_ERR("failed to register APUSYS driver");
		return -ENODEV;
	}


	if (register_rpmsg_driver(&reviser_rpmsg_driver)) {
		LOG_ERR("failed to register RMPSG driver");
		return -ENODEV;
	}


	return ret;
}


void reviser_exit(void)
{
	unregister_rpmsg_driver(&reviser_rpmsg_driver);
	platform_driver_unregister(&reviser_driver);
}


