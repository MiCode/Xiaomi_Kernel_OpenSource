/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_ROTATOR_UTIL_H__
#define __SDE_ROTATOR_UTIL_H__

#include <linux/types.h>
#include <linux/file.h>
#include <linux/kref.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>

#include "sde_rotator_hwio.h"
#include "sde_rotator_base.h"
#include "sde_rotator_sync.h"
#include "sde_rotator_io_util.h"
#include "sde_rotator_formats.h"

#define SDE_ROT_MAX_IMG_WIDTH		0x3FFF
#define SDE_ROT_MAX_IMG_HEIGHT		0x3FFF

#define SDEROT_DBG(fmt, ...)	pr_debug("<SDEROT_DBG> " fmt, ##__VA_ARGS__)
#define SDEROT_INFO(fmt, ...)	pr_info("<SDEROT_INFO> " fmt, ##__VA_ARGS__)
#define SDEROT_INFO_ONCE(fmt, ...)  \
	pr_info_once("<SDEROT_INFO> " fmt, ##__VA_ARGS__)
#define SDEROT_WARN(fmt, ...)	pr_warn("<SDEROT_WARN> " fmt, ##__VA_ARGS__)
#define SDEROT_ERR(fmt, ...)	pr_err("<SDEROT_ERR> " fmt, ##__VA_ARGS__)
#define SDEDEV_DBG(dev, fmt, ...)	\
	dev_dbg(dev, "<SDEROT_DBG> " fmt, ##__VA_ARGS__)
#define SDEDEV_INFO(dev, fmt, ...)	\
	dev_info(dev, "<SDEROT_INFO> " fmt, ##__VA_ARGS__)
#define SDEDEV_WARN(dev, fmt, ...)	\
	dev_warn(dev, "<SDEROT_WARN> " fmt, ##__VA_ARGS__)
#define SDEDEV_ERR(dev, fmt, ...)	\
	dev_err(dev, "<SDEROT_ERR> " fmt, ##__VA_ARGS__)

#define PHY_ADDR_4G (1ULL<<32)

struct sde_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
};

/* sde flag values */
#define SDE_ROT_NOP			0
#define SDE_FLIP_LR			0x1
#define SDE_FLIP_UD			0x2
#define SDE_ROT_90			0x4
#define SDE_ROT_180			(SDE_FLIP_UD|SDE_FLIP_LR)
#define SDE_ROT_270			(SDE_ROT_90|SDE_FLIP_UD|SDE_FLIP_LR)
#define SDE_DEINTERLACE			0x80000000
#define SDE_SOURCE_ROTATED_90		0x00100000
#define SDE_SECURE_OVERLAY_SESSION	0x00008000
#define SDE_ROT_EXT_DMA_BUF		0x00010000
#define SDE_SECURE_CAMERA_SESSION	0x00020000
#define SDE_ROT_EXT_IOVA			0x00040000

struct sde_rot_data_type;

struct sde_fb_data {
	uint32_t offset;
	struct dma_buf *buffer;
	struct ion_handle *handle;
	int memory_id;
	int id;
	uint32_t flags;
	uint32_t priv;
	dma_addr_t addr;
	u32 len;
};

struct sde_layer_plane {
	/* DMA buffer file descriptor information. */
	int fd;
	struct dma_buf *buffer;
	struct ion_handle *handle;

	/* i/o virtual address & length */
	dma_addr_t addr;
	u32 len;

	/* Pixel offset in the dma buffer. */
	uint32_t offset;

	/* Number of bytes in one scan line including padding bytes. */
	uint32_t stride;
};

struct sde_layer_buffer {
	/* layer width in pixels. */
	uint32_t width;

	/* layer height in pixels. */
	uint32_t height;

	/*
	 * layer format in DRM-style fourcc, refer drm_fourcc.h for
	 * standard formats
	 */
	uint32_t format;

	/* plane to hold the fd, offset, etc for all color components */
	struct sde_layer_plane planes[SDE_ROT_MAX_PLANES];

	/* valid planes count in layer planes list */
	uint32_t plane_count;

	/* compression ratio factor, value depends on the pixel format */
	struct sde_mult_factor comp_ratio;

	/*
	 * SyncFence associated with this buffer. It is used in two ways.
	 *
	 * 1. Driver waits to consume the buffer till producer signals in case
	 * of primary and external display.
	 *
	 * 2. Writeback device uses buffer structure for output buffer where
	 * driver is producer. However, client sends the fence with buffer to
	 * indicate that consumer is still using the buffer and it is not ready
	 * for new content.
	 */
	struct sde_rot_sync_fence *fence;

	/* indicate if this is a stream (inline) buffer */
	bool sbuf;

	/* specify the system cache id in stream buffer mode */
	int scid;

	/* indicate if system cache writeback is required */
	bool writeback;
};

struct sde_mdp_plane_sizes {
	u32 num_planes;
	u32 plane_size[SDE_ROT_MAX_PLANES];
	u32 total_size;
	u32 ystride[SDE_ROT_MAX_PLANES];
	u32 rau_cnt;
	u32 rau_h[2];
};

struct sde_mdp_img_data {
	dma_addr_t addr;
	unsigned long len;
	u32 offset;
	u32 flags;
	bool mapped;
	bool skip_detach;
	struct fd srcp_f;
	struct dma_buf *srcp_dma_buf;
	struct dma_buf_attachment *srcp_attachment;
	struct sg_table *srcp_table;
};

struct sde_mdp_data {
	u8 num_planes;
	struct sde_mdp_img_data p[SDE_ROT_MAX_PLANES];
	bool sbuf;
	int scid;
	bool writeback;
};

void sde_mdp_get_v_h_subsample_rate(u8 chroma_sample,
		u8 *v_sample, u8 *h_sample);

static inline u32 sde_mdp_general_align(u32 data, u32 alignment)
{
	return ((data + alignment - 1)/alignment) * alignment;
}

void sde_rot_data_calc_offset(struct sde_mdp_data *data, u16 x, u16 y,
	struct sde_mdp_plane_sizes *ps, struct sde_mdp_format_params *fmt);

int sde_validate_offset_for_ubwc_format(
	struct sde_mdp_format_params *fmt, u16 x, u16 y);

int sde_mdp_data_get_and_validate_size(struct sde_mdp_data *data,
	struct sde_fb_data *planes, int num_planes, u32 flags,
	struct device *dev, bool rotator, int dir,
	struct sde_layer_buffer *buffer);

int sde_mdp_get_plane_sizes(struct sde_mdp_format_params *fmt, u32 w, u32 h,
	struct sde_mdp_plane_sizes *ps, u32 bwc_mode,
	bool rotation);

int sde_mdp_data_map(struct sde_mdp_data *data, bool rotator, int dir);

int sde_mdp_data_check(struct sde_mdp_data *data,
			struct sde_mdp_plane_sizes *ps,
			struct sde_mdp_format_params *fmt);

void sde_mdp_data_free(struct sde_mdp_data *data, bool rotator, int dir);
#endif /* __SDE_ROTATOR_UTIL_H__ */
