/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-device.h>
#include <linux/iommu.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>
#include <linux/suspend.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_fw.h"

#define VDEC_HW_ACTIVE	0x10
#define VDEC_IRQ_CFG	0x11
#define VDEC_IRQ_CLR	0x10
#define VDEC_IRQ_CFG_REG	0xa4

module_param(mtk_v4l2_dbg_level, int, 0644);
module_param(mtk_vcodec_dbg, bool, 0644);
struct mtk_vcodec_dev *vdec_dev;

/* Wake up context wait_queue */
static void wake_up_ctx(struct mtk_vcodec_ctx *ctx)
{
	ctx->int_cond = 1;
	wake_up_interruptible(&ctx->queue);
}

static irqreturn_t mtk_vcodec_dec_irq_handler(int irq, void *priv)
{
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status = 0;
	unsigned int dec_done_status = 0;
	void __iomem *vdec_misc_addr = dev->reg_base[VDEC_MISC] +
					VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev);

	/* check if HW active or not */
	cg_status = readl(dev->reg_base[0]);
	if ((cg_status & VDEC_HW_ACTIVE) != 0) {
		mtk_v4l2_err("DEC ISR, VDEC active is not 0x0 (0x%08x)",
			     cg_status);
		return IRQ_HANDLED;
	}

	dec_done_status = readl(vdec_misc_addr);
	ctx->irq_status = dec_done_status;
	if ((dec_done_status & MTK_VDEC_IRQ_STATUS_DEC_SUCCESS) !=
		MTK_VDEC_IRQ_STATUS_DEC_SUCCESS)
		return IRQ_HANDLED;

	/* clear interrupt */
	writel((readl(vdec_misc_addr) | VDEC_IRQ_CFG),
		dev->reg_base[VDEC_MISC] + VDEC_IRQ_CFG_REG);
	writel((readl(vdec_misc_addr) & ~VDEC_IRQ_CLR),
		dev->reg_base[VDEC_MISC] + VDEC_IRQ_CFG_REG);

	wake_up_ctx(ctx);

	mtk_v4l2_debug(3,
			"mtk_vcodec_dec_irq_handler :wake up ctx %d, dec_done_status=%x",
			ctx->id, dec_done_status);

	return IRQ_HANDLED;
}

static int fops_vcodec_open(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = NULL;
	struct mtk_video_dec_buf *mtk_buf = NULL;
	int ret = 0;
	struct vb2_queue *src_vq;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mtk_buf = kzalloc(sizeof(*mtk_buf), GFP_KERNEL);
	if (!mtk_buf) {
		kfree(ctx);
		return -ENOMEM;
	}

	mutex_lock(&dev->dev_mutex);
	ctx->empty_flush_buf = mtk_buf;
	ctx->id = dev->id_counter++;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	ctx->dev = dev;
	init_waitqueue_head(&ctx->queue);
	mutex_init(&ctx->lock);

	ctx->type = MTK_INST_DECODER;
	ret = dev->vdec_pdata->ctrls_setup(ctx);
	if (ret) {
		mtk_v4l2_err("Failed to setup mt vcodec controls");
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
		&mtk_vcodec_dec_queue_init);
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		mtk_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)",
			ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	ctx->empty_flush_buf->vb.vb2_buf.vb2_queue = src_vq;
	ctx->empty_flush_buf->lastframe = EOS;
	ctx->empty_flush_buf->vb.vb2_buf.planes[0].bytesused = 1;
	mtk_vcodec_dec_set_default_params(ctx);

	if (v4l2_fh_is_singular(&ctx->fh)) {
		mtk_vcodec_dec_pw_on(&dev->pm);
		/*
		 * load fireware to checks if it was loaded already and
		 * does nothing in that case
		 */
		ret = mtk_vcodec_fw_load_firmware(dev->ipi_msg_handle);
		if (ret < 0) {
			/*
			 * Return 0 if downloading firmware successfully,
			 * otherwise it is failed
			 */
			mtk_v4l2_err("failed to load firmware!");
			goto err_load_fw;
		}

		dev->dec_capability = mtk_vcodec_fw_get_vdec_capa(dev->ipi_msg_handle);
		mtk_v4l2_debug(0, "decoder capability %x", dev->dec_capability);
	}

	list_add(&ctx->list, &dev->ctx_list);

	mutex_unlock(&dev->dev_mutex);
	mtk_v4l2_debug(0, "%s decoder [%d]", dev_name(&dev->plat_dev->dev),
			ctx->id);
	return ret;

	/* Deinit when failure occurred */
err_load_fw:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx->empty_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	mtk_v4l2_debug(0, "[%d] decoder", ctx->id);
	mutex_lock(&dev->dev_mutex);

	/*
	 * Call v4l2_m2m_ctx_release before mtk_vcodec_dec_release. First, it
	 * makes sure the worker thread is not running after vdec_if_deinit.
	 * Second, the decoder will be flushed and all the buffers will be
	 * returned in stop_streaming.
	 */
	mtk_vcodec_dec_empty_queues(ctx);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mtk_vcodec_dec_release(ctx);

	if (v4l2_fh_is_singular(&ctx->fh))
		mtk_vcodec_dec_pw_off(&dev->pm);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	list_del_init(&ctx->list);
	kfree(ctx->empty_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);
	return 0;
}

static const struct v4l2_file_operations mtk_vcodec_fops = {
	.owner		= THIS_MODULE,
	.open		= fops_vcodec_open,
	.release	= fops_vcodec_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not to enter suspend.
 **/
static int mtk_vcodec_dec_suspend(struct device *pDev)
{
	if (mutex_is_locked(&vdec_dev->dec_mutex)) {
		mtk_v4l2_debug(0, "fail due to videocodec activity");
		return -EBUSY;
	}
	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_dec_resume(struct device *pDev)
{
	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_dec_suspend_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int wait_cnt = 0;

	mtk_v4l2_debug(1, "action = %ld", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		vdec_dev->is_codec_suspending = 1;
		do {
			usleep_range(10000, 20000);
			wait_cnt++;
			if (wait_cnt > 5) {
				mtk_v4l2_err("waiting fail");
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				return NOTIFY_DONE;
			}
		} while (mutex_is_locked(&vdec_dev->dec_mutex));

		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		vdec_dev->is_codec_suspending = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static int mtk_vcodec_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	struct video_device *vfd_dec;
	struct resource *res;
	phandle rproc_phandle;
	enum mtk_vcodec_fw_type fw_type;
	int i, ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev->ctx_list);
	dev->plat_dev = pdev;

	dev->vdec_pdata = of_device_get_match_data(&pdev->dev);
	if (!of_property_read_u32(pdev->dev.of_node, "mediatek,vpu",
				  &rproc_phandle)) {
		fw_type = VPU;
	} else if (!of_property_read_u32(pdev->dev.of_node, "mediatek,scp",
				  &rproc_phandle)) {
		fw_type = SCP;
	} else if (!of_property_read_u32(pdev->dev.of_node, "mediatek,vcu",
				  &rproc_phandle)) {
		fw_type = VCU;
	} else {
		mtk_v4l2_err("Could not get vdec IPI device");
		return -ENODEV;
	}
	dev->ipi_msg_handle = mtk_vcodec_fw_select(dev, fw_type, rproc_phandle,
						   VPU_RST_DEC);
	if (IS_ERR(dev->ipi_msg_handle))
		return PTR_ERR(dev->ipi_msg_handle);

	ret = mtk_vcodec_init_dec_pm(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get mt vcodec clock source");
		return ret;
	}

	for (i = 0; i < NUM_MAX_VDEC_REG_BASE; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_err(&pdev->dev, "get memory resource failed.");
			ret = -ENXIO;
			goto err_res;
		}
		dev->reg_base[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR((__force void *)dev->reg_base[i])) {
			ret = PTR_ERR((__force void *)dev->reg_base[i]);
			goto err_res;
		}
		mtk_v4l2_debug(2, "reg[%d] base=%p", i, dev->reg_base[i]);
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource");
		ret = -ENOENT;
		goto err_res;
	}

	dev->dec_irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, dev->dec_irq,
			mtk_vcodec_dec_irq_handler, 0, pdev->name, dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install dev->dec_irq %d (%d)",
			dev->dec_irq,
			ret);
		goto err_res;
	}

	disable_irq(dev->dec_irq);
	mutex_init(&dev->dec_mutex);
	mutex_init(&dev->dev_mutex);
	mutex_init(&dev->dec_dvfs_mutex);
	spin_lock_init(&dev->irqlock);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		"[/MTK_V4L2_VDEC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_v4l2_err("v4l2_device_register err=%d", ret);
		goto err_res;
	}

	init_waitqueue_head(&dev->queue);

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		mtk_v4l2_err("Failed to allocate video device");
		ret = -ENOMEM;
		goto err_dec_alloc;
	}
	vfd_dec->fops		= &mtk_vcodec_fops;
	vfd_dec->ioctl_ops	= &mtk_vdec_ioctl_ops;
	vfd_dec->release	= video_device_release;
	vfd_dec->lock		= &dev->dev_mutex;
	vfd_dec->v4l2_dev	= &dev->v4l2_dev;
	vfd_dec->vfl_dir	= VFL_DIR_M2M;
	vfd_dec->device_caps	= V4L2_CAP_VIDEO_M2M_MPLANE |
			V4L2_CAP_STREAMING;

	snprintf(vfd_dec->name, sizeof(vfd_dec->name), "%s",
		MTK_VCODEC_DEC_NAME);
	video_set_drvdata(vfd_dec, dev);
	dev->vfd_dec = vfd_dec;
	platform_set_drvdata(pdev, dev);

	dev->m2m_dev_dec = v4l2_m2m_init(&mtk_vdec_m2m_ops);
	if (IS_ERR((__force void *)dev->m2m_dev_dec)) {
		mtk_v4l2_err("Failed to init mem2mem dec device");
		ret = PTR_ERR((__force void *)dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

	dev->decode_workqueue =
		alloc_ordered_workqueue(MTK_VCODEC_DEC_NAME,
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->decode_workqueue) {
		mtk_v4l2_err("Failed to create decode workqueue");
		ret = -EINVAL;
		goto err_event_workq;
	}

	if (dev->vdec_pdata->uses_stateless_api) {
		dev->mdev_dec.dev = &pdev->dev;
		strscpy(dev->mdev_dec.model, MTK_VCODEC_DEC_NAME,
				sizeof(dev->mdev_dec.model));

		media_device_init(&dev->mdev_dec);
		dev->mdev_dec.ops = &mtk_vcodec_media_ops;
		dev->v4l2_dev.mdev = &dev->mdev_dec;

		ret = v4l2_m2m_register_media_controller(dev->m2m_dev_dec,
			dev->vfd_dec, MEDIA_ENT_F_PROC_VIDEO_DECODER);
		if (ret) {
			mtk_v4l2_err("Failed to register media controller");
			goto err_reg_cont;
		}

		ret = media_device_register(&dev->mdev_dec);
		if (ret) {
			mtk_v4l2_err("Failed to register media device");
			goto err_media_reg;
		}

		mtk_v4l2_debug(0, "media registered as /dev/media%d",
			vfd_dec->num);
	}
	ret = video_register_device(vfd_dec, VFL_TYPE_GRABBER, 0);
	if (ret) {
		mtk_v4l2_err("Failed to register video device");
		goto err_dec_reg;
	}

	mtk_v4l2_debug(0, "decoder registered as /dev/video%d",
		vfd_dec->num);

	mtk_prepare_vdec_dvfs();
	mtk_prepare_vdec_emi_bw();
	pm_notifier(mtk_vcodec_dec_suspend_notifier, 0);
	dev->is_codec_suspending = 0;
	vdec_dev = dev;
	return 0;

err_dec_reg:
	if (dev->vdec_pdata->uses_stateless_api)
		media_device_unregister(&dev->mdev_dec);
err_media_reg:
	if (dev->vdec_pdata->uses_stateless_api)
		v4l2_m2m_unregister_media_controller(dev->m2m_dev_dec);
err_reg_cont:
	destroy_workqueue(dev->decode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	video_unregister_device(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:
	mtk_vcodec_release_dec_pm(dev);
	return ret;
}

extern const struct mtk_vcodec_dec_pdata mtk_frame_8173_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_req_8183_pdata;

static const struct of_device_id mtk_vcodec_match[] = {
	{.compatible = "mediatek,mt8173-vcodec-dec", .data = &mtk_frame_8173_pdata},
	{.compatible = "mediatek,mt8183-vcodec-dec", .data = &mtk_req_8183_pdata},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vcodec_match);

static int mtk_vcodec_dec_remove(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev = platform_get_drvdata(pdev);

	mtk_unprepare_vdec_emi_bw();
	mtk_unprepare_vdec_dvfs();

	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);

	if (media_devnode_is_registered(dev->mdev_dec.devnode)) {
		media_device_unregister(&dev->mdev_dec);
		v4l2_m2m_unregister_media_controller(dev->m2m_dev_dec);
		media_device_cleanup(&dev->mdev_dec);
	}

	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);

	if (dev->vfd_dec)
		video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);
	mtk_vcodec_release_dec_pm(dev);
	return 0;
}

static const struct dev_pm_ops mtk_vcodec_dec_pm_ops = {
	.suspend = mtk_vcodec_dec_suspend,
	.resume = mtk_vcodec_dec_resume,
};
static struct platform_driver mtk_vcodec_dec_driver = {
	.probe	= mtk_vcodec_probe,
	.remove	= mtk_vcodec_dec_remove,
	.driver	= {
		.name	= MTK_VCODEC_DEC_NAME,
		.pm = &mtk_vcodec_dec_pm_ops,
		.of_match_table = mtk_vcodec_match,
	},
};

module_platform_driver(mtk_vcodec_dec_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec V4L2 decoder driver");
