// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <drm/drm_prime.h>

#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "sprd_dpu1.h"
#include "sprd_bl.h"

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE	0x04
/* Layer registers offset */
#define DPU_LAY_REG_OFFSET	0x0C

#define DPU_MAX_REG_OFFSET	0x8AC

#define DPU_LAY_COUNT	8

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
#define REG_DPU_MODE	0x10
#define REG_DPU_SECURE	0x14
#define REG_PANEL_SIZE	0x20
//#define REG_BLEND_SIZE	0x24
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

/* DPU cursor config registers */
#define REG_CURSOR_EN			0x400

/* DPU sdp config registers */
#define REG_SDP_CFG			0x420
#define REG_SDP_LUT_ADDR	0x424
#define REG_SDP_LUT_WDATA	0x428
#define REG_SDP_LUT_RDATA	0x42C

/* PQ mmu config registers */
#define REG_DPU_MMU_EN			0x804
#define REG_DPU_MMU_VAOR_ADDR_RD	0x854
#define REG_DPU_MMU_VAOR_ADDR_WR	0x858
#define REG_DPU_MMU_INV_ADDR_RD		0x85C
#define REG_DPU_MMU_INV_ADDR_WR		0x860
#define REG_DPU_MMU_INT_EN		0x8A0
#define REG_DPU_MMU_INT_CLR		0x8A4
#define REG_DPU_MMU_INT_STS		0x8A8
#define REG_DPU_MMU_INT_RAW		0x8AC

/* Global control bits */
#define BIT_DPU_RUN			BIT(0)
#define BIT_DPU_STOP			BIT(1)
#define BIT_DPU_REG_UPDATE		BIT(2)

/* DPU display mode bit */
#define BIT_DPU_MODE_BYPASS		BIT(0)

/* Layer control bits */
#define BIT_DPU_LAY_EN				BIT(0)
#define BIT_DPU_LAY_LAYER_ALPHA			(0x01 << 2)
#define BIT_DPU_LAY_COMBO_ALPHA			(0x02 << 2)
#define BIT_DPU_LAY_FORMAT_YUV422_2PLANE	(0x00 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_2PLANE	(0x01 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_3PLANE	(0x02 << 4)
#define BIT_DPU_LAY_FORMAT_ARGB8888		(0x03 << 4)
#define BIT_DPU_LAY_FORMAT_RGB565		(0x04 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_10	(0x05 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_ARGB8888	(0x08 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_RGB565		(0x09 << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_YUV420		(0x0A << 4)
#define BIT_DPU_LAY_FORMAT_XFBC_YUV420_10	(0x0B << 4)
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3	(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0	(0x01 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B2B3B0B1	(0x02 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2	(0x03 << 8)
#define BIT_DPU_LAY_NO_SWITCH			(0x00 << 10)
#define BIT_DPU_LAY_RGB888_RB_SWITCH		(0x01 << 10)
#define BIT_DPU_LAY_RGB565_RB_SWITCH		(0x01 << 12)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 13)
#define BIT_DPU_LAY_MODE_BLEND_MODE		BIT(16)

/* Interrupt control & status bits */
#define BIT_DPU_INT_DONE		BIT(0)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_ERR		BIT(7)
#define BIT_DPU_INT_FBC_PLD_ERR		BIT(8)
#define BIT_DPU_INT_FBC_HDR_ERR		BIT(9)
#define BIT_DPU_INT_SDP_DONE		BIT(10)

/* DPI control bits */
#define BIT_DPI_HSYNC_POLARITY		BIT(0)
#define BIT_DPI_VSYNC_POLARITY		BIT(1)
#define BIT_FHD_OUT_FORMAT_RGB888		(0x0 << 4)
#define BIT_FHD_OUT_FORMAT_RGB666		(0x1 << 4)
#define BIT_FHD_OUT_FORMAT_YUV444_BT601	(0x4 << 4)
#define BIT_FHD_OUT_FORMAT_YUV422_BT601	(0x5 << 4)
#define BIT_FHD_OUT_FORMAT_YUV444_BT709	(0x6 << 4)
#define BIT_FHD_OUT_FORMAT_YUV422_BT709	(0x7 << 4)

/* sdp config bits */
#define BIT_SDP_EN			BIT(0)
//#define BIT_SDP_DATA_NUM	BIT(8)

/* mmu interrupt bits */
#define BIT_DPU_INT_MMU_PAOR_WR_MASK	BIT(7)
#define BIT_DPU_INT_MMU_PAOR_RD_MASK	BIT(6)
#define BIT_DPU_INT_MMU_UNS_WR_MASK	BIT(5)
#define BIT_DPU_INT_MMU_UNS_RD_MASK	BIT(4)
#define BIT_DPU_INT_MMU_INV_WR_MASK	BIT(3)
#define BIT_DPU_INT_MMU_INV_RD_MASK	BIT(2)
#define BIT_DPU_INT_MMU_VAOR_WR_MASK	BIT(1)
#define BIT_DPU_INT_MMU_VAOR_RD_MASK	BIT(0)

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

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-lite-r3p0";
}

/*
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
*/

/*
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
	}

	return val;
}
*/

static u32 dpu_isr(struct dpu_context *ctx)
{
	u32 reg_val, int_mask = 0;
	//u32 mmu_reg_val, mmu_int_mask = 0;

	reg_val = DPU_REG_RD(ctx->base + REG_DPU_INT_STS);
	//mmu_reg_val = DPU_REG_RD(ctx->base + REG_DPU_MMU_INT_STS);

	/* disable err interrupt */
	if (reg_val & BIT_DPU_INT_ERR)
		int_mask |= BIT_DPU_INT_ERR;

	/* dpu update done isr */
	if (reg_val & BIT_DPU_INT_UPDATE_DONE) {
		ctx->evt_update = true;
		wake_up_interruptible_all(&ctx->wait_queue);
	}

	/* dpu vsync isr */
	//if (reg_val & BIT_DPU_INT_VSYNC) {
		/* write back feature */
	//	if ((ctx->vsync_count == ctx->max_vsync_count) && ctx->wb_en)
	//		schedule_work(&ctx->wb_work);

	//	ctx->vsync_count++;
	//}

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
		//if ((ctx->vsync_count > ctx->max_vsync_count) && ctx->wb_en) {
		//	ctx->wb_en = false;
		//	schedule_work(&ctx->wb_work);
		//}

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

	//mmu_int_mask |= check_mmu_isr(ctx, mmu_reg_val);

	//DPU_REG_WR(ctx->base + REG_DPU_MMU_INT_CLR, mmu_reg_val);
	//DPU_REG_CLR(ctx->base + REG_DPU_MMU_INT_EN, mmu_int_mask);

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
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);

	dpu_wait_stop_done(ctx);

	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

	ctx->stopped = false;

	pr_info("dpu run\n");
}

static void dpu_sdp_set(struct dpu_context *ctx,
			u32 size, u32 *data)
{
	//struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;
	/* hardware SDP transfer num, 36 bytes unit */
	u32 num_sdp = size * 4 / 36;

	for (i = 0; i < size; i++) {
		DPU_REG_WR(ctx->base + REG_SDP_LUT_ADDR, i);
		DPU_REG_WR(ctx->base + REG_SDP_LUT_WDATA, data[i]);
	}

	DPU_REG_WR(ctx->base + REG_SDP_CFG, (num_sdp << 8) | BIT_SDP_EN);
}

static void dpu_sdp_disable(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_SDP_CFG, 0);
}

static int dpu_init(struct dpu_context *ctx)
{
	u32 reg_val, size;

	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;

	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	//DPU_REG_WR(ctx->base + REG_BLEND_SIZE, size);

	DPU_REG_WR(ctx->base + REG_DPU_CFG0, 0x00);
	reg_val = (ctx->qos_cfg.awqos_high << 12) |
		(ctx->qos_cfg.awqos_low << 8) |
		(ctx->qos_cfg.arqos_high << 4) |
		(ctx->qos_cfg.arqos_low) | BIT(22) | BIT(23) | BIT(24);
	DPU_REG_WR(ctx->base + REG_DPU_CFG1, reg_val);
	//DPU_REG_WR(ctx->base + REG_DPU_CFG2, 0x14002);

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_sdp_disable(ctx);
	DPU_REG_WR(ctx->base + REG_CURSOR_EN, 0x0);

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	ctx->panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_YUV420_10,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_XFBC_YUV420,
	DPU_LAYER_FORMAT_XFBC_YUV420_10,
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
	case DRM_FORMAT_P010:
		if (compression)
			/* 2-Lane: Yuv420 10bit*/
			reg_val |= BIT_DPU_LAY_FORMAT_XFBC_YUV420_10 << 4;
		else
			reg_val |= BIT_DPU_LAY_FORMAT_YUV420_10 << 4;
		/* Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3 << 10;
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
		reg_val  &= (~BIT_DPU_LAY_MODE_BLEND_MODE);
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT_DPU_LAY_COMBO_ALPHA;
		/* blending mode select - pre-mult mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_MODE;
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

	if (!ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
		dpu_wait_update_done(ctx);
	}
}

static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer)
{
	const struct drm_format_info *info;
	u32 size, offset, ctrl, reg_val, pitch;
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

static void dpu_flip(struct dpu_context *ctx, struct sprd_plane planes[], u8 count)
{
	int i;
	u32 reg_val = 0;
	struct sprd_plane_state *state;
	struct sprd_layer_state *layer;

	/* reset the bgcolor to black */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* bypass mode only use layer7 */
	if (ctx->bypass_mode) {
		if (count != 1)
			pr_err("bypass mode layer count error:%d", count);
		state = to_sprd_plane_state(planes[7].base.state);
		layer = &state->layer;
		layer->index = 0;
		if (ctx->static_metadata_changed) {
			dpu_sdp_set(ctx, 9, ctx->hdr_static_metadata);
			ctx->static_metadata_changed = false;
		}
	}

	/* start configure dpu layers */
	for (i = 0; i < count; i++) {
		state = to_sprd_plane_state(planes[i].base.state);
		dpu_layer(ctx, &state->layer);
	}

	/* update trigger and wait */
	if (!ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_REG_UPDATE);
		dpu_wait_update_done(ctx);
	}

	reg_val |= BIT_DPU_INT_ERR;

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg_val |= BIT_DPU_INT_FBC_PLD_ERR |
		BIT_DPU_INT_FBC_HDR_ERR;
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, reg_val);

	/*
	mmu_reg_val = BIT_DPU_INT_MMU_VAOR_RD_MASK |
			BIT_DPU_INT_MMU_VAOR_WR_MASK |
			BIT_DPU_INT_MMU_INV_RD_MASK |
			BIT_DPU_INT_MMU_INV_WR_MASK |
			BIT_DPU_INT_MMU_UNS_RD_MASK |
			BIT_DPU_INT_MMU_UNS_WR_MASK |
			BIT_DPU_INT_MMU_PAOR_RD_MASK |
			BIT_DPU_INT_MMU_PAOR_WR_MASK;
	DPU_REG_SET(ctx->base + REG_DPU_MMU_INT_EN, mmu_reg_val);
	*/
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	u32 int_mask = 0;
	//u32 mmu_int_mask = 0;
	u32 reg_val;

	if (!(ctx->vm.flags & DISPLAY_FLAGS_HSYNC_LOW))
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPI_HSYNC_POLARITY);
	if (!(ctx->vm.flags & DISPLAY_FLAGS_VSYNC_LOW))
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_DPI_VSYNC_POLARITY);

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

	if (ctx->bypass_mode)
		DPU_REG_WR(ctx->base + REG_DPU_MODE, BIT_DPU_MODE_BYPASS);
	else
		DPU_REG_CLR(ctx->base + REG_DPU_MODE, BIT_DPU_MODE_BYPASS);

	/* enable dpu update done INT */
	int_mask |= BIT_DPU_INT_UPDATE_DONE;
	/* enable dpu DONE  INT */
	int_mask |= BIT_DPU_INT_DONE;
	/* enable dpu dpi vsync */
	int_mask |= BIT_DPU_INT_VSYNC;
	/* enable underflow err INT */
	int_mask |= BIT_DPU_INT_ERR;
	/* enable write back done INT */
	int_mask |= BIT_DPU_INT_WB_DONE;
	/* enable write back fail INT */
	int_mask |= BIT_DPU_INT_WB_ERR;
	/* enable ifbc payload error INT */
	int_mask |= BIT_DPU_INT_FBC_PLD_ERR;
	/* enable ifbc header error INT */
	int_mask |= BIT_DPU_INT_FBC_HDR_ERR;


	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, int_mask);

	/*
	mmu_int_mask = BIT_DPU_INT_MMU_VAOR_RD_MASK |
			BIT_DPU_INT_MMU_VAOR_WR_MASK |
			BIT_DPU_INT_MMU_INV_RD_MASK |
			BIT_DPU_INT_MMU_INV_WR_MASK |
			BIT_DPU_INT_MMU_UNS_RD_MASK |
			BIT_DPU_INT_MMU_UNS_WR_MASK |
			BIT_DPU_INT_MMU_PAOR_RD_MASK |
			BIT_DPU_INT_MMU_PAOR_WR_MASK;
	DPU_REG_WR(ctx->base + REG_DPU_MMU_INT_EN, mmu_int_mask);
	*/
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
	struct device_node *qos_np;
	int ret;

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


	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET / 4;

	//ctx->wb_configed = false;

	return 0;
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_display_mode *mode)
{
	drm_display_mode_to_videomode(mode, &ctx->vm);

	/* only 3840x2160 use bypass mode */
	if (ctx->vm.hactive == 3840 &&
		ctx->vm.vactive == 2160) {
		ctx->bypass_mode = true;
		pr_info("bypass_mode\n");
	} else {
		ctx->bypass_mode = false;
		pr_info("normal mode\n");
	}

	pr_info("switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

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
	DRM_FORMAT_YUV420, DRM_FORMAT_P010,
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_lite_r3p0_core_ops = {
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
	.modeset = dpu_modeset,
};
