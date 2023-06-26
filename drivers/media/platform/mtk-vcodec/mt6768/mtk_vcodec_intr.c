/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
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

#include <linux/errno.h>
#include <linux/wait.h>

#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "smi_public.h"

int mtk_vcodec_wait_for_done_ctx(struct mtk_vcodec_ctx  *ctx,
	int core_id, int command, unsigned int timeout_ms)
{
	int status = 0;
#ifndef FPGA_INTERRUPT_API_DISABLE
	wait_queue_head_t *waitqueue;
	long timeout_jiff, ret;

	core_id = 0;
	waitqueue = (wait_queue_head_t *)&ctx->queue[core_id];
	timeout_jiff = msecs_to_jiffies(timeout_ms);

	ret = wait_event_interruptible_timeout(*waitqueue,
		ctx->int_cond[core_id],
		timeout_jiff);

	if (!ret) {
		status = -1;    /* timeout */
		mtk_v4l2_err("[%d] cmd=%d, ctx->type=%d, wait_event_interruptible_timeout time=%ums out %d %d!",
			ctx->id, ctx->type, command, timeout_ms,
			ctx->int_cond[core_id], ctx->int_type);
		//smi_debug_bus_hang_detect(0, "VCODEC");
	} else if (-ERESTARTSYS == ret) {
		mtk_v4l2_err("[%d] cmd=%d, ctx->type=%d, wait_event_interruptible_timeout interrupted by a signal %d %d",
			ctx->id, ctx->type, command, ctx->int_cond[core_id],
			ctx->int_type);
		status = -1;
	}

	ctx->int_cond[core_id] = 0;
	ctx->int_type = 0;
#endif
	return status;
}
EXPORT_SYMBOL(mtk_vcodec_wait_for_done_ctx);

/* Wake up context wait_queue */
void wake_up_dec_ctx(struct mtk_vcodec_ctx *ctx)
{
	ctx->int_cond[0] = 1;
	wake_up_interruptible(&ctx->queue[0]);
}

irqreturn_t mtk_vcodec_dec_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status = 0;
	unsigned int dec_done_status = 0;
	void __iomem *vdec_misc_addr = dev->dec_reg_base[VDEC_MISC] +
		MTK_VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev, 0);

	/* check if HW active or not */
	cg_status = readl(dev->dec_reg_base[0]);
	if ((cg_status & MTK_VDEC_HW_ACTIVE) != 0) {
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
	writel((readl(vdec_misc_addr) | MTK_VDEC_IRQ_CFG),
		   dev->dec_reg_base[VDEC_MISC] + MTK_VDEC_IRQ_CFG_REG);
	writel((readl(vdec_misc_addr) & ~MTK_VDEC_IRQ_CLR),
		   dev->dec_reg_base[VDEC_MISC] + MTK_VDEC_IRQ_CFG_REG);

	wake_up_dec_ctx(ctx);

	mtk_v4l2_debug(4,
				   "%s :wake up ctx %d, dec_done_status=%x",
				   __func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_dec_irq_handler);

void clean_irq_status(unsigned int irq_status, void __iomem *addr)
{
	if (irq_status & MTK_VENC_IRQ_STATUS_PAUSE)
		writel(MTK_VENC_IRQ_STATUS_PAUSE, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_SWITCH)
		writel(MTK_VENC_IRQ_STATUS_SWITCH, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_DRAM)
		writel(MTK_VENC_IRQ_STATUS_DRAM, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_SPS)
		writel(MTK_VENC_IRQ_STATUS_SPS, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_PPS)
		writel(MTK_VENC_IRQ_STATUS_PPS, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_FRM)
		writel(MTK_VENC_IRQ_STATUS_FRM, addr);

	if (irq_status & MTK_VENC_IRQ_STATUS_VPS)
		writel(MTK_VENC_IRQ_STATUS_VPS, addr);

}

/* Wake up context wait_queue */
void wake_up_enc_ctx(struct mtk_vcodec_ctx *ctx, unsigned int reason)
{
	ctx->int_cond[0] = 1;
	ctx->int_type = reason;
	wake_up_interruptible(&ctx->queue[0]);
}

irqreturn_t mtk_vcodec_enc_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned long flags;
	void __iomem *addr;

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_enc_ctx[0];
	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (!ctx)
		return IRQ_HANDLED;

	mtk_v4l2_debug(1, "id=%d", ctx->id);
	addr = dev->enc_reg_base[VENC_SYS] + MTK_VENC_IRQ_ACK_OFFSET;

	ctx->irq_status = readl(dev->enc_reg_base[VENC_SYS] +
		(MTK_VENC_IRQ_STATUS_OFFSET));

	clean_irq_status(ctx->irq_status, addr);

	wake_up_enc_ctx(ctx, MTK_INST_IRQ_RECEIVED);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_enc_irq_handler);

irqreturn_t mtk_vcodec_enc_lt_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned long flags;
	void __iomem *addr;

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_enc_ctx[0];
	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (!ctx)
		return IRQ_HANDLED;

	mtk_v4l2_debug(1, "id=%d", ctx->id);
	ctx->irq_status = readl(dev->enc_reg_base[VENC_LT_SYS] +
		(MTK_VENC_IRQ_STATUS_OFFSET));

	addr = dev->enc_reg_base[VENC_LT_SYS] + MTK_VENC_IRQ_ACK_OFFSET;

	clean_irq_status(ctx->irq_status, addr);

	wake_up_enc_ctx(ctx, MTK_INST_IRQ_RECEIVED);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_enc_lt_irq_handler);



int mtk_vcodec_dec_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	int ret = 0;

	dev->dec_irq[0] = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, dev->dec_irq[0],
		mtk_vcodec_dec_irq_handler, 0, pdev->name, dev);
	if (ret) {
		mtk_v4l2_debug(1, "Failed to install dev->dec_irq %d (%d)",
				dev->dec_irq[0],
				ret);
		return -1;
	}
	disable_irq(dev->dec_irq[0]);
#endif
	return 0;

}
EXPORT_SYMBOL(mtk_vcodec_dec_irq_setup);


int mtk_vcodec_enc_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	int ret = 0;

	dev->enc_irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, dev->enc_irq,
						   mtk_vcodec_enc_irq_handler,
						   0, pdev->name, dev);
	if (ret) {
		mtk_v4l2_debug(1, "Failed to install dev->enc_irq %d (%d)",
				dev->enc_irq,
				ret);
		return -1;
	}
	disable_irq(dev->enc_irq);
#endif
	return 0;

}
EXPORT_SYMBOL(mtk_vcodec_enc_irq_setup);


void mtk_vcodec_gce_timeout_dump(void *ctx)
{
}
EXPORT_SYMBOL(mtk_vcodec_gce_timeout_dump);
