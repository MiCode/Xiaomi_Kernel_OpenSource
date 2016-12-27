/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_SSPP_H
#define _SDE_HW_SSPP_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_formats.h"

struct sde_hw_pipe;

/**
 * Flags
 */
#define SDE_SSPP_SECURE_OVERLAY_SESSION 0x1
#define SDE_SSPP_FLIP_LR	 0x2
#define SDE_SSPP_FLIP_UD	 0x4
#define SDE_SSPP_SOURCE_ROTATED_90 0x8
#define SDE_SSPP_ROT_90  0x10
#define SDE_SSPP_SOLID_FILL 0x20

/**
 * Define all scaler feature bits in catalog
 */
#define SDE_SSPP_SCALER ((1UL << SDE_SSPP_SCALER_RGB) | \
	(1UL << SDE_SSPP_SCALER_QSEED2) | \
	(1UL << SDE_SSPP_SCALER_QSEED3))

/**
 * Component indices
 */
enum {
	SDE_SSPP_COMP_0,
	SDE_SSPP_COMP_1_2,
	SDE_SSPP_COMP_2,
	SDE_SSPP_COMP_3,

	SDE_SSPP_COMP_MAX
};

enum {
	SDE_FRAME_LINEAR,
	SDE_FRAME_TILE_A4X,
	SDE_FRAME_TILE_A5X,
};

enum sde_hw_filter {
	SDE_SCALE_FILTER_NEAREST = 0,
	SDE_SCALE_FILTER_BIL,
	SDE_SCALE_FILTER_PCMN,
	SDE_SCALE_FILTER_CA,
	SDE_SCALE_FILTER_MAX
};

struct sde_hw_sharp_cfg {
	u32 strength;
	u32 edge_thr;
	u32 smooth_thr;
	u32 noise_thr;
};

struct sde_hw_pixel_ext {
	/* scaling factors are enabled for this input layer */
	uint8_t enable_pxl_ext;

	int init_phase_x[SDE_MAX_PLANES];
	int phase_step_x[SDE_MAX_PLANES];
	int init_phase_y[SDE_MAX_PLANES];
	int phase_step_y[SDE_MAX_PLANES];

	/*
	 * Number of pixels extension in left, right, top and bottom direction
	 * for all color components. This pixel value for each color component
	 * should be sum of fetch + repeat pixels.
	 */
	int num_ext_pxls_left[SDE_MAX_PLANES];
	int num_ext_pxls_right[SDE_MAX_PLANES];
	int num_ext_pxls_top[SDE_MAX_PLANES];
	int num_ext_pxls_btm[SDE_MAX_PLANES];

	/*
	 * Number of pixels needs to be overfetched in left, right, top and
	 * bottom directions from source image for scaling.
	 */
	int left_ftch[SDE_MAX_PLANES];
	int right_ftch[SDE_MAX_PLANES];
	int top_ftch[SDE_MAX_PLANES];
	int btm_ftch[SDE_MAX_PLANES];

	/*
	 * Number of pixels needs to be repeated in left, right, top and
	 * bottom directions for scaling.
	 */
	int left_rpt[SDE_MAX_PLANES];
	int right_rpt[SDE_MAX_PLANES];
	int top_rpt[SDE_MAX_PLANES];
	int btm_rpt[SDE_MAX_PLANES];

	uint32_t roi_w[SDE_MAX_PLANES];
	uint32_t roi_h[SDE_MAX_PLANES];

	/*
	 * Filter type to be used for scaling in horizontal and vertical
	 * directions
	 */
	enum sde_hw_filter horz_filter[SDE_MAX_PLANES];
	enum sde_hw_filter vert_filter[SDE_MAX_PLANES];

};

struct sde_hw_scaler3_cfg {
	uint32_t filter_mode;
};

/**
 * struct sde_hw_pipe_cfg : Pipe description
 * @layout:    format layout information for programming buffer to hardware
 * @src_rect:  src ROI, caller takes into account the different operations
 *             such as decimation, flip etc to program this field
 * @dest_rect: destination ROI.
 * @ horz_decimation : horizontal decimation factor( 0, 2, 4, 8, 16)
 * @ vert_decimation : vertical decimation factor( 0, 2, 4, 8, 16)
 *              2: Read 1 line/pixel drop 1 line/pixel
 *              4: Read 1 line/pixel drop 3  lines/pixels
 *              8: Read 1 line/pixel drop 7 lines/pixels
 *              16: Read 1 line/pixel drop 15 line/pixels
 */
struct sde_hw_pipe_cfg {
	struct sde_hw_fmt_layout layout;
	struct sde_rect src_rect;
	struct sde_rect dst_rect;
	u8 horz_decimation;
	u8 vert_decimation;
};

/**
 * struct danger_safe_cfg:
 * @danger_lut:
 * @safe_lut:
 */
struct danger_safe_cfg {
	u32 danger_lut;
	u32 safe_lut;
};

/**
 * struct sde_hw_sspp_ops - interface to the SSPP Hw driver functions
 * Caller must call the init function to get the pipe context for each pipe
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_sspp_ops {
	/**
	 * setup_format - setup pixel format cropping rectangle, flip
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe config structure
	 * @flags: Extra flags for format config
	 */
	void (*setup_format)(struct sde_hw_pipe *ctx,
			const struct sde_format *fmt, u32 flags);

	/**
	 * setup_rects - setup pipe ROI rectangles
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe config structure
	 * @pe_ext: Pointer to pixel ext settings
	 */
	void (*setup_rects)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cfg *cfg,
			struct sde_hw_pixel_ext *pe_ext);

	/**
	 * setup_sourceaddress - setup pipe source addresses
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe config structure
	 */
	void (*setup_sourceaddress)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cfg *cfg);

	/**
	 * setup_csc - setup color space coversion
	 * @ctx: Pointer to pipe context
	 * @data: Pointer to config structure
	 */
	void (*setup_csc)(struct sde_hw_pipe *ctx, struct sde_csc_cfg *data);

	/**
	 * setup_solidfill - enable/disable colorfill
	 * @ctx: Pointer to pipe context
	 * @const_color: Fill color value
	 * @flags: Pipe flags
	 */
	void (*setup_solidfill)(struct sde_hw_pipe *ctx, u32 color);

	/**
	 * setup_sharpening - setup sharpening
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to config structure
	 */
	void (*setup_sharpening)(struct sde_hw_pipe *ctx,
			struct sde_hw_sharp_cfg *cfg);

	/**
	 * setup_pa_memcolor - setup source color processing
	 * @ctx: Pointer to pipe context
	 * @memcolortype: Memcolor type
	 * @en: PA enable
	 */
	void (*setup_pa_memcolor)(struct sde_hw_pipe *ctx,
			u32 memcolortype, u8 en);

	/**
	 * setup_igc - setup inverse gamma correction
	 * @ctx: Pointer to pipe context
	 */
	void (*setup_igc)(struct sde_hw_pipe *ctx);

	/**
	 * setup_danger_safe - setup danger safe LUTS
	 * @ctx: Pointer to pipe context
	 * @danger_lut: Danger LUT setting
	 * @safe_lut: Safe LUT setting
	 */
	void (*setup_danger_safe)(struct sde_hw_pipe *ctx,
			u32 danger_lut,
			u32 safe_lut);

	/**
	 * setup_histogram - setup histograms
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to histogram configuration
	 */
	void (*setup_histogram)(struct sde_hw_pipe *ctx,
			void *cfg);
};

/**
 * struct sde_hw_pipe - pipe description
 * @base_off:     mdp register mapped offset
 * @blk_off:      pipe offset relative to mdss offset
 * @length        length of register block offset
 * @hwversion     mdss hw version number
 * @idx:          pipe index
 * @type :        pipe type, VIG/DMA/RGB/CURSOR, certain operations are not
 *                supported for each pipe type
 * @pipe_hw_cap:  pointer to layer_cfg
 * @highest_bank_bit:
 * @ops:          pointer to operations possible for this pipe
 */
struct sde_hw_pipe {
	/* base */
	 struct sde_hw_blk_reg_map hw;

	/* Pipe */
	enum sde_sspp idx;
	const struct sde_sspp_cfg *cap;
	u32 highest_bank_bit;

	/* Ops */
	struct sde_hw_sspp_ops ops;
};

/**
 * sde_hw_sspp_init - initializes the sspp hw driver object.
 * Should be called once before accessing every pipe.
 * @idx:  Pipe index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @catalog : Pointer to mdss catalog data
 */
struct sde_hw_pipe *sde_hw_sspp_init(enum sde_sspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *catalog);

/**
 * sde_hw_sspp_destroy(): Destroys SSPP driver context
 * should be called during Hw pipe cleanup.
 * @ctx:  Pointer to SSPP driver context returned by sde_hw_sspp_init
 */
void sde_hw_sspp_destroy(struct sde_hw_pipe *ctx);

#endif /*_SDE_HW_SSPP_H */

