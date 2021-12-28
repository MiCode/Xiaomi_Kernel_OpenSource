/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_DRM_H_
#define _SDE_DRM_H_

#include <drm/drm.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Total number of supported color planes */
#define SDE_MAX_PLANES  4

/* Total number of parameterized detail enhancer mapping curves */
#define SDE_MAX_DE_CURVES 3

 /* Y/RGB and UV filter configuration */
#define FILTER_EDGE_DIRECTED_2D		0x0
#define FILTER_CIRCULAR_2D		0x1
#define FILTER_SEPARABLE_1D		0x2
#define FILTER_BILINEAR			0x3

/* Alpha filters */
#define FILTER_ALPHA_DROP_REPEAT	0x0
#define FILTER_ALPHA_BILINEAR		0x1
#define FILTER_ALPHA_2D			0x3

/* Blend filters */
#define FILTER_BLEND_CIRCULAR_2D	0x0
#define FILTER_BLEND_SEPARABLE_1D	0x1

/* LUT configuration flags */
#define SCALER_LUT_SWAP			0x1
#define SCALER_LUT_DIR_WR		0x2
#define SCALER_LUT_Y_CIR_WR		0x4
#define SCALER_LUT_UV_CIR_WR		0x8
#define SCALER_LUT_Y_SEP_WR		0x10
#define SCALER_LUT_UV_SEP_WR		0x20

/**
 * DRM format modifier tokens
 *
 * @DRM_FORMAT_MOD_QCOM_DX:         Refers to a DX variant of the base format.
 *                                  Implementation may be platform and
 *                                  base-format specific.
 */
#define DRM_FORMAT_MOD_QCOM_DX	fourcc_mod_code(QCOM, 0x2)

/**
 * @DRM_FORMAT_MOD_QCOM_TIGHT:      Refers to a tightly packed variant of the
 *                                  base variant. Implementation may be
 *                                  platform and base-format specific.
 */
#define DRM_FORMAT_MOD_QCOM_TIGHT	fourcc_mod_code(QCOM, 0x4)

/**
 * @DRM_FORMAT_MOD_QCOM_TILE:       Refers to a tile variant of the base format.
 *                                  Implementation may be platform and
 *                                  base-format specific.
 */
#define DRM_FORMAT_MOD_QCOM_TILE	fourcc_mod_code(QCOM, 0x8)

/**
 * @DRM_FORMAT_MOD_QCOM_ALPHA_SWAP:	Refers to a pixel format for which
 *					its alpha ordering has been reversed.
 *					Implementation may be platform and
 *					base-format specific.
 */
#define DRM_FORMAT_MOD_QCOM_ALPHA_SWAP	fourcc_mod_code(QCOM, 0x10)

/**
 * Blend operations for "blend_op" property
 *
 * @SDE_DRM_BLEND_OP_NOT_DEFINED:   No blend operation defined for the layer.
 * @SDE_DRM_BLEND_OP_OPAQUE:        Apply a constant blend operation. The layer
 *                                  would appear opaque in case fg plane alpha
 *                                  is 0xff.
 * @SDE_DRM_BLEND_OP_PREMULTIPLIED: Apply source over blend rule. Layer already
 *                                  has alpha pre-multiplication done. If the fg
 *                                  plane alpha is less than 0xff, apply
 *                                  modulation as well. This operation is
 *                                  intended on layers having alpha channel.
 * @SDE_DRM_BLEND_OP_COVERAGE:      Apply source over blend rule. Layer is not
 *                                  alpha pre-multiplied. Apply
 *                                  pre-multiplication. If fg plane alpha is
 *                                  less than 0xff, apply modulation as well.
 * @SDE_DRM_BLEND_OP_MAX:           Used to track maximum blend operation
 *                                  possible by mdp.
 * @SDE_DRM_BLEND_OP_SKIP:          Skip staging the layer in the layer mixer.
 */
#define SDE_DRM_BLEND_OP_NOT_DEFINED    0
#define SDE_DRM_BLEND_OP_OPAQUE         1
#define SDE_DRM_BLEND_OP_PREMULTIPLIED  2
#define SDE_DRM_BLEND_OP_COVERAGE       3
#define SDE_DRM_BLEND_OP_MAX            4
#define SDE_DRM_BLEND_OP_SKIP           5

/**
 * Bit masks for "src_config" property
 * construct bitmask via (1UL << SDE_DRM_<flag>)
 */
#define SDE_DRM_DEINTERLACE         0   /* Specifies interlaced input */

/* DRM bitmasks are restricted to 0..63 */
#define SDE_DRM_BITMASK_COUNT       64

/**
 * Framebuffer modes for "fb_translation_mode" PLANE and CONNECTOR property
 *
 * @SDE_DRM_FB_NON_SEC:          IOMMU configuration for this framebuffer mode
 *                               is non-secure domain and requires
 *                               both stage I and stage II translations when
 *                               this buffer is accessed by the display HW.
 *                               This is the default mode of all frambuffers.
 * @SDE_DRM_FB_SEC:              IOMMU configuration for this framebuffer mode
 *                               is secure domain and requires
 *                               both stage I and stage II translations when
 *                               this buffer is accessed by the display HW.
 * @SDE_DRM_FB_NON_SEC_DIR_TRANS: IOMMU configuration for this framebuffer mode
 *                               is non-secure domain and requires
 *                               only stage II translation when
 *                               this buffer is accessed by the display HW.
 * @SDE_DRM_FB_SEC_DIR_TRANS:    IOMMU configuration for this framebuffer mode
 *                               is secure domain and requires
 *                               only stage II translation when
 *                               this buffer is accessed by the display HW.
 */

#define SDE_DRM_FB_NON_SEC              0
#define SDE_DRM_FB_SEC                  1
#define SDE_DRM_FB_NON_SEC_DIR_TRANS    2
#define SDE_DRM_FB_SEC_DIR_TRANS        3

/**
 * Secure levels for "security_level" CRTC property.
 *                        CRTC property which specifies what plane types
 *                        can be attached to this CRTC. Plane component
 *                        derives the plane type based on the FB_MODE.
 * @ SDE_DRM_SEC_NON_SEC: Both Secure and non-secure plane types can be
 *                        attached to this CRTC. This is the default state of
 *                        the CRTC.
 * @ SDE_DRM_SEC_ONLY:    Only secure planes can be added to this CRTC. If a
 *                        CRTC is instructed to be in this mode it follows the
 *                        platform dependent restrictions.
 */
#define SDE_DRM_SEC_NON_SEC            0
#define SDE_DRM_SEC_ONLY               1

/**
 * struct sde_drm_pix_ext_v1 - version 1 of pixel ext structure
 * @num_ext_pxls_lr: Number of total horizontal pixels
 * @num_ext_pxls_tb: Number of total vertical lines
 * @left_ftch:       Number of extra pixels to overfetch from left
 * @right_ftch:      Number of extra pixels to overfetch from right
 * @top_ftch:        Number of extra lines to overfetch from top
 * @btm_ftch:        Number of extra lines to overfetch from bottom
 * @left_rpt:        Number of extra pixels to repeat from left
 * @right_rpt:       Number of extra pixels to repeat from right
 * @top_rpt:         Number of extra lines to repeat from top
 * @btm_rpt:         Number of extra lines to repeat from bottom
 */
struct sde_drm_pix_ext_v1 {
	/*
	 * Number of pixels ext in left, right, top and bottom direction
	 * for all color components.
	 */
	__s32 num_ext_pxls_lr[SDE_MAX_PLANES];
	__s32 num_ext_pxls_tb[SDE_MAX_PLANES];

	/*
	 * Number of pixels needs to be overfetched in left, right, top
	 * and bottom directions from source image for scaling.
	 */
	__s32 left_ftch[SDE_MAX_PLANES];
	__s32 right_ftch[SDE_MAX_PLANES];
	__s32 top_ftch[SDE_MAX_PLANES];
	__s32 btm_ftch[SDE_MAX_PLANES];
	/*
	 * Number of pixels needs to be repeated in left, right, top and
	 * bottom directions for scaling.
	 */
	__s32 left_rpt[SDE_MAX_PLANES];
	__s32 right_rpt[SDE_MAX_PLANES];
	__s32 top_rpt[SDE_MAX_PLANES];
	__s32 btm_rpt[SDE_MAX_PLANES];

};

/**
 * struct sde_drm_scaler_v1 - version 1 of struct sde_drm_scaler
 * @lr:            Pixel extension settings for left/right
 * @tb:            Pixel extension settings for top/botton
 * @init_phase_x:  Initial scaler phase values for x
 * @phase_step_x:  Phase step values for x
 * @init_phase_y:  Initial scaler phase values for y
 * @phase_step_y:  Phase step values for y
 * @horz_filter:   Horizontal filter array
 * @vert_filter:   Vertical filter array
 */
struct sde_drm_scaler_v1 {
	/*
	 * Pix ext settings
	 */
	struct sde_drm_pix_ext_v1 pe;
	/*
	 * Phase settings
	 */
	__s32 init_phase_x[SDE_MAX_PLANES];
	__s32 phase_step_x[SDE_MAX_PLANES];
	__s32 init_phase_y[SDE_MAX_PLANES];
	__s32 phase_step_y[SDE_MAX_PLANES];

	/*
	 * Filter type to be used for scaling in horizontal and vertical
	 * directions
	 */
	__u32 horz_filter[SDE_MAX_PLANES];
	__u32 vert_filter[SDE_MAX_PLANES];
};

/**
 * struct sde_drm_de_v1 - version 1 of detail enhancer structure
 * @enable:         Enables/disables detail enhancer
 * @sharpen_level1: Sharpening strength for noise
 * @sharpen_level2: Sharpening strength for context
 * @clip:           Clip coefficient
 * @limit:          Detail enhancer limit factor
 * @thr_quiet:      Quite zone threshold
 * @thr_dieout:     Die-out zone threshold
 * @thr_low:        Linear zone left threshold
 * @thr_high:       Linear zone right threshold
 * @prec_shift:     Detail enhancer precision
 * @adjust_a:       Mapping curves A coefficients
 * @adjust_b:       Mapping curves B coefficients
 * @adjust_c:       Mapping curves C coefficients
 */
struct sde_drm_de_v1 {
	__u32 enable;
	__s16 sharpen_level1;
	__s16 sharpen_level2;
	__u16 clip;
	__u16 limit;
	__u16 thr_quiet;
	__u16 thr_dieout;
	__u16 thr_low;
	__u16 thr_high;
	__u16 prec_shift;
	__s16 adjust_a[SDE_MAX_DE_CURVES];
	__s16 adjust_b[SDE_MAX_DE_CURVES];
	__s16 adjust_c[SDE_MAX_DE_CURVES];
};

/*
 * Scaler configuration flags
 */

/* Disable dynamic expansion */
#define SDE_DYN_EXP_DISABLE 0x1

#define SDE_DRM_QSEED3LITE
#define SDE_DRM_QSEED4
#define SDE_DRM_INLINE_PREDOWNSCALE

/**
 * struct sde_drm_scaler_v2 - version 2 of struct sde_drm_scaler
 * @enable:            Scaler enable
 * @dir_en:            Detail enhancer enable
 * @pe:                Pixel extension settings
 * @horz_decimate:     Horizontal decimation factor
 * @vert_decimate:     Vertical decimation factor
 * @init_phase_x:      Initial scaler phase values for x
 * @phase_step_x:      Phase step values for x
 * @init_phase_y:      Initial scaler phase values for y
 * @phase_step_y:      Phase step values for y
 * @preload_x:         Horizontal preload value
 * @preload_y:         Vertical preload value
 * @src_width:         Source width
 * @src_height:        Source height
 * @dst_width:         Destination width
 * @dst_height:        Destination height
 * @y_rgb_filter_cfg:  Y/RGB plane filter configuration
 * @uv_filter_cfg:     UV plane filter configuration
 * @alpha_filter_cfg:  Alpha filter configuration
 * @blend_cfg:         Selection of blend coefficients
 * @lut_flag:          LUT configuration flags
 * @dir_lut_idx:       2d 4x4 LUT index
 * @y_rgb_cir_lut_idx: Y/RGB circular LUT index
 * @uv_cir_lut_idx:    UV circular LUT index
 * @y_rgb_sep_lut_idx: Y/RGB separable LUT index
 * @uv_sep_lut_idx:    UV separable LUT index
 * @de:                Detail enhancer settings
 * @dir_weight:        Directional Weight
 * @unsharp_mask_blend: Unsharp Blend Filter Ratio
 * @de_blend:          Ratio of two unsharp mask filters
 * @flags:             Scaler configuration flags
 * @pre_downscale_x_0  Pre-downscale ratio, x-direction, plane 0(Y/RGB)
 * @pre_downscale_x_1  Pre-downscale ratio, x-direction, plane 1(UV)
 * @pre_downscale_y_0  Pre-downscale ratio, y-direction, plane 0(Y/RGB)
 * @pre_downscale_y_1  Pre-downscale ratio, y-direction, plane 1(UV)
 */
struct sde_drm_scaler_v2 {
	/*
	 * General definitions
	 */
	__u32 enable;
	__u32 dir_en;

	/*
	 * Pix ext settings
	 */
	struct sde_drm_pix_ext_v1 pe;

	/*
	 * Decimation settings
	 */
	__u32 horz_decimate;
	__u32 vert_decimate;

	/*
	 * Phase settings
	 */
	__s32 init_phase_x[SDE_MAX_PLANES];
	__s32 phase_step_x[SDE_MAX_PLANES];
	__s32 init_phase_y[SDE_MAX_PLANES];
	__s32 phase_step_y[SDE_MAX_PLANES];

	__u32 preload_x[SDE_MAX_PLANES];
	__u32 preload_y[SDE_MAX_PLANES];
	__u32 src_width[SDE_MAX_PLANES];
	__u32 src_height[SDE_MAX_PLANES];

	__u32 dst_width;
	__u32 dst_height;

	__u32 y_rgb_filter_cfg;
	__u32 uv_filter_cfg;
	__u32 alpha_filter_cfg;
	__u32 blend_cfg;

	__u32 lut_flag;
	__u32 dir_lut_idx;

	/* for Y(RGB) and UV planes*/
	__u32 y_rgb_cir_lut_idx;
	__u32 uv_cir_lut_idx;
	__u32 y_rgb_sep_lut_idx;
	__u32 uv_sep_lut_idx;

	/*
	 * Detail enhancer settings
	 */
	struct sde_drm_de_v1 de;
	__u32 dir_weight;
	__u32 unsharp_mask_blend;
	__u32 de_blend;
	__u32 flags;

	/*
	 * Inline pre-downscale settings
	 */
	__u32 pre_downscale_x_0;
	__u32 pre_downscale_x_1;
	__u32 pre_downscale_y_0;
	__u32 pre_downscale_y_1;
};

/* Number of dest scalers supported */
#define SDE_MAX_DS_COUNT 2

/*
 * Destination scaler flag config
 */
#define SDE_DRM_DESTSCALER_ENABLE           0x1
#define SDE_DRM_DESTSCALER_SCALE_UPDATE     0x2
#define SDE_DRM_DESTSCALER_ENHANCER_UPDATE  0x4
#define SDE_DRM_DESTSCALER_PU_ENABLE        0x8

/**
 * struct sde_drm_dest_scaler_cfg - destination scaler config structure
 * @flags:      Flag to switch between mode for destination scaler
 *              refer to destination scaler flag config
 * @index:      Destination scaler selection index
 * @lm_width:   Layer mixer width configuration
 * @lm_height:  Layer mixer height configuration
 * @scaler_cfg: The scaling parameters for all the mode except disable
 *              Userspace pointer to struct sde_drm_scaler_v2
 */
struct sde_drm_dest_scaler_cfg {
	__u32 flags;
	__u32 index;
	__u32 lm_width;
	__u32 lm_height;
	__u64 scaler_cfg;
};

/**
 * struct sde_drm_dest_scaler_data - destination scaler data struct
 * @num_dest_scaler: Number of dest scalers to be configured
 * @ds_cfg:          Destination scaler block configuration
 */
struct sde_drm_dest_scaler_data {
	__u32 num_dest_scaler;
	struct sde_drm_dest_scaler_cfg ds_cfg[SDE_MAX_DS_COUNT];
};

/*
 * Define constants for struct sde_drm_csc
 */
#define SDE_CSC_MATRIX_COEFF_SIZE   9
#define SDE_CSC_CLAMP_SIZE          6
#define SDE_CSC_BIAS_SIZE           3

/**
 * struct sde_drm_csc_v1 - version 1 of struct sde_drm_csc
 * @ctm_coeff:          Matrix coefficients, in S31.32 format
 * @pre_bias:           Pre-bias array values
 * @post_bias:          Post-bias array values
 * @pre_clamp:          Pre-clamp array values
 * @post_clamp:         Post-clamp array values
 */
struct sde_drm_csc_v1 {
	__s64 ctm_coeff[SDE_CSC_MATRIX_COEFF_SIZE];
	__u32 pre_bias[SDE_CSC_BIAS_SIZE];
	__u32 post_bias[SDE_CSC_BIAS_SIZE];
	__u32 pre_clamp[SDE_CSC_CLAMP_SIZE];
	__u32 post_clamp[SDE_CSC_CLAMP_SIZE];
};

/**
 * struct sde_drm_color - struct to store the color and alpha values
 * @color_0: Color 0 value
 * @color_1: Color 1 value
 * @color_2: Color 2 value
 * @color_3: Color 3 value
 */
struct sde_drm_color {
	__u32 color_0;
	__u32 color_1;
	__u32 color_2;
	__u32 color_3;
};

/* Total number of supported dim layers */
#define SDE_MAX_DIM_LAYERS 7

/* SDE_DRM_DIM_LAYER_CONFIG_FLAG - flags for Dim Layer */
/* Color fill inside of the rect, including border */
#define SDE_DRM_DIM_LAYER_INCLUSIVE     0x1
/* Color fill outside of the rect, excluding border */
#define SDE_DRM_DIM_LAYER_EXCLUSIVE     0x2

 /* bitmask for allowed_dsc_reservation_switch property */
#define SDE_DP_DSC_RESERVATION_SWITCH (1 << 0)

/**
 * struct sde_drm_dim_layer - dim layer cfg struct
 * @flags:         Refer SDE_DRM_DIM_LAYER_CONFIG_FLAG for possible values
 * @stage:         Blending stage of the dim layer
 * @color_fill:    Color fill for dim layer
 * @rect:          Dim layer coordinates
 */
struct sde_drm_dim_layer_cfg {
	__u32 flags;
	__u32 stage;
	struct sde_drm_color color_fill;
	struct drm_clip_rect rect;
};

/**
 * struct sde_drm_dim_layer_v1 - version 1 of dim layer struct
 * @num_layers:    Numer of Dim Layers
 * @layer:         Dim layer user cfgs ptr for the num_layers
 */
struct sde_drm_dim_layer_v1 {
	__u32 num_layers;
	struct sde_drm_dim_layer_cfg layer_cfg[SDE_MAX_DIM_LAYERS];
};

/* Writeback Config version definition */
#define SDE_DRM_WB_CFG		0x1

/* SDE_DRM_WB_CONFIG_FLAGS - Writeback configuration flags */
#define SDE_DRM_WB_CFG_FLAGS_CONNECTED	(1<<0)

/**
 * struct sde_drm_wb_cfg - Writeback configuration structure
 * @flags:		see DRM_MSM_WB_CONFIG_FLAGS
 * @connector_id:	writeback connector identifier
 * @count_modes:	Count of modes in modes_ptr
 * @modes:		Pointer to struct drm_mode_modeinfo
 */
struct sde_drm_wb_cfg {
	__u32 flags;
	__u32 connector_id;
	__u32 count_modes;
	__u64 modes;
};

#define SDE_MAX_ROI_V1	4

/**
 * struct sde_drm_roi_v1 - list of regions of interest for a drm object
 * @num_rects: number of valid rectangles in the roi array
 * @roi: list of roi rectangles
 */
struct sde_drm_roi_v1 {
	__u32 num_rects;
	struct drm_clip_rect roi[SDE_MAX_ROI_V1];
};

/**
 * Define extended power modes supported by the SDE connectors.
 */
#define SDE_MODE_DPMS_ON	0
#define SDE_MODE_DPMS_LP1	1
#define SDE_MODE_DPMS_LP2	2
#define SDE_MODE_DPMS_STANDBY	3
#define SDE_MODE_DPMS_SUSPEND	4
#define SDE_MODE_DPMS_OFF	5

/**
 * sde recovery events for notifying client
 */
#define SDE_RECOVERY_SUCCESS		0
#define SDE_RECOVERY_CAPTURE		1
#define SDE_RECOVERY_HARD_RESET		2

/**
 * Define UBWC statistics config
 */
#define UBWC_STATS_MAX_ROI		0x3

/**
 * struct sde_drm_ubwc_stats_roi - region of interest for ubwc stats
 * y_coord0: first y offset from top of display
 * y_coord1: second y offset from top of display
 */
struct sde_drm_ubwc_stats_roi {
	__u16 y_coord0;
	__u16 y_coord1;
};

/**
 * struct sde_drm_ubwc_stats_data: ubwc statistics
 * roi: region of interest
 * worst_bw: worst bandwidth, per roi
 * worst_bw_y_coord: y offset (row) location of worst bandwidth, per roi
 * total_bw: total bandwidth, per roi
 * error: error status
 * meta_error: meta error data
 */
struct sde_drm_ubwc_stats_data {
	struct sde_drm_ubwc_stats_roi roi;
	__u16 worst_bw[UBWC_STATS_MAX_ROI];
	__u16 worst_bw_y_coord[UBWC_STATS_MAX_ROI];
	__u32 total_bw[UBWC_STATS_MAX_ROI];
	__u32 error;
	__u32 meta_error;
};

/**
 * Define frame data config
 */
#define SDE_FRAME_DATA_BUFFER_MAX	0x3
#define SDE_FRAME_DATA_GUARD_BYTES	0xFF
#define SDE_FRAME_DATA_MAX_PLANES	0x10

/**
 * struct sde_drm_frame_data_buffers_ctrl - control frame data buffers
 * num_buffers: number of allocated buffers
 * fds: fd list for allocated buffers
 */
struct sde_drm_frame_data_buffers_ctrl {
	__u32 num_buffers;
	__u32 fds[SDE_FRAME_DATA_BUFFER_MAX];
};

/**
 * struct sde_drm_frame_data_buf - frame data buffer info sent to userspace
 * fd: buffer fd
 * offset: offset from buffer address
 * status: status flag
 */
struct sde_drm_frame_data_buf {
	__u32 fd;
	__u32 offset;
	__u32 status;
};

/**
 * struct sde_drm_plane_frame_data - definition of plane frame data struct
 * plane_id: drm plane id
 * ubwc_stats: ubwc statistics
 */
struct sde_drm_plane_frame_data {
	__u32 plane_id;

	struct sde_drm_ubwc_stats_data ubwc_stats;
};

/**
 * struct sde_drm_frame_data_packet - definition of frame data struct
 * frame_count: interface frame count
 * commit_count: sw commit count
 * plane_frame_data: data available per plane
 */
struct sde_drm_frame_data_packet {
	__u32 frame_count;
	__u64 commit_count;

	struct sde_drm_plane_frame_data plane_frame_data[SDE_FRAME_DATA_MAX_PLANES];
};

/*
 * Colorimetry Data Block values
 * These bit nums are defined as per the CTA spec
 * and indicate the colorspaces supported by the sink
 */
#define DRM_EDID_CLRMETRY_xvYCC_601   (1 << 0)
#define DRM_EDID_CLRMETRY_xvYCC_709   (1 << 1)
#define DRM_EDID_CLRMETRY_sYCC_601    (1 << 2)
#define DRM_EDID_CLRMETRY_ADOBE_YCC_601  (1 << 3)
#define DRM_EDID_CLRMETRY_ADOBE_RGB     (1 << 4)
#define DRM_EDID_CLRMETRY_BT2020_CYCC (1 << 5)
#define DRM_EDID_CLRMETRY_BT2020_YCC  (1 << 6)
#define DRM_EDID_CLRMETRY_BT2020_RGB  (1 << 7)
#define DRM_EDID_CLRMETRY_DCI_P3      (1 << 15)

/*
 * HDR Metadata
 * These are defined as per EDID spec and shall be used by the sink
 * to set the HDR metadata for playback from userspace.
 */

#define HDR_PRIMARIES_COUNT   3

/* HDR EOTF */
#define HDR_EOTF_SDR_LUM_RANGE	0x0
#define HDR_EOTF_HDR_LUM_RANGE	0x1
#define HDR_EOTF_SMTPE_ST2084	0x2
#define HDR_EOTF_HLG		0x3

#define DRM_MSM_EXT_HDR_METADATA
#define DRM_MSM_EXT_HDR_PLUS_METADATA
struct drm_msm_ext_hdr_metadata {
	__u32 hdr_state;        /* HDR state */
	__u32 eotf;             /* electro optical transfer function */
	__u32 hdr_supported;    /* HDR supported */
	__u32 display_primaries_x[HDR_PRIMARIES_COUNT]; /* Primaries x */
	__u32 display_primaries_y[HDR_PRIMARIES_COUNT]; /* Primaries y */
	__u32 white_point_x;    /* white_point_x */
	__u32 white_point_y;    /* white_point_y */
	__u32 max_luminance;    /* Max luminance */
	__u32 min_luminance;    /* Min Luminance */
	__u32 max_content_light_level; /* max content light level */
	__u32 max_average_light_level; /* max average light level */

	__u64 hdr_plus_payload;     /* user pointer to dynamic HDR payload */
	__u32 hdr_plus_payload_size;/* size of dynamic HDR payload data */
};

/**
 * HDR sink properties
 * These are defined as per EDID spec and shall be used by the userspace
 * to determine the HDR properties to be set to the sink.
 */
#define DRM_MSM_EXT_HDR_PROPERTIES
#define DRM_MSM_EXT_HDR_PLUS_PROPERTIES
struct drm_msm_ext_hdr_properties {
	__u8 hdr_metadata_type_one;   /* static metadata type one */
	__u32 hdr_supported;          /* HDR supported */
	__u32 hdr_eotf;               /* electro optical transfer function */
	__u32 hdr_max_luminance;      /* Max luminance */
	__u32 hdr_avg_luminance;      /* Avg luminance */
	__u32 hdr_min_luminance;      /* Min Luminance */

	__u32 hdr_plus_supported;     /* HDR10+ supported */
};

/* HDR WRGB x and y index */
#define DISPLAY_PRIMARIES_WX 0
#define DISPLAY_PRIMARIES_WY 1
#define DISPLAY_PRIMARIES_RX 2
#define DISPLAY_PRIMARIES_RY 3
#define DISPLAY_PRIMARIES_GX 4
#define DISPLAY_PRIMARIES_GY 5
#define DISPLAY_PRIMARIES_BX 6
#define DISPLAY_PRIMARIES_BY 7
#define DISPLAY_PRIMARIES_MAX 8

struct drm_panel_hdr_properties {
	__u32 hdr_enabled;

	/* WRGB X and y values arrayed in format */
	/* [WX, WY, RX, RY, GX, GY, BX, BY] */
	__u32 display_primaries[DISPLAY_PRIMARIES_MAX];

	/* peak brightness supported by panel */
	__u32 peak_brightness;
	/* Blackness level supported by panel */
	__u32 blackness_level;
};

/**
 * struct drm_msm_event_req - Payload to event enable/disable ioctls.
 * @object_id: DRM object id. e.g.: for crtc pass crtc id.
 * @object_type: DRM object type. e.g.: for crtc set it to DRM_MODE_OBJECT_CRTC.
 * @event: Event for which notification is being enabled/disabled.
 *         e.g.: for Histogram set - DRM_EVENT_HISTOGRAM.
 * @client_context: Opaque pointer that will be returned during event response
 *                  notification.
 * @index: Object index(e.g.: crtc index), optional for user-space to set.
 *         Driver will override value based on object_id and object_type.
 */
struct drm_msm_event_req {
	__u32 object_id;
	__u32 object_type;
	__u32 event;
	__u64 client_context;
	__u32 index;
};

/**
 * struct drm_msm_event_resp - payload returned when read is called for
 *                            custom notifications.
 * @base: Event type and length of complete notification payload.
 * @info: Contains information about DRM that which raised this event.
 * @data: Custom payload that driver returns for event type.
 *        size of data = base.length - (sizeof(base) + sizeof(info))
 */
struct drm_msm_event_resp {
	struct drm_event base;
	struct drm_msm_event_req info;
	__u8 data[];
};

/**
 * struct drm_msm_power_ctrl: Payload to enable/disable the power vote
 * @enable: enable/disable the power vote
 * @flags:  operation control flags, for future use
 */
struct drm_msm_power_ctrl {
	__u32 enable;
	__u32 flags;
};

/**
 * struct drm_msm_early_wakeup: Payload to early wake up display
 * @wakeup_hint:  early wakeup hint.
 * @connector_id: connector id. e.g.: for connector pass connector id.
 */
struct drm_msm_early_wakeup {
	__u32 wakeup_hint;
	__u32 connector_id;
};

/**
 * struct drm_msm_display_hint: Payload for display hint
 * @hint_flags:  display hint flags.
 * @data: data struct. e.g.: for display hint parameter.
 *        Userspace pointer to struct base on hint flags.
 */
struct drm_msm_display_hint {
	__u64 data;
	__u32 hint_flags;
};

#define DRM_NOISE_LAYER_CFG
#define DRM_NOISE_TEMPORAL_FLAG (1 << 0)
#define DRM_NOISE_ATTN_MAX 255
#define DRM_NOISE_STREN_MAX 6

/**
 * struct drm_msm_noise_layer_cfg: Payload to enable/disable noise blend
 * @flags: operation control flags, for future use
 * @zposn: noise zorder
 * @zposattn: attenuation zorder
 * @attn_factor: attenuation factor in range of 1 to 255
 * @stength: strength in range of 0 to 6
 * @alpha_noise: attenuation in range of 1 to 255
*/
struct drm_msm_noise_layer_cfg {
	__u64 flags;
	__u32 zposn;
	__u32 zposattn;
	__u32 attn_factor;
	__u32 strength;
	__u32 alpha_noise;
};

#define DRM_SDE_WB_CONFIG              0x40
#define DRM_MSM_REGISTER_EVENT         0x41
#define DRM_MSM_DEREGISTER_EVENT       0x42
#define DRM_MSM_RMFB2                  0x43
#define DRM_MSM_POWER_CTRL             0x44
#define DRM_MSM_DISPLAY_HINT           0x45

/* sde custom events */
#define DRM_EVENT_HISTOGRAM 0x80000000
#define DRM_EVENT_AD_BACKLIGHT 0x80000001
#define DRM_EVENT_CRTC_POWER 0x80000002
#define DRM_EVENT_SYS_BACKLIGHT 0x80000003
#define DRM_EVENT_SDE_POWER 0x80000004
#define DRM_EVENT_IDLE_NOTIFY 0x80000005
#define DRM_EVENT_PANEL_DEAD 0x80000006 /* ESD event */
#define DRM_EVENT_SDE_HW_RECOVERY 0X80000007
#define DRM_EVENT_LTM_HIST 0X80000008
#define DRM_EVENT_LTM_WB_PB 0X80000009
#define DRM_EVENT_LTM_OFF 0X8000000A
#define DRM_EVENT_MMRM_CB 0X8000000B
#define DRM_EVENT_FRAME_DATA 0x8000000C
#define DRM_EVENT_DIMMING_BL 0X8000000D

#ifndef DRM_MODE_FLAG_VID_MODE_PANEL
#define DRM_MODE_FLAG_VID_MODE_PANEL        0x01
#endif
#ifndef DRM_MODE_FLAG_CMD_MODE_PANEL
#define DRM_MODE_FLAG_CMD_MODE_PANEL        0x02
#endif

/* display hint flags*/
#define DRM_MSM_DISPLAY_EARLY_WAKEUP_HINT         0x01
#define DRM_MSM_DISPLAY_POWER_COLLAPSE_HINT       0x02
#define DRM_MSM_DISPLAY_IDLE_TIMEOUT_HINT         0x04
#define DRM_MSM_DISPLAY_MODE_CHANGE_HINT          0x08

#define DRM_MSM_WAKE_UP_ALL_DISPLAYS        0xFFFFFFFF

#define DRM_IOCTL_SDE_WB_CONFIG \
	DRM_IOW((DRM_COMMAND_BASE + DRM_SDE_WB_CONFIG), struct sde_drm_wb_cfg)
#define DRM_IOCTL_MSM_REGISTER_EVENT   DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_REGISTER_EVENT), struct drm_msm_event_req)
#define DRM_IOCTL_MSM_DEREGISTER_EVENT DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_DEREGISTER_EVENT), struct drm_msm_event_req)
#define DRM_IOCTL_MSM_RMFB2 DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_RMFB2), unsigned int)
#define DRM_IOCTL_MSM_POWER_CTRL DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_POWER_CTRL), struct drm_msm_power_ctrl)
#define DRM_IOCTL_MSM_DISPLAY_HINT DRM_IOW((DRM_COMMAND_BASE + \
			DRM_MSM_DISPLAY_HINT), struct drm_msm_display_hint)

#if defined(__cplusplus)
}
#endif

#endif /* _SDE_DRM_H_ */
