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
#include "sde_color_processing.h"

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

/**
 * struct sde_hw_scaler3_de_cfg : QSEEDv3 detail enhancer configuration
 * @enable:         detail enhancer enable/disable
 * @sharpen_level1: sharpening strength for noise
 * @sharpen_level2: sharpening strength for signal
 * @ clip:          clip shift
 * @ limit:         limit value
 * @ thr_quiet:     quiet threshold
 * @ thr_dieout:    dieout threshold
 * @ thr_high:      low threshold
 * @ thr_high:      high threshold
 * @ prec_shift:    precision shift
 * @ adjust_a:      A-coefficients for mapping curve
 * @ adjust_b:      B-coefficients for mapping curve
 * @ adjust_c:      C-coefficients for mapping curve
 */
struct sde_hw_scaler3_de_cfg {
	u32 enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[SDE_MAX_DE_CURVES];
	int16_t adjust_b[SDE_MAX_DE_CURVES];
	int16_t adjust_c[SDE_MAX_DE_CURVES];
};

/**
 * struct sde_hw_scaler3_cfg : QSEEDv3 configuration
 * @enable:        scaler enable
 * @dir_en:        direction detection block enable
 * @ init_phase_x: horizontal initial phase
 * @ phase_step_x: horizontal phase step
 * @ init_phase_y: vertical initial phase
 * @ phase_step_y: vertical phase step
 * @ preload_x:    horizontal preload value
 * @ preload_y:    vertical preload value
 * @ src_width:    source width
 * @ src_height:   source height
 * @ dst_width:    destination width
 * @ dst_height:   destination height
 * @ y_rgb_filter_cfg: y/rgb plane filter configuration
 * @ uv_filter_cfg: uv plane filter configuration
 * @ alpha_filter_cfg: alpha filter configuration
 * @ blend_cfg:    blend coefficients configuration
 * @ lut_flag:     scaler LUT update flags
 *                 0x1 swap LUT bank
 *                 0x2 update 2D filter LUT
 *                 0x4 update y circular filter LUT
 *                 0x8 update uv circular filter LUT
 *                 0x10 update y separable filter LUT
 *                 0x20 update uv separable filter LUT
 * @ dir_lut_idx:  2D filter LUT index
 * @ y_rgb_cir_lut_idx: y circular filter LUT index
 * @ uv_cir_lut_idx: uv circular filter LUT index
 * @ y_rgb_sep_lut_idx: y circular filter LUT index
 * @ uv_sep_lut_idx: uv separable filter LUT index
 * @ dir_lut:      pointer to 2D LUT
 * @ cir_lut:      pointer to circular filter LUT
 * @ sep_lut:      pointer to separable filter LUT
 * @ de: detail enhancer configuration
 */
struct sde_hw_scaler3_cfg {
	u32 enable;
	u32 dir_en;
	int32_t init_phase_x[SDE_MAX_PLANES];
	int32_t phase_step_x[SDE_MAX_PLANES];
	int32_t init_phase_y[SDE_MAX_PLANES];
	int32_t phase_step_y[SDE_MAX_PLANES];

	u32 preload_x[SDE_MAX_PLANES];
	u32 preload_y[SDE_MAX_PLANES];
	u32 src_width[SDE_MAX_PLANES];
	u32 src_height[SDE_MAX_PLANES];

	u32 dst_width;
	u32 dst_height;

	u32 y_rgb_filter_cfg;
	u32 uv_filter_cfg;
	u32 alpha_filter_cfg;
	u32 blend_cfg;

	u32 lut_flag;
	u32 dir_lut_idx;

	u32 y_rgb_cir_lut_idx;
	u32 uv_cir_lut_idx;
	u32 y_rgb_sep_lut_idx;
	u32 uv_sep_lut_idx;
	u32 *dir_lut;
	size_t dir_len;
	u32 *cir_lut;
	size_t cir_len;
	u32 *sep_lut;
	size_t sep_len;

	/*
	 * Detail enhancer settings
	 */
	struct sde_hw_scaler3_de_cfg de;
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
 * struct sde_hw_pipe_qos_cfg : Source pipe QoS configuration
 * @danger_lut: LUT for generate danger level based on fill level
 * @safe_lut: LUT for generate safe level based on fill level
 * @creq_lut: LUT for generate creq level based on fill level
 * @creq_vblank: creq value generated to vbif during vertical blanking
 * @danger_vblank: danger value generated during vertical blanking
 * @vblank_en: enable creq_vblank and danger_vblank during vblank
 * @danger_safe_en: enable danger safe generation
 */
struct sde_hw_pipe_qos_cfg {
	u32 danger_lut;
	u32 safe_lut;
	u32 creq_lut;
	u32 creq_vblank;
	u32 danger_vblank;
	bool vblank_en;
	bool danger_safe_en;
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
	 * @scale_cfg: Pointer to scaler settings
	 */
	void (*setup_rects)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cfg *cfg,
			struct sde_hw_pixel_ext *pe_ext,
			void *scale_cfg);

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
	 * setup_pa_hue(): Setup source hue adjustment
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to hue data
	 */
	void (*setup_pa_hue)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_pa_sat(): Setup source saturation adjustment
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to saturation data
	 */
	void (*setup_pa_sat)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_pa_val(): Setup source value adjustment
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to value data
	 */
	void (*setup_pa_val)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_pa_cont(): Setup source contrast adjustment
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer contrast data
	 */
	void (*setup_pa_cont)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_pa_memcolor - setup source color processing
	 * @ctx: Pointer to pipe context
	 * @type: Memcolor type (Skin, sky or foliage)
	 * @cfg: Pointer to memory color config data
	 */
	void (*setup_pa_memcolor)(struct sde_hw_pipe *ctx,
			enum sde_memcolor_type type, void *cfg);

	/**
	 * setup_igc - setup inverse gamma correction
	 * @ctx: Pointer to pipe context
	 */
	void (*setup_igc)(struct sde_hw_pipe *ctx);

	/**
	 * setup_danger_safe_lut - setup danger safe LUTs
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe QoS configuration
	 *
	 */
	void (*setup_danger_safe_lut)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_qos_cfg *cfg);

	/**
	 * setup_creq_lut - setup CREQ LUT
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe QoS configuration
	 *
	 */
	void (*setup_creq_lut)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_qos_cfg *cfg);

	/**
	 * setup_qos_ctrl - setup QoS control
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe QoS configuration
	 *
	 */
	void (*setup_qos_ctrl)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_qos_cfg *cfg);

	/**
	 * setup_histogram - setup histograms
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to histogram configuration
	 */
	void (*setup_histogram)(struct sde_hw_pipe *ctx,
			void *cfg);

	/**
	 * setup_scaler - setup scaler
	 * @ctx: Pointer to pipe context
	 * @pipe_cfg: Pointer to pipe configuration
	 * @pe_cfg: Pointer to pixel extension configuration
	 * @scaler_cfg: Pointer to scaler configuration
	 */
	void (*setup_scaler)(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *pipe_cfg,
		struct sde_hw_pixel_ext *pe_cfg,
		void *scaler_cfg);
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

