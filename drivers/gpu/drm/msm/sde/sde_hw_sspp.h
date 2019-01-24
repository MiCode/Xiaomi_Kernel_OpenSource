/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_SSPP_H
#define _SDE_HW_SSPP_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_reg_dma.h"
#include "sde_hw_blk.h"
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
#define SDE_SSPP_RIGHT	 0x40

/**
 * Define all scaler feature bits in catalog
 */
#define SDE_SSPP_SCALER ((1UL << SDE_SSPP_SCALER_RGB) | \
	(1UL << SDE_SSPP_SCALER_QSEED2) | \
	(1UL << SDE_SSPP_SCALER_QSEED3) | \
	(1UL << SDE_SSPP_SCALER_QSEED3LITE))

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

/**
 * SDE_SSPP_RECT_SOLO - multirect disabled
 * SDE_SSPP_RECT_0 - rect0 of a multirect pipe
 * SDE_SSPP_RECT_1 - rect1 of a multirect pipe
 * SDE_SSPP_RECT_MAX - max enum of multirect pipe
 *
 * Note: HW supports multirect with either RECT0 or
 * RECT1. Considering no benefit of such configs over
 * SOLO mode and to keep the plane management simple,
 * we dont support single rect multirect configs.
 */
enum sde_sspp_multirect_index {
	SDE_SSPP_RECT_SOLO = 0,
	SDE_SSPP_RECT_0,
	SDE_SSPP_RECT_1,
	SDE_SSPP_RECT_MAX,
};

enum sde_sspp_multirect_mode {
	SDE_SSPP_MULTIRECT_NONE = 0,
	SDE_SSPP_MULTIRECT_PARALLEL,
	SDE_SSPP_MULTIRECT_TIME_MX,
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

enum sde_hw_filter_alpa {
	SDE_SCALE_ALPHA_PIXEL_REP,
	SDE_SCALE_ALPHA_BIL
};

enum sde_hw_filter_yuv {
	SDE_SCALE_2D_4X4,
	SDE_SCALE_2D_CIR,
	SDE_SCALE_1D_SEP,
	SDE_SCALE_BIL
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
 * @index:     index of the rectangle of SSPP
 * @mode:      parallel or time multiplex multirect mode
 */
struct sde_hw_pipe_cfg {
	struct sde_hw_fmt_layout layout;
	struct sde_rect src_rect;
	struct sde_rect dst_rect;
	u8 horz_decimation;
	u8 vert_decimation;
	enum sde_sspp_multirect_index index;
	enum sde_sspp_multirect_mode mode;
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
	u64 creq_lut;
	u32 creq_vblank;
	u32 danger_vblank;
	bool vblank_en;
	bool danger_safe_en;
};

/**
 * enum CDP preload ahead address size
 */
enum {
	SDE_SSPP_CDP_PRELOAD_AHEAD_32,
	SDE_SSPP_CDP_PRELOAD_AHEAD_64
};

/**
 * struct sde_hw_pipe_cdp_cfg : CDP configuration
 * @enable: true to enable CDP
 * @ubwc_meta_enable: true to enable ubwc metadata preload
 * @tile_amortize_enable: true to enable amortization control for tile format
 * @preload_ahead: number of request to preload ahead
 *	SDE_SSPP_CDP_PRELOAD_AHEAD_32,
 *	SDE_SSPP_CDP_PRELOAD_AHEAD_64
 */
struct sde_hw_pipe_cdp_cfg {
	bool enable;
	bool ubwc_meta_enable;
	bool tile_amortize_enable;
	u32 preload_ahead;
};

/**
 * enum system cache rotation operation mode
 */
enum {
	SDE_PIPE_SC_OP_MODE_OFFLINE,
	SDE_PIPE_SC_OP_MODE_INLINE_SINGLE,
	SDE_PIPE_SC_OP_MODE_INLINE_LEFT,
	SDE_PIPE_SC_OP_MODE_INLINE_RIGHT,
};

/**
 * enum system cache read operation type
 */
enum {
	SDE_PIPE_SC_RD_OP_TYPE_CACHEABLE,
	SDE_PIPE_SC_RD_OP_TYPE_INVALIDATE,
	SDE_PIPE_SC_RD_OP_TYPE_EVICTION,
};

/**
 * struct sde_hw_pipe_sc_cfg - system cache configuration
 * @op_mode: rotation operating mode
 * @rd_en: system cache read enable
 * @rd_scid: system cache read block id
 * @rd_noallocate: system cache read no allocate attribute
 * @rd_op_type: system cache read operation type
 * @flags: dirty flags to change the configuration
 */
struct sde_hw_pipe_sc_cfg {
	u32 op_mode;
	bool rd_en;
	u32 rd_scid;
	bool rd_noallocate;
	u32 rd_op_type;
	u32 flags;
};

/**
 * struct sde_hw_pipe_uidle_cfg - uidle configuration
 * @enable: disables uidle
 * @fal_allowed_threshold: minimum fl to allow uidle
 * @fal10_exit_threshold: number of lines to indicate fal_10_exit is okay
 * @fal10_threshold: number of lines where fal_10_is okay
 * @fal1_threshold: number of lines where fal_1 is okay
 */
struct sde_hw_pipe_uidle_cfg {
	u32 enable;
	u32 fal_allowed_threshold;
	u32 fal10_exit_threshold;
	u32 fal10_threshold;
	u32 fal1_threshold;
};

/**
 * struct sde_hw_pipe_ts_cfg - traffic shaper configuration
 * @size: size to prefill in bytes, or zero to disable
 * @time: time to prefill in usec, or zero to disable
 */
struct sde_hw_pipe_ts_cfg {
	u64 size;
	u64 time;
};

/**
 * Maximum number of stream buffer plane
 */
#define SDE_PIPE_SBUF_PLANE_NUM	2

/**
 * struct sde_hw_sspp_ops - interface to the SSPP Hw driver functions
 * Caller must call the init function to get the pipe context for each pipe
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_sspp_ops {
	/**
	 * setup_format - setup pixel format cropping rectangle, flip
	 * @ctx: Pointer to pipe context
	 * @fmt: Pointer to sde_format structure
	 * @blend_enabled: flag indicating blend enabled or disabled on plane
	 * @flags: Extra flags for format config
	 * @index: rectangle index in multirect
	 */
	void (*setup_format)(struct sde_hw_pipe *ctx,
			const struct sde_format *fmt,
			bool blend_enabled, u32 flags,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_rects - setup pipe ROI rectangles
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe config structure
	 * @index: rectangle index in multirect
	 */
	void (*setup_rects)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cfg *cfg,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_pe - setup pipe pixel extension
	 * @ctx: Pointer to pipe context
	 * @pe_ext: Pointer to pixel ext settings
	 */
	void (*setup_pe)(struct sde_hw_pipe *ctx,
			struct sde_hw_pixel_ext *pe_ext);

	/**
	 * setup_excl_rect - setup pipe exclusion rectangle
	 * @ctx: Pointer to pipe context
	 * @excl_rect: Pointer to exclclusion rect structure
	 * @index: rectangle index in multirect
	 */
	void (*setup_excl_rect)(struct sde_hw_pipe *ctx,
			struct sde_rect *excl_rect,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_sourceaddress - setup pipe source addresses
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe config structure
	 * @index: rectangle index in multirect
	 */
	void (*setup_sourceaddress)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cfg *cfg,
			enum sde_sspp_multirect_index index);

	/* get_sourceaddress - get pipe current source addresses of a plane
	 * @ctx: Pointer to pipe context
	 * @is_virtual: If true get address programmed for R1 in multirect
	 */
	u32 (*get_sourceaddress)(struct sde_hw_pipe *ctx, bool is_virtual);

	/**
	 * setup_csc - setup color space conversion
	 * @ctx: Pointer to pipe context
	 * @data: Pointer to config structure
	 */
	void (*setup_csc)(struct sde_hw_pipe *ctx, struct sde_csc_cfg *data);

	/**
	 * setup_solidfill - enable/disable colorfill
	 * @ctx: Pointer to pipe context
	 * @const_color: Fill color value
	 * @flags: Pipe flags
	 * @index: rectangle index in multirect
	 */
	void (*setup_solidfill)(struct sde_hw_pipe *ctx, u32 color,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_multirect - setup multirect configuration
	 * @ctx: Pointer to pipe context
	 * @index: rectangle index in multirect
	 * @mode: parallel fetch / time multiplex multirect mode
	 */

	void (*setup_multirect)(struct sde_hw_pipe *ctx,
			enum sde_sspp_multirect_index index,
			enum sde_sspp_multirect_mode mode);

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
	 * setup_vig_gamut - setup 3D LUT Gamut in VIG pipes
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to vig gamut data
	 */
	void (*setup_vig_gamut)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_vig_igc - setup 1D LUT IGC in VIG pipes
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to vig igc data
	 */
	void (*setup_vig_igc)(struct sde_hw_pipe *ctx, void *cfg);

	/**
	 * setup_dma_igc - setup 1D LUT IGC in DMA pipes
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to dma igc data
	 * @idx: multirect index
	 */
	void (*setup_dma_igc)(struct sde_hw_pipe *ctx, void *cfg,
				enum sde_sspp_multirect_index idx);

	/**
	 * setup_dma_gc - setup 1D LUT GC in DMA pipes
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to dma gc data
	 * @idx: multirect index
	 */
	void (*setup_dma_gc)(struct sde_hw_pipe *ctx, void *cfg,
				enum sde_sspp_multirect_index idx);

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

	/**
	 * setup_scaler_lut - setup scaler lut
	 * @buf: Defines structure for reg dma ops on the reg dma buffer.
	 * @scaler3_cfg: QSEEDv3 configuration
	 * @offset: Scaler Offset
	 */
	void (*setup_scaler_lut)(struct sde_reg_dma_setup_ops_cfg *buf,
			struct sde_hw_scaler3_cfg *scaler3_cfg,
			u32 offset);

	/**
	 * get_scaler_ver - get scaler h/w version
	 * @ctx: Pointer to pipe context
	 */
	u32 (*get_scaler_ver)(struct sde_hw_pipe *ctx);

	/**
	 * setup_sys_cache - setup system cache configuration
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to system cache configuration
	 */
	void (*setup_sys_cache)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_sc_cfg *cfg);

	 /**
	  * setup_uidle - set uidle configuration
	  * @ctx: Pointer to pipe context
	  * @cfg: Pointer to uidle configuration
	  * @index: rectangle index in multirect
	  */
	 void (*setup_uidle)(struct sde_hw_pipe *ctx,
			 struct sde_hw_pipe_uidle_cfg *cfg,
			 enum sde_sspp_multirect_index index);

	/**
	 * setup_ts_prefill - setup prefill traffic shaper
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to traffic shaper configuration
	 * @index: rectangle index in multirect
	 */
	void (*setup_ts_prefill)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_ts_cfg *cfg,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_cdp - setup client driven prefetch
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to cdp configuration
	 * @index: rectangle index in multirect
	 */
	void (*setup_cdp)(struct sde_hw_pipe *ctx,
			struct sde_hw_pipe_cdp_cfg *cfg,
			enum sde_sspp_multirect_index index);

	/**
	 * setup_secure_address - setup secureity status of the source address
	 * @ctx: Pointer to pipe context
	 * @index: rectangle index in multirect
	 * @enable: enable content protected buffer state
	 */
	void (*setup_secure_address)(struct sde_hw_pipe *ctx,
			enum sde_sspp_multirect_index index,
		bool enable);

	/**
	 * set_src_split_order - setup source split order priority
	 * @ctx: Pointer to pipe context
	 * @index: rectangle index in multirect
	 * @enable: enable src split order
	 */
	void (*set_src_split_order)(struct sde_hw_pipe *ctx,
			enum sde_sspp_multirect_index index, bool enable);

	/**
	 * setup_inverse_pma - enable/disable alpha unmultiply unit (PMA)
	 * @ctx: Pointer to pipe context
	 * @index: Rectangle index in multirect
	 * @enable: PMA enable/disable settings
	 */
	void (*setup_inverse_pma)(struct sde_hw_pipe *ctx,
			enum sde_sspp_multirect_index index, u32 enable);

	/**
	 * setup_dgm_csc - setup DGM color space conversion block and update lut
	 * @ctx: Pointer to pipe context
	 * @index: Rectangle index in multirect
	 * @data: Pointer to config structure
	 */
	void (*setup_dgm_csc)(struct sde_hw_pipe *ctx,
		enum sde_sspp_multirect_index index, struct sde_csc_cfg *data);

	/**
	 * clear_ubwc_error - clear the ubwc error-code registers
	 * @ctx: Pointer to pipe context
	 */
	void (*clear_ubwc_error)(struct sde_hw_pipe *ctx);

	/**
	 * get_ubwc_error - get the ubwc error-code
	 * @ctx: Pointer to pipe context
	 */
	u32 (*get_ubwc_error)(struct sde_hw_pipe *ctx);
};

/**
 * struct sde_hw_pipe - pipe description
 * @base: hardware block base structure
 * @hw: block hardware details
 * @catalog: back pointer to catalog
 * @mdp: pointer to associated mdp portion of the catalog
 * @idx: pipe index
 * @cap: pointer to layer_cfg
 * @ops: pointer to operations possible for this pipe
 */
struct sde_hw_pipe {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;
	struct sde_mdss_cfg *catalog;
	struct sde_mdp_cfg *mdp;

	/* Pipe */
	enum sde_sspp idx;
	struct sde_sspp_cfg *cap;

	/* Ops */
	struct sde_hw_sspp_ops ops;
	struct sde_hw_ctl *ctl;
};

/**
 * sde_hw_pipe - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_pipe *to_sde_hw_pipe(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_pipe, base);
}

/**
 * sde_hw_sspp_init - initializes the sspp hw driver object.
 * Should be called once before accessing every pipe.
 * @idx:  Pipe index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @catalog : Pointer to mdss catalog data
 * @is_virtual_pipe: is this pipe virtual pipe
 */
struct sde_hw_pipe *sde_hw_sspp_init(enum sde_sspp idx,
		void __iomem *addr, struct sde_mdss_cfg *catalog,
		bool is_virtual_pipe);

/**
 * sde_hw_sspp_destroy(): Destroys SSPP driver context
 * should be called during Hw pipe cleanup.
 * @ctx:  Pointer to SSPP driver context returned by sde_hw_sspp_init
 */
void sde_hw_sspp_destroy(struct sde_hw_pipe *ctx);

#endif /*_SDE_HW_SSPP_H */

