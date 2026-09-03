// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/apsys_dvfs.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/trusty/smcall.h>
#include <drm/drm_prime.h>

#include "dpu_enhance_param.h"
#include "dpu_r5p0_corner_param.h"
#include "disp_trusty.h"
#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "sprd_dpu.h"
#include "sprd_bl.h"
//#include <../drivers/trusty/trusty.h>

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

#define SLP_BRIGHTNESS_THRESHOLD 0x20

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE	0x04
/* Layer registers offset */
#define DPU_LAY_REG_OFFSET	0x0C

#define DPU_LAY_COUNT	8

#define DPU_MAX_REG_OFFSET	0x8AC

#define DPU_REG_RD(reg) readl_relaxed(reg)

#define DPU_REG_WR(reg, mask) writel_relaxed(mask, reg)

#define DPU_REG_SET(reg, mask) \
	writel_relaxed(readl_relaxed(reg) | mask, reg)

#define DPU_REG_CLR(reg, mask) \
	writel_relaxed(readl_relaxed(reg) & ~mask, reg)

#define DPU_LAY_REG(reg, index) \
	(reg + index * DPU_LAY_REG_OFFSET * DPU_REG_SIZE)

#define DPU_LAY_PLANE_ADDR(reg, index, plane) \
	(reg + index * DPU_LAY_REG_OFFSET * DPU_REG_SIZE + plane * DPU_REG_SIZE)

/* Global control registers */
#define REG_DPU_CTRL	0x04
#define REG_DPU_CFG0	0x08
#define REG_DPU_CFG1	0x0C
#define REG_DPU_CFG2	0x10
#define REG_DPU_SECURE	0x14
#define REG_PANEL_SIZE	0x20
#define REG_BLEND_SIZE	0x24
#define REG_BG_COLOR	0x2C

/* Layer0 control registers */
#define REG_LAY_BASE_ADDR	0x30
#define REG_LAY_CTRL		0x40
#define REG_LAY_SIZE		0x44
#define REG_LAY_PITCH		0x48
#define REG_LAY_POS		0x4C
#define REG_LAY_ALPHA		0x50
#define REG_LAY_PALLETE		0x58
#define REG_LAY_CROP_START	0x5C

/* Write back config registers */
#define REG_WB_BASE_ADDR	0x1B0
#define REG_WB_CTRL		0x1B4
#define REG_WB_CFG		0x1B8
#define REG_WB_PITCH		0x1BC

/* Interrupt control registers */
#define REG_DPU_INT_EN		0x1E0
#define REG_DPU_INT_CLR		0x1E4
#define REG_DPU_INT_STS		0x1E8
#define REG_DPU_INT_RAW		0x1EC

/* DPI control registers */
#define REG_DPI_CTRL		0x1F0
#define REG_DPI_H_TIMING	0x1F4
#define REG_DPI_V_TIMING	0x1F8

/* PQ Enhance config registers */
#define REG_DPU_ENHANCE_CFG	0x200
#define REG_EPF_EPSILON		0x210
#define REG_EPF_GAIN0_3		0x214
#define REG_EPF_GAIN4_7		0x218
#define REG_EPF_DIFF		0x21C
#define REG_HSV_LUT_ADDR	0x240
#define REG_HSV_LUT_WDATA	0x244
#define REG_HSV_LUT_RDATA	0x248
#define REG_CM_COEF01_00	0x280
#define REG_CM_COEF03_02	0x284
#define REG_CM_COEF11_10	0x288
#define REG_CM_COEF13_12	0x28C
#define REG_CM_COEF21_20	0x290
#define REG_CM_COEF23_22	0x294
#define REG_SLP_CFG0		0x2C0
#define REG_SLP_CFG1		0x2C4
#define REG_SLP_CFG2		0x2C8
#define REG_SLP_CFG3		0x2CC
#define REG_SLP_LUT_ADDR	0x2D0
#define REG_SLP_LUT_WDATA	0x2D4
#define REG_SLP_LUT_RDATA	0x2D8
#define REG_TREED_LUT_ADDR	0x2DC
#define REG_TREED_LUT_WDATA	0x2E0
#define REG_TREED_LUT_RDATA	0x2E4
#define REG_GAMMA_LUT_ADDR	0x300
#define REG_GAMMA_LUT_WDATA	0x304
#define REG_GAMMA_LUT_RDATA	0x308
#define REG_DPU_STS0		0x360
#define REG_SLP_CFG4		0x3C0
#define REG_SLP_CFG5		0x3C4
#define REG_SLP_CFG6		0x3C8
#define REG_SLP_CFG7		0x3CC
#define REG_SLP_CFG8		0x3D0
#define REG_SLP_CFG9		0x3D4
#define REG_SLP_CFG10		0x3D8
#define REG_CABC_HIST0		0X400

/* Corner config registers */
#define REG_CORNER_CONFIG		0x500
#define REG_TOP_CORNER_LUT_ADDR		0x504
#define REG_TOP_CORNER_LUT_WDATA	0x508
#define REG_BOT_CORNER_LUT_ADDR		0x510
#define REG_BOT_CORNER_LUT_WDATA	0x514

/* mmu config registers */
#define REG_DPU_MMU_EN			0x804
#define REG_DPU_MMU_VAOR_ADDR_RD	0x854
#define REG_DPU_MMU_VAOR_ADDR_WR	0x858
#define REG_DPU_MMU_INV_ADDR_RD		0x860
#define REG_DPU_MMU_INV_ADDR_WR		0x864
#define REG_DPU_MMU_INT_EN		0x8A0
#define REG_DPU_MMU_INT_CLR		0x8A4
#define REG_DPU_MMU_INT_STS		0x8A8
#define REG_DPU_MMU_INT_RAW		0x8AC

/* Global control bits */
#define BIT_DPU_RUN			BIT(0)
#define BIT_DPU_STOP			BIT(1)
#define BIT_DPU_REG_UPDATE		BIT(2)
#define BIT_DPU_IF_EDPI			BIT(0)

/* Layer control bits */
#define BIT_DPU_LAY_EN				BIT(0)
#define BIT_DPU_LAY_LAYER_ALPHA			(0x01 << 2)
#define BIT_DPU_LAY_COMBO_ALPHA			(0x02 << 2)
#define BIT_DPU_LAY_FORMAT_YUV422_2PLANE		(0x00 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_2PLANE		(0x01 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_3PLANE		(0x02 << 4)
#define BIT_DPU_LAY_FORMAT_ARGB8888			(0x03 << 4)
#define BIT_DPU_LAY_FORMAT_RGB565			(0x04 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_ARGB8888		(0x08 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_RGB565			(0x09 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_YUV420			(0x0A << 4)
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3		(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0		(0x01 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B2B3B0B1		(0x02 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2		(0x03 << 8)
#define BIT_DPU_LAY_NO_SWITCH			(0x00 << 10)
#define BIT_DPU_LAY_RGB888_RB_SWITCH		(0x01 << 10)
#define BIT_DPU_LAY_RGB565_RB_SWITCH		(0x01 << 12)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 13)
#define BIT_DPU_LAY_MODE_BLEND_NORMAL		(0x00 << 16)
#define BIT_DPU_LAY_MODE_BLEND_PREMULT		(0x01 << 16)

/* Interrupt control & status bits */
#define BIT_DPU_INT_DONE		BIT(0)
#define BIT_DPU_INT_TE			BIT(1)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_ERR		BIT(7)
#define BIT_DPU_INT_FBC_PLD_ERR		BIT(8)
#define BIT_DPU_INT_FBC_HDR_ERR		BIT(9)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN		BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD	BIT(10)
#define BIT_DPU_DPI_HALT_EN		BIT(16)

/* Corner config bits */
#define BIT_TOP_CORNER_EN		BIT(0)
#define BIT_BOT_CORNER_EN		BIT(16)

/* enhance config bits */
#define BIT_DPU_SCALING_EN		BIT(0)

/* mmu interrupt bits */
#define BIT_DPU_INT_MMU_PAOR_WR_MASK	BIT(7)
#define BIT_DPU_INT_MMU_PAOR_RD_MASK	BIT(6)
#define BIT_DPU_INT_MMU_UNS_WR_MASK	BIT(5)
#define BIT_DPU_INT_MMU_UNS_RD_MASK	BIT(4)
#define BIT_DPU_INT_MMU_INV_WR_MASK	BIT(3)
#define BIT_DPU_INT_MMU_INV_RD_MASK	BIT(2)
#define BIT_DPU_INT_MMU_VAOR_WR_MASK	BIT(1)
#define BIT_DPU_INT_MMU_VAOR_RD_MASK	BIT(0)

#define CABC_MODE_UI			(1 << 2)
#define CABC_MODE_GAME			(1 << 3)
#define CABC_MODE_VIDEO			(1 << 4)
#define CABC_MODE_IMAGE			(1 << 5)
#define CABC_MODE_CAMERA		(1 << 6)
#define CABC_MODE_FULL_FRAME		(1 << 7)
#define CABC_BRIGHTNESS_STEP		8
#define CABC_BRIGHTNESS			32
#define CABC_FST_MAX_BRIGHT_TH		64
#define CABC_FST_MAX_BRIGHT_TH_STEP0	0
#define CABC_FST_MAX_BRIGHT_TH_STEP1	0
#define CABC_FST_MAX_BRIGHT_TH_STEP2	0
#define CABC_FST_MAX_BRIGHT_TH_STEP3	0
#define CABC_FST_MAX_BRIGHT_TH_STEP4	0
#define CABC_HIST_EXB_NO		3
#define CABC_HIST_EXB_PERCENT		0
#define CABC_MASK_HEIGHT		0
#define CABC_FST_PTH_INDEX0		0
#define CABC_FST_PTH_INDEX1		0
#define CABC_FST_PTH_INDEX2		0
#define CABC_FST_PTH_INDEX3		0
#define CABC_HIST9_INDEX0		0
#define CABC_HIST9_INDEX1		32
#define CABC_HIST9_INDEX2		64
#define CABC_HIST9_INDEX3		128
#define CABC_HIST9_INDEX4		160
#define CABC_HIST9_INDEX5		180
#define CABC_HIST9_INDEX6		200
#define CABC_HIST9_INDEX7		235
#define CABC_HIST9_INDEX8		252
#define CABC_GLB_X0			0
#define CABC_GLB_X1			0
#define CABC_GLB_X2			220
#define CABC_GLB_S0			0
#define CABC_GLB_S1			0
#define CABC_GLB_S2			0
#define CABC_FAST_AMBIENT_TH		100
#define CABC_SCENE_CHANGE_PERCENT_TH	200
#define CABC_LOCAL_WEIGHT		0
#define CABC_FST_PTH			0
#define CABC_BL_COEF			1020

enum {
	CABC_DISABLED,
	CABC_STOPPING,
	CABC_WORKING
};

enum sprd_fw_attr {
	FW_ATTR_NON_SECURE = 0,
	FW_ATTR_SECURE,
	FW_ATTR_PROTECTED,
};

struct hsv_entry {
	u16 hue;
	u16 sat;
};

struct hsv_lut {
	struct hsv_entry table[360];
};

struct cm_cfg {
	u16 coef00;
	u16 coef01;
	u16 coef02;
	u16 coef03;
	u16 coef10;
	u16 coef11;
	u16 coef12;
	u16 coef13;
	u16 coef20;
	u16 coef21;
	u16 coef22;
	u16 coef23;
};

struct ltm_cfg {
	u16 limit_hclip;
	u16 limit_lclip;
	u16 limit_clip_step;
};

struct cabc_para {
	u32 cabc_hist[32];
	u16 gain0;
	u16 gain1;
	u32 gain2;
	u8 p0;
	u8 p1;
	u8 p2;
	u16 bl_fix;
	u16 cur_bl;
	u8 video_mode;
	u8 slp_brightness;
	u8 slp_local_weight;
	u8 dci_en;
	u8 slp_en;
};

struct slp_cfg {
	u8 brightness;
	u16 brightness_step;
	u8 fst_max_bright_th;
	u8 fst_max_bright_th_step[5];
	u8 hist_exb_no;
	u8 hist_exb_percent;
	u16 mask_height;
	u8 fst_pth_index[4];
	u8 hist9_index[9];
	u8 glb_x[3];
	u16 glb_s[3];
	u8 fast_ambient_th;
	u8 scene_change_percent_th;
	u8 local_weight;
	u8 fst_pth;
	u8 cabc_endv;
	u8 cabc_startv;
};

struct gamma_lut {
	u16 r[256];
	u16 g[256];
	u16 b[256];
};

struct epf_cfg {
	u16 epsilon0;
	u16 epsilon1;
	u8 gain0;
	u8 gain1;
	u8 gain2;
	u8 gain3;
	u8 gain4;
	u8 gain5;
	u8 gain6;
	u8 gain7;
	u8 max_diff;
	u8 min_diff;
};

struct threed_lut {
	u32 value[729];
};

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
};

struct dpu_enhance {
	int enhance_en;
	int cabc_state;
	int frame_no;
	bool cabc_bl_set;

	struct hsv_lut hsv_copy;
	struct cm_cfg cm_copy;
	struct ltm_cfg ltm_copy;
	struct slp_cfg slp_copy;
	struct gamma_lut gamma_copy;
	struct epf_cfg epf_copy;
	struct threed_lut lut3d_copy;
	struct backlight_device *bl_dev;
	struct cabc_para cabc_para;
};

static void dpu_sr_config(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer);
static void dpu_enhance_reload(struct dpu_context *ctx);
static int dpu_cabc_trigger(struct dpu_context *ctx);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-r5p0";
}

static bool dpu_check_raw_int(struct dpu_context *ctx, u32 mask)
{
	u32 reg_val;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_RAW);
	if (reg_val & mask)
		return true;

	pr_err("dpu_int_raw:0x%x\n", reg_val);
	return false;
}

static void dpu_corner_init(struct dpu_context *ctx)
{
	int corner_radius = ctx->corner_radius;
	int i;

	DPU_REG_SET(ctx->base + REG_CORNER_CONFIG,
			corner_radius << 24 | corner_radius << 8);

	for (i = 0; i < corner_radius; i++) {
		DPU_REG_WR(ctx->base + REG_TOP_CORNER_LUT_ADDR, i);
		DPU_REG_WR(ctx->base + REG_TOP_CORNER_LUT_WDATA,
				dpu_r5p0_corner_param[corner_radius][i]);
		DPU_REG_WR(ctx->base + REG_BOT_CORNER_LUT_ADDR, i);
		DPU_REG_WR(ctx->base + REG_BOT_CORNER_LUT_WDATA,
				dpu_r5p0_corner_param[corner_radius][corner_radius - i - 1]);
	}

	DPU_REG_SET(ctx->base + REG_CORNER_CONFIG,
			BIT_TOP_CORNER_EN | BIT_BOT_CORNER_EN);
}

static void dpu_dump(struct dpu_context *ctx)
{
	u32 *reg = (u32 *)ctx->base;
	int i;

	pr_info("      0          4          8          C\n");
	for (i = 0; i < 256; i += 4) {
		pr_info("%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			i * 4, reg[i], reg[i + 1], reg[i + 2], reg[i + 3]);
	}
}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	u32 mmu_mask = BIT_DPU_INT_MMU_VAOR_RD_MASK |
			BIT_DPU_INT_MMU_VAOR_WR_MASK |
			BIT_DPU_INT_MMU_INV_RD_MASK |
			BIT_DPU_INT_MMU_INV_WR_MASK |
			BIT_DPU_INT_MMU_UNS_RD_MASK |
			BIT_DPU_INT_MMU_UNS_WR_MASK |
			BIT_DPU_INT_MMU_PAOR_RD_MASK |
			BIT_DPU_INT_MMU_PAOR_WR_MASK;
	u32 val = reg_val & mmu_mask;

	if (val) {
		pr_err("--- iommu interrupt err: 0x%04x ---\n", val);

		pr_err("iommu invalid read error, addr: 0x%08x\n",
			DPU_REG_RD(ctx->base + REG_DPU_MMU_INV_ADDR_RD));
		pr_err("iommu invalid write error, addr: 0x%08x\n",
			DPU_REG_RD(ctx->base + REG_DPU_MMU_INV_ADDR_WR));
		pr_err("iommu va out of range read error, addr: 0x%08x\n",
			DPU_REG_RD(ctx->base + REG_DPU_MMU_VAOR_ADDR_RD));
		pr_err("iommu va out of range write error, addr: 0x%08x\n",
			DPU_REG_RD(ctx->base + REG_DPU_MMU_VAOR_ADDR_WR));

		pr_err("BUG: iommu failure at %s:%d/%s()!\n",
			__FILE__, __LINE__, __func__);

		dpu_dump(ctx);

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	u32 reg_val, int_mask = 0;
	u32 mmu_reg_val, mmu_int_mask = 0;
	struct dpu_enhance *enhance = ctx->enhance;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_STS);
	mmu_reg_val = DPU_REG_RD(ctx->base + REG_DPU_MMU_INT_STS);

	/* disable err interrupt */
	if (reg_val & BIT_DPU_INT_ERR)
		int_mask |= BIT_DPU_INT_ERR;

	/* dpu update done isr */
	if (reg_val & BIT_DPU_INT_UPDATE_DONE) {
		/* dpu dvfs feature */
		tasklet_schedule(&ctx->dvfs_task);

		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu vsync isr */
	if (reg_val & BIT_DPU_INT_VSYNC) {
		/* write back feature */
		if ((ctx->vsync_count == ctx->max_vsync_count) && ctx->wb_en)
			schedule_work(&ctx->wb_work);

		/* cabc update backlight */
		if (enhance->cabc_bl_set)
			schedule_work(&ctx->cabc_bl_update);

		ctx->vsync_count++;
	}

	/* dpu stop done isr */
	if (reg_val & BIT_DPU_INT_DONE) {
		ctx->evt_stop = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & BIT_DPU_INT_WB_DONE) {
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Otherwise the new frame will be covered
		 * by the write back buffer, which is not what we wanted.
		 */
		if ((ctx->vsync_count > ctx->max_vsync_count) && ctx->wb_en) {
			ctx->wb_en = false;
			schedule_work(&ctx->wb_work);
			/*reg_val |= DISPC_INT_FENCE_SIGNAL_REQUEST;*/
		}

		pr_debug("wb done\n");
	}

	/* dpu write back error isr */
	if (reg_val & BIT_DPU_INT_WB_ERR) {
		pr_err("dpu write back fail\n");
		/*give a new chance to write back*/
		ctx->wb_en = true;
		ctx->vsync_count = 0;
	}

	/* dpu afbc payload error isr */
	if (reg_val & BIT_DPU_INT_FBC_PLD_ERR) {
		int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
		pr_err("dpu afbc payload error\n");
	}

	/* dpu afbc header error isr */
	if (reg_val & BIT_DPU_INT_FBC_HDR_ERR) {
		int_mask |= BIT_DPU_INT_FBC_HDR_ERR;
		pr_err("dpu afbc header error\n");
	}

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, reg_val);
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, int_mask);

	mmu_int_mask |= check_mmu_isr(ctx, mmu_reg_val);

	DPU_REG_WR(ctx->base + REG_DPU_MMU_INT_CLR, mmu_reg_val);
	DPU_REG_CLR(ctx->base + REG_DPU_MMU_INT_EN, mmu_int_mask);

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->stopped)
		return 0;

	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_stop,
						msecs_to_jiffies(500));
	ctx->evt_stop = false;

	ctx->stopped = true;

	if (!rc) {
		pr_err("dpu wait for stop done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	ctx->evt_update = false;

	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_update,
						msecs_to_jiffies(500));

	if (!rc) {
		pr_err("dpu wait for reg update done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void dpu_stop(struct dpu_context *ctx)
{
	if (ctx->if_type == SPRD_DPU_IF_DPI)
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);

	dpu_wait_stop_done(ctx);

	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

	ctx->stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		/*
		 * If the panel read GRAM speed faster than
		 * DSI write GRAM speed, it will display some
		 * mass on screen when backlight on. So wait
		 * a TE period after flush the GRAM.
		 */
		if (!ctx->panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			ctx->panel_ready = true;
		}
	}
}

static void dpu_cabc_work_func(struct work_struct *data)
{
	struct dpu_context *ctx;
	if (data) {
		ctx = container_of(data, struct dpu_context, cabc_work);

		down(&ctx->lock);
		if (ctx->enabled)
			dpu_cabc_trigger(ctx);

		up(&ctx->lock);
	} else {
		DRM_ERROR("null param data, skip work func\n");
	}
}

static void dpu_cabc_bl_update_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, cabc_bl_update);
	struct dpu_enhance *enhance = ctx->enhance;

	if (enhance->bl_dev) {
		struct sprd_backlight *bl = bl_get_data(enhance->bl_dev);
		if (enhance->cabc_state == CABC_WORKING) {
			sprd_backlight_normalize_map(enhance->bl_dev, &enhance->cabc_para.cur_bl);

			bl->cabc_en = true;
			bl->cabc_level = enhance->cabc_para.bl_fix *
					enhance->cabc_para.cur_bl / CABC_BL_COEF;
			bl->cabc_refer_level = enhance->cabc_para.cur_bl;
			sprd_cabc_backlight_update(enhance->bl_dev);
		} else {
			bl->cabc_en = false;
			backlight_update_status(enhance->bl_dev);
		}
	}

	enhance->cabc_bl_set = false;
}

static void dpu_wb_trigger(struct dpu_context *ctx, u8 count, bool debug)
{
	u32 vcnt;
	int mode_width  = DPU_REG_RD(ctx->base + REG_BLEND_SIZE) & 0xFFFF;
	int mode_height = DPU_REG_RD(ctx->base + REG_BLEND_SIZE) >> 16;

	ctx->wb_layer.dst_w = mode_width;
	ctx->wb_layer.dst_h = mode_height;
	ctx->wb_layer.xfbc = ctx->wb_xfbc_en;
	ctx->wb_layer.pitch[0] = ALIGN(mode_width, 16) * 4;
	ctx->wb_layer.fbc_hsize_r = XFBC8888_HEADER_SIZE(mode_width,
					mode_height) / 128;

	DPU_REG_WR(ctx->base + REG_WB_PITCH, ALIGN((mode_width), 16));

	ctx->wb_layer.xfbc = ctx->wb_xfbc_en;

	if (ctx->wb_xfbc_en && !debug)
		DPU_REG_WR(ctx->base + REG_WB_CFG, (ctx->wb_layer.fbc_hsize_r << 16) | BIT(0));
	else
		DPU_REG_WR(ctx->base + REG_WB_CFG, 0);

	DPU_REG_WR(ctx->base + REG_WB_BASE_ADDR, ctx->wb_addr_p);

	vcnt = (DPU_REG_RD(ctx->base + REG_DPU_STS0) & 0x1FFF);
	/*
	 * Due to AISC design problem, after the wb enable, Dpu
	 * update register operation must be connected immediately.
	 * There can be no vsync interrupts between them.
	 */
	if (vcnt * 100 / mode_height < 70) {
		if (debug)
			/* writeback debug trigger */
			DPU_REG_WR(ctx->base + REG_WB_CTRL, BIT(1));
		else
			DPU_REG_SET(ctx->base + REG_WB_CTRL, BIT(0));

		/* update trigger */
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(2));

		dpu_wait_update_done(ctx);

		pr_debug("write back trigger\n");
	} else
		ctx->vsync_count = 0;
}

static void dpu_wb_flip(struct dpu_context *ctx)
{
	dpu_clean_all(ctx);
	dpu_layer(ctx, &ctx->wb_layer);

	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(2));
	dpu_wait_update_done(ctx);

	pr_debug("write back flip\n");
}

static void dpu_wb_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);

	down(&ctx->lock);

	if (!ctx->enabled) {
		up(&ctx->lock);
		pr_err("dpu is not initialized\n");
		return;
	}

	if (ctx->flip_pending) {
		up(&ctx->lock);
		pr_warn("dpu flip is disabled\n");
		return;
	}

	if (ctx->wb_en && (ctx->vsync_count > ctx->max_vsync_count))
		dpu_wb_trigger(ctx, 1, false);
	else if (!ctx->wb_en)
		dpu_wb_flip(ctx);

	up(&ctx->lock);
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct drm_device *drm = dpu->crtc->base.dev;

	ctx->wb_configed = true;
	if (ctx->wb_configed) {
		pr_debug("write back has configed\n");
		return 0;
	}

	ctx->wb_buf_size = XFBC8888_BUFFER_SIZE(ctx->vm.hactive,
						ctx->vm.vactive);
	pr_info("use cma memory for writeback, size:0x%zx\n", ctx->wb_buf_size);
	ctx->wb_addr_v = dma_alloc_wc(drm->dev, ctx->wb_buf_size, &ctx->wb_addr_p, GFP_KERNEL);
	if (!ctx->wb_addr_p) {
		ctx->max_vsync_count = 0;
		return -ENOMEM;
	}

	ctx->wb_xfbc_en = 1;
	ctx->wb_layer.index = 7;
	ctx->wb_layer.planes = 1;
	ctx->wb_layer.alpha = 0xff;
	ctx->wb_layer.format = DRM_FORMAT_ABGR8888;
	ctx->wb_layer.addr[0] = ctx->wb_addr_p;

	ctx->max_vsync_count = 4;

	ctx->wb_configed = true;

	INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	return 0;
}

static void dpu_dvfs_task_func(unsigned long data)
{
	struct dpu_context *ctx = (struct dpu_context *)data;
	struct sprd_layer_state layer, layers[8];
	int i, j, max_x, max_y, min_x, min_y;
	int layer_en, max, maxs[8], count = 0;
	u32 dvfs_freq;

	if (!ctx->enabled) {
		pr_err("dpu is not initialized\n");
		return;
	}

	/*
	 * Count the current total number of active layers
	 * and the corresponding pos_x, pos_y, size_x and size_y.
	 */
	for (i = 0; i < DPU_LAY_COUNT; i++) {
		layer_en = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i)) & BIT(0);
		if (layer_en) {
			layers[count].dst_x = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_POS, i)) & 0xffff;
			layers[count].dst_y = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_POS, i)) >> 16;
			layers[count].dst_w = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_SIZE, i)) & 0xffff;
			layers[count].dst_h = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_SIZE, i)) >> 16;
			count++;
		}
	}

	/*
	 * Calculate the number of overlaps between each
	 * layer with other layers, not include itself.
	 */
	for (i = 0; i < count; i++) {
		layer.dst_x = layers[i].dst_x;
		layer.dst_y = layers[i].dst_y;
		layer.dst_w = layers[i].dst_w;
		layer.dst_h = layers[i].dst_h;
		maxs[i] = 1;

		for (j = 0; j < count; j++) {
			if (layer.dst_x + layer.dst_w > layers[j].dst_x &&
				layers[j].dst_x + layers[j].dst_w > layer.dst_x &&
				layer.dst_y + layer.dst_h > layers[j].dst_y &&
				layers[j].dst_y + layers[j].dst_h > layer.dst_y &&
				i != j) {
				max_x = max(layers[i].dst_x, layers[j].dst_x);
				max_y = max(layers[i].dst_y, layers[j].dst_y);
				min_x = min(layers[i].dst_x + layers[i].dst_w,
					layers[j].dst_x + layers[j].dst_w);
				min_y = min(layers[i].dst_y + layers[i].dst_h,
					layers[j].dst_y + layers[j].dst_h);

				layer.dst_x = max_x;
				layer.dst_y = max_y;
				layer.dst_w = min_x - max_x;
				layer.dst_h = min_y - max_y;

				maxs[i]++;
			}
		}
	}

	/* take the maximum number of overlaps */
	max = maxs[0];
	for (i = 1; i < count; i++) {
		if (maxs[i] > max)
			max = maxs[i];
	}

	/*
	 * Determine which frequency to use based on the
	 * maximum number of overlaps.
	 * Every IP here may be different, so need to modify it
	 * according to the actual dpu core clock.
	 */
	if (max <= 3)
		dvfs_freq = 307200000;
	else
		dvfs_freq = 384000000;

#ifdef CONFIG_DVFS_APSYS_SPRD
	dpu_dvfs_notifier_call_chain(&dvfs_freq);
#endif
}

static int dpu_init(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance = ctx->enhance;
	u32 reg_val, size;
	//int ret;

	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;

	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	DPU_REG_WR(ctx->base + REG_BLEND_SIZE, size);

	DPU_REG_WR(ctx->base + REG_DPU_CFG0, 0x00);
	reg_val = (ctx->qos_cfg.awqos_high << 12) |
		(ctx->qos_cfg.awqos_low << 8) |
		(ctx->qos_cfg.arqos_high << 4) |
		(ctx->qos_cfg.arqos_low) | BIT(18) | BIT(22);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, reg_val);
	DPU_REG_WR(ctx->base + REG_DPU_CFG2, 0x14002);

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_enhance_reload(ctx);

	if (ctx->corner_radius)
		dpu_corner_init(ctx);

	dpu_write_back_config(ctx);

	enhance->frame_no = 0;

	// ret = trusty_fast_call32(NULL, SMC_FC_DPU_FW_SET_SECURITY, FW_ATTR_SECURE, 0, 0);
	// if (ret)
	// 	pr_err("Trusty fastcall set firewall failed, ret = %d\n", ret);

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	//int ret;

	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	// ret = trusty_fast_call32(NULL, SMC_FC_DPU_FW_SET_SECURITY, FW_ATTR_NON_SECURE, 0, 0);
	// if (ret)
	// 	pr_err("Trusty fastcall clear firewall failed, ret = %d\n", ret);

	ctx->panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_XFBC_YUV420,
	DPU_LAYER_FORMAT_MAX_TYPES,
};

enum {
	DPU_LAYER_ROTATION_0,
	DPU_LAYER_ROTATION_90,
	DPU_LAYER_ROTATION_180,
	DPU_LAYER_ROTATION_270,
	DPU_LAYER_ROTATION_0_M,
	DPU_LAYER_ROTATION_90_M,
	DPU_LAYER_ROTATION_180_M,
	DPU_LAYER_ROTATION_270_M,
};

static u32 to_dpu_rotation(u32 angle)
{
	u32 rot = DPU_LAYER_ROTATION_0;

	switch (angle) {
	case 0:
	case DRM_MODE_ROTATE_0:
		rot = DPU_LAYER_ROTATION_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = DPU_LAYER_ROTATION_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = DPU_LAYER_ROTATION_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = DPU_LAYER_ROTATION_270;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_0_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_270_M;
		break;
	default:
		pr_err("rotation convert unsupport angle (drm)= 0x%x\n", angle);
		break;
	}

	return rot;
}

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT_DPU_LAY_EN;

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		fallthrough;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		fallthrough;
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB888_RB_SWITCH;
		fallthrough;
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_ARGB8888;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGB565_RB_SWITCH;
		fallthrough;
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_RGB565;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_RGB565;
		break;
	case DRM_FORMAT_NV12:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_YUV420;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	case DRM_FORMAT_NV21:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_YUV420;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2;
		break;
	case DRM_FORMAT_NV61:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	default:
		pr_err("error: invalid format %c%c%c%c\n", format,
						format >> 8,
						format >> 16,
						format >> 24);
		break;
	}

	switch (blending) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		/* don't do blending, maybe RGBX */
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/* blending mode select - normal mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_NORMAL;
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/* blending mode select - pre-mult mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_PREMULT;
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	}

	reg_val |= y2r_coef << 28;
	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;

	for (i = 0; i < 8; i++)
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i), 0x00);
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	DPU_REG_WR(ctx->base + REG_BG_COLOR, color);

	dpu_clean_all(ctx);

	if ((ctx->if_type == SPRD_DPU_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
		dpu_wait_update_done(ctx);
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
		ctx->stopped = false;
	}
}

static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer)
{
	const struct drm_format_info *info;
	u32 size, offset, ctrl, reg_val, pitch;
	int i;

	/* for secure displaying, just use layer 7 as secure layer */
	if (hwlayer->secure_en || ctx->secure_debug)
		hwlayer->index = 7;

	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_POS,
				hwlayer->index), offset);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_SIZE,
				hwlayer->index), size);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_ALPHA,
				hwlayer->index), hwlayer->alpha);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PALLETE,
				hwlayer->index), hwlayer->pallete_color);

		/* pallete layer enable */
		reg_val = BIT_DPU_LAY_EN |
			  BIT_DPU_LAY_LAYER_ALPHA |
			  BIT_DPU_LAY_PALLETE_EN;
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
				hwlayer->index), reg_val);

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h, hwlayer->pallete_color);
		return;
	}

	if (hwlayer->src_w && hwlayer->src_h)
		size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	else
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);

	for (i = 0; i < hwlayer->planes; i++) {
		if (hwlayer->addr[i] % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, hwlayer->addr[i]);
		DPU_REG_WR(ctx->base + DPU_LAY_PLANE_ADDR(REG_LAY_BASE_ADDR,
				hwlayer->index, i), hwlayer->addr[i]);
	}

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_POS,
			hwlayer->index), offset);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_SIZE,
			hwlayer->index), size);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CROP_START,
			hwlayer->index), hwlayer->src_y << 16 | hwlayer->src_x);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_ALPHA,
			hwlayer->index), hwlayer->alpha);

	info = drm_format_info(hwlayer->format);
	if (hwlayer->planes == 3) {
		/* UV pitch is 1/2 of Y pitch*/
		pitch = (hwlayer->pitch[0] / info->cpp[0]) |
				(hwlayer->pitch[0] / info->cpp[0] << 15);
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
				hwlayer->index), pitch);
	} else {
		pitch = hwlayer->pitch[0] / info->cpp[0];
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
				hwlayer->index), pitch);
	}

	ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
			hwlayer->index), ctrl);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static int dpu_vrr(struct dpu_context *ctx)
{
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
			struct sprd_dpu, ctx);
	u32 reg_val;

	dpu_stop(ctx);
	reg_val = (ctx->vm.vsync_len << 0) |
		(ctx->vm.vback_porch << 8) |
		(ctx->vm.vfront_porch << 20);
	DPU_REG_WR(ctx->base + REG_DPI_V_TIMING, reg_val);

	reg_val = (ctx->vm.hsync_len << 0) |
		(ctx->vm.hback_porch << 8) |
		(ctx->vm.hfront_porch << 20);
	DPU_REG_WR(ctx->base + REG_DPI_H_TIMING, reg_val);

	sprd_dsi_vrr_timing(dpu->dsi);
	reg_val = DPU_REG_RD(ctx->base + REG_DPU_CTRL);
	dpu_run(ctx);
	dpu->crtc->fps_mode_changed = false;

	return 0;
}

static void dpu_scaling(struct dpu_context *ctx,
			struct sprd_plane planes[], u8 count)
{
	int i;
	u16 src_w;
	u16 src_h;
	struct sprd_layer_state *layer_state;
	struct sprd_plane_state *plane_state;
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;
	struct sprd_dpu *dpu = container_of(ctx, struct sprd_dpu, ctx);

	if (dpu->crtc->sr_mode_changed) {
		pr_debug("------------------------------------\n");
		for (i = 0; i < count; i++) {
			plane_state = to_sprd_plane_state(planes[i].base.state);
			layer_state = &plane_state->layer;
			pr_debug("layer[%d] : %dx%d --- (%d)\n", i,
				layer_state->dst_w, layer_state->dst_h,
				scale_cfg->in_w);
			if (layer_state->dst_w != scale_cfg->in_w) {
				scale_cfg->skip_layer_index = i;
				break;
			}
		}

		plane_state = to_sprd_plane_state(planes[count - 1].base.state);
		layer_state = &plane_state->layer;
		if  (layer_state->dst_w <= scale_cfg->in_w) {
			dpu_sr_config(ctx);
			dpu->crtc->sr_mode_changed = false;
			pr_info("do scaling enhace, bottom layer(%dx%d)\n",
					layer_state->dst_w, layer_state->dst_h);
		}
	} else {
		if (count == 1) {
			plane_state = to_sprd_plane_state(planes[count - 1].base.state);
			layer_state = &plane_state->layer;
			if (layer_state->rotation & (DRM_MODE_ROTATE_90 |
							DRM_MODE_ROTATE_270)) {
				src_w = layer_state->src_h;
				src_h = layer_state->src_w;
			} else {
				src_w = layer_state->src_w;
				src_h = layer_state->src_h;
			}
			if (src_w == layer_state->dst_w
			&& src_h == layer_state->dst_h) {
				DPU_REG_WR(ctx->base + REG_BLEND_SIZE,
					(scale_cfg->in_h << 16) | scale_cfg->in_w);
				if (!scale_cfg->need_scale)
					DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
				else
					DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
			} else {
				/*
				 * When the layer src size is not euqal to the
				 * dst size, screened by dpu hal,the single
				 * layer need to scaling-up. Regardless of
				 * whether the SR function is turned on, dpu
				 * blend size should be set to the layer src
				 * size.
				 */
				DPU_REG_WR(ctx->base + REG_BLEND_SIZE, (src_h << 16) | src_w);
				/*
				 * When the layer src size is equal to panel
				 * size, close dpu scaling-up function.
				 */
				if (src_h == ctx->vm.vactive &&
						src_w == ctx->vm.hactive)
					DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
				else
					DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
			}
		} else {
			DPU_REG_WR(ctx->base + REG_BLEND_SIZE, (scale_cfg->in_h << 16) |
					  scale_cfg->in_w);
			if (!scale_cfg->need_scale)
				DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
			else
				DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
		}
	}
}

static void dpu_flip(struct dpu_context *ctx, struct sprd_plane planes[], u8 count)
{
	int i;
	u32 reg_val, mmu_reg_val;
	struct sprd_plane_state *state;
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;
	struct sprd_dpu *dpu = container_of(ctx, struct sprd_dpu, ctx);

	ctx->vsync_count = 0;
	if (ctx->max_vsync_count > 0 && count > 1)
		ctx->wb_en = true;

	/*
	 * Make sure the dpu is in stop status. DPU_R5P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* to check if dpu need change the frame rate */
	if (dpu->crtc->fps_mode_changed)
		dpu_vrr(ctx);

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* to check if dpu need scaling the frame for SR */
	if (!dpu->dsi->ctx.surface_mode)
		dpu_scaling(ctx, planes, count);

	/* start configure dpu layers */
	for (i = 0; i < count; i++) {
		state = to_sprd_plane_state(planes[i].base.state);

		if (scale_cfg->skip_layer_index == i && scale_cfg->skip_layer_index) {
			scale_cfg->skip_layer_index = 0;
			break;
		}

		dpu_layer(ctx, &state->layer);
	}

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		if (!ctx->stopped) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
			dpu_wait_update_done(ctx);
		}

		DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_ERR);
	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

		ctx->stopped = false;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg_val = BIT_DPU_INT_FBC_PLD_ERR |
		BIT_DPU_INT_FBC_HDR_ERR;
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, reg_val);

	mmu_reg_val = BIT_DPU_INT_MMU_VAOR_RD_MASK |
			BIT_DPU_INT_MMU_VAOR_WR_MASK |
			BIT_DPU_INT_MMU_INV_RD_MASK |
			BIT_DPU_INT_MMU_INV_WR_MASK |
			BIT_DPU_INT_MMU_UNS_RD_MASK |
			BIT_DPU_INT_MMU_UNS_WR_MASK |
			BIT_DPU_INT_MMU_PAOR_RD_MASK |
			BIT_DPU_INT_MMU_PAOR_WR_MASK;
	DPU_REG_SET(ctx->base + REG_DPU_MMU_INT_EN, mmu_reg_val);
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	u32 int_mask = 0;
	u32 mmu_int_mask = 0;
	u32 reg_val;

	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		/* use dpi as interface */
		DPU_REG_CLR(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* enable Halt function for SPRD DSI */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_DPI_HALT_EN);


		/* set dpi timing */
		reg_val = ctx->vm.hsync_len << 0 |
			  ctx->vm.hback_porch << 8 |
			  ctx->vm.hfront_porch << 20;
		DPU_REG_WR(ctx->base + REG_DPI_H_TIMING, reg_val);

		reg_val = ctx->vm.vsync_len << 0 |
			  ctx->vm.vback_porch << 8 |
			  ctx->vm.vfront_porch << 20;
		DPU_REG_WR(ctx->base + REG_DPI_V_TIMING, reg_val);

		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 32, "
				"underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= BIT_DPU_INT_UPDATE_DONE;
		/* enable dpu DONE  INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable dpu dpi vsync */
		int_mask |= BIT_DPU_INT_VSYNC;
		/* enable dpu TE INT */
		int_mask |= BIT_DPU_INT_TE;
		/* enable underflow err INT */
		int_mask |= BIT_DPU_INT_ERR;
		/* enable write back done INT */
		int_mask |= BIT_DPU_INT_WB_DONE;
		/* enable write back fail INT */
		int_mask |= BIT_DPU_INT_WB_ERR;

	} else if (ctx->if_type == SPRD_DPU_IF_EDPI) {
		/* use edpi as interface */
		DPU_REG_SET(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* use external te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_FROM_EXTERNAL_PAD);

		/* enable te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_TE_EN);

		/* enable stop DONE INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable TE INT */
		int_mask |= BIT_DPU_INT_TE;
	}

	/* enable ifbc payload error INT */
	int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
	/* enable ifbc header error INT */
	int_mask |= BIT_DPU_INT_FBC_HDR_ERR;


	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, int_mask);

	mmu_int_mask = BIT_DPU_INT_MMU_VAOR_RD_MASK |
			BIT_DPU_INT_MMU_VAOR_WR_MASK |
			BIT_DPU_INT_MMU_INV_RD_MASK |
			BIT_DPU_INT_MMU_INV_WR_MASK |
			BIT_DPU_INT_MMU_UNS_RD_MASK |
			BIT_DPU_INT_MMU_UNS_WR_MASK |
			BIT_DPU_INT_MMU_PAOR_RD_MASK |
			BIT_DPU_INT_MMU_PAOR_WR_MASK;
	DPU_REG_WR(ctx->base + REG_DPU_MMU_INT_EN, mmu_int_mask);
}

static void enable_vsync(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static void disable_vsync(struct dpu_context *ctx)
{
	//DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static int dpu_context_init(struct dpu_context *ctx, struct device_node *np)
{
	struct dpu_enhance *enhance;
	struct device_node *qos_np, *bl_np;
	int ret;

	enhance = kzalloc(sizeof(*enhance), GFP_KERNEL);
	if (!enhance)
		return -ENOMEM;

	bl_np = of_parse_phandle(np, "sprd,backlight", 0);
	if (bl_np) {
		enhance->bl_dev = of_find_backlight_by_node(bl_np);
		of_node_put(bl_np);
		if (IS_ERR_OR_NULL(enhance->bl_dev)) {
			DRM_WARN("backlight is not ready, dpu probe deferred\n");
			kfree(enhance);
			return -EPROBE_DEFER;
		}
	} else {
		pr_warn("dpu backlight node not found\n");
	}

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&ctx->corner_radius);
	if (!ret)
		pr_info("round corner support, radius = %d.\n",
					ctx->corner_radius);

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "arqos-low",
					&ctx->qos_cfg.arqos_low);
	if (ret) {
		pr_warn("read arqos-low failed, use default\n");
		ctx->qos_cfg.arqos_low = 0xc;
	}

	ret = of_property_read_u8(qos_np, "arqos-high",
					&ctx->qos_cfg.arqos_high);
	if (ret) {
		pr_warn("read arqos-high failed, use default\n");
		ctx->qos_cfg.arqos_high = 0xd;
	}

	ret = of_property_read_u8(qos_np, "awqos-low",
					&ctx->qos_cfg.awqos_low);
	if (ret) {
		pr_warn("read awqos_low failed, use default\n");
		ctx->qos_cfg.awqos_low = 0xa;
	}

	ret = of_property_read_u8(qos_np, "awqos-high",
					&ctx->qos_cfg.awqos_high);
	if (ret) {
		pr_warn("read awqos-high failed, use default\n");
		ctx->qos_cfg.awqos_high = 0xa;
	}

	of_node_put(qos_np);

	ctx->enhance = enhance;
	enhance->cabc_state = CABC_DISABLED;
	INIT_WORK(&ctx->cabc_work, dpu_cabc_work_func);
	INIT_WORK(&ctx->cabc_bl_update, dpu_cabc_bl_update_func);

	tasklet_init(&ctx->dvfs_task, dpu_dvfs_task_func,
			(unsigned long)ctx);

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET / 4;

	ctx->wb_configed = false;

	/* Allocate memory for trusty */
	ctx->tos_msg = kmalloc(sizeof(struct disp_message) + sizeof(struct layer_reg), GFP_KERNEL);
	if (!ctx->tos_msg)
		return -ENOMEM;

	return 0;
}

static void dpu_epf_set(struct dpu_context *ctx, struct epf_cfg *epf)
{
	DPU_REG_WR(ctx->base + REG_EPF_EPSILON, (epf->epsilon1 << 16) | epf->epsilon0);
	DPU_REG_WR(ctx->base + REG_EPF_GAIN0_3, (epf->gain3 << 24) | (epf->gain2 << 16) |
				(epf->gain1 << 8) | epf->gain0);
	DPU_REG_WR(ctx->base + REG_EPF_GAIN4_7, (epf->gain7 << 24) | (epf->gain6 << 16) |
				(epf->gain5 << 8) | epf->gain4);
	DPU_REG_WR(ctx->base + REG_EPF_DIFF, (epf->max_diff << 8) | epf->min_diff);
}

static void dpu_enhance_backup(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance->enhance_en |= *p;
		pr_info("enhance module enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		enhance->enhance_en &= ~(*p);
		pr_info("enhance module disable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&enhance->hsv_copy, param, sizeof(enhance->hsv_copy));
		enhance->enhance_en |= BIT(2);
		pr_info("enhance hsv backup\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&enhance->cm_copy, param, sizeof(enhance->cm_copy));
		enhance->enhance_en |= BIT(3);
		pr_info("enhance cm backup\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		memcpy(&enhance->ltm_copy, param, sizeof(enhance->ltm_copy));
		enhance->enhance_en |= BIT(6);
		pr_info("enhance ltm backup\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		enhance->slp_copy.cabc_startv = 0;
		enhance->slp_copy.cabc_endv = 255;
		enhance->enhance_en |= BIT(4);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		enhance->enhance_en |= BIT(5) | BIT(10);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&enhance->epf_copy, param, sizeof(enhance->epf_copy));
		enhance->enhance_en |= BIT(1);
		pr_info("enhance epf backup\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&enhance->lut3d_copy, param, sizeof(enhance->lut3d_copy));
		enhance->enhance_en |= BIT(9);
		pr_info("enhance lut3d backup\n");
		break;
	case ENHANCE_CFG_ID_CABC_STATE:
		p = param;
		enhance->cabc_state = *p;
		return;
	default:
		break;
	}
}

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct cm_cfg cm;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	struct cabc_para cabc_param;
	bool dpu_stopped;
	u32 *p;
	int i;

	if (!ctx->enabled) {
		dpu_enhance_backup(ctx, id, param);
		return;
	}

	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, *p);
		pr_info("enhance module enable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, *p);
		pr_info("enhance module disable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&enhance->hsv_copy, param, sizeof(enhance->hsv_copy));
		hsv = &enhance->hsv_copy;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_HSV_LUT_WDATA, (hsv->table[i].sat << 16) |
						hsv->table[i].hue);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(2));
		pr_info("enhance hsv set\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&enhance->cm_copy, param, sizeof(enhance->cm_copy));
		memcpy(&cm, &enhance->cm_copy, sizeof(struct cm_cfg));
		DPU_REG_WR(ctx->base + REG_CM_COEF01_00, (cm.coef01 << 16) | cm.coef00);
		DPU_REG_WR(ctx->base + REG_CM_COEF03_02, (cm.coef03 << 16) | cm.coef02);
		DPU_REG_WR(ctx->base + REG_CM_COEF11_10, (cm.coef11 << 16) | cm.coef10);
		DPU_REG_WR(ctx->base + REG_CM_COEF13_12, (cm.coef13 << 16) | cm.coef12);
		DPU_REG_WR(ctx->base + REG_CM_COEF21_20, (cm.coef21 << 16) | cm.coef20);
		DPU_REG_WR(ctx->base + REG_CM_COEF23_22, (cm.coef23 << 16) | cm.coef22);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(3));
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		memcpy(&enhance->ltm_copy, param, sizeof(struct ltm_cfg));
		ltm = &enhance->ltm_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG8, ((ltm->limit_hclip & 0x1ff) << 23) |
			((ltm->limit_lclip & 0x1ff) << 14) |
			((ltm->limit_clip_step & 0x1fff) << 0));
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(6));
		pr_info("enhance ltm set\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		enhance->slp_copy.cabc_startv = 0;
		enhance->slp_copy.cabc_endv = 255;
		slp = &enhance->slp_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->brightness_step << 0) |
			((slp->brightness & 0x7f) << 16));
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9));
		DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3));
		DPU_REG_WR(ctx->base + REG_SLP_CFG4, (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG5, (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) | (slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG6, (slp->hist9_index[8] << 24) |
			(slp->glb_x[0] << 16) | (slp->glb_x[1] << 8) |
			(slp->glb_x[2] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((slp->glb_s[0] & 0x1ff) << 23) |
			((slp->glb_s[1] & 0x1ff) << 14) |
			((slp->glb_s[2] & 0x1ff) << 5));
		DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((slp->local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6));
		DPU_REG_WR(ctx->base + REG_SLP_CFG10, (slp->cabc_endv << 8) |
			(slp->cabc_startv << 0));
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(4));
		pr_info("enhance slp set\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		gamma = &enhance->gamma_copy;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_WDATA, (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i]);
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(5) | BIT(10));
		pr_info("enhance gamma set\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&enhance->epf_copy, param, sizeof(enhance->epf_copy));
		epf = &enhance->epf_copy;
		dpu_epf_set(ctx, epf);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(1));
		pr_info("enhance epf set\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&enhance->lut3d_copy, param, sizeof(enhance->lut3d_copy));
		lut3d = &enhance->lut3d_copy;
		/* Fixme: Not sure why temp variables are used here */
		dpu_stopped = ctx->stopped;

		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_ADDR, i);
			ndelay(1);
			DPU_REG_WR(ctx->base + REG_TREED_LUT_WDATA, lut3d->value[i]);
			pr_debug("0x%x:0x%x\n", i, lut3d->value[i]);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(9));
		if ((ctx->if_type == SPRD_DPU_IF_DPI) && !dpu_stopped)
			dpu_run(ctx);
		pr_info("enhance lut3d set\n");
		enhance->enhance_en = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
		return;
	case ENHANCE_CFG_ID_CABC_MODE:
		p = param;

		if (*p & CABC_MODE_UI)
			enhance->cabc_para.video_mode = 0;
		else if (*p & CABC_MODE_FULL_FRAME)
			enhance->cabc_para.video_mode = 1;
		else if (*p & CABC_MODE_VIDEO)
			enhance->cabc_para.video_mode = 2;
		pr_info("enhance CABC mode: 0x%x\n", *p);
		return;
	case ENHANCE_CFG_ID_CABC_PARAM:
		memcpy(&cabc_param, param, sizeof(struct cabc_para));
		enhance->cabc_para.slp_brightness = cabc_param.slp_brightness;
		enhance->cabc_para.slp_local_weight = cabc_param.slp_local_weight;
		enhance->cabc_para.bl_fix = cabc_param.bl_fix;
		enhance->cabc_para.gain0 = cabc_param.gain0;
		enhance->cabc_para.gain1 = cabc_param.gain1;
		enhance->cabc_para.gain2 = cabc_param.gain2;
		enhance->cabc_para.p0 = cabc_param.p0;
		enhance->cabc_para.p1 = cabc_param.p1;
		return;
	case ENHANCE_CFG_ID_CABC_RUN:
		if (enhance->cabc_state != CABC_DISABLED)
			schedule_work(&ctx->cabc_work);
		return;
	case ENHANCE_CFG_ID_CABC_STATE:
		p = param;
		enhance->cabc_state = *p;
		enhance->frame_no = 0;
		return;
	default:
		break;
	}

	if ((ctx->if_type == SPRD_DPU_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(2));
		dpu_wait_update_done(ctx);
	} else if ((ctx->if_type == SPRD_DPU_IF_EDPI) && ctx->panel_ready) {
		/*
		 * In EDPI mode, we need to wait panel initializatin
		 * completed. Otherwise, the dpu enhance settings may
		 * start before panel initialization.
		 */
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(0));
		ctx->stopped = false;
	}

	enhance->enhance_en = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
}

static void dpu_enhance_get(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct epf_cfg *epf;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	u32 *p32;
	int i, val;
	u16 *p16;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		*p32 = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
		pr_info("enhance module enable get\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		epf = param;

		val = DPU_REG_RD(ctx->base + REG_EPF_EPSILON);
		epf->epsilon0 = val;
		epf->epsilon1 = val >> 16;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN0_3);
		epf->gain0 = val;
		epf->gain1 = val >> 8;
		epf->gain2 = val >> 16;
		epf->gain3 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN4_7);
		epf->gain4 = val;
		epf->gain5 = val >> 8;
		epf->gain6 = val >> 16;
		epf->gain7 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_DIFF);
		epf->min_diff = val;
		epf->max_diff = val >> 8;
		pr_info("enhance epf get\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			*p32++ = DPU_REG_RD(ctx->base + REG_HSV_LUT_RDATA);
		}
		dpu_run(ctx);
		pr_info("enhance hsv get\n");
		break;
	case ENHANCE_CFG_ID_CM:
		p32 = param;
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF01_00);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF03_02);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF11_10);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF13_12);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF21_20);
		*p32++ = DPU_REG_RD(ctx->base + REG_CM_COEF23_22);
		pr_info("enhance cm get\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		ltm = param;
		val = DPU_REG_RD(ctx->base + REG_SLP_CFG8);
		ltm->limit_hclip = (val >> 23) & 0x1ff;
		ltm->limit_lclip = (val >> 14) & 0x1ff;
		ltm->limit_clip_step = (val >> 0) & 0x1fff;
		pr_info("enhance ltm get\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		slp = param;
		val = DPU_REG_RD(ctx->base + REG_SLP_CFG0);
		slp->brightness = (val >> 16) & 0x7f;
		slp->brightness_step = (val >> 0) & 0xffff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG1);
		slp->fst_max_bright_th = (val >> 21) & 0x7f;
		slp->fst_max_bright_th_step[0] = (val >> 14) & 0x7f;
		slp->fst_max_bright_th_step[1] = (val >> 7) & 0x7f;
		slp->fst_max_bright_th_step[2] = (val >> 0) & 0x7f;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG2);
		slp->fst_max_bright_th_step[3] = (val >> 25) & 0x7f;
		slp->fst_max_bright_th_step[4] = (val >> 18) & 0x7f;
		slp->hist_exb_no = (val >> 16) & 0x3;
		slp->hist_exb_percent = (val >> 9) & 0x7f;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG3);
		slp->mask_height = (val >> 19) & 0xfff;
		slp->fst_pth_index[0] = (val >> 15) & 0xf;
		slp->fst_pth_index[1] = (val >> 11) & 0xf;
		slp->fst_pth_index[2] = (val >> 7) & 0xf;
		slp->fst_pth_index[3] = (val >> 3) & 0xf;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG4);
		slp->hist9_index[0] = (val >> 24) & 0xff;
		slp->hist9_index[1] = (val >> 16) & 0xff;
		slp->hist9_index[2] = (val >> 8) & 0xff;
		slp->hist9_index[3] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG5);
		slp->hist9_index[4] = (val >> 24) & 0xff;
		slp->hist9_index[5] = (val >> 16) & 0xff;
		slp->hist9_index[6] = (val >> 8) & 0xff;
		slp->hist9_index[7] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG6);
		slp->hist9_index[8] = (val >> 24) & 0xff;
		slp->glb_x[0] = (val >> 16) & 0xff;
		slp->glb_x[1] = (val >> 8) & 0xff;
		slp->glb_x[2] = (val >> 0) & 0xff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG7);
		slp->glb_s[0] = (val >> 23) & 0x1ff;
		slp->glb_s[1] = (val >> 14) & 0x1ff;
		slp->glb_s[2] = (val >> 5) & 0x1ff;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG9);
		slp->fast_ambient_th = (val >> 25) & 0x7f;
		slp->scene_change_percent_th = (val >> 17) & 0xff;
		slp->local_weight = (val >> 13) & 0xf;
		slp->fst_pth = (val >> 6) & 0x7f;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG10);
		slp->cabc_endv = (val >> 8) & 0xff;
		slp->cabc_startv = (val >> 0) & 0xff;
		pr_info("enhance slp get\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		dpu_stop(ctx);
		gamma = param;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			val = DPU_REG_RD(ctx->base + REG_GAMMA_LUT_RDATA);
			gamma->r[i] = (val >> 20) & 0x3FF;
			gamma->g[i] = (val >> 10) & 0x3FF;
			gamma->b[i] = val & 0x3FF;
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		dpu_run(ctx);
		pr_info("enhance gamma get\n");
		break;
	case ENHANCE_CFG_ID_SLP_LUT:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_SLP_LUT_ADDR, i);
			udelay(1);
			*p32++ = DPU_REG_RD(ctx->base + REG_SLP_LUT_RDATA);
		}
		dpu_run(ctx);
		pr_info("enhance slp lut get\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		lut3d = param;
		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_ADDR, i);
			udelay(1);
			lut3d->value[i] = DPU_REG_RD(ctx->base + REG_TREED_LUT_RDATA);
			pr_debug("0x%02x: 0x%x\n", i, lut3d->value[i]);
		}
		dpu_run(ctx);
		pr_info("enhance lut3d get\n");
		break;
	case ENHANCE_CFG_ID_CABC_HIST:
		p32 = param;
		for (i = 0; i < 32; i++) {
			*p32++ = DPU_REG_RD(ctx->base + REG_CABC_HIST0 + i * DPU_REG_SIZE);
		}
		break;
	case ENHANCE_CFG_ID_CABC_CUR_BL:
		p16 = param;
		*p16 = enhance->cabc_para.cur_bl;
		break;
	case ENHANCE_CFG_ID_VSYNC_COUNT:
		p32 = param;
		*p32 = ctx->vsync_count;
		break;
	case ENHANCE_CFG_ID_FRAME_NO:
		p32 = param;
		*p32 = enhance->frame_no;
		break;
	case ENHANCE_CFG_ID_CABC_STATE:
		p32 = param;
		*p32 = enhance->cabc_state;
		break;
	default:
		break;
	}
}

static void dpu_enhance_reload(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct ltm_cfg *ltm;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	struct threed_lut *lut3d;
	int i;

	for (i = 0; i < 256; i++) {
		DPU_REG_WR(ctx->base + REG_SLP_LUT_ADDR, i);
		udelay(1);
		DPU_REG_WR(ctx->base + REG_SLP_LUT_WDATA, slp_lut[i]);
	}
	pr_info("enhance slp lut reload\n");

	if (enhance->enhance_en & BIT(1)) {
		epf = &enhance->epf_copy;
		dpu_epf_set(ctx, epf);
		pr_info("enhance epf reload\n");
	}

	if (enhance->enhance_en & BIT(2)) {
		hsv = &enhance->hsv_copy;
		for (i = 0; i < 360; i++) {
			DPU_REG_WR(ctx->base + REG_HSV_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_HSV_LUT_WDATA, (hsv->table[i].sat << 16) |
						hsv->table[i].hue);
		}
		pr_info("enhance hsv reload\n");
	}

	if (enhance->enhance_en & BIT(3)) {
		cm = &enhance->cm_copy;
		DPU_REG_WR(ctx->base + REG_CM_COEF01_00, (cm->coef01 << 16) | cm->coef00);
		DPU_REG_WR(ctx->base + REG_CM_COEF03_02, (cm->coef03 << 16) | cm->coef02);
		DPU_REG_WR(ctx->base + REG_CM_COEF11_10, (cm->coef11 << 16) | cm->coef10);
		DPU_REG_WR(ctx->base + REG_CM_COEF13_12, (cm->coef13 << 16) | cm->coef12);
		DPU_REG_WR(ctx->base + REG_CM_COEF21_20, (cm->coef21 << 16) | cm->coef20);
		DPU_REG_WR(ctx->base + REG_CM_COEF23_22, (cm->coef23 << 16) | cm->coef22);
		pr_info("enhance cm reload\n");
	}

	if (enhance->enhance_en & BIT(4)) {
		slp = &enhance->slp_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->brightness_step << 0) |
			((slp->brightness & 0x7f) << 16));
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9));
		DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3));
		DPU_REG_WR(ctx->base + REG_SLP_CFG4, (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG5, (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) | (slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG6, (slp->hist9_index[8] << 24) |
			(CABC_GLB_X0 << 16) | (CABC_GLB_X1 << 8) |
			(CABC_GLB_X2 << 0));
		DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((CABC_GLB_S0 & 0x1ff) << 23) |
			((CABC_GLB_S1 & 0x1ff) << 14) |
			((CABC_GLB_S2 & 0x1ff) << 5));
		DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((enhance->cabc_para.slp_local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6));
		DPU_REG_WR(ctx->base + REG_SLP_CFG10, (slp->cabc_endv << 8)|
			(slp->cabc_startv << 0));
		pr_info("enhance slp reload\n");
	}

	if (enhance->enhance_en & BIT(5)) {
		gamma = &enhance->gamma_copy;
		for (i = 0; i < 256; i++) {
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_WDATA, (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i]);
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		pr_info("enhance gamma reload\n");
	}

	if (enhance->enhance_en & BIT(6)) {
		ltm = &enhance->ltm_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG8, ((ltm->limit_hclip & 0x1ff) << 23) |
			((ltm->limit_lclip & 0x1ff) << 14) |
			((ltm->limit_clip_step & 0x1fff) << 0));
		pr_info("enhance ltm reload\n");
	}

	if (enhance->enhance_en & BIT(9)) {
		lut3d = &enhance->lut3d_copy;
		for (i = 0; i < 729; i++) {
			DPU_REG_WR(ctx->base + REG_TREED_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_TREED_LUT_WDATA, lut3d->value[i]);
			pr_debug("0x%02x:0x%x\n", i, lut3d->value[i]);
		}
		pr_info("enhance lut3d reload\n");
	}

	DPU_REG_WR(ctx->base + REG_DPU_ENHANCE_CFG, enhance->enhance_en);
}

static void dpu_sr_config(struct dpu_context *ctx)
{
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;

	DPU_REG_WR(ctx->base + REG_BLEND_SIZE,
			(scale_cfg->in_h << 16) | scale_cfg->in_w);

	if (scale_cfg->need_scale)
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
	else
		DPU_REG_CLR(ctx->base + REG_DPU_ENHANCE_CFG, BIT_DPU_SCALING_EN);
}

static int dpu_cabc_trigger(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance = ctx->enhance;

	if (enhance->cabc_state != CABC_WORKING) {
		if ((enhance->cabc_state == CABC_STOPPING) && (enhance->bl_dev) &&
				enhance->cabc_para.slp_brightness <= CABC_BRIGHTNESS) {
			DPU_REG_WR(ctx->base + REG_SLP_CFG0, (CABC_BRIGHTNESS_STEP << 0) |
				((enhance->cabc_para.slp_brightness & 0x7f) << 16));
			DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((CABC_FST_MAX_BRIGHT_TH & 0x7f) << 21) |
				((CABC_FST_MAX_BRIGHT_TH_STEP0 & 0x7f) << 14) |
				((CABC_FST_MAX_BRIGHT_TH_STEP1 & 0x7f) << 7) |
				((CABC_FST_MAX_BRIGHT_TH_STEP2 & 0x7f) << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((CABC_FST_MAX_BRIGHT_TH_STEP3 & 0x7f) << 25) |
				((CABC_FST_MAX_BRIGHT_TH_STEP4 & 0x7f) << 18) |
				((CABC_HIST_EXB_NO & 0x3) << 16) |
				((CABC_HIST_EXB_PERCENT & 0x7f) << 9));
			DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((CABC_MASK_HEIGHT & 0xfff) << 19) |
				((CABC_FST_PTH_INDEX0 & 0xf) << 15) |
				((CABC_FST_PTH_INDEX1 & 0xf) << 11) |
				((CABC_FST_PTH_INDEX2 & 0xf) << 7) |
				((CABC_FST_PTH_INDEX3 & 0xf) << 3));
			DPU_REG_WR(ctx->base + REG_SLP_CFG4, (CABC_HIST9_INDEX0 << 24) |
				(CABC_HIST9_INDEX1 << 16) | (CABC_HIST9_INDEX2 << 8) |
				(CABC_HIST9_INDEX3 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG5, (CABC_HIST9_INDEX4 << 24) |
				(CABC_HIST9_INDEX5 << 16) | (CABC_HIST9_INDEX6 << 8) |
				(CABC_HIST9_INDEX7 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG6, (CABC_HIST9_INDEX8 << 24) |
				(CABC_GLB_X0 << 16) | (CABC_GLB_X1 << 8) |
				(CABC_GLB_X2 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((CABC_GLB_S0 & 0x1ff) << 23) |
				((CABC_GLB_S1 & 0x1ff) << 14) |
				((CABC_GLB_S2 & 0x1ff) << 5));
			DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((CABC_FAST_AMBIENT_TH & 0x7f) << 25) |
				(CABC_SCENE_CHANGE_PERCENT_TH << 17) |
				((enhance->cabc_para.slp_local_weight & 0xf) << 13) |
				((CABC_FST_PTH & 0x7f) << 6));
			enhance->frame_no = 0;
			enhance->cabc_bl_set = true;

			enhance->cabc_state = CABC_DISABLED;

		}
		return 0;
	}

	if (enhance->frame_no == 0) {
		enhance->enhance_en |= BIT(8);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, enhance->enhance_en);
		enhance->slp_copy.cabc_startv = 0;
		enhance->slp_copy.cabc_endv = 255;
		DPU_REG_WR(ctx->base + REG_SLP_CFG10, (enhance->slp_copy.cabc_endv << 8) |
			(enhance->slp_copy.cabc_startv << 0));
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT(2));
		dpu_wait_update_done(ctx);
		enhance->frame_no++;
	} else {
		if (!(enhance->enhance_en & BIT(8))) {
			enhance->frame_no = 0;
			return 0;
		}
		if (enhance->cabc_para.slp_brightness <= CABC_BRIGHTNESS) {
			DPU_REG_WR(ctx->base + REG_SLP_CFG0, (CABC_BRIGHTNESS_STEP << 0) |
				((enhance->cabc_para.slp_brightness & 0x7f) << 16));
			DPU_REG_WR(ctx->base + REG_SLP_CFG1, ((CABC_FST_MAX_BRIGHT_TH & 0x7f) << 21) |
				((CABC_FST_MAX_BRIGHT_TH_STEP0 & 0x7f) << 14) |
				((CABC_FST_MAX_BRIGHT_TH_STEP1 & 0x7f) << 7) |
				((CABC_FST_MAX_BRIGHT_TH_STEP2 & 0x7f) << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG2, ((CABC_FST_MAX_BRIGHT_TH_STEP3 & 0x7f) << 25) |
				((CABC_FST_MAX_BRIGHT_TH_STEP4 & 0x7f) << 18) |
				((CABC_HIST_EXB_NO & 0x3) << 16) |
				((CABC_HIST_EXB_PERCENT & 0x7f) << 9));
			DPU_REG_WR(ctx->base + REG_SLP_CFG3, ((CABC_MASK_HEIGHT & 0xfff) << 19) |
				((CABC_FST_PTH_INDEX0 & 0xf) << 15) |
				((CABC_FST_PTH_INDEX1 & 0xf) << 11) |
				((CABC_FST_PTH_INDEX2 & 0xf) << 7) |
				((CABC_FST_PTH_INDEX3 & 0xf) << 3));
			DPU_REG_WR(ctx->base + REG_SLP_CFG4, (CABC_HIST9_INDEX0 << 24) |
				(CABC_HIST9_INDEX1 << 16) | (CABC_HIST9_INDEX2 << 8) |
				(CABC_HIST9_INDEX3 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG5, (CABC_HIST9_INDEX4 << 24) |
				(CABC_HIST9_INDEX5 << 16) | (CABC_HIST9_INDEX6 << 8) |
				(CABC_HIST9_INDEX7 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG6, (CABC_HIST9_INDEX8 << 24) |
				(enhance->cabc_para.p0 << 16) | (enhance->cabc_para.p1 << 8) |
				(CABC_GLB_X2 << 0));
			DPU_REG_WR(ctx->base + REG_SLP_CFG7, ((enhance->cabc_para.gain0 & 0x1ff) << 23) |
				((enhance->cabc_para.gain1 & 0x1ff) << 14) |
				((enhance->cabc_para.gain2 & 0x1ff) << 5));
			DPU_REG_WR(ctx->base + REG_SLP_CFG9, ((CABC_FAST_AMBIENT_TH & 0x7f) << 25) |
				(CABC_SCENE_CHANGE_PERCENT_TH << 17) |
				((enhance->cabc_para.slp_local_weight & 0xf) << 13) |
				((CABC_FST_PTH & 0x7f) << 6));
		}
		if (enhance->bl_dev)
			enhance->cabc_bl_set = true;

		if (enhance->frame_no == 1)
			enhance->frame_no++;
	}
	return 0;
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_display_mode *mode)
{
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;
	struct sprd_dpu *dpu = container_of(ctx, struct sprd_dpu, ctx);
	struct sprd_panel *panel =
		(struct sprd_panel *)container_of(dpu->dsi->panel, struct sprd_panel, base);
	struct sprd_crtc_state *state = to_sprd_crtc_state(dpu->crtc->base.state);
	struct sprd_dsi *dsi = dpu->dsi;
	static unsigned int now_vtotal;
	static unsigned int now_htotal;
	struct drm_display_mode *actual_mode;
	u32 mode_vrefresh, temp_vrefresh;
	int i;

	scale_cfg->in_w = mode->hdisplay;
	scale_cfg->in_h = mode->vdisplay;
	mode_vrefresh = drm_mode_vrefresh(mode);
	actual_mode = mode;

	if (state->resolution_change) {
		if ((mode->hdisplay != ctx->vm.hactive) || (mode->vdisplay != ctx->vm.vactive))
			scale_cfg->need_scale = true;
		else
			scale_cfg->need_scale = false;
	}

	if (state->frame_rate_change) {
		if ((mode->hdisplay != ctx->vm.hactive) || (mode->vdisplay != ctx->vm.vactive)) {
			for (i = 0; i <= panel->info.display_mode_count; i++) {
				temp_vrefresh = drm_mode_vrefresh(&panel->info.buildin_modes[i]);
				if ((panel->info.buildin_modes[i].hdisplay == ctx->vm.hactive) &&
					(panel->info.buildin_modes[i].vdisplay == ctx->vm.vactive) &&
					(temp_vrefresh == mode_vrefresh)) {
					actual_mode = &(panel->info.buildin_modes[i]);
					break;
				}
			}
		}

		if (!now_htotal && !now_vtotal) {
			now_htotal = ctx->vm.hactive + ctx->vm.hfront_porch +
				ctx->vm.hback_porch + ctx->vm.hsync_len;
			now_vtotal = ctx->vm.vactive + ctx->vm.vfront_porch +
				ctx->vm.vback_porch + ctx->vm.vsync_len;
		}

		if ((actual_mode->vtotal + actual_mode->htotal) !=
			(now_htotal + now_vtotal)) {
			drm_display_mode_to_videomode(actual_mode, &ctx->vm);
			drm_display_mode_to_videomode(actual_mode, &dsi->ctx.vm);
			now_htotal = ctx->vm.hactive + ctx->vm.hfront_porch +
				ctx->vm.hback_porch + ctx->vm.hsync_len;
			now_vtotal = ctx->vm.vactive + ctx->vm.vfront_porch +
				ctx->vm.vback_porch + ctx->vm.vsync_len;
		}
	}

	pr_info("begin switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420,
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_r5p0_core_ops = {
	.version = dpu_version,
	.init = dpu_init,
	.fini = dpu_fini,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.flip = dpu_flip,
	.bg_color = dpu_bgcolor,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.context_init = dpu_context_init,
	.enhance_set = dpu_enhance_set,
	.enhance_get = dpu_enhance_get,
	.modeset = dpu_modeset,
	.write_back = dpu_wb_trigger,
	.check_raw_int = dpu_check_raw_int,
};
