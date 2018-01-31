/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_ROTATOR_INLINE_H__
#define __SDE_ROTATOR_INLINE_H__

#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>

#include "sde_rotator_formats.h"

#define SDE_ROTATOR_INLINE_PLANE_MAX	4

/*
 * enum sde_rotator_inline_cmd_type - inline rotator command stages
 * @SDE_ROTATOR_INLINE_CMD_VALIDATE: validate command only
 * @SDE_ROTATOR_INLINE_CMD_COMMIT: commit command to hardware
 * @SDE_ROTATOR_INLINE_CMD_START: ready to start inline rotation
 * @SDE_ROTATOR_INLINE_CMD_CLEANUP: cleanup after commit is done
 * @SDE_ROTATOR_INLINE_CMD_ABORT: abort current commit and reset
 */
enum sde_rotator_inline_cmd_type {
	SDE_ROTATOR_INLINE_CMD_VALIDATE,
	SDE_ROTATOR_INLINE_CMD_COMMIT,
	SDE_ROTATOR_INLINE_CMD_START,
	SDE_ROTATOR_INLINE_CMD_CLEANUP,
	SDE_ROTATOR_INLINE_CMD_ABORT,
};

/**
 * sde_rotator_inline_cmd - inline rotation command
 * @sequence_id: unique command sequence identifier
 * @video_mode: true if video interface is connected
 * @fps: frame rate in frame-per-second
 * @rot90: rotate 90 counterclockwise
 * @hflip: horizontal flip prior to rotation
 * @vflip: vertical flip prior to rotation
 * @secure: true if buffer is in secure domain
 * @prefill_bw: prefill bandwidth in Bps
 * @clkrate: clock rate in Hz
 * @data_bw: data bus bandwidth in Bps
 * @src_addr: source i/o buffer virtual address
 * @src_len: source i/o buffer length
 * @src_planes: source plane number
 * @src_pixfmt: v4l2 fourcc pixel format of source buffer
 * @src_width: width of source buffer
 * @src_height: height of source buffer
 * @src_rect_x: roi x coordinate of source buffer
 * @src_rect_y: roi y coordinate of source buffer
 * @src_rect_w: roi width of source buffer
 * @src_rect_h: roi height of source buffer
 * @dst_addr: destination i/o virtual buffer address
 * @dst_len: destination i/o buffer length
 * @dst_planes: destination plane number
 * @dst_pixfmt: v4l2 fourcc pixel format of destination buffer
 * @dst_rect_x: roi x coordinate of destination buffer
 * @dst_rect_y: roi y coordinate of destination buffer
 * @dst_rect_w: roi width of destination buffer
 * @dst_rect_h: roi height of destination buffer
 * @dst_writeback: true if cache writeback is required
 * @priv_handle: private handle of rotator session
 */
struct sde_rotator_inline_cmd {
	u32 sequence_id;
	bool video_mode;
	u32 fps;
	bool rot90;
	bool hflip;
	bool vflip;
	bool secure;
	u64 prefill_bw;
	u64 clkrate;
	u64 data_bw;
	dma_addr_t src_addr[SDE_ROTATOR_INLINE_PLANE_MAX];
	u32 src_len[SDE_ROTATOR_INLINE_PLANE_MAX];
	u32 src_planes;
	u32 src_pixfmt;
	u32 src_width;
	u32 src_height;
	u32 src_rect_x;
	u32 src_rect_y;
	u32 src_rect_w;
	u32 src_rect_h;
	dma_addr_t dst_addr[SDE_ROTATOR_INLINE_PLANE_MAX];
	u32 dst_len[SDE_ROTATOR_INLINE_PLANE_MAX];
	u32 dst_planes;
	u32 dst_pixfmt;
	u32 dst_rect_x;
	u32 dst_rect_y;
	u32 dst_rect_w;
	u32 dst_rect_h;
	bool dst_writeback;
	void *priv_handle;
};

void *sde_rotator_inline_open(struct platform_device *pdev);
int sde_rotator_inline_get_dst_pixfmt(struct platform_device *pdev,
		u32 src_pixfmt, u32 *dst_pixfmt);
int sde_rotator_inline_get_downscale_caps(struct platform_device *pdev,
		char *downscale_caps, int len);
int sde_rotator_inline_get_maxlinewidth(struct platform_device *pdev);
int sde_rotator_inline_get_pixfmt_caps(struct platform_device *pdev,
		bool input, u32 *pixfmt, int len);
int sde_rotator_inline_commit(void *handle, struct sde_rotator_inline_cmd *cmd,
		enum sde_rotator_inline_cmd_type cmd_type);
int sde_rotator_inline_release(void *handle);
void sde_rotator_inline_reg_dump(struct platform_device *pdev);

#endif /* __SDE_ROTATOR_INLINE_H__ */
