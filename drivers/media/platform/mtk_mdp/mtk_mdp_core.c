/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <soc/mediatek/smi.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/sched.h>
#include <linux/kthread.h>

#include "mtk_mdp_core.h"
#include "mtk_vpu.h"


struct mtk_mdp_ctx *mdp_ctx_global;

static const struct mtk_mdp_pix_max mtk_mdp_size_max = {
	.org_scaler_bypass_w	= 4096,
	.org_scaler_bypass_h	= 4096,
	.org_scaler_input_w	= 4096,
	.org_scaler_input_h	= 4096,
	.real_rot_dis_w		= 4096,
	.real_rot_dis_h		= 4096,
	.real_rot_en_w		= 4096,
	.real_rot_en_h		= 4096,
	.target_rot_dis_w	= 4096,
	.target_rot_dis_h	= 4096,
	.target_rot_en_w	= 4096,
	.target_rot_en_h	= 4096,
};

static const struct mtk_mdp_pix_min mtk_mdp_size_min = {
	.org_w			= 16,
	.org_h			= 16,
	.real_w			= 16,
	.real_h			= 16,
	.target_rot_dis_w	= 16,
	.target_rot_dis_h	= 16,
	.target_rot_en_w	= 16,
	.target_rot_en_h	= 16,
};

static const struct mtk_mdp_pix_align mtk_mdp_size_align = {
	.org_h			= 16,
	.org_w			= 16,
	.offset_h		= 2,
	.real_w			= 16,
	.real_h			= 16,
	.target_w		= 2,
	.target_h		= 2,
};

static const struct mtk_mdp_variant mtk_mdp_default_variant = {
	.pix_max		= &mtk_mdp_size_max,
	.pix_min		= &mtk_mdp_size_min,
	.pix_align		= &mtk_mdp_size_align,
	.in_buf_cnt		= 32,
	.out_buf_cnt		= 32,
	.h_sc_up_max		= 32,
	.v_sc_up_max		= 32,
	.h_sc_down_max		= 32,
	.v_sc_down_max		= 128,
};

static const struct of_device_id mt_mtk_mdp_match[] = {
	{
		.compatible = "mediatek,mt2701-mdp",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mt_mtk_mdp_match);

static int mtk_mdp_probe(struct platform_device *pdev)
{
	struct mtk_mdp_dev *mdp;
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct device_node *np;
	struct dma_iommu_mapping *dma_mapping;
	struct platform_device *npdev;
	struct device *class_dev;
	dev_t mtk_mdp_devno;
	struct cdev *mtk_mdp_cdev;
	struct class *mtk_mdp_class;

	pr_debug("mtk_mdp_probe\n");

	if (alloc_chrdev_region(&(mtk_mdp_devno), 0, 1, "mdp")) {
		pr_debug("failed to alloc_chrdev_region\n");
		return -EFAULT;
	}

	mtk_mdp_cdev = cdev_alloc();
	mtk_mdp_cdev->owner = THIS_MODULE;

	cdev_add(mtk_mdp_cdev, mtk_mdp_devno, 1);

	mtk_mdp_class = class_create(THIS_MODULE, "mdp");

	class_dev =	device_create(mtk_mdp_class, NULL, mtk_mdp_devno, NULL, "mdp");
	if ((IS_ERR(class_dev))) {
		pr_err("%s: class_dev=0x%p\n", __func__, class_dev);
		BUG();
	}

	mdp = devm_kzalloc(dev, sizeof(struct mtk_mdp_dev), GFP_KERNEL);
	if (!mdp)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdp);
	mdp->id = pdev->id;
	mdp->variant = &mtk_mdp_default_variant;
	mdp->pdev = pdev;
	mdp->mtk_mdp_devno = mtk_mdp_devno;
	mdp->mtk_mdp_cdev = mtk_mdp_cdev;
	mdp->mtk_mdp_class = mtk_mdp_class;

	mdp_ctx_global =  kzalloc(sizeof(struct mtk_mdp_ctx), GFP_KERNEL);
	if (!mdp_ctx_global) {
		ret = -ENOMEM;
		goto err_ctx_alloc;
	}

	mutex_init(&mdp->vpulock);

	mdp->vpu_dev = vpu_get_plat_device(pdev);

	ret = mtk_mdp_vpu_register(&mdp_ctx_global->vpu, mdp->pdev);
	if (ret < 0) {
		dev_err(&mdp->pdev->dev, "mdp_vpu register failed\n");
		goto err_load_vpu;
	}

	mdp_ctx_global->mdp_dev = mdp;

	dev_dbg(dev, "mdp-%d registered successfully\n", mdp->id);

    /*attach to iommu device , to get mva buffer for test thread*/
	np = of_parse_phandle(pdev->dev.of_node, "iommus", 0);
	if (!np)
		goto err_load_vpu;
	npdev = of_find_device_by_node(np);
	of_node_put(np);
	if (WARN_ON(!npdev))
		goto err_load_vpu;

	dma_mapping = npdev->dev.archdata.iommu;
	arm_iommu_attach_device(&pdev->dev, dma_mapping);

	dev_err(&mdp->pdev->dev, "mdp_vpu register failed\n");

	return 0;

err_load_vpu:
	mutex_destroy(&mdp->vpulock);
	kfree(&mdp_ctx_global);
err_ctx_alloc:
	/*release memory*/
	ret = -EFAULT;
	dev_dbg(dev, "err %d\n", ret);
	devm_kfree(dev, mdp);
	return ret;
}
EXPORT_SYMBOL(mdp_ctx_global);

static int mtk_mdp_remove(struct platform_device *pdev)
{
	dev_t mtk_mdp_devno;
	struct cdev *mtk_mdp_cdev;
	struct class *mtk_mdp_class;
	struct mtk_mdp_dev *mtk_mdp = platform_get_drvdata(pdev);

	mtk_mdp_devno = mtk_mdp->mtk_mdp_devno;
	mtk_mdp_cdev = mtk_mdp->mtk_mdp_cdev;
	mtk_mdp_class = mtk_mdp->mtk_mdp_class;

	device_destroy(mtk_mdp_class, mtk_mdp_devno);
	class_destroy(mtk_mdp_class);
	cdev_del(mtk_mdp_cdev);
	unregister_chrdev_region(mtk_mdp_devno, 1);
	devm_kfree(&pdev->dev, mtk_mdp);

	dev_dbg(&pdev->dev, "%s driver unloaded\n", pdev->name);
	return 0;
}


static struct platform_driver mtk_mdp_driver = {
	.probe		= mtk_mdp_probe,
	.remove		= mtk_mdp_remove,
	.driver = {
		.name	= MTK_MDP_MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mt_mtk_mdp_match,
	}
};


module_platform_driver(mtk_mdp_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek image processor driver-mdp");
