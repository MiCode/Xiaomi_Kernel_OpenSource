/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include "sprd_corner.h"

#define STEP (256)

#define USE_EXTERNAL_SOURCE 0

#if (USE_EXTERNAL_SOURCE)
static unsigned char layer_top_header[] = {
#include "lcd_top_corner.h"
};
static unsigned char layer_bottom_header[] = {
#include "lcd_bottom_corner.h"
};
#endif

struct sprd_layer_state corner_layer_top = {
	.planes = 1,
	.xfbc = 0,
	.format = DRM_FORMAT_ABGR8888,
	.blending = DRM_MODE_BLEND_COVERAGE,
	.alpha = 0xff,
};

struct sprd_layer_state corner_layer_bottom = {
	.planes = 1,
	.xfbc = 0,
	.format = DRM_FORMAT_ABGR8888,
	.blending = DRM_MODE_BLEND_COVERAGE,
	.alpha = 0xff,
};

static int sprd_corner_create(struct dpu_context *ctx)
{
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct drm_device *drm = dpu->crtc->base.dev;
	int buf_size;

	buf_size = ctx->vm.vactive * ctx->vm.hactive * 4;
	ctx->layer_top = dma_alloc_wc(drm->dev, buf_size, &ctx->layer_top_p, GFP_KERNEL);
	if (!ctx->layer_top) {
		DRM_ERROR("%s(): failed to allocate layer_top cma buf with %u\n",
			__func__, buf_size);
		return -ENOMEM;
	}

	ctx->layer_bottom = dma_alloc_wc(drm->dev, buf_size, &ctx->layer_bottom_p, GFP_KERNEL);
	if (!ctx->layer_bottom) {
		DRM_ERROR("%s(): failed to allocate layer_bottom cma buf with %u\n",
			__func__, buf_size);
		return -ENOMEM;
	}
	DRM_INFO("top vaddr:%px, buttom vaddr :%px top vaddr:0x%llx, buttom vaddr :0x%llx\n",
		ctx->layer_top,ctx->layer_bottom,ctx->layer_top_p,ctx->layer_bottom_p);

	return 0;
}

void sprd_corner_destroy(struct dpu_context *ctx)
{
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct drm_device *drm = dpu->crtc->base.dev;
	int buf_size;

	buf_size = ctx->vm.vactive * ctx->vm.hactive * 4;

	dma_free_wc(drm->dev, buf_size, ctx->layer_top, ctx->layer_top_p);
	dma_free_wc(drm->dev, buf_size, ctx->layer_bottom, ctx->layer_bottom_p);
}

static unsigned int gdi_sqrt(unsigned int x)
{
	unsigned int root = 0;
	unsigned int seed = (1 << 30);
	while (seed > x) {
		seed >>= 2;
	}

	while (seed != 0) {
		if (x >= seed + root) {
			x -= seed + root;
			root += seed * 2;
		}
		root >>= 1;
		seed >>= 2;
	}

	return root;
}

static void draw_sector(unsigned int *alpha, int width, int height,
		int center_x, int center_y, int radius)
{
	int x = 0;
	int y = 0;
	int line = 0;
	unsigned int *puiAlpha = NULL;

	for (y = max(0, center_y + 1); y < min(center_y + radius * 707 / 1000 + 1,
				height); y++) {
		line = gdi_sqrt((radius * STEP) * (radius * STEP) - ((y - center_y) * STEP)
				* ((y - center_y) * STEP));
		puiAlpha = &alpha[y * width + center_x + line / STEP];
		if (center_x + line / STEP >= 0 && center_x + line / STEP < width) {
			*puiAlpha = (0xff - ((line % STEP) * 0x01)) << 24;
			puiAlpha++;
		}

		x = max(0, center_x + line / STEP + 1);
		puiAlpha = &alpha[y * width + x];
		for (; x < min(center_x + radius, width); x++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha++;
		}
	}

	for (x = max(0, center_x + 1); x < min(center_x + radius * 707 / 1000 + 1,
				width); x++) {
		line = gdi_sqrt((radius * STEP) * (radius * STEP) - ((x - center_x) * STEP)
				* ((x - center_x) * STEP));
		puiAlpha = &alpha[(center_y + line / STEP) * width + x];
		if (center_y + line / STEP >= 0 && center_y + line / STEP < height) {
			*puiAlpha = (0xff - ((line % STEP) * 0x01)) << 24;
			puiAlpha += width;
		}

		y = max(0, center_y + line / STEP + 1);
		puiAlpha = &alpha[y * width + x];
		for (; y < min(center_y + radius, height); y++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha += width;
		}
	}

	for (y = max(center_y + radius * 707 / 1000 + 1, 0); y < min(center_y + radius,
				height); y++) {
		x = max(center_x + radius * 707 / 1000 + 1, 0);
		puiAlpha = &alpha[y * width + x];
		for (; x < min(center_x + radius, width); x++) {
			*puiAlpha = (0xff - 0) << 24;
			puiAlpha++;
		}
	}
}

void  sprd_corner_draw(unsigned int *corner, int radius, int width)
{
	int i, j;

	draw_sector(corner, width, radius, width - radius, 0, radius);
	for (i = 0; i < radius; i++) {
		for (j = 0; j < radius; j++)
			corner[i * width + j] = corner[(i + 1) * width - j - 1];
	}
}

void sprd_corner_x_mirrored(unsigned int *dst, unsigned int *src,
		int width, int radius)
{
	int i, j;
	for (i = 0; i < radius; i++) {
		for (j = 0; j < radius; j++)
			dst[(radius - 1 - i) * width + j] = src[i * width + j];
	}

	for (i = 0; i < radius; i++) {
		for (j = width - radius; j < width; j++)
			dst[(radius - 1 - i) * width + j] = src[i * width + j];
	}
}

int sprd_corner_hwlayer_init(struct dpu_context *ctx)
{
	int ret;
	int corner_radius = ctx->sprd_corner_radius;

	ret = sprd_corner_create(ctx);
	if (ret < 0) {
		DRM_ERROR("%s(): sprd_corner_create failed\n", __func__);
		return -ENOMEM;
	}

#if USE_EXTERNAL_SOURCE
	memcpy(ctx->layer_top, layer_top_header, ctx->vm.hactive * corner_radius * 4);
	memcpy(ctx->layer_bottom, layer_bottom_header, ctx->vm.hactive * corner_radius * 4);
#else
	sprd_corner_draw(ctx->layer_bottom, corner_radius, ctx->vm.hactive);
	sprd_corner_x_mirrored(ctx->layer_top, ctx->layer_bottom, ctx->vm.hactive, corner_radius);
#endif

	corner_layer_top.dst_x = 0;
	corner_layer_top.dst_y = 0;
	corner_layer_top.dst_w = ctx->vm.hactive;
	corner_layer_top.dst_h = corner_radius;
	corner_layer_top.pitch[0] = ctx->vm.hactive * 4;
	corner_layer_top.addr[0] = (u32)ctx->layer_top_p;

	corner_layer_bottom.dst_x = 0;
	corner_layer_bottom.dst_y = ctx->vm.vactive - corner_radius;
	corner_layer_bottom.dst_w = ctx->vm.hactive;
	corner_layer_bottom.dst_h = corner_radius;
	corner_layer_bottom.pitch[0] = ctx->vm.hactive * 4;
	corner_layer_bottom.addr[0] = (u32)ctx->layer_bottom_p;

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("infi.chen <infi.chen@unisoc.com>");
MODULE_AUTHOR("shenhui.sun <shenhui.sun@unisoc.com>");
