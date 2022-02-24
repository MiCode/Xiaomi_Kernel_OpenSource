// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/wait.h>

#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

int mtk_vcodec_wait_for_done_ctx(struct mtk_vcodec_ctx  *ctx,
	int core_id, int command, unsigned int timeout_ms)

{
	int status = 0;
#ifndef FPGA_INTERRUPT_API_DISABLE
	wait_queue_head_t *waitqueue;
	long timeout_jiff, ret;

	if (core_id >= MTK_VDEC_HW_NUM ||
		core_id < 0) {
		mtk_v4l2_err("ctx %d, invalid core_id=%d", ctx->id, core_id);
		return -1;
	}

	waitqueue = (wait_queue_head_t *)&ctx->queue[core_id];
	timeout_jiff = msecs_to_jiffies(timeout_ms);

	ret = wait_event_interruptible_timeout(*waitqueue,
		ctx->int_cond[core_id],
		timeout_jiff);

	if (!ret) {
		status = -1;    /* timeout */
		mtk_v4l2_err("[%d] cmd=%d, ctx->type=%d, core_id %d timeout time=%ums out %d %d!",
			ctx->id, ctx->type, core_id, command, timeout_ms,
			ctx->int_cond[core_id], ctx->int_type);
	} else if (-ERESTARTSYS == ret) {
		mtk_v4l2_err("[%d] cmd=%d, ctx->type=%d, core_id %d timeout interrupted by a signal %d %d",
			ctx->id, ctx->type, core_id,
			command, ctx->int_cond[core_id],
			ctx->int_type);
		status = -2;
	}

	ctx->int_cond[core_id] = 0;
	ctx->int_type = 0;
#endif
	return status;
}
EXPORT_SYMBOL(mtk_vcodec_wait_for_done_ctx);

/* Wake up context wait_queue */
void wake_up_dec_ctx(struct mtk_vcodec_ctx *ctx, int core_id)
{
	ctx->int_cond[core_id] = 1;
	wake_up_interruptible(&ctx->queue[core_id]);
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

	ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_CORE);
	if (ctx == NULL)
		return IRQ_HANDLED;

	if (ctx->dec_params.svp_mode) {
		mtk_v4l2_debug(4, "svp_mode %d don't handle",
			ctx->dec_params.svp_mode);
		return IRQ_HANDLED;
	}

	/* check if HW active or not */
	cg_status = readl(dev->dec_reg_base[VDEC_SYS]);
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

	wake_up_dec_ctx(ctx, MTK_VDEC_CORE);

	mtk_v4l2_debug(4,
				   "%s :wake up ctx %d, dec_done_status=%x",
				   __func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_dec_irq_handler);

irqreturn_t mtk_vcodec_lat_dec_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status = 0;
	unsigned int dec_done_status = 0;
	void __iomem *vdec_misc_addr = dev->dec_reg_base[VDEC_LAT_MISC] +
		MTK_VDEC_IRQ_CFG_REG;

	ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_LAT);
	if (ctx == NULL)
		return IRQ_HANDLED;

	/* check if HW active or not */
	cg_status = readl(dev->dec_reg_base[VDEC_BASE]);
	if ((cg_status & MTK_VDEC_HW_ACTIVE) != 0) {
		mtk_v4l2_err("DEC LAT ISR, VDEC active is not 0x0 (0x%08x)",
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
		   dev->dec_reg_base[VDEC_LAT_MISC] + MTK_VDEC_IRQ_CFG_REG);
	writel((readl(vdec_misc_addr) & ~MTK_VDEC_IRQ_CLR),
		   dev->dec_reg_base[VDEC_LAT_MISC] + MTK_VDEC_IRQ_CFG_REG);

	wake_up_dec_ctx(ctx, MTK_VDEC_LAT);

	mtk_v4l2_debug(4,
				   "%s :wake up ctx %d, dec_done_status=%x",
				   __func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_lat_dec_irq_handler);


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
	if (ctx == NULL)
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


int mtk_vcodec_dec_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	int i = 0;
	int ret = 0;

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		dev->dec_irq[i] = platform_get_irq(pdev, i);
		if (i == MTK_VDEC_CORE)
			ret = devm_request_irq(&pdev->dev, dev->dec_irq[i],
				mtk_vcodec_dec_irq_handler,
				IRQF_NO_THREAD | IRQF_SHARED | IRQF_PROBE_SHARED, pdev->name, dev);
		else if (i == MTK_VDEC_LAT)
			ret = devm_request_irq(&pdev->dev, dev->dec_irq[i],
				mtk_vcodec_lat_dec_irq_handler, 0,
					pdev->name, dev);
		if (ret) {
			mtk_v4l2_debug(1, "Failed to install dev->dec_irq[%d] %d (%d)",
					i, dev->dec_irq[i],
					ret);
		}
		disable_irq(dev->dec_irq[i]);
	}
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
	struct mtk_vcodec_ctx *curr_ctx = ctx;

	if (curr_ctx->type == MTK_INST_ENCODER)
		mtk_vcodec_enc_timeout_dump(ctx);
	else if (curr_ctx->type == MTK_INST_DECODER)
		mtk_vcodec_dec_timeout_dump(ctx);
}
EXPORT_SYMBOL(mtk_vcodec_gce_timeout_dump);

void mtk_vcodec_enc_timeout_dump(void *ctx)
{
	unsigned long value;
	int i = 0, j = 0;

	struct mtk_vcodec_ctx *curr_ctx = ctx;
	struct mtk_vcodec_dev *dev = NULL;

	#define REG1_COUNT 13
	#define REG2_COUNT 46

	unsigned int Reg_1[REG1_COUNT] = {
		0x14, 0xEC, 0x1C0, 0x1168, 0x11C0,
		0x11C4, 0xF4, 0x5C, 0x60, 0x130,
		0x24, 0x114C, 0x1164};
	unsigned int Reg_2[REG2_COUNT] = {
		0xEC, 0x200, 0x204, 0x208, 0x20C,
		0x210, 0x214, 0x218, 0x21C,
		0x220, 0x224, 0x228, 0x22C,
		0x230, 0x234, 0x238, 0x23C,
		0x240, 0x244, 0x248, 0x24C,
		0x250, 0x254, 0x258, 0x25C,
		0x260, 0x264, 0x268, 0x26C,
		0x270, 0x274, 0x278, 0x27C,
		0x280,
		0x22C, 0x230, 0xF4, 0x1168,
		0x11C0, 0x11C4, 0x1030, 0x240,
		0x248, 0x250, 0x130, 0x140};

	if (ctx == NULL) {
		mtk_v4l2_debug(0, "can't dump venc for NULL ctx");
		return;
	}

	dev = curr_ctx->dev;

	mtk_v4l2_debug(0, "ctx: %p, is_codec_suspending: %d",
	    ctx, dev->is_codec_suspending);

	for (j = 0; j < MTK_VENC_CORE_1; j++) {
		for (i = 0; i < REG1_COUNT; i++) {
			value = readl(dev->enc_reg_base[j] + Reg_1[i]);
			mtk_v4l2_debug(0, "[%d] 0x%x = 0x%lx",
			    j, Reg_1[i], value);
		}
	}


	writel(1, dev->enc_reg_base[0] + 0xEC);
	writel(0, dev->enc_reg_base[0] + 0xF4);

	for (j = 0; j < MTK_VENC_CORE_1; j++) {
		for (i = 0; i < REG2_COUNT; i++) {
			value = readl(dev->enc_reg_base[j] + Reg_2[i]);
			mtk_v4l2_debug(0, "[%d] 0x%x = 0x%lx",
			    j, Reg_2[i], value);
		}
	}

}

void mtk_vcodec_dec_timeout_dump(void *ctx)
{
	unsigned long value;
	int i = 0;

	struct mtk_vcodec_ctx *curr_ctx = ctx;
	struct mtk_vcodec_dev *dev = NULL;

	#define LAT_REG_COUNT 26
	#define CORE_MISC_REG_COUNT 30
	#define CORE_VLD_REG_COUNT 5
	unsigned int lat_reg[LAT_REG_COUNT] = {
		0x5120, 0x512C, 0x5090, // BITCNT, POS, picture start
		0x50E0, 0x50E4, 0x50E8,
		0x1120, 0x1124, 0x1128, 0x112C, 0x196C, // input, cycle
		0x108, 0x110, 0x114, 0x118, 0x11C, // SMI
		0x120, 0x124, 0x128, 0x12C, 0x130, 0x134, 0x138, 0x13C,
		0x854, 0x878}; // crc, wptr
	unsigned int core_misc_reg[CORE_MISC_REG_COUNT] = {
		0x3120, 0x312C, 0x3090, // BITCNT, POS, picture start
		0x30E0, 0x30E4, 0x30E8, // error
		0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
		0xC8, 0xCC, 0x178, // power
		0x108, 0x110, 0x114, 0x118, 0x11C, // SMI
		0x120, 0x124, 0x128, 0x12C, 0x130, 0x134, 0x138, 0x13C};
	unsigned int core_vld_reg[CORE_VLD_REG_COUNT] = {
		0x120, 0x124, 0x128, 0x12C, 0x96C}; // input, cycle

	if (ctx == NULL) {
		mtk_v4l2_debug(0, "can't dump vdec for NULL ctx");
		return;
	}

	dev = curr_ctx->dev;

	mtk_v4l2_debug(0, "ctx: %p, is_codec_suspending: %d",
	    ctx, dev->is_codec_suspending);

	for (i = 0; i < LAT_REG_COUNT; i++) {
		value = readl(dev->dec_reg_base[VDEC_LAT_MISC] + lat_reg[i]);
		mtk_v4l2_debug(0, "[LAT][MISC] 0x%x(%d) = 0x%lx",
			lat_reg[i], ((lat_reg[i]&0x0FFF)%0x800)/4, value);
	}
	for (i = 0; i < CORE_MISC_REG_COUNT; i++) {
		value = readl(dev->dec_reg_base[VDEC_MISC] + core_misc_reg[i]);
		mtk_v4l2_debug(0, "[CORE][MISC] 0x%x(%d) = 0x%lx",
			core_misc_reg[i], (core_misc_reg[i] & 0x0FFF)/4, value);
	}
	for (i = 0; i < CORE_VLD_REG_COUNT; i++) {
		value = readl(dev->dec_reg_base[VDEC_VLD] + core_vld_reg[i]);
		mtk_v4l2_debug(0, "[CORE][VLD] 0x%x(%d) = 0x%lx",
			core_vld_reg[i], (core_vld_reg[i] % 0x800)/4, value);
	}
}
