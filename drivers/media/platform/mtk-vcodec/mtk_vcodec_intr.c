// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/module.h>

#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

int g_dec_irq[MTK_VDEC_HW_NUM] = { 0 };

enum mtk_vdec_hw_id mtk_vdec_map_irq_to_hwid(int irq)
{
	enum mtk_vdec_hw_id core_id = MTK_VDEC_HW_NUM;

	if (irq == g_dec_irq[MTK_VDEC_CORE])
		core_id = MTK_VDEC_CORE;
	else if (irq == g_dec_irq[MTK_VDEC_CORE1])
		core_id = MTK_VDEC_CORE1;
	else if (irq == g_dec_irq[MTK_VDEC_LAT])
		core_id = MTK_VDEC_LAT;
	else if (irq == g_dec_irq[MTK_VDEC_LAT1])
		core_id = MTK_VDEC_LAT1;
	else if (irq == g_dec_irq[MTK_VDEC_LINE_COUNT])
		core_id = MTK_VDEC_LINE_COUNT;

	return core_id;
}
int mtk_vcodec_wait_for_done_ctx(struct mtk_vcodec_ctx  *ctx,
	int core_id, int command, unsigned int timeout_ms)

{
	int status = 0;
#ifndef FPGA_INTERRUPT_API_DISABLE
	wait_queue_head_t *waitqueue;
	long timeout_jiff, ret;

	if (ctx == NULL) {
		mtk_v4l2_err("ctx is NULL! (core_id %d, command 0x%x)", core_id, command);
		return -1;
	}

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
			ctx->id, command, ctx->type, core_id, timeout_ms,
			ctx->int_cond[core_id], ctx->int_type);
		if (ctx->type == MTK_INST_ENCODER)
			mtk_vcodec_enc_timeout_dump(ctx);
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
EXPORT_SYMBOL_GPL(mtk_vcodec_wait_for_done_ctx);

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
	enum mtk_vdec_hw_id core_id;
	enum mtk_dec_dtsi_reg_idx misc_index;
	void __iomem *vdec_misc_addr;

	core_id = mtk_vdec_map_irq_to_hwid(irq);
	if (!((core_id == MTK_VDEC_CORE) || (core_id == MTK_VDEC_CORE1))) {
		mtk_v4l2_err("DEC ISR, core_id(%d) is not right", core_id);
		return IRQ_HANDLED;
	}

	ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_CORE);
	if (ctx == NULL)
		return IRQ_HANDLED;

	misc_index = ((core_id == MTK_VDEC_CORE) ? VDEC_MISC : VDEC_CORE1_MISC);
	vdec_misc_addr = dev->dec_reg_base[misc_index] +
		MTK_VDEC_IRQ_CFG_REG;

	if (ctx->dec_params.svp_mode != OPEN_TEE) {
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
		//rdma crc not to handle
		if ((readl(dev->dec_reg_base[misc_index] + 103 * 4) & 0x10) == 0x10)
			return IRQ_HANDLED;
		/* clear interrupt */
		if ((core_id == MTK_VDEC_CORE) &&
			((readl(dev->dec_reg_base[VDEC_BASE] +
			RW_MCORE_EnableDecode) & 0x01) == 1)) { /*mcore_top base*/
			//clear multi core interrupt
			/*set 1 to clear interrupt*/
			writel((readl(dev->dec_reg_base[VDEC_SOC_GCON] +  9 * 4) | 0x10000),
				(dev->dec_reg_base[VDEC_SOC_GCON] +  9 * 4));
			/*set 0 to clear status*/
			writel((readl(dev->dec_reg_base[VDEC_SOC_GCON] +  9 * 4) & ~0x10000),
				(dev->dec_reg_base[VDEC_SOC_GCON] +  9 * 4));
		} else {
			writel((readl(vdec_misc_addr) | MTK_VDEC_IRQ_CFG),
				vdec_misc_addr);
			writel((readl(vdec_misc_addr) & ~MTK_VDEC_IRQ_CLR),
				vdec_misc_addr);
		}
	}
	wake_up_dec_ctx(ctx, core_id);

	mtk_v4l2_debug(4,
				   "%s :wake up ctx %d, dec_done_status=%x",
				   __func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_dec_irq_handler);

irqreturn_t mtk_vcodec_lat_dec_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	u32 cg_status = 0;
	unsigned int dec_done_status = 0;
	enum mtk_vdec_hw_id core_id;
	enum mtk_dec_dtsi_reg_idx misc_index;
	void __iomem *vdec_misc_addr;

	core_id = mtk_vdec_map_irq_to_hwid(irq);
	if (!((core_id == MTK_VDEC_LAT) || (core_id == MTK_VDEC_LAT1))) {
		mtk_v4l2_err("LAT ISR, core_id(%d) is not right", core_id);
		return IRQ_HANDLED;
	}

	ctx = mtk_vcodec_get_curr_ctx(dev, MTK_VDEC_LAT);
	if (ctx == NULL)
		return IRQ_HANDLED;

	misc_index = ((core_id == MTK_VDEC_LAT) ?
		VDEC_LAT_MISC : VDEC_LAT1_MISC);
	vdec_misc_addr = dev->dec_reg_base[misc_index] +
		MTK_VDEC_IRQ_CFG_REG;

	if (ctx->dec_params.svp_mode != OPEN_TEE) {
		/* check if HW active or not */
		cg_status = readl(dev->dec_reg_base[0]);
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
		if ((core_id == MTK_VDEC_LAT) &&
			(dev->dec_reg_base[VDEC_LAT_TOP] != NULL) &&
			((readl(dev->dec_reg_base[VDEC_LAT_TOP] + 75 * 4) & 0x01)
			== 1)) {  //multi lat clear interrupt
			if (((readl(dev->dec_reg_base[VDEC_LAT_WDMA] + 9 * 4) &
				0x01) == 0) ||
				((readl(dev->dec_reg_base[VDEC_LAT1_WDMA] + 9 * 4) &
				0x01) == 0)) {
				writel((readl(dev->dec_reg_base[VDEC_LAT_WDMA] + 9 * 4)
					| (1 << 24)),
					dev->dec_reg_base[VDEC_SOC_GCON] + 9 * 4);
			} else {
				if (readl(dev->dec_reg_base[VDEC_LAT_WDMA] + 9 * 4) &
					0x01) {
					writel((readl(dev->dec_reg_base[VDEC_LAT_MISC] +
						MTK_VDEC_IRQ_CFG_REG) |
						MTK_VDEC_IRQ_CFG),
						dev->dec_reg_base[VDEC_LAT_MISC] +
						MTK_VDEC_IRQ_CFG_REG);
					writel((readl(dev->dec_reg_base[VDEC_LAT_MISC] +
						MTK_VDEC_IRQ_CFG_REG) &
						~MTK_VDEC_IRQ_CLR),
						dev->dec_reg_base[VDEC_LAT_MISC] +
						MTK_VDEC_IRQ_CFG_REG);
				}
				if (readl(dev->dec_reg_base[VDEC_LAT1_WDMA] + 9 * 4) &
					0x01) {
					writel((readl(dev->dec_reg_base[VDEC_LAT1_MISC]
						+ MTK_VDEC_IRQ_CFG_REG) |
						MTK_VDEC_IRQ_CFG),
						dev->dec_reg_base[VDEC_LAT1_MISC] +
						MTK_VDEC_IRQ_CFG_REG);
					writel((readl(dev->dec_reg_base[VDEC_LAT1_MISC]
						+ MTK_VDEC_IRQ_CFG_REG)
						& ~MTK_VDEC_IRQ_CLR),
						dev->dec_reg_base[VDEC_LAT1_MISC] +
						MTK_VDEC_IRQ_CFG_REG);
				}

			}

		} else {
			writel((readl(vdec_misc_addr) | MTK_VDEC_IRQ_CFG),
				vdec_misc_addr);
			writel((readl(vdec_misc_addr) & ~MTK_VDEC_IRQ_CLR),
				vdec_misc_addr);
		}
	}
	wake_up_dec_ctx(ctx, core_id);

	mtk_v4l2_debug(4,
				   "%s :wake up ctx %d, dec_done_status=%x",
				   __func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_lat_dec_irq_handler);
irqreturn_t mtk_vcodec_line_count_irq_handler(int irq,
	void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned int dec_done_status = 0;

	enum mtk_vdec_hw_id core_id;

	core_id = mtk_vdec_map_irq_to_hwid(irq);
	if (!(core_id == MTK_VDEC_LINE_COUNT)) {
		mtk_v4l2_err("LINE_COUNT ISR, core_id(%d) is not right",
			core_id);
		return IRQ_HANDLED;
	}

	ctx = mtk_vcodec_get_curr_ctx(dev, core_id);
	if (ctx == NULL)
		return IRQ_HANDLED;

	if (ctx->dec_params.svp_mode != OPEN_TEE) {
		dec_done_status = readl(dev->dec_reg_base[VDEC_UFO_ENC] + 122 * 4);
		ctx->irq_status = dec_done_status;
		if ((dec_done_status & (1 << 16)) != (1 << 16)) {
			mtk_v4l2_err("DEC line_count ISR, VDEC active is not 1 (0x%08x)",
				dec_done_status);
			return IRQ_HANDLED;
		}

		/* clear interrupt */
		writel((readl(dev->dec_reg_base[VDEC_UFO_ENC] + 122 * 4) | 0x1),
			dev->dec_reg_base[VDEC_UFO_ENC] + 122 * 4);

		writel((readl(dev->dec_reg_base[VDEC_UFO_ENC] + 122 * 4) & ~0x1),
			dev->dec_reg_base[VDEC_UFO_ENC] + 122 * 4);
	}
	wake_up_dec_ctx(ctx, core_id);

	mtk_v4l2_debug(4,
		"%s :wake up ctx %d, dec_done_status=%x",
		__func__, ctx->id, dec_done_status);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(mtk_vcodec_line_count_irq_handler);

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
void wake_up_enc_ctx(struct mtk_vcodec_ctx *ctx, unsigned int reason, int core_id)
{
	ctx->int_cond[core_id] = 1;
	ctx->int_type = reason;
	wake_up_interruptible(&ctx->queue[core_id]);
}

irqreturn_t mtk_vcodec_enc_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned long flags;
	void __iomem *addr;

	if (!dev) {
		mtk_v4l2_err("dev null invalid");
		return IRQ_NONE;
	}

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

	wake_up_enc_ctx(ctx, MTK_INST_IRQ_RECEIVED, 0);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_enc_irq_handler);

irqreturn_t mtk_vcodec_c1_enc_irq_handler(int irq, void *priv)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	struct mtk_vcodec_dev *dev = priv;
	struct mtk_vcodec_ctx *ctx;
	unsigned long flags;
	void __iomem *addr;

	if (!dev) {
		mtk_v4l2_err("dev null invalid");
		return IRQ_NONE;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_enc_ctx[1];
	spin_unlock_irqrestore(&dev->irqlock, flags);
	if (ctx == NULL)
		return IRQ_HANDLED;

	mtk_v4l2_debug(1, "id=%d", ctx->id);
	addr = dev->enc_reg_base[VENC_C1_SYS] + MTK_VENC_IRQ_ACK_OFFSET;

	ctx->irq_status = readl(dev->enc_reg_base[VENC_C1_SYS] +
		(MTK_VENC_IRQ_STATUS_OFFSET));

	clean_irq_status(ctx->irq_status, addr);

	wake_up_enc_ctx(ctx, MTK_INST_IRQ_RECEIVED, 1);
#endif
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_c1_enc_irq_handler);



int mtk_vcodec_dec_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	int i = 0;
	int ret = 0;

	if (!pdev || !dev) {
		mtk_v4l2_err("pdev %p dev %p invalid", pdev, dev);
		return -1;
	}

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		dev->dec_irq[i] = platform_get_irq(pdev, i);
		g_dec_irq[i] = dev->dec_irq[i];
		if ((i == MTK_VDEC_CORE) || (i == MTK_VDEC_CORE1))
			ret = devm_request_irq(&pdev->dev, dev->dec_irq[i],
				mtk_vcodec_dec_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED,
				pdev->name, dev);
		else if ((i == MTK_VDEC_LAT) || (i == MTK_VDEC_LAT1))
			ret = devm_request_irq(&pdev->dev, dev->dec_irq[i],
				mtk_vcodec_lat_dec_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED,
					pdev->name, dev);
		else if (i == MTK_VDEC_LINE_COUNT)
			ret = devm_request_irq(&pdev->dev, dev->dec_irq[i],
				mtk_vcodec_line_count_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED,
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
EXPORT_SYMBOL_GPL(mtk_vcodec_dec_irq_setup);


int mtk_vcodec_enc_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev)
{
#ifndef FPGA_INTERRUPT_API_DISABLE
	int i = 0;
	int ret = 0;

	if (!pdev || !dev) {
		mtk_v4l2_err("pdev %p dev %p invalid", pdev, dev);
		return -1;
	}

	for (i = 0; i < MTK_VENC_HW_NUM; i++) {
		dev->enc_irq[i] = platform_get_irq(pdev, i);
		if (dev->enc_irq[i] < 0) {
			pr_info("no IRQ resource, hw id: %d", i);
			break;
		}

		pr_info("get IRQ resource, hw id: %d irq %d", i, dev->enc_irq[i]);

		if (i == MTK_VENC_CORE_0)
			ret = devm_request_irq(&pdev->dev, dev->enc_irq[i],
							   mtk_vcodec_enc_irq_handler,
							   0, pdev->name, dev);
		else if (i == MTK_VENC_CORE_1)
			ret = devm_request_irq(&pdev->dev, dev->enc_irq[i],
							   mtk_vcodec_c1_enc_irq_handler,
							   0, pdev->name, dev);

		if (ret) {
			mtk_v4l2_debug(1, "Failed to install dev->enc_irq %d (%d)",
					dev->enc_irq[i],
					ret);
			return -1;
		}
		disable_irq(dev->enc_irq[i]);
	}
#endif
	return 0;

}
EXPORT_SYMBOL_GPL(mtk_vcodec_enc_irq_setup);


void mtk_vcodec_gce_timeout_dump(void *ctx)
{
	struct mtk_vcodec_ctx *curr_ctx = ctx;

	if (!ctx) {
		mtk_v4l2_err("ctx null invalid");
		return;
	}

	if (curr_ctx->type == MTK_INST_ENCODER)
		mtk_vcodec_enc_timeout_dump(ctx);
	else if (curr_ctx->type == MTK_INST_DECODER)
		mtk_vcodec_dec_timeout_dump(ctx);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_gce_timeout_dump);

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
		0xCB, 0xCC, 0x178, // power
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

	if (dev->vdec_hw_ipm == VCODEC_IPM_V2) {
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
}


MODULE_LICENSE("GPL v2");

