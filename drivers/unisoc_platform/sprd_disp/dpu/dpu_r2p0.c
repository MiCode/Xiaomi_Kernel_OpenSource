// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "sprd_crtc.h"
#include "sprd_corner.h"
#include "sprd_plane.h"
#include "sprd_dpu.h"

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((w) * (h) / (8 * 8) / 2, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (w * h * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE	0x04
/* Layer registers offset */
#define DPU_LAY_REG_OFFSET	0x0C

#define DPU_MAX_REG_OFFSET	0x870

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

/* Write back control registers */
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

/* DPU enhance config register */
#define REG_DPU_ENHANCE_CFG	0x200

/* DPU enhance epf register */
#define REG_EPF_EPSILON		0x210
#define REG_EPF_GAIN0_3		0x214
#define REG_EPF_GAIN4_7		0x218
#define REG_EPF_DIFF		0x21C

/* DPU enhance hsv register */
#define REG_HSV_LUT_ADDR		0x240
#define REG_HSV_LUT_WDATA		0x244
#define REG_HSV_LUT_RDATA		0x248

/* DPU enhance coef register */
#define REG_CM_COEF01_00		0x280
#define REG_CM_COEF03_02		0x284
#define REG_CM_COEF11_10		0x288
#define REG_CM_COEF13_12		0x28C
#define REG_CM_COEF21_20		0x290
#define REG_CM_COEF23_22		0x294

/* DPU enhance slp register */
#define REG_SLP_CFG0		0x2C0
#define REG_SLP_CFG1		0x2C4

/* DPU enhance gamma register */
#define REG_GAMMA_LUT_ADDR		0x300
#define REG_GAMMA_LUT_WDATA		0x304
#define REG_GAMMA_LUT_RDATA		0x308

/* DPU enhance checksum register */
#define REG_CHECKSUM_EN				0x340
#define REG_CHECKSUM0_START_POS		0x344
#define REG_CHECKSUM0_END_POS		0x348
#define REG_CHECKSUM1_START_POS		0x34C
#define REG_CHECKSUM1_END_POS		0x350
#define REG_CHECKSUM0_RESULT			0x354
#define REG_CHECKSUM1_RESULT			0x358

/* MMU control registers */
#define REG_MMU_EN			0x800
#define REG_MMU_VPN_RANGE		0x80C
#define REG_MMU_VAOR_ADDR_RD		0x818
#define REG_MMU_VAOR_ADDR_WR		0x81C
#define REG_MMU_INV_ADDR_RD		0x820
#define REG_MMU_INV_ADDR_WR		0x824
#define REG_MMU_PPN1			0x83C
#define REG_MMU_RANGE1			0x840
#define REG_MMU_PPN2			0x844
#define REG_MMU_RANGE2			0x848

/* Global control bits */
#define BIT_DPU_RUN			BIT(0)
#define BIT_DPU_STOP			BIT(1)
#define BIT_DPU_REG_UPDATE		BIT(2)
#define BIT_DPU_IF_EDPI			BIT(0)
#define BIT_DPU_COEF_NARROW_RANGE		BIT(4)
#define BIT_DPU_Y2R_COEF_ITU709_STANDARD	BIT(5)

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
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3		(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0		(0x01 << 8)
#define BIT_DPU_LAY_NO_SWITCH			(0x00 << 10)
#define BIT_DPU_LAY_RB_OR_UV_SWITCH		(0x01 << 10)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 12)
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
#define BIT_DPU_INT_MMU_VAOR_RD		BIT(16)
#define BIT_DPU_INT_MMU_VAOR_WR		BIT(17)
#define BIT_DPU_INT_MMU_INV_RD		BIT(18)
#define BIT_DPU_INT_MMU_INV_WR		BIT(19)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN		BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD	BIT(10)
#define BIT_DPU_DPI_HALT_EN		BIT(16)

#define SLP_BRIGHTNESS_THRESHOLD 0x20

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

struct hsv_entry {
	u16 hue;
	u16 sat;
};

struct hsv_lut {
	struct hsv_entry table[360];
};

struct gamma_lut {
	u16 r[256];
	u16 g[256];
	u16 b[256];
};

struct cm_cfg {
	short coef00;
	short coef01;
	short coef02;
	short coef03;
	short coef10;
	short coef11;
	short coef12;
	short coef13;
	short coef20;
	short coef21;
	short coef22;
	short coef23;
};

struct slp_cfg {
	u8 brightness;
	u8 conversion_matrix;
	u8 brightness_step;
	u8 second_bright_factor;
	u8 first_percent_th;
	u8 first_max_bright_th;
};

struct dpu_enhance {
	u32 enhance_en;

	struct cm_cfg cm_copy;
	struct slp_cfg slp_copy;
	struct gamma_lut gamma_copy;
	struct hsv_lut hsv_copy;
	struct epf_cfg epf_copy;
};

static void dpu_enhance_reload(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-r2p0";
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
	static bool corner_is_inited;

	if (!corner_is_inited && ctx->sprd_corner_support) {
		sprd_corner_hwlayer_init(ctx);

		corner_layer_top.index = 5;
		corner_layer_bottom.index = 6;
		corner_is_inited = 1;
	}
}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	u32 mmu_mask = BIT_DPU_INT_MMU_VAOR_RD |
			BIT_DPU_INT_MMU_VAOR_WR |
			BIT_DPU_INT_MMU_INV_RD |
			BIT_DPU_INT_MMU_INV_WR;
	u32 val = reg_val & mmu_mask;
	int i, j;

	if (val) {
		pr_err("iommu interrupt err: 0x%04x\n", val);

		if (val & BIT_DPU_INT_MMU_INV_RD)
			pr_err("iommu invalid read error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_INV_ADDR_RD));
		if (val & BIT_DPU_INT_MMU_INV_WR)
			pr_err("iommu invalid write error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_INV_ADDR_WR));
		if (val & BIT_DPU_INT_MMU_VAOR_RD)
			pr_err("iommu va out of range read error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_VAOR_ADDR_RD));
		if (val & BIT_DPU_INT_MMU_VAOR_WR)
			pr_err("iommu va out of range write error, addr: 0x%08x\n",
				DPU_REG_RD(ctx->base + REG_MMU_VAOR_ADDR_WR));

		for (i = 0; i < 8; i++) {
			reg_val = DPU_REG_RD(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i));
			if (reg_val & 0x1) {
				for(j = 0; j < 3; j++) {
					pr_info("layer%d: 0x%08x 0x%08x 0x%08x ctrl: 0x%08x\n", i,
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_PLANE_ADDR(
                                                	REG_LAY_BASE_ADDR, i, j)),
						DPU_REG_RD(ctx->base + DPU_LAY_REG(
							REG_LAY_CTRL, i)));
				}
			}
		}

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	u32 reg_val, int_mask = 0;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_STS);

	/* disable err interrupt */
	if (reg_val & BIT_DPU_INT_ERR)
		int_mask |= BIT_DPU_INT_ERR;

	/* dpu update done isr */
	if (reg_val & BIT_DPU_INT_UPDATE_DONE) {
		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu stop done isr */
	if (reg_val & BIT_DPU_INT_DONE) {
		ctx->evt_stop = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu ifbc payload error isr */
	if (reg_val & BIT_DPU_INT_FBC_PLD_ERR) {
		int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
		pr_err("dpu ifbc payload error\n");
	}

	/* dpu ifbc header error isr */
	if (reg_val & BIT_DPU_INT_FBC_HDR_ERR) {
		int_mask |= BIT_DPU_INT_FBC_HDR_ERR;
		pr_err("dpu ifbc header error\n");
	}

	int_mask |= check_mmu_isr(ctx, reg_val);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, reg_val);
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, int_mask);

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

static int dpu_init(struct dpu_context *ctx)
{
	u32 reg_val, size;

	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;

	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	DPU_REG_WR(ctx->base + REG_BLEND_SIZE, size);

	reg_val = BIT_DPU_COEF_NARROW_RANGE | BIT_DPU_Y2R_COEF_ITU709_STANDARD;
	DPU_REG_WR(ctx->base + REG_DPU_CFG0, reg_val);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, 0x004466da);
	DPU_REG_WR(ctx->base + REG_DPU_CFG2, 0x00);

	ctx->prev_y2r_coef = 3;

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_MMU_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_PPN1, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_RANGE1, 0xffff);
	DPU_REG_WR(ctx->base + REG_MMU_PPN2, 0x00);
	DPU_REG_WR(ctx->base + REG_MMU_RANGE2, 0xffff);
	DPU_REG_WR(ctx->base + REG_MMU_VPN_RANGE, 0x1ffff);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_enhance_reload(ctx);

	dpu_corner_init(ctx);

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	ctx->panel_ready = false;
}

static int dpu_context_init(struct dpu_context *ctx, struct device_node *np)
{
	struct dpu_enhance *enhance;
	int ret = 0;

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&ctx->sprd_corner_radius);
	if (!ret) {
		ctx->sprd_corner_support = 1;
		ctx->corner_size = ctx->sprd_corner_radius;
		pr_info("round corner support, radius = %d.\n",
					ctx->sprd_corner_radius);
	}


	enhance = kzalloc(sizeof(*enhance), GFP_KERNEL);
	if (!enhance)
		return -ENOMEM;

	ctx->enhance = enhance;

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET / 4;

	return 0;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
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

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 rotation)
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
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
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
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
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
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		fallthrough;
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_RGB565;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_RGB565;
		break;
	case DRM_FORMAT_NV12:
		/* 2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_NV21:
		/* 2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		break;
	case DRM_FORMAT_NV16:
		/* 2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
		break;
	case DRM_FORMAT_NV61:
		/* 2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_NO_SWITCH;
		break;
	case DRM_FORMAT_YVU420:
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_3PLANE;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_RB_OR_UV_SWITCH;
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
		/*Normal mode*/
		reg_val |= BIT_DPU_LAY_MODE_BLEND_NORMAL;
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/*Pre-mult mode*/
		reg_val |= BIT_DPU_LAY_MODE_BLEND_PREMULT;
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	}

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

static int check_layer_y2r_coef(struct dpu_context *ctx,
								struct sprd_plane planes[], u8 count)
{
	int i;

	for (i = (count - 1); i >= 0; i--) {
		struct sprd_plane_state *state = to_sprd_plane_state(planes[i].base.state);
		struct sprd_layer_state *layer = &state->layer;
		switch (layer->format) {
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV61:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YVU420:
			if (layer->y2r_coef == ctx->prev_y2r_coef)
				return -1;

			/* need to config dpu y2r coef */
			ctx->prev_y2r_coef = layer->y2r_coef;
			return ctx->prev_y2r_coef;
		default:
			break;
		}
	}

	/* not find yuv layer */
	return -1;
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
	u32 addr, size, offset, ctrl, reg_val, pitch;
	int i;

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
		addr = hwlayer->addr[i];

		/* dpu r2p0 just support xfbc-rgb */
		if (hwlayer->xfbc)
			addr += hwlayer->fbc_hsize_r;

		if (addr % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, addr);
		DPU_REG_WR(ctx->base + DPU_LAY_PLANE_ADDR(REG_LAY_BASE_ADDR,
				hwlayer->index, i), addr);
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
		hwlayer->xfbc, hwlayer->rotation);

	/*
	 * if layer0 blend mode is premult mode
	 * use layer alpha.
	 * blend mode use normal mode.
	 */
	if (hwlayer->index == 0 &&
		(hwlayer->blending == DRM_MODE_BLEND_PREMULTI)) {
		ctrl &= ~BIT(3); // Fix it later
		ctrl |= BIT_DPU_LAY_LAYER_ALPHA;
		ctrl &= ~BIT(16);
	}

	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
			hwlayer->index), ctrl);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_flip(struct dpu_context *ctx, struct sprd_plane planes[], u8 count)
{
	int i;
	u32 reg_val;
	int y2r_coef;

	/*
	 * Make sure the dpu is in stop status. DPU_R2P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DPU_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* set Y2R conversion coef */
	y2r_coef = check_layer_y2r_coef(ctx, planes, count);
	if (y2r_coef >= 0) {
		/* write dpu_cfg0 register after dpu is in idle status */
		if (ctx->if_type == SPRD_DPU_IF_DPI)
			dpu_stop(ctx);

		DPU_REG_CLR(ctx->base + REG_DPU_CFG0, (0x7 << 4));
		DPU_REG_SET(ctx->base + REG_DPU_CFG0, (y2r_coef << 4));
	}

	/* reset the bgcolor to black */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* start configure dpu layers */
	for (i = 0; i < count; i++) {
		struct sprd_plane_state *state;

		state = to_sprd_plane_state(planes[i].base.state);
		dpu_layer(ctx, &state->layer);
	}

	/* special case for round corner */
	if (ctx->sprd_corner_support) {
		dpu_layer(ctx, &corner_layer_top);
		dpu_layer(ctx, &corner_layer_bottom);
	}

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		if (!ctx->stopped) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
			dpu_wait_update_done(ctx);
		} else if (y2r_coef >= 0) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);
			ctx->stopped = false;
			pr_info("dpu start\n");
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
		  BIT_DPU_INT_FBC_HDR_ERR |
		  BIT_DPU_INT_MMU_VAOR_RD |
		  BIT_DPU_INT_MMU_VAOR_WR |
		  BIT_DPU_INT_MMU_INV_RD |
		  BIT_DPU_INT_MMU_INV_WR;
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, reg_val);

}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	u32 int_mask = 0;
	u32 reg_val;

	if (ctx->if_type == SPRD_DPU_IF_DPI) {
		/* use dpi as interface */
		DPU_REG_CLR(ctx->base + REG_DPU_CFG0, BIT_DPU_IF_EDPI);

		/* disable Halt function for SPRD DSI */
		DPU_REG_CLR(ctx->base + REG_DPI_CTRL, BIT_DPU_DPI_HALT_EN);

		/* select te from external pad */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPU_EDPI_FROM_EXTERNAL_PAD);

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
	/* enable iommu va out of range read error INT */
	int_mask |= BIT_DPU_INT_MMU_VAOR_RD;
	/* enable iommu va out of range write error INT */
	int_mask |= BIT_DPU_INT_MMU_VAOR_WR;
	/* enable iommu invalid read error INT */
	int_mask |= BIT_DPU_INT_MMU_INV_RD;
	/* enable iommu invalid write error INT */
	int_mask |= BIT_DPU_INT_MMU_INV_WR;

	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, int_mask);
}

static void enable_vsync(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static void disable_vsync(struct dpu_context *ctx)
{
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_VSYNC);
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_YVU420,
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

static void dpu_enhance_backup(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance->enhance_en |= *p;
		pr_info("enhance enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		enhance->enhance_en &= ~(*p);
		pr_info("enhance disable backup: 0x%x\n", *p);
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
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		enhance->enhance_en |= BIT(4);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		enhance->enhance_en |= BIT(5);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&enhance->epf_copy, param, sizeof(enhance->epf_copy));
		enhance->enhance_en |= BIT(1);
		break;
	default:
		break;
	}
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

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	u32 *p, i;

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
		cm = &enhance->cm_copy;
		DPU_REG_WR(ctx->base + REG_CM_COEF01_00, (cm->coef01 << 16) | cm->coef00);
		DPU_REG_WR(ctx->base + REG_CM_COEF03_02, (cm->coef03 << 16) | cm->coef02);
		DPU_REG_WR(ctx->base + REG_CM_COEF11_10, (cm->coef11 << 16) | cm->coef10);
		DPU_REG_WR(ctx->base + REG_CM_COEF13_12, (cm->coef13 << 16) | cm->coef12);
		DPU_REG_WR(ctx->base + REG_CM_COEF21_20, (cm->coef21 << 16) | cm->coef20);
		DPU_REG_WR(ctx->base + REG_CM_COEF23_22, (cm->coef23 << 16) | cm->coef22);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(3));
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&enhance->slp_copy, param, sizeof(enhance->slp_copy));
		slp = &enhance->slp_copy;
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->second_bright_factor << 24) |
				(slp->brightness_step << 16) |
				(slp->conversion_matrix << 8) |
				slp->brightness);
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, (slp->first_max_bright_th << 8) |
				slp->first_percent_th);
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(4));
		pr_info("enhance slp set\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&enhance->gamma_copy, param, sizeof(enhance->gamma_copy));
		gamma = &enhance->gamma_copy;
		for (i = 0; i < 256; i++) {
			DPU_REG_SET(ctx->base + REG_GAMMA_LUT_ADDR, i);
			udelay(1);
			DPU_REG_WR(ctx->base + REG_GAMMA_LUT_ADDR, (gamma->r[i] << 20) |
						(gamma->g[i] << 10) | gamma->b[i]);
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		DPU_REG_SET(ctx->base + REG_DPU_ENHANCE_CFG, BIT(5));
		pr_info("enhance gamma set\n");
		break;
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
	struct epf_cfg *ep;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	u32 *p32, i, val;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		*p32 = DPU_REG_RD(ctx->base + REG_DPU_ENHANCE_CFG);
		pr_info("enhance module enable get\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		ep = param;

		val = DPU_REG_RD(ctx->base + REG_EPF_EPSILON);
		ep->epsilon0 = val;
		ep->epsilon1 = val >> 16;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN0_3);
		ep->gain0 = val;
		ep->gain1 = val >> 8;
		ep->gain2 = val >> 16;
		ep->gain3 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_GAIN4_7);
		ep->gain4 = val;
		ep->gain5 = val >> 8;
		ep->gain6 = val >> 16;
		ep->gain7 = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_EPF_DIFF);
		ep->min_diff = val;
		ep->max_diff = val >> 8;
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
	case ENHANCE_CFG_ID_SLP:
		slp = param;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG0);
		slp->brightness = val;
		slp->conversion_matrix = val >> 8;
		slp->brightness_step = val >> 16;
		slp->second_bright_factor = val >> 24;

		val = DPU_REG_RD(ctx->base + REG_SLP_CFG1);
		slp->first_percent_th = val;
		slp->first_max_bright_th = val >> 8;
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
	default:
		break;
	}
}

static void dpu_enhance_reload(struct dpu_context *ctx)
{
	struct dpu_enhance *enhance = ctx->enhance;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	int i;

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
		DPU_REG_WR(ctx->base + REG_SLP_CFG0, (slp->second_bright_factor << 24) |
				(slp->brightness_step << 16) |
				(slp->conversion_matrix << 8) |
				slp->brightness);
		DPU_REG_WR(ctx->base + REG_SLP_CFG1, (slp->first_max_bright_th << 8) |
				slp->first_percent_th);
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

	DPU_REG_WR(ctx->base + REG_DPU_ENHANCE_CFG, enhance->enhance_en);
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_display_mode *mode)
{
	struct scale_config_param *scale_cfg = &ctx->scale_cfg;

	scale_cfg->in_w = mode->hdisplay;
	scale_cfg->in_h = mode->vdisplay;

	if ((mode->hdisplay != ctx->vm.hactive) ||
	    (mode->vdisplay != ctx->vm.vactive))
		scale_cfg->need_scale = true;
	else
		scale_cfg->need_scale = false;

	scale_cfg->sr_mode_changed = true;
	pr_info("begin switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
}

const struct dpu_core_ops dpu_r2p0_core_ops = {
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
	.check_raw_int = dpu_check_raw_int,
	.enhance_set = dpu_enhance_set,
	.enhance_get = dpu_enhance_get,
	.modeset = dpu_modeset,
};
