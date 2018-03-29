/*
* Copyright (c) 2015 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "mtkbuf-dma-cache-sg.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_iommu.h"
#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_drv_if.h"
#include "mtk_vpu.h"


/* Wake up context wait_queue */
static void wake_up_ctx(struct mtk_vcodec_ctx *ctx, unsigned int reason)
{
	ctx->int_cond = 1;
	ctx->int_type = reason;
	wake_up_interruptible(&ctx->queue);
}

static irqreturn_t mtk_vcodec_dec_irq_handler(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned int cg_status = 0;
	unsigned int dec_done_status = 0;

	if (dev->curr_ctx == -1) {
		mtk_v4l2_err("curr_ctx = -1");
		return IRQ_HANDLED;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (ctx == NULL) {
		mtk_v4l2_err("mtk_vcodec_dec_irq_handler +   curr_ctx  = NULL\n");
		return IRQ_HANDLED;
	}
	mtk_v4l2_debug(3, "mtk_vcodec_dec_irq_handler:  ctx =%d\n", ctx->idx);

	cg_status = readl(dev->reg_base[0] + (0));
	if ((cg_status & VDEC_HW_ACTIVE) != 0) {
		mtk_v4l2_err("DEC ISR, VDEC active is not 0x0 (0x%08x)",
			       cg_status);
		return IRQ_HANDLED;
	}

	dec_done_status = readl(dev->reg_base[1] + VDEC_IRQ_CFG_REG);
	ctx->irq_status = dec_done_status;
	if ((dec_done_status & (0x1 << 16)) != 0x10000)
		return IRQ_HANDLED;

	/* clear interrupt */
	writel((readl(dev->reg_base[1] + VDEC_IRQ_CFG_REG) | VDEC_IRQ_CFG),
	       dev->reg_base[1] + VDEC_IRQ_CFG_REG);
	writel((readl(dev->reg_base[1] + VDEC_IRQ_CFG_REG) & ~VDEC_IRQ_CLR),
	       dev->reg_base[1] + VDEC_IRQ_CFG_REG);

	mtk_v4l2_debug(3,
			 "mtk_vcodec_dec_irq_handler :wake up ctx %d, dec_done_status=%x\n",
			 ctx->idx, dec_done_status);

	wake_up_ctx(ctx, MTK_INST_IRQ_RECEIVED);

	mtk_v4l2_debug(3, "mtk_vcodec_dec_irq_handler -\n");

	return IRQ_HANDLED;
}

static int fops_vcodec_open(struct file *file)
{
	struct video_device *vfd = video_devdata(file);
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = NULL;
	int ret = 0;

	mutex_lock(&dev->dev_mutex);

	ctx = devm_kzalloc(&dev->plat_dev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (dev->num_instances >= MTK_VCODEC_MAX_INSTANCES) {
		mtk_v4l2_err("Too many open contexts %d\n", dev->num_instances);
		ret = -EBUSY;
		goto err_no_ctx;
	}

	ctx->idx = ffz(dev->instance_mask[0]);
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->dev = dev;

	if (vfd == dev->vfd_dec) {
		ctx->type = MTK_INST_DECODER;
		mtk_vcodec_dec_set_default_params(ctx);
		/* Setup ctrl handler */
		ret = mtk_vdec_ctrls_setup(ctx);
		if (ret) {
			mtk_v4l2_err("Failed to setup mt vcodec controls\n");
			goto err_ctrls_setup;
		}
		ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
						 &m2mctx_vdec_queue_init);
		if (IS_ERR(ctx->m2m_ctx)) {
			ret = PTR_ERR(ctx->m2m_ctx);
			mtk_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)\n",
				       ret);
			goto err_ctx_init;
		}
	} else {
		mtk_v4l2_err("Invalid vfd !\n");
		ret = -ENOENT;
		goto err_ctx_init;
	}

	init_waitqueue_head(&ctx->queue);
	dev->num_instances++;

	if (dev->num_instances == 1) {
		mtk_vcodec_dec_pw_on(&dev->pm);

		ret = vpu_load_firmware(dev->vpu_plat_dev, false);
		if (ret < 0) {
			mtk_v4l2_err("vpu_load_firmware failed!\n");
			goto err_load_fw;
		}

		dev->dec_capability =
			vpu_get_vdec_hw_capa(dev->vpu_plat_dev);
		mtk_v4l2_debug(0, "decoder capability %x",
			       dev->dec_capability);
	}

	set_bit(ctx->idx, &dev->instance_mask[0]);
	dev->ctx[ctx->idx] = ctx;
	mutex_unlock(&dev->dev_mutex);
	mtk_v4l2_debug(0, "%s decoder [%d]", dev_name(&dev->plat_dev->dev), ctx->idx);

	return ret;

	/* Deinit when failure occurred */
err_load_fw:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	dev->num_instances--;

err_ctx_init:
err_ctrls_setup:
		mtk_vdec_ctrls_free(ctx);
err_no_ctx:
	devm_kfree(&dev->plat_dev->dev, ctx);
err_alloc:
	mutex_unlock(&dev->dev_mutex);
	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	mtk_v4l2_debug(0, "[%d] decoder\n", ctx->idx);

	mutex_lock(&dev->dev_mutex);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mtk_vcodec_vdec_release(ctx);
	mtk_vdec_ctrls_free(ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	dev->ctx[ctx->idx] = NULL;
	dev->num_instances--;
	if (dev->num_instances == 0)
		mtk_vcodec_dec_pw_off(&dev->pm);
	clear_bit(ctx->idx, &dev->instance_mask[0]);
	devm_kfree(&dev->plat_dev->dev, ctx);
	mutex_unlock(&dev->dev_mutex);
	return 0;
}

static unsigned int fops_vcodec_poll(struct file *file,
				     struct poll_table_struct *wait)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_vcodec_dev *dev = ctx->dev;
	int ret;

	mutex_lock(&dev->dev_mutex);
	ret = v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	return v4l2_m2m_mmap(file, ctx->m2m_ctx, vma);
}

static const struct v4l2_file_operations mtk_vcodec_fops = {
	.owner				= THIS_MODULE,
	.open				= fops_vcodec_open,
	.release			= fops_vcodec_release,
	.poll				= fops_vcodec_poll,
	.unlocked_ioctl			= video_ioctl2,
	.mmap				= fops_vcodec_mmap,
};

static int mtk_vcodec_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	struct video_device *vfd_dec;
	struct resource *res;
	int i, ret;
	int reg_base_num;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->plat_dev = pdev;
	dev->vpu_plat_dev = vpu_get_plat_device(dev->plat_dev);
	if (dev->vpu_plat_dev == NULL) {
		mtk_v4l2_err("[VPU] vpu device in not ready\n");
		return -EPROBE_DEFER;
	}

	ret = mtk_vcodec_init_dec_pm(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get mt vcodec clock source\n");
		return ret;
	}

	reg_base_num = NUM_MAX_VDEC_REG_BASE;
	for (i = 0; i < reg_base_num; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_err(&pdev->dev, "get memory resource failed.\n");
			ret = -ENXIO;
			goto err_res;
		}
		dev->reg_base[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dev->reg_base[i])) {
			dev_err(&pdev->dev,
				"devm_ioremap_resource %d failed.\n", i);
			ret = PTR_ERR(dev->reg_base);
			goto err_res;
		}
		mtk_v4l2_debug(2, "reg[%d] base=%p", i, dev->reg_base[i]);
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto err_res;
	}

	dev->dec_irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, dev->dec_irq,
			       mtk_vcodec_dec_irq_handler, 0, pdev->name, dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install dev->dec_irq %d (%d)\n",
			dev->dec_irq,
			ret);
		ret = -EINVAL;
		goto err_res;
	}

	disable_irq(dev->dec_irq);
	mutex_init(&dev->dev_mutex);
	mutex_init(&dev->dec_mutex);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		 "[MTK_V4L2]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_v4l2_err("v4l2_device_register err=%d\n", ret);
		return ret;
	}

	init_waitqueue_head(&dev->queue);

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		mtk_v4l2_err("Failed to allocate video device\n");
		ret = -ENOMEM;
		goto err_dec_alloc;
	}
	vfd_dec->fops		= &mtk_vcodec_fops;
	vfd_dec->ioctl_ops	= &mtk_vdec_ioctl_ops;
	vfd_dec->release	= video_device_release;
	vfd_dec->lock		= &dev->dev_mutex;
	vfd_dec->v4l2_dev	= &dev->v4l2_dev;
	vfd_dec->vfl_dir	= VFL_DIR_M2M;
	vfd_dec->debug		= 0;
	snprintf(vfd_dec->name, sizeof(vfd_dec->name), "%s",
		 MTK_VCODEC_DEC_NAME);
	video_set_drvdata(vfd_dec, dev);
	dev->vfd_dec = vfd_dec;
	platform_set_drvdata(pdev, dev);
	ret = video_register_device(vfd_dec, VFL_TYPE_GRABBER, 0);
	if (ret) {
		mtk_v4l2_err("Failed to register video device\n");
		goto err_dec_reg;
	}
	mtk_v4l2_debug(0, "decoder registered as /dev/video%d\n",
			 vfd_dec->num);

	dev->alloc_ctx = mtk_dma_sg_init_ctx(&pdev->dev);
	if (IS_ERR(dev->alloc_ctx)) {
		mtk_v4l2_err("Failed to alloc vb2 dma context 0\n");
		ret = PTR_ERR(dev->alloc_ctx);
		goto err_vb2_ctx_init;
	}

	dev->m2m_dev_dec = v4l2_m2m_init(&mtk_vdec_m2m_ops);
	if (IS_ERR(dev->m2m_dev_dec)) {
		mtk_v4l2_err("Failed to init mem2mem dec device\n");
		ret = PTR_ERR(dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

#ifdef CONFIG_MTK_IOMMU
	ret = mtk_vcodec_iommu_init(&pdev->dev);
	if (ret) {
		mtk_v4l2_err("Failed to attach iommu device err = %d\n", ret);
		goto err_iommu_attach;
	}
#endif

	dev->decode_workqueue =
			alloc_ordered_workqueue(MTK_VCODEC_DEC_NAME,
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->decode_workqueue) {
		mtk_v4l2_err("Failed to create decode workqueue\n");
		ret = -EINVAL;
		goto err_event_workq;
	}

	return 0;

err_event_workq:
#ifdef CONFIG_MTK_IOMMU
	mtk_vcodec_iommu_deinit(&pdev->dev);
err_iommu_attach:
#endif
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	vb2_dma_contig_cleanup_ctx(dev->alloc_ctx);
err_vb2_ctx_init:
	video_unregister_device(vfd_dec);
err_dec_reg:
	video_device_release(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:
	mtk_vcodec_release_dec_pm(dev);
	return ret;
}

static const struct of_device_id mtk_vcodec_match[] = {
	{.compatible = "mediatek,mt2701-vcodec-dec",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vcodec_match);

static int mtk_vcodec_remove(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev = platform_get_drvdata(pdev);

	mtk_v4l2_debug_enter();
	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);
	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);
	if (dev->alloc_ctx)
		vb2_dma_contig_cleanup_ctx(dev->alloc_ctx);

#ifdef CONFIG_MTK_IOMMU
	mtk_vcodec_iommu_deinit(&pdev->dev);
#endif

	if (dev->vfd_dec) {
		video_unregister_device(dev->vfd_dec);
		video_device_release(dev->vfd_dec);
	}

	v4l2_device_unregister(&dev->v4l2_dev);
	mtk_vcodec_release_dec_pm(dev);
	return 0;
}

static int mtk_vcodec_vdec_pm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_vcodec_dev *m_dev = platform_get_drvdata(pdev);

	mtk_v4l2_debug(3, "m_dev->num_instances=%d", m_dev->num_instances);

	if (m_dev->num_instances == 0)
		return 0;

	return 0;
}

static int mtk_vcodec_vdec_pm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_vcodec_dev *m_dev = platform_get_drvdata(pdev);

	mtk_v4l2_debug(3, "m_dev->num_instances=%d", m_dev->num_instances);

	if (m_dev->num_instances == 0)
		return 0;

	return 0;
}

static const struct dev_pm_ops mtk_vcodec_vdec_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_vcodec_vdec_pm_runtime_suspend,
			mtk_vcodec_vdec_pm_runtime_resume,
					   NULL)
};

static struct platform_driver mtk_vcodec_driver = {
	.probe	= mtk_vcodec_probe,
	.remove	= mtk_vcodec_remove,
	.driver	= {
		.name	= MTK_VCODEC_DEC_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mtk_vcodec_match,
		.pm = &mtk_vcodec_vdec_pm_ops,
	},
};

module_platform_driver(mtk_vcodec_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec V4L2 driver");
