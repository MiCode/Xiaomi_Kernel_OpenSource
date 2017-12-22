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

#ifndef _SDE_HW_ROT_H
#define _SDE_HW_ROT_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_blk.h"

#define SDE_HW_ROT_NAME_SIZE	80

struct sde_hw_rot;

/**
 * enum sde_hw_rot_cmd_type - type of rotator hardware command
 * @SDE_HW_ROT_CMD_VALDIATE: validate rotator command; do not commit
 * @SDE_HW_ROT_CMD_COMMIT: commit/execute rotator command
 * @SDE_HW_ROT_CMD_START: mdp is ready to start
 * @SDE_HW_ROT_CMD_CLEANUP: cleanup rotator command after it is done
 * @SDE_HW_ROT_CMD_RESET: request rotator h/w reset
 */
enum sde_hw_rot_cmd_type {
	SDE_HW_ROT_CMD_VALIDATE,
	SDE_HW_ROT_CMD_COMMIT,
	SDE_HW_ROT_CMD_START,
	SDE_HW_ROT_CMD_CLEANUP,
	SDE_HW_ROT_CMD_RESET,
};

/**
 * struct sde_hw_rot_cmd - definition of hardware rotation command
 * @master: true if client is the master in source split inline rotation
 * @sequence_id: command sequence identifier
 * @fps: frame rate of the stream in frame per second
 * @rot90: true if rotation 90 in counter clockwise is required
 * @hflip: true if horizontal flip is required prior to rotation
 * @vflip: true if vertical flip is required prior to rotation
 * @secure: true if image content is in secure domain
 * @video_mode: true if rotator is feeding into video interface
 * @clkrate : clock rate in Hz
 * @prefill_bw: prefill bandwidth in Bps (video mode only)
 * @src_iova: source i/o virtual address
 * @src_len: source i/o buffer length
 * @src_planes: source plane number
 * @src_format: pointer to source sde pixel format
 * @src_pixel_format: source pixel format in drm fourcc
 * @src_modifier: source pixel format modifier
 * @src_width: source width in pixel
 * @src_height: source height in pixel
 * @src_rect_x: source rectangle x coordinate
 * @src_rect_y: source rectangle y coordinate
 * @src_rect_w: source rectangle width
 * @src_rect_h: source rectangle height
 * @dst_writeback: true if writeback of rotated output is required
 * @dst_iova: destination i/o virtual address
 * @dst_len: destination i/o buffer length
 * @dst_planes: destination plane number
 * @dst_format: pointer to destination sde pixel format (input/output)
 * @dst_pixel_format: destination pixel format in drm fourcc (input/output)
 * @dst_modifier: destination pixel format modifier (input/output)
 * @dst_rect_x: destination rectangle x coordinate
 * @dst_rect_y: destination rectangle y coordinate
 * @dst_rect_w: destination rectangle width
 * @dst_rect_h: destination rectangle height
 * @crtc_h: sspp output height
 * @priv_handle: private handle of rotator driver (output)
 */
struct sde_hw_rot_cmd {
	bool master;
	u32 sequence_id;
	u32 fps;
	bool rot90;
	bool hflip;
	bool vflip;
	bool secure;
	bool video_mode;
	u64 clkrate;
	u64 prefill_bw;
	dma_addr_t src_iova[4];
	u32 src_len[4];
	u32 src_planes;
	const struct sde_format *src_format;
	u32 src_pixel_format;
	u64 src_modifier;
	u32 src_width;
	u32 src_height;
	u32 src_stride;
	u32 src_rect_x;
	u32 src_rect_y;
	u32 src_rect_w;
	u32 src_rect_h;
	bool dst_writeback;
	dma_addr_t dst_iova[4];
	u32 dst_len[4];
	u32 dst_planes;
	const struct sde_format *dst_format;
	u32 dst_pixel_format;
	u64 dst_modifier;
	u32 dst_rect_x;
	u32 dst_rect_y;
	u32 dst_rect_w;
	u32 dst_rect_h;
	u32 crtc_h;
	void *priv_handle;
};

/**
 * struct sde_hw_rot_ops - interface to the rotator hw driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_rot_ops {
	int (*commit)(struct sde_hw_rot *hw, struct sde_hw_rot_cmd *data,
			enum sde_hw_rot_cmd_type cmd);
	const struct sde_format_extended *(*get_format_caps)(
			struct sde_hw_rot *hw);
	const char *(*get_downscale_caps)(struct sde_hw_rot *hw);
	size_t (*get_cache_size)(struct sde_hw_rot *hw);
	int (*get_maxlinewidth)(struct sde_hw_rot *hw);
};

/**
 * struct sde_hw_rot : ROT driver object
 * @base: hw block base object
 * @hw: hardware address map
 * @idx: instance index
 * @caps: capabilities bitmask
 * @catalog: pointer to hardware catalog
 * @ops: operation table
 * @rot_ctx: pointer to private rotator context
 * @format_caps: pointer to pixel format capability  array
 * @downscale_caps: pointer to scaling capability string
 */
struct sde_hw_rot {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;
	char name[SDE_HW_ROT_NAME_SIZE];
	int idx;
	const struct sde_rot_cfg *caps;
	struct sde_mdss_cfg *catalog;
	struct sde_hw_rot_ops ops;
	void *rot_ctx;
	struct sde_format_extended *format_caps;
	char *downscale_caps;
};

/**
 * sde_hw_rot_init - initialize and return rotator hw driver object.
 * @idx:  wb_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_rot *sde_hw_rot_init(enum sde_rot idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_rot_destroy - destroy rotator hw driver object.
 * @hw_rot:  Pointer to rotator hw driver object
 */
void sde_hw_rot_destroy(struct sde_hw_rot *hw_rot);

/**
 * to_sde_hw_rot - convert base object sde_hw_base to rotator object
 * @hw: Pointer to base hardware block
 * return: Pointer to rotator hardware block
 */
static inline struct sde_hw_rot *to_sde_hw_rot(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_rot, base);
}

/**
 * sde_hw_rot_get - get next available hardware rotator, or increment reference
 *	count if hardware rotator provided
 * @hw_rot: Pointer to hardware rotator
 * return: Pointer to rotator hardware block if success; NULL otherwise
 */
struct sde_hw_rot *sde_hw_rot_get(struct sde_hw_rot *hw_rot);

/**
 * sde_hw_rot_put - put the given hardware rotator
 * @hw_rot: Pointer to hardware rotator
 * return: none
 */
void sde_hw_rot_put(struct sde_hw_rot *hw_rot);

#endif /*_SDE_HW_ROT_H */
