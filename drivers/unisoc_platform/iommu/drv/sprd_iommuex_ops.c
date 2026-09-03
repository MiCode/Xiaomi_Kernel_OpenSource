// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/device.h>
#include "api/sprd_iommu_api.h"

static void sprd_iommuex_clk_enable(struct sprd_iommu_dev *dev)
{
	if (dev->mmu_mclock)
		clk_prepare_enable(dev->mmu_mclock);

	if (dev->mmu_clock)
		clk_prepare_enable(dev->mmu_clock);

	if (dev->mmu_axiclock)
		clk_prepare_enable(dev->mmu_axiclock);
}

static void sprd_iommuex_clk_disable(struct sprd_iommu_dev *dev)
{
	if (dev->mmu_clock)
		clk_disable_unprepare(dev->mmu_clock);

	if (dev->mmu_mclock)
		clk_disable_unprepare(dev->mmu_mclock);

	if (dev->mmu_axiclock)
		clk_disable_unprepare(dev->mmu_axiclock);
}

static int get_iommuex_type(int revision, int *pchip)
{
	enum sprd_iommu_type type = SPRD_IOMMU_NOT_SUPPORT;

	switch (revision) {
	case 7:
	{
		type = SPRD_IOMMUEX_SHARKLE;
		break;
	}
	case 8:
	{
		type = SPRD_IOMMUEX_PIKE2;
		break;
	}
	case 9:
	{
		type = SPRD_IOMMUEX_SHARKL3;
		break;
	}
	case 10:
	{
		type = SPRD_IOMMUEX_SHARKL5;
		break;
	}
	case 11:
	{
		type = SPRD_IOMMUEX_ROC1;
		break;
	}
	default:
	{
		type  = SPRD_IOMMU_NOT_SUPPORT;
		break;
	}
	}
	return type;
}

static int sprd_iommuex_hw_init(struct sprd_iommu_dev *dev,
			struct sprd_iommu_init_data *data)
{
	void *p_iommu_hdl = NULL;
	struct device *dev_priv = (struct device *)(dev->drv_dev);
	struct sprd_iommu_init_param iommu_init_param;
	struct device_node *np = NULL;
	int chip = 0;

	IOMMU_INFO("begin\n");

	np = dev_priv->of_node;
	if (!np)
		return -1;

	memset(&iommu_init_param, 0, sizeof(struct sprd_iommu_init_param));

	dev->pool = gen_pool_create(12, -1);
	if (!dev->pool) {
		IOMMU_ERR("%s gen_pool_create error\n", data->name);
		return -1;
	}

	gen_pool_add(dev->pool, data->iova_base, data->iova_size, -1);
	iommu_init_param.iommu_type =
				get_iommuex_type(data->iommuex_rev, &chip);
	iommu_init_param.chip = chip;

	/*master reg base addr*/
	iommu_init_param.master_reg_addr = data->pgt_base;
	/*iommu base reg*/
	iommu_init_param.phys_offset = data->phys_offset;
	iommu_init_param.ctrl_reg_addr = data->ctrl_reg;
	/*va base addr*/
	iommu_init_param.fm_base_addr = data->iova_base;
	iommu_init_param.fm_ram_size = data->iova_size;
	iommu_init_param.iommu_id = data->id;
	iommu_init_param.faultpage_addr = data->fault_page;
	iommu_init_param.pagt_base_virt = data->pagt_base_virt;
	iommu_init_param.pagt_base_ddr = data->pagt_base_ddr;
	iommu_init_param.pagt_ddr_size = data->pagt_ddr_size;

	if (data->id == IOMMU_EX_ISP)
		iommu_init_param.pgt_size = data->pgt_size;
	/*sprd_iommuex_clk_enable(dev);*/

	sprd_iommudrv_init(&iommu_init_param, (sprd_iommu_hdl *)&p_iommu_hdl);

	dev->private = p_iommu_hdl;
	IOMMU_INFO("done\n");

	return 0;
}

static int sprd_iommuex_hw_exit(struct sprd_iommu_dev *dev)
{
	sprd_iommudrv_uninit(dev->private);
	dev->private = NULL;

	gen_pool_destroy(dev->pool);
	return 0;
}

static unsigned long sprd_iommuex_iova_alloc(struct sprd_iommu_dev *dev,
					size_t iova_length,
					struct sprd_iommu_map_data  *p_param)
{
	unsigned long iova = 0;

	iova = gen_pool_alloc(dev->pool, iova_length);
	return iova;
}

static void sprd_iommuex_iova_free(struct sprd_iommu_dev *dev,
	unsigned long iova, size_t iova_length)
{
	gen_pool_free(dev->pool, iova, iova_length);
}

static int sprd_iommuex_iova_map(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length,
				struct sg_table *table,
				struct sprd_iommu_map_data  *p_param)
{
	int err = -1;
	struct sprd_iommu_map_param map_param;

	memset(&map_param, 0, sizeof(map_param));
	/*TODO:warning need deal*/
	map_param.channel_type = (enum sprd_iommu_ch_type)p_param->ch_type;
	map_param.channel_bypass = 0;
	map_param.start_virt_addr = iova;
	map_param.total_map_size = iova_length;

	map_param.p_sg_table = table;
	err = sprd_iommudrv_map(dev->private, &map_param);
	if (err == SPRD_NO_ERR)
		err = 0;
	else {
		IOMMU_ERR("map error 0x%x\n", err);
		err = -1;
	}
	return err;
}

static int sprd_iommuex_hw_suspend(struct sprd_iommu_dev *dev)
{
	sprd_iommuex_clk_disable(dev);
	return 0;
}

static int sprd_iommuex_hw_resume(struct sprd_iommu_dev *dev)
{
	/*set clk back first*/
	sprd_iommuex_clk_enable(dev);
	return 0;
}

static int sprd_iommuex_hw_restore(struct sprd_iommu_dev *dev)
{
	sprd_iommudrv_reset(dev->private, 0);
	return 0;
}

static int sprd_iommuex_hw_set_bypass(struct sprd_iommu_dev *dev,
				      bool vaor_bp_en)
{
	sprd_iommudrv_set_bypass(dev->private, vaor_bp_en);
	return 0;
}

static int sprd_iommuex_iova_unmap(struct sprd_iommu_dev *dev,
	unsigned long iova, size_t iova_length)
{
	int err = -1;

	struct sprd_iommu_unmap_param unmap_param;

	memset(&unmap_param, 0, sizeof(struct sprd_iommu_unmap_param));
	unmap_param.start_virt_addr = iova;
	unmap_param.total_map_size = iova_length;
	unmap_param.ch_type = (enum sprd_iommu_ch_type)(dev->ch_type);
	unmap_param.ch_id = dev->channel_id;

	err = sprd_iommudrv_unmap(dev->private, &unmap_param);

	if (err == SPRD_NO_ERR)
		return 0;
	else
		return -1;
}

static int sprd_iommuex_iova_unmap_orphaned(struct sprd_iommu_dev *dev,
	unsigned long iova, size_t iova_length)
{
	int err = -1;

	struct sprd_iommu_unmap_param unmap_param;

	memset(&unmap_param, 0, sizeof(struct sprd_iommu_unmap_param));
	unmap_param.start_virt_addr = iova;
	unmap_param.total_map_size = iova_length;
	unmap_param.ch_type = (enum sprd_iommu_ch_type)(dev->ch_type);
	unmap_param.ch_id = dev->channel_id;

	err = sprd_iommudrv_unmap_orphaned(dev->private, &unmap_param);

	if (err == SPRD_NO_ERR)
		return 0;
	else
		return -1;
}

static void sprd_iommuex_hw_release(struct sprd_iommu_dev *dev)
{
	sprd_iommudrv_release(dev->private);
}


struct sprd_iommu_ops sprd_iommuex_hw_ops = {
	.init = sprd_iommuex_hw_init,
	.exit = sprd_iommuex_hw_exit,
	.iova_alloc = sprd_iommuex_iova_alloc,
	.iova_free = sprd_iommuex_iova_free,
	.iova_map = sprd_iommuex_iova_map,
	.iova_unmap = sprd_iommuex_iova_unmap,
	.backup = NULL,
	.restore = sprd_iommuex_hw_restore,
	.set_bypass = sprd_iommuex_hw_set_bypass,
	.enable = NULL,
	.disable = NULL,
	.dump = NULL,
	.open = NULL,
	.release = sprd_iommuex_hw_release,
	.suspend = sprd_iommuex_hw_suspend,
	.resume = sprd_iommuex_hw_resume,
	.iova_unmap_orphaned = sprd_iommuex_iova_unmap_orphaned,
};
