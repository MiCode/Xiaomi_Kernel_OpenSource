// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "sprd_crtc.h"
#include "sprd_plane.h"
#include "sprd_dpu.h"

/* DPU registers size, 4 Bytes(32 Bits) */
#define DPU_REG_SIZE		0x04

/* DPU meomory address to DDRC offset */
#define DPU_MEM_DDRC_ADDR_OFFSET 0x80000000

/* Layer registers offset */
#define DPU_LAY_REG_OFFSET	0x0C

#define DPU_MAX_REG_OFFSET	0x948

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
#define REG_DPU_VERSION		0x00
#define REG_DPU_CTRL		0x04
#define REG_PANEL_SIZE		0x08
#define REG_PANEL_RSTN		0x0C
#define REG_DPU_SECURE		0x10
#define REG_DPU_QOS		0x14
#define REG_BG_COLOR		0x1C

/* Layer0 control registers */
#define REG_LAY_BASE_ADDR	0x20
#define REG_LAY_CTRL		0x30
#define REG_LAY_SIZE		0x34
#define REG_LAY_PITCH		0x38
#define REG_LAY_POS		0x3C
#define REG_LAY_ALPHA		0x40
#define REG_LAY_PALLETE		0x48
#define REG_LAY_CROP_START	0x4C

/* Write back control registers */
#define REG_WB_BASE_ADDR	0x140
#define REG_WB_CTRL		0x144
#define REG_WB_PITCH		0x148

/* Y2R control registers */
#define REG_Y2R_CTRL		0x150
#define REG_Y2R_Y_PARAM		0x154
#define REG_Y2R_U_PARAM		0x158
#define REG_Y2R_V_PARAM		0x15C

/* Interrupt control registers */
#define REG_DPU_INT_EN		0x160
#define REG_DPU_INT_CLR		0x164
#define REG_DPU_INT_STS		0x168
#define REG_DPU_INT_RAW		0x16C

/* Dpi ctrl registers */
#define REG_DPI_CTRL		0x170
#define REG_DPI_H_TIMING	0x174
#define REG_DPI_V_TIMING	0x178

/* Global control bits */
#define BIT_DPU_RUN		BIT(0)
#define BIT_DPU_STOP		BIT(1)
#define BIT_DPU_IF		BIT(2)
#define BIT_RUN_MODE		BIT(3)
#define BIT_REG_UPDATE_MODE	BIT(4)
#define BIT_REG_UPDATE		BIT(5)
#define BIT_DITHER_EN		BIT(6)
#define BIT_BUSY_MODE		BIT(7)

/* Dpi control bits */
#define BIT_PIXELS_DATA_WIDTH	(0x2 << 6)
#define BIT_EDPI_TE_EN		BIT(8)
#define BIT_EDPI_TE_POL		BIT(9)
#define BIT_EDPI_TE_SEL		BIT(10)
#define BIT_DPI_HALT_EN		BIT(16)

/* Layer control bits */
#define BIT_DPU_LAY_EN				BIT(0)
#define BIT_DPU_LAY_PIXEL_ALPHA			(0x00 << 2)
#define BIT_DPU_LAY_LAYER_ALPHA			(0x01 << 2)
#define BIT_DPU_LAY_PALLETE_EN			(0x01 << 3)
#define BIT_DPU_LAY_FORMAT_YUV422_2PLANE	(0x00 << 4)
#define BIT_DPU_LAY_FORMAT_YUV420_2PLANE	(0x01 << 4)
#define BIT_DPU_LAY_FORMAT_ARGB8888		(0x02 << 4)
#define BIT_DPU_LAY_FORMAT_RGB565		(0x03 << 4)
#define BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3	(0x00 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0	(0x01 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B2B3B0B1	(0x02 << 8)
#define BIT_DPU_LAY_DATA_ENDIAN_B1B0B3B2	(0x03 << 8)
#define BIT_DPU_LAY_RGBUV_SWITCH		(0x01 << 15)
#define BIT_DPU_LAY_MODE_NORMAL_PREMULT		(~BIT(16))
#define BIT_DPU_LAY_MODE_BLEND_PREMULT		(0x01 << 16)

/* Interrupt control & status bits */
#define BIT_DPU_INT_DONE		BIT(0)
#define BIT_DPU_INT_TE			BIT(1)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_EDPI_TE		BIT(3)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_DPI_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_FAIL		BIT(7)
#define BIT_DPU_INT_MMU_VAOR_RD		BIT(16)
#define BIT_DPU_INT_MMU_VAOR_WR		BIT(17)
#define BIT_DPU_INT_MMU_INV_RD		BIT(18)
#define BIT_DPU_INT_MMU_INV_WR		BIT(19)

/* DPI control bits */
#define BIT_DPU_EDPI_TE_EN		BIT(8)
#define BIT_DPU_EDPI_FROM_EXTERNAL_PAD	BIT(10)
#define BIT_DPU_DPI_HALT_EN		BIT(16)

#define DISPC_BRIGHTNESS           (0x00 << 16)
#define DISPC_CONTRAST             (0x100 << 0)
#define DISPC_OFFSET_U             (0x80 << 16)
#define DISPC_SATURATION_U         (0x100 << 0)
#define DISPC_OFFSET_V             (0x80 << 16)
#define DISPC_SATURATION_V         (0x100 << 0)

static bool panel_ready = true;

static int boot_charging;

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		struct sprd_layer_state *hwlayer);

static void dpu_version(struct dpu_context *ctx)
{
	ctx->version = "dpu-lite-r1p0";
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

static void dpu_charger_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline, *mode;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);

	if (ret) {
		pr_err("Can't not parse bootargs\n");
		return;
	}

	mode = strstr(cmdline, "androidboot.mode=charger");

	if (mode)
		boot_charging = 1;
	else
		boot_charging = 0;

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

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, reg_val);
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, int_mask);

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->stopped)
		return 0;

	/* wait for stop done interrupt */
	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_stop,
					     msecs_to_jiffies(500));
	ctx->evt_stop = false;

	ctx->stopped = true;

	if (!rc) {
		/* time out */
		pr_err("dpu wait for stop done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	ctx->evt_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(ctx->wait_queue, ctx->evt_update,
					     msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -ETIMEDOUT;
	}

	return 0;
}

enum {
	SPRD_DISPC_IF_DBI = 0,
	SPRD_DISPC_IF_DPI,
	SPRD_DISPC_IF_EDPI,
	SPRD_DISPC_IF_LIMIT
};

static void dpu_stop(struct dpu_context *ctx)
{
	if (ctx->if_type == SPRD_DISPC_IF_DPI)
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);

	dpu_wait_stop_done(ctx);
	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

	ctx->stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/*
		 * If the panel read GRAM speed faster than
		 * DSI write GRAM speed, it will display some
		 * mass on screen when backlight on. So wait
		 * a TE period after flush the GRAM.
		 */
		if (!panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			panel_ready = true;
		}
	}
}

static int dpu_init(struct dpu_context *ctx)
{
	u32 size;

	/* set bg color */
	DPU_REG_WR(ctx->base + REG_BG_COLOR, 0x00);

	/* enable dithering */
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DITHER_EN);

	/* enable DISPC Power Control */
	DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_BUSY_MODE);

	/* clear update down register*/
	DPU_REG_SET(ctx->base + REG_DPU_INT_CLR, BIT_DPU_INT_UPDATE_DONE);

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	DPU_REG_WR(ctx->base + REG_PANEL_SIZE, size);
	DPU_REG_WR(ctx->base + REG_DPU_QOS, 0x411f0);
	DPU_REG_WR(ctx->base + REG_Y2R_CTRL, 0x1);
	DPU_REG_WR(ctx->base + REG_Y2R_Y_PARAM, DISPC_BRIGHTNESS | DISPC_CONTRAST);
	DPU_REG_WR(ctx->base + REG_Y2R_U_PARAM, DISPC_OFFSET_U | DISPC_SATURATION_U);
	DPU_REG_WR(ctx->base + REG_Y2R_V_PARAM, DISPC_OFFSET_V | DISPC_SATURATION_V);

	if (ctx->stopped)
		dpu_clean_all(ctx);

	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xffff);

	dpu_charger_mode();

	return 0;
}

static void dpu_fini(struct dpu_context *ctx)
{
	DPU_REG_WR(ctx->base + REG_DPU_INT_EN, 0x00);
	DPU_REG_WR(ctx->base + REG_DPU_INT_CLR, 0xff);

	panel_ready = false;
}

static u32 dpu_img_ctrl(u32 format, u32 blending)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT_DPU_LAY_EN;

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		fallthrough;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGBUV_SWITCH;
		fallthrough;
	case DRM_FORMAT_ARGB8888:
		reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGBUV_SWITCH;
		fallthrough;
	case DRM_FORMAT_XRGB8888:
		reg_val |= BIT_DPU_LAY_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT_DPU_LAY_RGBUV_SWITCH;
		fallthrough;
	case DRM_FORMAT_RGB565:
		reg_val |= BIT_DPU_LAY_FORMAT_RGB565;
		break;
	case DRM_FORMAT_NV12:
		/*2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		break;
	case DRM_FORMAT_NV21:
		/*2-Lane: Yuv420 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV420_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B0B1B2B3;
		/*UV switch */
		reg_val |= BIT_DPU_LAY_RGBUV_SWITCH;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
		reg_val |= BIT_DPU_LAY_DATA_ENDIAN_B3B2B1B0;
		break;
	case DRM_FORMAT_NV61:
		/*2-Lane: Yuv422 */
		reg_val |= BIT_DPU_LAY_FORMAT_YUV422_2PLANE;
		/*Y endian */
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
		/* blending mode select - normal mode */
		reg_val &= BIT_DPU_LAY_MODE_NORMAL_PREMULT;
		break;
	case DRM_MODE_BLEND_PREMULTI:
		if (format == DRM_FORMAT_BGR565 ||
		    format == DRM_FORMAT_RGB565 ||
		    format == DRM_FORMAT_XRGB8888 ||
		    format == DRM_FORMAT_XBGR8888 ||
		    format == DRM_FORMAT_RGBX8888 ||
		    format == DRM_FORMAT_BGRX8888) {
			/* When the format is rgb565 or
			 * rgbx888, pixel alpha is zero.
			 * Layer alpha should be configured
			 * as block alpha.
			 */
			reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		}
		/* blending mode select - pre-mult mode */
		reg_val |= BIT_DPU_LAY_MODE_BLEND_PREMULT;
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT_DPU_LAY_LAYER_ALPHA;
		break;
	}

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;

	for (i = 0; i < 6; i++)
		DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL, i), 0x00);
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	DPU_REG_WR(ctx->base + REG_BG_COLOR, color);

	dpu_clean_all(ctx);

	if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !ctx->stopped) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_REG_UPDATE);
		dpu_wait_update_done(ctx);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
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
			 hwlayer->dst_x, hwlayer->dst_y, hwlayer->dst_w, hwlayer->dst_h,
			 hwlayer->pallete_en);
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
		/* for poweroff charging , iommu not enabled,
		   sharkle DPU is direct connect with DDRC, so memory addr need remove offset */
		if (boot_charging && (hwlayer->addr[i] >= DPU_MEM_DDRC_ADDR_OFFSET)) {
			hwlayer->addr[i] -= DPU_MEM_DDRC_ADDR_OFFSET;
		}
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
	pitch = hwlayer->pitch[0] / info->cpp[0];
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_PITCH,
		   hwlayer->index), pitch);

	ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending);
	DPU_REG_WR(ctx->base + DPU_LAY_REG(REG_LAY_CTRL,
		   hwlayer->index), ctrl);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
		 hwlayer->dst_x, hwlayer->dst_y, hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
		 hwlayer->src_x, hwlayer->src_y, hwlayer->src_w, hwlayer->src_h);
}

static void dpu_flip(struct dpu_context *ctx, struct sprd_plane planes[], u8 count)
{
	int i;
	u32 reg_val;

	/*
	 * Make sure the dpu is in stop status. In EDPI mode, the shadow
	 * registers can only be updated in the rising edge of DPU_RUN bit.
	 * And actually run when TE signal occurred.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

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

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->stopped) {
			DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_REG_UPDATE);
			dpu_wait_update_done(ctx);
		}

		DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_ERR);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_RUN);

		ctx->stopped = false;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg_val = BIT_DPU_INT_MMU_VAOR_RD | BIT_DPU_INT_MMU_VAOR_WR
		  | BIT_DPU_INT_MMU_INV_RD | BIT_DPU_INT_MMU_INV_WR;
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, reg_val);
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	u32 int_mask = 0;
	u32 reg_val;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/*use dpi as interface */
		DPU_REG_CLR(ctx->base + REG_DPU_CTRL, BIT_DPU_STOP);
		DPU_REG_CLR(ctx->base + REG_DPU_CTRL, BIT_DPU_IF);

		/* disable Halt function for SPRD DSI */
		DPU_REG_CLR(ctx->base + REG_DPI_CTRL, BIT_DPI_HALT_EN);

		/* select te from external pad */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_EDPI_TE_SEL);

		/* dpu pixel data width is 24 bit*/
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_PIXELS_DATA_WIDTH);

		/* set dpi timing */
		reg_val = (ctx->vm.hsync_len << 0) |
			  (ctx->vm.hback_porch << 8) |
			  (ctx->vm.hfront_porch << 20);
		DPU_REG_WR(ctx->base + REG_DPI_H_TIMING, reg_val);

		reg_val = (ctx->vm.vsync_len << 0) |
			  (ctx->vm.vback_porch << 8) |
			  (ctx->vm.vfront_porch << 20);
		DPU_REG_WR(ctx->base + REG_DPI_V_TIMING, reg_val);

		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warn: (vsync + vbp) < 16, underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= BIT_DPU_INT_UPDATE_DONE;
		/* enable dpu DONE  INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable dpu dpi vsync */
		int_mask |= BIT_DPU_INT_DPI_VSYNC;
		/* enable dpu TE INT */
		int_mask |= BIT_DPU_INT_TE;
		/* enable underflow err INT */
		int_mask |= BIT_DPU_INT_ERR;
		/* enable write back done INT */
		int_mask |= BIT_DPU_INT_WB_DONE;
		/* enable write back fail INT */
		int_mask |= BIT_DPU_INT_WB_FAIL;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/*use edpi as interface */
		DPU_REG_SET(ctx->base + REG_DPU_CTRL, BIT_DPU_IF);

		/* use external te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_EDPI_TE_SEL);

		/* enable te */
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_EDPI_TE_EN);

		/* dpu pixel data width is 24 bit*/
		DPU_REG_SET(ctx->base + REG_DPI_CTRL, BIT_PIXELS_DATA_WIDTH);

		/* enable stop DONE INT */
		int_mask |= BIT_DPU_INT_DONE;
		/* enable TE INT */
		int_mask |= BIT_DPU_INT_TE;
	}

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
	DPU_REG_SET(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_DPI_VSYNC);
}

static void disable_vsync(struct dpu_context *ctx)
{
	DPU_REG_CLR(ctx->base + REG_DPU_INT_EN, BIT_DPU_INT_DPI_VSYNC);
}

static int dpu_context_init(struct dpu_context *ctx, struct device_node *np)
{
	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = DPU_MAX_REG_OFFSET / 4;

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
};

static void dpu_capability(struct dpu_context *ctx,
			struct sprd_crtc_capability *cap)
{
	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);
}

const struct dpu_core_ops dpu_lite_r1p0_core_ops = {
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
};
