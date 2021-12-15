/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_trace.h"
#include "mtk_drm_drv.h"
#include "cmdq-sec.h"

#define DISP_REG_WDMA_INTEN 0x0000
#define INTEN_FLD_FME_CPL_INTEN REG_FLD_MSB_LSB(0, 0)
#define INTEN_FLD_FME_UND_INTEN REG_FLD_MSB_LSB(1, 1)
#define DISP_REG_WDMA_INTSTA 0x0004
#define DISP_REG_WDMA_EN 0x0008
#define WDMA_EN BIT(0)
#define DISP_REG_WDMA_RST 0x000c
#define DISP_REG_WDMA_SMI_CON 0x0010
#define SMI_CON_FLD_THRESHOLD REG_FLD_MSB_LSB(3, 0)
#define SMI_CON_FLD_SLOW_ENABLE REG_FLD_MSB_LSB(4, 4)
#define SMI_CON_FLD_SLOW_LEVEL REG_FLD_MSB_LSB(7, 5)
#define SMI_CON_FLD_SLOW_COUNT REG_FLD_MSB_LSB(15, 8)
#define SMI_CON_FLD_SMI_Y_REPEAT_NUM REG_FLD_MSB_LSB(19, 16)
#define SMI_CON_FLD_SMI_U_REPEAT_NUM REG_FLD_MSB_LSB(23, 20)
#define SMI_CON_FLD_SMI_V_REPEAT_NUM REG_FLD_MSB_LSB(27, 24)
#define SMI_CON_FLD_SMI_OBUF_FULL_REQ REG_FLD_MSB_LSB(28, 28)
#define DISP_REG_WDMA_CFG 0x0014
#define CFG_FLD_UFO_DCP_ENABLE REG_FLD_MSB_LSB(18, 18)
#define WDMA_OUT_FMT (0xf << 4)
#define WDMA_CT_EN BIT(11)
#define WDMA_CON_SWAP BIT(16)
#define WDMA_UFO_DCP_ENABLE BIT(18)
#define WDMA_INT_MTX_SEL (0xf << 24)
#define DISP_REG_WDMA_SRC_SIZE 0x0018
#define DISP_REG_WDMA_CLIP_SIZE 0x001c
#define DISP_REG_WDMA_CLIP_COORD 0x0020
#define DISP_REG_WDMA_SHADOW_CTRL 0x0024
#define WDMA_BYPASS_SHADOW BIT(1)
#define WDMA_READ_WRK_REG BIT(2)
#define DISP_REG_WDMA_DST_WIN_BYTE 0x0028
#define DISP_REG_WDMA_ALPHA 0x002C
#define DISP_REG_WDMA_DST_UV_PITCH 0x0078
#define DISP_REG_WDMA_DST_ADDR_OFFSET0 0x0080
#define DISP_REG_WDMA_DST_ADDR_OFFSET1 0x0084
#define DISP_REG_WDMA_DST_ADDR_OFFSET2 0x0088

#define DISP_REG_WDMA_FLOW_CTRL_DBG 0x00A0
#define FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL REG_FLD_MSB_LSB(9, 0)
#define FLOW_CTRL_DBG_FLD_WDMA_IN_READY REG_FLD_MSB_LSB(14, 14)
#define FLOW_CTRL_DBG_FLD_WDMA_IN_VALID REG_FLD_MSB_LSB(15, 15)
#define DISP_REG_WDMA_EXEC_DBG 0x000A4
#define EXEC_DBG_FLD_WDMA_STA_EXEC REG_FLD_MSB_LSB(31, 0)
#define DISP_REG_WDMA_INPUT_CNT_DBG 0x000A8
#define DISP_REG_WDMA_DEBUG 0x000B8
#define DISP_REG_WDMA_BUF_CON3 0x0104
#define BUF_CON3_FLD_ISSUE_REQ_TH_Y REG_FLD_MSB_LSB(8, 0)
#define BUF_CON3_FLD_ISSUE_REQ_TH_U REG_FLD_MSB_LSB(24, 16)
#define DISP_REG_WDMA_BUF_CON4 0x0108
#define BUF_CON4_FLD_ISSUE_REQ_TH_V REG_FLD_MSB_LSB(8, 0)
#define DISP_REG_WDMA_BUF_CON1 0x0038
#define BUF_CON1_FLD_FIFO_PSEUDO_SIZE REG_FLD_MSB_LSB(9, 0)
#define BUF_CON1_FLD_FIFO_PSEUDO_SIZE_UV REG_FLD_MSB_LSB(18, 10)
#define BUF_CON1_FLD_URGENT_EN REG_FLD_MSB_LSB(26, 26)
#define BUF_CON1_FLD_ALPHA_MASK_EN REG_FLD_MSB_LSB(27, 27)
#define BUF_CON1_FLD_FRAME_END_ULTRA REG_FLD_MSB_LSB(28, 28)
#define BUF_CON1_FLD_GCLAST_EN REG_FLD_MSB_LSB(29, 29)
#define BUF_CON1_FLD_PRE_ULTRA_ENABLE REG_FLD_MSB_LSB(30, 30)
#define BUF_CON1_FLD_ULTRA_ENABLE REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_WDMA_BUF_CON5 0x0200
#define DISP_REG_WDMA_BUF_CON6 0x0204
#define DISP_REG_WDMA_BUF_CON7 0x0208
#define DISP_REG_WDMA_BUF_CON8 0x020C
#define DISP_REG_WDMA_BUF_CON9 0x0210
#define DISP_REG_WDMA_BUF_CON10 0x0214
#define DISP_REG_WDMA_BUF_CON11 0x0218
#define DISP_REG_WDMA_BUF_CON12 0x021C
#define DISP_REG_WDMA_BUF_CON13 0x0220
#define DISP_REG_WDMA_BUF_CON14 0x0224
#define DISP_REG_WDMA_BUF_CON15 0x0228
#define BUF_CON_FLD_PRE_ULTRA_LOW REG_FLD_MSB_LSB(9, 0)
#define BUF_CON_FLD_ULTRA_LOW REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_BUF_CON16 0x022C
#define BUF_CON_FLD_PRE_ULTRA_HIGH REG_FLD_MSB_LSB(9, 0)
#define BUF_CON_FLD_ULTRA_HIGH REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_BUF_CON17 0x0230
#define BUF_CON17_FLD_WDMA_DVFS_EN REG_FLD_MSB_LSB(0, 0)
#define BUF_CON17_FLD_DVFS_TH_Y REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_BUF_CON18 0x0234
#define BUF_CON18_FLD_DVFS_TH_U REG_FLD_MSB_LSB(9, 0)
#define BUF_CON18_FLD_DVFS_TH_V REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_DRS_CON0 0x0250
#define DISP_REG_WDMA_DRS_CON1 0x0254
#define DISP_REG_WDMA_DRS_CON2 0x0258
#define DISP_REG_WDMA_DRS_CON3 0x025C

#define DISP_REG_WDMA_URGENT_CON0 0x0260
#define FLD_WDMA_URGENT_LOW_Y REG_FLD_MSB_LSB(9, 0)
#define FLD_WDMA_URGENT_HIGH_Y REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_URGENT_CON1 0x0264
#define FLD_WDMA_URGENT_LOW_U REG_FLD_MSB_LSB(9, 0)
#define FLD_WDMA_URGENT_HIGH_U REG_FLD_MSB_LSB(25, 16)
#define DISP_REG_WDMA_URGENT_CON2 0x0268
#define FLD_WDMA_URGENT_LOW_V REG_FLD_MSB_LSB(9, 0)
#define FLD_WDMA_URGENT_HIGH_V REG_FLD_MSB_LSB(25, 16)

#define DISP_REG_WDMA_DST_ADDR0 0x0f00
#define DISP_REG_WDMA_DST_ADDR1 0x0f04
#define DISP_REG_WDMA_DST_ADDR2 0x0f08
#define DISP_REG_WDMA_RGB888 0x10

#define MEM_MODE_INPUT_FORMAT_RGB565 0x0U
#define MEM_MODE_INPUT_FORMAT_RGB888 (0x001U << 4)
#define MEM_MODE_INPUT_FORMAT_RGBA8888 (0x002U << 4)
#define MEM_MODE_INPUT_FORMAT_ARGB8888 (0x003U << 4)
#define MEM_MODE_INPUT_FORMAT_UYVY (0x004U << 4)
#define MEM_MODE_INPUT_FORMAT_YUYV (0x005U << 4)
#define MEM_MODE_INPUT_FORMAT_IYUV (0x008U << 4)
#define MEM_MODE_INPUT_SWAP BIT(16)

enum GS_WDMA_FLD {
	GS_WDMA_SMI_CON = 0, /* whole reg */
	GS_WDMA_BUF_CON1,    /* whole reg */
	GS_WDMA_PRE_ULTRA_LOW_Y,
	GS_WDMA_ULTRA_LOW_Y,
	GS_WDMA_PRE_ULTRA_HIGH_Y,
	GS_WDMA_ULTRA_HIGH_Y,
	GS_WDMA_PRE_ULTRA_LOW_U,
	GS_WDMA_ULTRA_LOW_U,
	GS_WDMA_PRE_ULTRA_HIGH_U,
	GS_WDMA_ULTRA_HIGH_U,
	GS_WDMA_PRE_ULTRA_LOW_V,
	GS_WDMA_ULTRA_LOW_V,
	GS_WDMA_PRE_ULTRA_HIGH_V,
	GS_WDMA_ULTRA_HIGH_V,
	GS_WDMA_PRE_ULTRA_LOW_Y_DVFS,
	GS_WDMA_ULTRA_LOW_Y_DVFS,
	GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS,
	GS_WDMA_ULTRA_HIGH_Y_DVFS,
	GS_WDMA_PRE_ULTRA_LOW_U_DVFS,
	GS_WDMA_ULTRA_LOW_U_DVFS,
	GS_WDMA_PRE_ULTRA_HIGH_U_DVFS,
	GS_WDMA_ULTRA_HIGH_U_DVFS,
	GS_WDMA_PRE_ULTRA_LOW_V_DVFS,
	GS_WDMA_ULTRA_LOW_V_DVFS,
	GS_WDMA_PRE_ULTRA_HIGH_V_DVFS,
	GS_WDMA_ULTRA_HIGH_V_DVFS,
	GS_WDMA_DVFS_EN,
	GS_WDMA_DVFS_TH_Y,
	GS_WDMA_DVFS_TH_U,
	GS_WDMA_DVFS_TH_V,
	GS_WDMA_URGENT_LOW_Y,
	GS_WDMA_URGENT_HIGH_Y,
	GS_WDMA_URGENT_LOW_U,
	GS_WDMA_URGENT_HIGH_U,
	GS_WDMA_URGENT_LOW_V,
	GS_WDMA_URGENT_HIGH_V,
	GS_WDMA_ISSUE_REG_TH_Y,
	GS_WDMA_ISSUE_REG_TH_U,
	GS_WDMA_ISSUE_REG_TH_V,
	GS_WDMA_FLD_NUM,
};

struct mtk_disp_wdma_data {
	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	bool support_shadow;
};

struct mtk_wdma_cfg_info {
	unsigned int addr;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
};

/**
 * struct mtk_disp_wdma - DISP_RDMA driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_wdma {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_wdma_data *data;
	struct mtk_wdma_cfg_info cfg_info;
};

static irqreturn_t mtk_wdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_wdma *priv = dev_id;
	struct mtk_ddp_comp *wdma = &priv->ddp_comp;
	struct mtk_cwb_info *cwb_info;
	struct mtk_drm_private *drm_priv;
	unsigned int buf_idx;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (mtk_drm_top_clk_isr_get("wdma_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(wdma->regs + DISP_REG_WDMA_INTSTA);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, val);

	if (wdma->id == DDP_COMPONENT_WDMA0)
		DRM_MMP_MARK(wdma0, val, 0);

	if (val & 0x2)
		DRM_MMP_MARK(abnormal_irq, val, wdma->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(wdma), val);

	writel(~val, wdma->regs + DISP_REG_WDMA_INTSTA);

	if (val & (1 << 0)) {
		struct mtk_drm_crtc *mtk_crtc = wdma->mtk_crtc;

		DDPIRQ("[IRQ] %s: frame complete!\n",
			mtk_dump_comp_str(wdma));
		drm_priv = mtk_crtc->base.dev->dev_private;
		cwb_info = mtk_crtc->cwb_info;
		if (cwb_info && cwb_info->enable &&
			cwb_info->comp->id == wdma->id &&
			!drm_priv->cwb_is_preempted) {
			buf_idx = cwb_info->buf_idx;
			cwb_info->buffer[buf_idx].timestamp = 100;
			atomic_set(&mtk_crtc->cwb_task_active, 1);
			wake_up_interruptible(&mtk_crtc->cwb_wq);
		}
		if (mtk_crtc->dc_main_path_commit_task) {
			atomic_set(
				&mtk_crtc->dc_main_path_commit_event, 1);
			wake_up_interruptible(
				&mtk_crtc->dc_main_path_commit_wq);
		}
		MMPathTraceDRM(wdma);
	}
	if (val & (1 << 1))
		DDPPR_ERR("[IRQ] %s: frame underrun!\n",
			  mtk_dump_comp_str(wdma));

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put("wdma_irq");

	return ret;
}

static inline struct mtk_disp_wdma *comp_to_wdma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_wdma, ddp_comp);
}

static void mtk_wdma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_wdma *wdma = comp_to_wdma(comp);
	const struct mtk_disp_wdma_data *data = wdma->data;
	unsigned int inten;
	bool en = 1;

	inten = REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
		REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1);
	mtk_ddp_write(comp, WDMA_EN, DISP_REG_WDMA_EN, handle);
	mtk_ddp_write(comp, inten, DISP_REG_WDMA_INTEN, handle);

	if (data && data->sodi_config)
		data->sodi_config(comp->mtk_crtc->base.dev, comp->id, handle,
				  &en);
}

static void mtk_wdma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_wdma *wdma = comp_to_wdma(comp);
	const struct mtk_disp_wdma_data *data = wdma->data;
	bool en = 0;

	mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_INTEN, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_EN, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_INTSTA, handle);

	if (data && data->sodi_config)
		data->sodi_config(comp->mtk_crtc->base.dev,
			comp->id, handle, &en);
	mtk_ddp_write(comp, 0x01, DISP_REG_WDMA_RST, handle);
	mtk_ddp_write(comp, 0x00, DISP_REG_WDMA_RST, handle);
}

static int mtk_wdma_is_busy(struct mtk_ddp_comp *comp)
{
	int ret, tmp;

	tmp = readl(comp->regs + DISP_REG_WDMA_FLOW_CTRL_DBG);
	ret = ((tmp & REG_FLD_MASK(FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL)) != 0x1) ? 1 : 0;

	DDPINFO("%s:%d is:%d regs:0x%x\n", __func__, __LINE__, ret, tmp);

	return ret;
}

static void mtk_wdma_prepare(struct mtk_ddp_comp *comp)
{
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	struct mtk_disp_wdma *wdma = comp_to_wdma(comp);
#endif

	mtk_ddp_comp_clk_prepare(comp);

#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (wdma->data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, 0x0,
			DISP_REG_WDMA_SHADOW_CTRL, WDMA_BYPASS_SHADOW);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, WDMA_BYPASS_SHADOW,
			DISP_REG_WDMA_SHADOW_CTRL, WDMA_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) \
	|| defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	/* Bypass shadow register and read shadow register */
	mtk_ddp_write_mask_cpu(comp, WDMA_BYPASS_SHADOW,
		DISP_REG_WDMA_SHADOW_CTRL, WDMA_BYPASS_SHADOW);
#endif
#endif
}

static void mtk_wdma_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_wdma_calc_golden_setting(struct golden_setting_context *gsc,
					 unsigned int format,
					 unsigned int is_primary_flag,
					 unsigned int *gs)
{
	unsigned int preultra_low_us = 7, preultra_high_us = 6;
	unsigned int ultra_low_us = 6, ultra_high_us = 4;
	unsigned int dvfs_offset = 2;
	unsigned int urgent_low_offset = 4, urgent_high_offset = 3;
	unsigned int Bpp = 3;
	unsigned int FP = 100;
	unsigned int res = 0;
	unsigned int frame_rate = 0;
	unsigned long long consume_rate = 0;
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
	unsigned int fifo_size = 325;
	unsigned int fifo_size_uv = 31;
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) \
	|| defined(CONFIG_MACH_MT6877)
	unsigned int fifo_size = 578;
	unsigned int fifo_size_uv = 29;
#endif
#if defined(CONFIG_MACH_MT6781)
	unsigned int fifo_size = 410;
	unsigned int fifo_size_uv = 200;
#endif
	unsigned int fifo;
	unsigned int factor1 = 4;
	unsigned int factor2 = 4;
	unsigned int tmp;

	frame_rate = 60;
	res = gsc->dst_width * gsc->dst_height;

	consume_rate = res * frame_rate;
	do_div(consume_rate, 1000);
	consume_rate *= 125; /* PF = 100 */
	do_div(consume_rate, 16 * 1000);

	/* WDMA_SMI_CON */
	if (format == DRM_FORMAT_YVU420 || format == DRM_FORMAT_YUV420)
		gs[GS_WDMA_SMI_CON] = 0x11140007;
	else
		gs[GS_WDMA_SMI_CON] = 0x12240007;

	/* WDMA_BUF_CON1 */
	if (!gsc->is_dc)
		gs[GS_WDMA_BUF_CON1] = 0xD4000000;
	else
		gs[GS_WDMA_BUF_CON1] = 0x40000000;

	switch (format) {
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		/* 3 plane */
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		fifo_size = 228;
		fifo_size_uv = 50;
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
		fifo_size = 402;
		fifo_size_uv = 99;
#endif
		fifo = fifo_size_uv;
		factor1 = 4;
		factor2 = 4;
		Bpp = 1;

		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		/* 2 plane */
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		fifo_size = 228;
		fifo_size_uv = 109;
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
		fifo_size = 402;
		fifo_size_uv = 201;
#endif
		fifo = fifo_size_uv;
		factor1 = 2;
		factor2 = 4;
		Bpp = 1;

		break;
	default:
		/* 1 plane */
		/* fifo_size keep default */
		/* Bpp keep default */
		factor1 = 4;
		factor2 = 4;
		fifo = fifo_size / 4;

		break;
	}

	gs[GS_WDMA_BUF_CON1] += (fifo_size_uv << 10) + fifo_size;

#if BITS_PER_LONG == 64
	/* WDMA_BUF_CON5 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * preultra_low_us, FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * ultra_low_us, FP);
	gs[GS_WDMA_ULTRA_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON6 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * preultra_high_us, FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * ultra_high_us, FP);
	gs[GS_WDMA_ULTRA_HIGH_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON7 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_low_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_low_us, FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON8 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_high_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_high_us, FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON9 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_low_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_low_us, FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON10 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_high_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_high_us, FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON11 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * (preultra_low_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;
	tmp = DIV_ROUND_UP(consume_rate * Bpp * (ultra_low_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_ULTRA_LOW_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON12 */
	tmp = DIV_ROUND_UP(
		consume_rate * Bpp * (preultra_high_us + dvfs_offset), FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;
	tmp = DIV_ROUND_UP(consume_rate * Bpp * (ultra_high_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_ULTRA_HIGH_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON13 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON14 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON15 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON16 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON17 */
	gs[GS_WDMA_DVFS_EN] = 1;
	gs[GS_WDMA_DVFS_TH_Y] = gs[GS_WDMA_ULTRA_HIGH_Y_DVFS];

	/* WDMA_BUF_CON18 */
	gs[GS_WDMA_DVFS_TH_U] = gs[GS_WDMA_ULTRA_HIGH_U_DVFS];
	gs[GS_WDMA_DVFS_TH_V] = gs[GS_WDMA_ULTRA_HIGH_V_DVFS];

	/* WDMA URGENT CONTROL 0 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * urgent_low_offset, FP);
	gs[GS_WDMA_URGENT_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * urgent_high_offset, FP);
	gs[GS_WDMA_URGENT_HIGH_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA URGENT CONTROL 1 */
	tmp = DIV_ROUND_UP(consume_rate * urgent_low_offset, FP * factor1);
	gs[GS_WDMA_URGENT_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * urgent_high_offset, FP * factor1);
	gs[GS_WDMA_URGENT_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA URGENT CONTROL 2 */
	tmp = DIV_ROUND_UP(consume_rate * urgent_low_offset, FP * factor2);
	gs[GS_WDMA_URGENT_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * urgent_high_offset, FP * factor2);
	gs[GS_WDMA_URGENT_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;

#elif BITS_PER_LONG == 32
	/* WDMA_BUF_CON5 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * preultra_low_us, FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * ultra_low_us, FP);
	gs[GS_WDMA_ULTRA_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON6 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * preultra_high_us, FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * ultra_high_us, FP);
	gs[GS_WDMA_ULTRA_HIGH_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON7 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * preultra_low_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * ultra_low_us, FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON8 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * preultra_high_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * ultra_high_us, FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON9 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * preultra_low_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * ultra_low_us, FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON10 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * preultra_high_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * ultra_high_us, FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON11 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * (preultra_low_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * (ultra_low_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_ULTRA_LOW_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON12 */
	tmp = DIV64_U64_ROUND_UP(
		consume_rate * Bpp * (preultra_high_us + dvfs_offset), FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * (ultra_high_us + dvfs_offset),
			   FP);
	gs[GS_WDMA_ULTRA_HIGH_Y_DVFS] =
		(fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON13 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON14 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
			   FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON15 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON16 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
			   FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA_BUF_CON17 */
	gs[GS_WDMA_DVFS_EN] = 1;
	gs[GS_WDMA_DVFS_TH_Y] = gs[GS_WDMA_ULTRA_HIGH_Y_DVFS];

	/* WDMA_BUF_CON18 */
	gs[GS_WDMA_DVFS_TH_U] = gs[GS_WDMA_ULTRA_HIGH_U_DVFS];
	gs[GS_WDMA_DVFS_TH_V] = gs[GS_WDMA_ULTRA_HIGH_V_DVFS];

	/* WDMA URGENT CONTROL 0 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * urgent_low_offset, FP);
	gs[GS_WDMA_URGENT_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * Bpp * urgent_high_offset, FP);
	gs[GS_WDMA_URGENT_HIGH_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA URGENT CONTROL 1 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * urgent_low_offset, FP * factor1);
	gs[GS_WDMA_URGENT_LOW_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * urgent_high_offset, FP * factor1);
	gs[GS_WDMA_URGENT_HIGH_U] = (fifo > tmp) ? (fifo - tmp) : 1;

	/* WDMA URGENT CONTROL 2 */
	tmp = DIV64_U64_ROUND_UP(consume_rate * urgent_low_offset, FP * factor2);
	gs[GS_WDMA_URGENT_LOW_V] = (fifo > tmp) ? (fifo - tmp) : 1;

	tmp = DIV64_U64_ROUND_UP(consume_rate * urgent_high_offset, FP * factor2);
	gs[GS_WDMA_URGENT_HIGH_V] = (fifo > tmp) ? (fifo - tmp) : 1;
#else
	#error "check for api use for this architeacture"
#endif
	/* WDMA Buf Constant 3 */
	gs[GS_WDMA_ISSUE_REG_TH_Y] = 16;
	gs[GS_WDMA_ISSUE_REG_TH_U] = 16;

	/* WDMA Buf Constant 4 */
	gs[GS_WDMA_ISSUE_REG_TH_V] = 16;
}

static void mtk_wdma_golden_setting(struct mtk_ddp_comp *comp,
				    struct golden_setting_context *gsc,
				    struct cmdq_pkt *handle)
{
	unsigned int gs[GS_WDMA_FLD_NUM];
	unsigned int value = 0;

	if (!gsc) {
		DDPPR_ERR("golden setting is null, %s,%d\n", __FILE__,
			  __LINE__);
		return;
	}
	mtk_wdma_calc_golden_setting(gsc, comp->fb->format->format, true, gs);

#if 0
	mtk_ddp_write(comp, 0x800000ff, 0x2C, handle);

	mtk_ddp_write(comp, 0xd4000529, 0x38, handle);

	mtk_ddp_write(comp, 0x00640043, 0x200, handle);
	mtk_ddp_write(comp, 0x00a50064, 0x204, handle);
	mtk_ddp_write(comp, 0x00390036, 0x208, handle);
	mtk_ddp_write(comp, 0x003f0039, 0x20C, handle);
	mtk_ddp_write(comp, 0x00390036, 0x210, handle);
	mtk_ddp_write(comp, 0x003f0039, 0x214, handle);
	mtk_ddp_write(comp, 0x00220001, 0x218, handle);
	mtk_ddp_write(comp, 0x00640022, 0x21C, handle);
	mtk_ddp_write(comp, 0x00340031, 0x220, handle);
	mtk_ddp_write(comp, 0x00390034, 0x224, handle);
	mtk_ddp_write(comp, 0x00340031, 0x228, handle);
	mtk_ddp_write(comp, 0x00390034, 0x22C, handle);

	mtk_ddp_write(comp, 0x00640001, 0x230, handle);
	mtk_ddp_write(comp, 0x00390039, 0x234, handle);

	mtk_ddp_write(comp, 0x00300000, 0x250, handle);
	mtk_ddp_write(comp, 0x00300030, 0x254, handle);
	mtk_ddp_write(comp, 0x00300000, 0x258, handle);
	mtk_ddp_write(comp, 0x00300030, 0x25C, handle);

	mtk_ddp_write(comp, 165 | (198 << 16), 0x260, handle);
	mtk_ddp_write(comp, 63 | (65 << 16), 0x264, handle);
	mtk_ddp_write(comp, 63 | (65 << 16), 0x268, handle);
#else
	/* WDMA_SMI_CON */
	value = gs[GS_WDMA_SMI_CON];
	mtk_ddp_write(comp, value, DISP_REG_WDMA_SMI_CON, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_SMI_CON, value);

	/* WDMA_BUF_CON1 */
	value = gs[GS_WDMA_BUF_CON1];
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON1, handle);
	//	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON1, value);

	/* WDMA BUF CONST 5 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_Y] + (gs[GS_WDMA_ULTRA_LOW_Y] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON5, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON5, value);

	/* WDMA BUF CONST 6 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_Y] + (gs[GS_WDMA_ULTRA_HIGH_Y] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON6, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON6, value);

	/* WDMA BUF CONST 7 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_U] + (gs[GS_WDMA_ULTRA_LOW_U] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON7, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON7, value);

	/* WDMA BUF CONST 8 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_U] + (gs[GS_WDMA_ULTRA_HIGH_U] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON8, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON8, value);

	/* WDMA BUF CONST 9 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_V] + (gs[GS_WDMA_ULTRA_LOW_V] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON9, handle);
	// DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON9, value);

	/* WDMA BUF CONST 10 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_V] + (gs[GS_WDMA_ULTRA_HIGH_V] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON10, handle);

	/* WDMA BUF CONST 11 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_Y_DVFS] +
		(gs[GS_WDMA_ULTRA_LOW_Y_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON11, handle);

	/* WDMA BUF CONST 12 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS] +
		(gs[GS_WDMA_ULTRA_HIGH_Y_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON12, handle);

	/* WDMA BUF CONST 13 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_U_DVFS] +
		(gs[GS_WDMA_ULTRA_LOW_U_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON13, handle);

	/* WDMA BUF CONST 14 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_U_DVFS] +
		(gs[GS_WDMA_ULTRA_HIGH_U_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON14, handle);

	/* WDMA BUF CONST 15 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_V_DVFS] +
		(gs[GS_WDMA_ULTRA_LOW_V_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON15, handle);

	/* WDMA BUF CONST 16 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_V_DVFS] +
		(gs[GS_WDMA_ULTRA_HIGH_V_DVFS] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON16, handle);

	/* WDMA BUF CONST 17 */
	value = gs[GS_WDMA_DVFS_EN] + (gs[GS_WDMA_DVFS_TH_Y] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON17, handle);

	/* WDMA BUF CONST 18 */
	value = gs[GS_WDMA_DVFS_TH_U] + (gs[GS_WDMA_DVFS_TH_V] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON18, handle);

	/* WDMA URGENT CON0 */
	value = gs[GS_WDMA_URGENT_LOW_Y] + (gs[GS_WDMA_URGENT_HIGH_Y] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_URGENT_CON0, handle);

	/* WDMA URGENT CON1 */
	value = gs[GS_WDMA_URGENT_LOW_U] + (gs[GS_WDMA_URGENT_HIGH_U] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_URGENT_CON1, handle);

	/* WDMA URGENT CON2 */
	value = gs[GS_WDMA_URGENT_LOW_V] + (gs[GS_WDMA_URGENT_HIGH_V] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_URGENT_CON2, handle);

	/* WDMA_BUF_CON3 */
	value = gs[GS_WDMA_ISSUE_REG_TH_Y] + (gs[GS_WDMA_ISSUE_REG_TH_U] << 16);
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON3, handle);

	/* WDMA_BUF_CON4 */
	value = gs[GS_WDMA_ISSUE_REG_TH_V];
	mtk_ddp_write(comp, value, DISP_REG_WDMA_BUF_CON4, handle);
#endif
}

static unsigned int wdma_fmt_convert(unsigned int fmt)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return MEM_MODE_INPUT_FORMAT_RGB565;
	case DRM_FORMAT_BGR565:
		return MEM_MODE_INPUT_FORMAT_RGB565 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_RGB888:
		return MEM_MODE_INPUT_FORMAT_RGB888;
	case DRM_FORMAT_BGR888:
		return MEM_MODE_INPUT_FORMAT_RGB888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		return MEM_MODE_INPUT_FORMAT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		return MEM_MODE_INPUT_FORMAT_ARGB8888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		return MEM_MODE_INPUT_FORMAT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return MEM_MODE_INPUT_FORMAT_RGBA8888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_UYVY:
		return MEM_MODE_INPUT_FORMAT_UYVY;
	case DRM_FORMAT_YUYV:
		return MEM_MODE_INPUT_FORMAT_YUYV;
	case DRM_FORMAT_YUV420:
		return MEM_MODE_INPUT_FORMAT_IYUV;
	case DRM_FORMAT_YVU420:
		return MEM_MODE_INPUT_FORMAT_IYUV | MEM_MODE_INPUT_SWAP;
	}
}

static int wdma_config_yuv420(struct mtk_ddp_comp *comp,
			      uint32_t fmt, unsigned int dstPitch,
			      unsigned int Height, unsigned long dstAddress,
			      uint32_t sec, void *handle, int sec_id)
{
	/* size_t size; */
	unsigned int u_off = 0;
	unsigned int v_off = 0;
	unsigned int u_stride = 0;
	unsigned int y_size = 0;
	unsigned int u_size = 0;
	/* unsigned int v_size = 0; */
	unsigned int stride = dstPitch;
	int has_v = 1;

	if (fmt != DRM_FORMAT_YUV420 && fmt != DRM_FORMAT_YVU420 &&
		fmt != DRM_FORMAT_NV12 && fmt != DRM_FORMAT_NV21)
		return 0;

	if (fmt == DRM_FORMAT_YUV420 || fmt == DRM_FORMAT_YVU420) {
		y_size = stride * Height;
		u_stride = ALIGN_TO(stride / 2, 16);
		u_size = u_stride * Height / 2;
		u_off = y_size;
		v_off = y_size + u_size;
	} else if (fmt == DRM_FORMAT_NV12 || fmt == DRM_FORMAT_NV21) {
		y_size = stride * Height;
		u_stride = stride / 2;
		u_size = u_stride * Height / 2;
		u_off = y_size;
		has_v = 0;
	}

	if (!sec) {
		mtk_ddp_write(comp, dstAddress + u_off,
			DISP_REG_WDMA_DST_ADDR1, handle);

		if (has_v)
			mtk_ddp_write(comp, dstAddress + v_off,
				DISP_REG_WDMA_DST_ADDR2, handle);
	} else {
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		cmdq_sec_pkt_write_reg(handle,
			comp->regs_pa + DISP_REG_WDMA_DST_ADDR1,
			dstAddress, CMDQ_IWC_H_2_MVA,
			u_off, u_size, 0, sec_id);
		if (has_v)
			cmdq_sec_pkt_write_reg(handle,
				comp->regs_pa + DISP_REG_WDMA_DST_ADDR2,
				dstAddress, CMDQ_IWC_H_2_MVA,
				v_off, u_size, 0, sec_id);
#endif
	}
	mtk_ddp_write_mask(comp, u_stride,
			DISP_REG_WDMA_DST_UV_PITCH, 0xFFFF, handle);
	return 0;
}

static bool is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return true;
	default:
		break;
	}

	return false;
}

static void mtk_wdma_config(struct mtk_ddp_comp *comp,
			    struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int size = 0;
	unsigned int con = 0;
	unsigned int addr = 0;
	struct mtk_disp_wdma *wdma = comp_to_wdma(comp);
	struct mtk_wdma_cfg_info *cfg_info = &wdma->cfg_info;
	int crtc_idx = drm_crtc_index(&comp->mtk_crtc->base);
	int clip_w, clip_h;
	struct golden_setting_context *gsc;
	u32 sec, buffer_size;
	int sec_id;

	if (!comp->fb) {
		if (crtc_idx != 2)
			DDPPR_ERR("%s fb is empty, CRTC%d\n",
				__func__, crtc_idx);
		return;
	}

	addr = (u32)mtk_fb_get_dma(comp->fb);
	if (!addr) {
		DDPPR_ERR("%s:%d C%d no dma_buf\n",
				__func__, __LINE__,
				crtc_idx);
		return;
	}
	sec = mtk_drm_fb_is_secure(comp->fb);
	sec_id = mtk_fb_get_sec_id(comp->fb);

	addr += comp->fb->offsets[0];
	if (!(comp->fb->format &&
		comp->fb->format->format)) {
		DDPPR_ERR("%s fb format is NULL, CRTC%d\n",
			__func__, crtc_idx);
		return;
	}
	con = wdma_fmt_convert(comp->fb->format->format);
	DDPINFO("%s fmt:0x%x, con:0x%x addr %x\n", __func__,
		comp->fb->format->format, con, addr);
	if (!addr) {
		DDPPR_ERR("%s wdma dst addr is zero\n", __func__);
		return;
	}

	clip_w = cfg->w;
	clip_h = cfg->h;
	if (is_yuv(comp->fb->format->format)) {
		if ((cfg->x + cfg->w) % 2)
			clip_w -= 1;

		if ((cfg->y + cfg->h) % 2)
			clip_h -= 1;
	}

	size = (cfg->w & 0x3FFFU) + ((cfg->h << 16U) & 0x3FFF0000U);
	mtk_ddp_write(comp, size, DISP_REG_WDMA_SRC_SIZE, handle);
	mtk_ddp_write(comp, (cfg->y << 16) | cfg->x,
		DISP_REG_WDMA_CLIP_COORD, handle);
	mtk_ddp_write(comp, (clip_h << 16) | clip_w,
		DISP_REG_WDMA_CLIP_SIZE, handle);
	mtk_ddp_write_mask(comp, con, DISP_REG_WDMA_CFG,
		WDMA_OUT_FMT | WDMA_CON_SWAP, handle);

	if (is_yuv(comp->fb->format->format)) {
		wdma_config_yuv420(comp, comp->fb->format->format,
				comp->fb->pitches[0], cfg->h,
				addr, sec, handle, sec_id);
		mtk_ddp_write_mask(comp, 0,
				DISP_REG_WDMA_CFG, WDMA_UFO_DCP_ENABLE, handle);
		mtk_ddp_write_mask(comp, WDMA_CT_EN,
				DISP_REG_WDMA_CFG, WDMA_CT_EN, handle);
		mtk_ddp_write_mask(comp, 0x02000000,
				DISP_REG_WDMA_CFG, WDMA_INT_MTX_SEL, handle);
	} else {
		mtk_ddp_write_mask(comp, 0,
				DISP_REG_WDMA_CFG, WDMA_UFO_DCP_ENABLE, handle);
		mtk_ddp_write_mask(comp, 0,
				DISP_REG_WDMA_CFG, WDMA_CT_EN, handle);
	}

	mtk_ddp_write(comp, comp->fb->pitches[0],
		DISP_REG_WDMA_DST_WIN_BYTE, handle);
	if (!sec) {
		mtk_ddp_write(comp, addr & 0xFFFFFFFFU,
				DISP_REG_WDMA_DST_ADDR0, handle);
	} else {
		buffer_size = clip_w * comp->fb->pitches[0];
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		cmdq_sec_pkt_write_reg(handle,
				comp->regs_pa + DISP_REG_WDMA_DST_ADDR0,
				addr & 0xFFFFFFFFU, CMDQ_IWC_H_2_MVA,
				0, buffer_size, 0, sec_id);
#endif
	}

	gsc = cfg->p_golden_setting_context;
	mtk_wdma_golden_setting(comp, gsc, handle);

	cfg_info->addr = addr;
	cfg_info->width = cfg->w;
	cfg_info->height = cfg->h;
	cfg_info->fmt = comp->fb->format->format;
}

static void mtk_wdma_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	unsigned int size = 0;
	unsigned int con = 0;
	unsigned int addr = 0;
	struct mtk_disp_wdma *wdma = comp_to_wdma(comp);
	struct mtk_wdma_cfg_info *cfg_info = &wdma->cfg_info;
	int crtc_idx = drm_crtc_index(&comp->mtk_crtc->base);
	int src_w, src_h, clip_w, clip_h, clip_x, clip_y;
	struct golden_setting_context *gsc;

	comp->fb = addon_config->addon_wdma_config.fb;
	if (!comp->fb) {
		DDPPR_ERR("%s fb is empty, CRTC%d\n", __func__, crtc_idx);
		return;
	}

	con = wdma_fmt_convert(comp->fb->format->format);

	addr = addon_config->addon_wdma_config.addr;
	if (!addr) {
		DDPPR_ERR("%s:%d C%d no dma_buf\n", __func__,
				__LINE__, crtc_idx);
		return;
	}
	cfg_info->addr = addr;
	mtk_ddp_write(comp, addr & 0xFFFFFFFFU,
		DISP_REG_WDMA_DST_ADDR0, handle);

	src_w = addon_config->addon_wdma_config.wdma_src_roi.width;
	src_h = addon_config->addon_wdma_config.wdma_src_roi.height;

	clip_w = addon_config->addon_wdma_config.wdma_dst_roi.width;
	clip_h = addon_config->addon_wdma_config.wdma_dst_roi.height;
	clip_x = addon_config->addon_wdma_config.wdma_dst_roi.x;
	clip_y = addon_config->addon_wdma_config.wdma_dst_roi.y;

	size = (src_w & 0x3FFFU) + ((src_h << 16U) & 0x3FFF0000U);
	mtk_ddp_write(comp, size, DISP_REG_WDMA_SRC_SIZE, handle);
	mtk_ddp_write(comp, (clip_y << 16) | clip_x,
		DISP_REG_WDMA_CLIP_COORD, handle);
	mtk_ddp_write(comp, (clip_h << 16) | clip_w,
		DISP_REG_WDMA_CLIP_SIZE, handle);
	mtk_ddp_write_mask(comp, con, DISP_REG_WDMA_CFG,
		WDMA_OUT_FMT | WDMA_CON_SWAP, handle);

	mtk_ddp_write_mask(comp, 0,
			DISP_REG_WDMA_CFG, WDMA_UFO_DCP_ENABLE, handle);
	mtk_ddp_write_mask(comp, 0,
			DISP_REG_WDMA_CFG, WDMA_CT_EN, handle);

	mtk_ddp_write(comp, clip_w * 3,
		DISP_REG_WDMA_DST_WIN_BYTE, handle);

	gsc = addon_config->addon_wdma_config.p_golden_setting_context;
	mtk_wdma_golden_setting(comp, gsc, handle);

	DDPMSG("[capture] config addr:0x%x, roi:(%d,%d,%d,%d)\n",
		addr, clip_x, clip_y, clip_w, clip_h);
	cfg_info->width = clip_w;
	cfg_info->height = clip_h;
	cfg_info->fmt = comp->fb->format->format;
}

void mtk_wdma_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int value;
	int i;

	DDPDUMP("-- %s Golden Setting --\n", mtk_dump_comp_str(comp));

	if (comp->mtk_crtc && comp->mtk_crtc->sec_on) {
		DDPDUMP("Skip dump secure wdma!\n");
		return;
	}

	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
		0x10, readl(DISP_REG_WDMA_SMI_CON + baddr),
		0x38, readl(DISP_REG_WDMA_BUF_CON1 + baddr));
	for (i = 0; i < 3; i++)
		DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
			0x200 + i * 0x10,
			readl(DISP_REG_WDMA_BUF_CON5 +
				     baddr + i * 0x10),
			readl(DISP_REG_WDMA_BUF_CON6 +
				     baddr + i * 0x10),
			readl(DISP_REG_WDMA_BUF_CON7 +
				     baddr + i * 0x10),
			readl(DISP_REG_WDMA_BUF_CON8 +
				     baddr + i * 0x10));
	DDPDUMP("0x%03x:0x%08x 0x%08x\n",
		0x230, readl(DISP_REG_WDMA_BUF_CON17 + baddr),
		readl(DISP_REG_WDMA_BUF_CON18 + baddr));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0x250, readl(DISP_REG_WDMA_DRS_CON0 + baddr),
		readl(DISP_REG_WDMA_DRS_CON1 + baddr),
		readl(DISP_REG_WDMA_DRS_CON2 + baddr),
		readl(DISP_REG_WDMA_DRS_CON3 + baddr));
	DDPDUMP("0x%03x:0x%08x 0x%08x\n",
		0x104, readl(DISP_REG_WDMA_BUF_CON3 + baddr),
		readl(DISP_REG_WDMA_BUF_CON4 + baddr));

	value = readl(DISP_REG_WDMA_SMI_CON + baddr);
	DDPDUMP("WDMA_SMI_CON:[3:0]:%x [4:4]:%x [7:5]:%x [15:8]:%x\n",
		REG_FLD_VAL_GET(SMI_CON_FLD_THRESHOLD, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SLOW_ENABLE, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SLOW_LEVEL, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SLOW_COUNT, value));
	DDPDUMP("WDMA_SMI_CON:[19:16]:%u [23:20]:%u [27:24]:%u [28]:%u\n",
		REG_FLD_VAL_GET(SMI_CON_FLD_SMI_Y_REPEAT_NUM, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SMI_U_REPEAT_NUM, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SMI_V_REPEAT_NUM, value),
		REG_FLD_VAL_GET(SMI_CON_FLD_SMI_OBUF_FULL_REQ, value));

	value = readl(DISP_REG_WDMA_BUF_CON1 + baddr);
	DDPDUMP("WDMA_BUF_CON1:[31]:%x [30]:%x [28]:%x [26]%d\n",
		REG_FLD_VAL_GET(BUF_CON1_FLD_ULTRA_ENABLE, value),
		REG_FLD_VAL_GET(BUF_CON1_FLD_PRE_ULTRA_ENABLE, value),
		REG_FLD_VAL_GET(BUF_CON1_FLD_FRAME_END_ULTRA, value),
		REG_FLD_VAL_GET(BUF_CON1_FLD_URGENT_EN, value));
	DDPDUMP("WDMA_BUF_CON1:[18:10]:%d [9:0]:%d\n",
		REG_FLD_VAL_GET(BUF_CON1_FLD_FIFO_PSEUDO_SIZE_UV, value),
		REG_FLD_VAL_GET(BUF_CON1_FLD_FIFO_PSEUDO_SIZE, value));

	value = readl(DISP_REG_WDMA_BUF_CON5 + baddr);
	DDPDUMP("WDMA_BUF_CON5:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON6 + baddr);
	DDPDUMP("WDMA_BUF_CON6:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON7 + baddr);
	DDPDUMP("WDMA_BUF_CON7:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON8 + baddr);
	DDPDUMP("WDMA_BUF_CON8:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON9 + baddr);
	DDPDUMP("WDMA_BUF_CON9:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON10 + baddr);
	DDPDUMP("WDMA_BUF_CON10:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON11 + baddr);
	DDPDUMP("WDMA_BUF_CON11:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON12 + baddr);
	DDPDUMP("WDMA_BUF_CON12:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON13 + baddr);
	DDPDUMP("WDMA_BUF_CON13:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON14 + baddr);
	DDPDUMP("WDMA_BUF_CON14:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON15 + baddr);
	DDPDUMP("WDMA_BUF_CON15:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_LOW, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_LOW, value));

	value = readl(DISP_REG_WDMA_BUF_CON16 + baddr);
	DDPDUMP("WDMA_BUF_CON16:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON_FLD_PRE_ULTRA_HIGH, value),
		REG_FLD_VAL_GET(BUF_CON_FLD_ULTRA_HIGH, value));

	value = readl(DISP_REG_WDMA_BUF_CON17 + baddr);
	DDPDUMP("WDMA_BUF_CON17:[0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON17_FLD_WDMA_DVFS_EN, value),
		REG_FLD_VAL_GET(BUF_CON17_FLD_DVFS_TH_Y, value));

	value = readl(DISP_REG_WDMA_BUF_CON18 + baddr);
	DDPDUMP("WDMA_BUF_CON18:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON18_FLD_DVFS_TH_U, value),
		REG_FLD_VAL_GET(BUF_CON18_FLD_DVFS_TH_V, value));

	value = readl(DISP_REG_WDMA_URGENT_CON0 + baddr);
	DDPDUMP("WDMA_URGENT_CON0:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_LOW_Y, value),
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_HIGH_Y, value));

	value = readl(DISP_REG_WDMA_URGENT_CON1 + baddr);
	DDPDUMP("WDMA_URGENT_CON1:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_LOW_U, value),
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_HIGH_U, value));

	value = readl(DISP_REG_WDMA_URGENT_CON2 + baddr);
	DDPDUMP("WDMA_URGENT_CON2:[9:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_LOW_V, value),
		REG_FLD_VAL_GET(FLD_WDMA_URGENT_HIGH_V, value));

	value = readl(DISP_REG_WDMA_BUF_CON3 + baddr);
	DDPDUMP("WDMA_BUF_CON3:[8:0]:%d [25:16]:%d\n",
		REG_FLD_VAL_GET(BUF_CON3_FLD_ISSUE_REQ_TH_Y, value),
		REG_FLD_VAL_GET(BUF_CON3_FLD_ISSUE_REQ_TH_U, value));

	value = readl(DISP_REG_WDMA_BUF_CON4 + baddr);
	DDPDUMP("WDMA_BUF_CON4:[8:0]:%d\n",
		REG_FLD_VAL_GET(BUF_CON4_FLD_ISSUE_REQ_TH_V, value));
}

int mtk_wdma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));

	if (comp->mtk_crtc && comp->mtk_crtc->sec_on) {
		DDPDUMP("Skip dump secure wdma!\n");
		return 0;
	}

	if (mtk_ddp_comp_helper_get_opt(comp,
					MTK_DRM_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int i = 0;

		for (i = 0; i < 0x300; i += 0x10)
			mtk_serial_dump_reg(baddr, i, 4);
	} else {
		DDPDUMP("0x000:0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(DISP_REG_WDMA_INTEN + baddr),
			readl(DISP_REG_WDMA_INTSTA + baddr),
			readl(DISP_REG_WDMA_EN + baddr),
			readl(DISP_REG_WDMA_RST + baddr));

		DDPDUMP("0x010:0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(DISP_REG_WDMA_SMI_CON + baddr),
			readl(DISP_REG_WDMA_CFG + baddr),
			readl(DISP_REG_WDMA_SRC_SIZE + baddr),
			readl(DISP_REG_WDMA_CLIP_SIZE + baddr));

		DDPDUMP("0x020:0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(DISP_REG_WDMA_CLIP_COORD + baddr),
			readl(DISP_REG_WDMA_SHADOW_CTRL + baddr),
			readl(DISP_REG_WDMA_DST_WIN_BYTE + baddr),
			readl(DISP_REG_WDMA_ALPHA + baddr));

		DDPDUMP("0x038:0x%08x 0x078:0x%08x\n",
			readl(DISP_REG_WDMA_BUF_CON1 + baddr),
			readl(DISP_REG_WDMA_DST_UV_PITCH + baddr));

		DDPDUMP("0x080:0x%08x 0x%08x 0x%08x\n",
			readl(DISP_REG_WDMA_DST_ADDR_OFFSET0 + baddr),
			readl(DISP_REG_WDMA_DST_ADDR_OFFSET1 + baddr),
			readl(DISP_REG_WDMA_DST_ADDR_OFFSET2 + baddr));

		DDPDUMP("0x0a0:0x%08x 0x%08x 0x%08x 0x0b8:0x%08x\n",
			readl(DISP_REG_WDMA_FLOW_CTRL_DBG + baddr),
			readl(DISP_REG_WDMA_EXEC_DBG + baddr),
			readl(DISP_REG_WDMA_INPUT_CNT_DBG + baddr),
			readl(DISP_REG_WDMA_DEBUG + baddr));

		DDPDUMP("0xf00:0x%08x 0x%08x 0x%08x\n",
			readl(DISP_REG_WDMA_DST_ADDR0 + baddr),
			readl(DISP_REG_WDMA_DST_ADDR1 + baddr),
			readl(DISP_REG_WDMA_DST_ADDR2 + baddr));
	}

	mtk_wdma_dump_golden_setting(comp);

	return 0;
}

static char *wdma_get_state(unsigned int status)
{
	switch (status) {
	case 0x1:
		return "idle";
	case 0x2:
		return "clear";
	case 0x4:
		return "prepare1";
	case 0x8:
		return "prepare2";
	case 0x10:
		return "data_transmit";
	case 0x20:
		return "eof_wait";
	case 0x40:
		return "soft_reset_wait";
	case 0x80:
		return "eof_done";
	case 0x100:
		return "soft_reset_done";
	case 0x200:
		return "frame_complete";
	}
	return "unknown-state";
}

int mtk_wdma_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== DISP %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	if (comp->mtk_crtc && comp->mtk_crtc->sec_on) {
		DDPDUMP("Skip dump secure wdma!\n");
		return 0;
	}

	DDPDUMP("en=%d,src(%dx%d),clip=(%d,%d,%dx%d)\n",
		readl(baddr + DISP_REG_WDMA_EN) & 0x01,
		readl(baddr + DISP_REG_WDMA_SRC_SIZE) & 0x3fff,
		(readl(baddr + DISP_REG_WDMA_SRC_SIZE) >> 16) & 0x3fff,
		readl(baddr + DISP_REG_WDMA_CLIP_COORD) & 0x3fff,
		(readl(baddr + DISP_REG_WDMA_CLIP_COORD) >> 16) & 0x3fff,
		readl(baddr + DISP_REG_WDMA_CLIP_SIZE) & 0x3fff,
		(readl(baddr + DISP_REG_WDMA_CLIP_SIZE) >> 16) & 0x3fff);
	DDPDUMP("pitch=(W=%d,UV=%d),addr=(0x%08x,0x%08x,0x%08x),cfg=0x%x\n",
		readl(baddr + DISP_REG_WDMA_DST_WIN_BYTE),
		readl(baddr + DISP_REG_WDMA_DST_UV_PITCH),
		readl(baddr + DISP_REG_WDMA_DST_ADDR0),
		readl(baddr + DISP_REG_WDMA_DST_ADDR1),
		readl(baddr + DISP_REG_WDMA_DST_ADDR2),
		readl(baddr + DISP_REG_WDMA_CFG));
	DDPDUMP("state=%s,in_req=%d(prev sent data)\n",
		wdma_get_state(DISP_REG_GET_FIELD(
			FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL,
			baddr + DISP_REG_WDMA_FLOW_CTRL_DBG)),
		REG_FLD_VAL_GET(FLOW_CTRL_DBG_FLD_WDMA_IN_VALID,
				readl(baddr + DISP_REG_WDMA_FLOW_CTRL_DBG)));
	DDPDUMP("in_ack=%d(ask data to prev),start=%d,end=%d,pos:in(%d,%d)\n",
		REG_FLD_VAL_GET(FLOW_CTRL_DBG_FLD_WDMA_IN_READY,
				readl(baddr + DISP_REG_WDMA_FLOW_CTRL_DBG)),
		readl(baddr + DISP_REG_WDMA_EXEC_DBG) & 0x3f,
		readl(baddr + DISP_REG_WDMA_EXEC_DBG) >> 16 & 0x3f,
		readl(baddr + DISP_REG_WDMA_INPUT_CNT_DBG) & 0x3fff,
		(readl(baddr + DISP_REG_WDMA_INPUT_CNT_DBG) >> 16) & 0x3fff);

	return 0;
}

int MMPathTraceWDMA(struct mtk_ddp_comp *ddp_comp, char *str,
	unsigned int strlen, unsigned int n)
{
	struct mtk_disp_wdma *wdma = comp_to_wdma(ddp_comp);
	struct mtk_wdma_cfg_info *cfg_info = &wdma->cfg_info;

	n += scnprintf(str + n, strlen - n,
		"out=0x%x, out_width=%d, out_height=%d, out_fmt=%s, out_bpp=%d",
		cfg_info->addr,
		cfg_info->width,
		cfg_info->height,
		mtk_get_format_name(cfg_info->fmt),
		mtk_get_format_bpp(cfg_info->fmt));

	return n;
}

static int mtk_wdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_disp_wdma *wdma = container_of(comp, struct mtk_disp_wdma, ddp_comp);

	switch (cmd) {
	case WDMA_WRITE_DST_ADDR0:
	{
		unsigned int addr = *(unsigned int *)params;

		mtk_ddp_write(comp, addr & 0xFFFFFFFFU,
			DISP_REG_WDMA_DST_ADDR0, handle);
		DDPMSG("[capture] update addr:0x%x\n", addr);
		wdma->cfg_info.addr = addr;
	}
		break;
	case WDMA_READ_DST_SIZE:
	{
		unsigned int val, w, h;
		struct mtk_cwb_info *cwb_info = (struct mtk_cwb_info *)params;

		val = readl(comp->regs + DISP_REG_WDMA_CLIP_SIZE);
		w = val & 0x3fff;
		h = (val >> 16) & 0x3fff;
		cwb_info->copy_w = w;
		cwb_info->copy_h = h;
		DDPDBG("[capture] sof get (w,h)=(%d,%d)\n", w, h);
	}
		break;
	case IRQ_LEVEL_IDLE: {
		mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_INTEN, handle);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_FME_UND_INTEN, 1);

		mtk_ddp_write(comp, inten, DISP_REG_WDMA_INTEN, handle);
		break;
	}
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_wdma_funcs = {
	.config = mtk_wdma_config,
	.addon_config = mtk_wdma_addon_config,
	.start = mtk_wdma_start,
	.stop = mtk_wdma_stop,
	.prepare = mtk_wdma_prepare,
	.unprepare = mtk_wdma_unprepare,
	.is_busy = mtk_wdma_is_busy,
	.io_cmd = mtk_wdma_io_cmd,
};

static int mtk_disp_wdma_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct mtk_disp_wdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_wdma_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_wdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_wdma_component_ops = {
	.bind = mtk_disp_wdma_bind, .unbind = mtk_disp_wdma_unbind,
};

static int mtk_disp_wdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_wdma *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_WDMA);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_wdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_wdma_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_wdma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	DDPMSG("%s-\n", __func__);

	return ret;
}

static int mtk_disp_wdma_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_wdma_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_wdma_data mt6779_wdma_driver_data = {
	.sodi_config = mt6779_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6885_wdma_driver_data = {
	.sodi_config = mt6885_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6873_wdma_driver_data = {
	.sodi_config = mt6873_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6853_wdma_driver_data = {
	.sodi_config = mt6853_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6877_wdma_driver_data = {
	.sodi_config = mt6877_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6833_wdma_driver_data = {
	.sodi_config = mt6833_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_wdma_data mt6781_wdma_driver_data = {
	.sodi_config = mt6781_mtk_sodi_config,
	.support_shadow = false,
};

static const struct of_device_id mtk_disp_wdma_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-wdma"},
	{.compatible = "mediatek,mt6779-disp-wdma",
	 .data = &mt6779_wdma_driver_data},
	{.compatible = "mediatek,mt8173-disp-wdma"},
	{.compatible = "mediatek,mt6885-disp-wdma",
	 .data = &mt6885_wdma_driver_data},
	{.compatible = "mediatek,mt6873-disp-wdma",
	 .data = &mt6873_wdma_driver_data},
	{.compatible = "mediatek,mt6853-disp-wdma",
	 .data = &mt6853_wdma_driver_data},
	{.compatible = "mediatek,mt6877-disp-wdma",
	 .data = &mt6877_wdma_driver_data},
	{.compatible = "mediatek,mt6833-disp-wdma",
	 .data = &mt6833_wdma_driver_data},
	{.compatible = "mediatek,mt6781-disp-wdma",
	 .data = &mt6781_wdma_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_wdma_driver_dt_match);

struct platform_driver mtk_disp_wdma_driver = {
	.probe = mtk_disp_wdma_probe,
	.remove = mtk_disp_wdma_remove,
	.driver = {

			.name = "mediatek-disp-wdma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_wdma_driver_dt_match,
		},
};
