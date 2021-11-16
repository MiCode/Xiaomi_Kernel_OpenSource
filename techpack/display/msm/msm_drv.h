/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_DRV_H__
#define __MSM_DRV_H__

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/iommu.h>
#include <linux/types.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/sde_io_util.h>
#include <linux/sde_vm_event.h>
#include <linux/sizes.h>
#include <linux/kthread.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/msm_drm.h>
#include <drm/sde_drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_dsc.h>

#include "sde_power_handle.h"

#define GET_MAJOR_REV(rev)		((rev) >> 28)
#define GET_MINOR_REV(rev)		(((rev) >> 16) & 0xFFF)
#define GET_STEP_REV(rev)		((rev) & 0xFFFF)

struct msm_kms;
struct msm_gpu;
struct msm_mmu;
struct msm_mdss;
struct msm_rd_state;
struct msm_perf_state;
struct msm_gem_submit;
struct msm_fence_context;
struct msm_fence_cb;
struct msm_gem_address_space;
struct msm_gem_vma;

#define NUM_DOMAINS    4    /* one for KMS, then one per gpu core (?) */
#define MAX_CRTCS      16
#define MAX_PLANES     20
#define MAX_ENCODERS   16
#define MAX_BRIDGES    16
#define MAX_CONNECTORS 16

#define MSM_RGB 0x0
#define MSM_YUV 0x1

#define MSM_CHROMA_444 0x0
#define MSM_CHROMA_422 0x1
#define MSM_CHROMA_420 0x2

#define TEARDOWN_DEADLOCK_RETRY_MAX 5

extern atomic_t resume_pending;
extern wait_queue_head_t resume_wait_q;

struct msm_file_private {
	rwlock_t queuelock;
	struct list_head submitqueues;

	int queueid;

	/* update the refcount when user driver calls power_ctrl IOCTL */
	unsigned short enable_refcnt;

	/* protects enable_refcnt */
	struct mutex power_lock;
};

enum msm_mdp_plane_property {
	/* blob properties, always put these first */
	PLANE_PROP_CSC_V1,
	PLANE_PROP_CSC_DMA_V1,
	PLANE_PROP_INFO,
	PLANE_PROP_SCALER_LUT_ED,
	PLANE_PROP_SCALER_LUT_CIR,
	PLANE_PROP_SCALER_LUT_SEP,
	PLANE_PROP_SKIN_COLOR,
	PLANE_PROP_SKY_COLOR,
	PLANE_PROP_FOLIAGE_COLOR,
	PLANE_PROP_VIG_GAMUT,
	PLANE_PROP_VIG_IGC,
	PLANE_PROP_DMA_IGC,
	PLANE_PROP_DMA_GC,

	/* # of blob properties */
	PLANE_PROP_BLOBCOUNT,

	/* range properties */
	PLANE_PROP_ZPOS = PLANE_PROP_BLOBCOUNT,
	PLANE_PROP_ALPHA,
	PLANE_PROP_COLOR_FILL,
	PLANE_PROP_H_DECIMATE,
	PLANE_PROP_V_DECIMATE,
	PLANE_PROP_INPUT_FENCE,
	PLANE_PROP_HUE_ADJUST,
	PLANE_PROP_SATURATION_ADJUST,
	PLANE_PROP_VALUE_ADJUST,
	PLANE_PROP_CONTRAST_ADJUST,
	PLANE_PROP_EXCL_RECT_V1,
	PLANE_PROP_PREFILL_SIZE,
	PLANE_PROP_PREFILL_TIME,
	PLANE_PROP_SCALER_V1,
	PLANE_PROP_SCALER_V2,
	PLANE_PROP_INVERSE_PMA,

	/* enum/bitmask properties */
	PLANE_PROP_BLEND_OP,
	PLANE_PROP_SRC_CONFIG,
	PLANE_PROP_FB_TRANSLATION_MODE,
	PLANE_PROP_MULTIRECT_MODE,

	/* total # of properties */
	PLANE_PROP_COUNT
};

enum msm_mdp_crtc_property {
	CRTC_PROP_INFO,
	CRTC_PROP_DEST_SCALER_LUT_ED,
	CRTC_PROP_DEST_SCALER_LUT_CIR,
	CRTC_PROP_DEST_SCALER_LUT_SEP,
	CRTC_PROP_DSPP_INFO,

	/* # of blob properties */
	CRTC_PROP_BLOBCOUNT,

	/* range properties */
	CRTC_PROP_INPUT_FENCE_TIMEOUT = CRTC_PROP_BLOBCOUNT,
	CRTC_PROP_OUTPUT_FENCE,
	CRTC_PROP_OUTPUT_FENCE_OFFSET,
	CRTC_PROP_DIM_LAYER_V1,
	CRTC_PROP_CORE_CLK,
	CRTC_PROP_CORE_AB,
	CRTC_PROP_CORE_IB,
	CRTC_PROP_LLCC_AB,
	CRTC_PROP_LLCC_IB,
	CRTC_PROP_DRAM_AB,
	CRTC_PROP_DRAM_IB,
	CRTC_PROP_ROT_PREFILL_BW,
	CRTC_PROP_ROT_CLK,
	CRTC_PROP_ROI_V1,
	CRTC_PROP_SECURITY_LEVEL,
	CRTC_PROP_IDLE_TIMEOUT,
	CRTC_PROP_DEST_SCALER,
	CRTC_PROP_CAPTURE_OUTPUT,

	CRTC_PROP_IDLE_PC_STATE,
	CRTC_PROP_CACHE_STATE,
	CRTC_PROP_VM_REQ_STATE,

	CRCT_PROP_MI_FOD_SYNC_INFO,

	/* total # of properties */
	CRTC_PROP_COUNT
};

enum msm_mdp_conn_property {
	/* blob properties, always put these first */
	CONNECTOR_PROP_SDE_INFO,
	CONNECTOR_PROP_MODE_INFO,
	CONNECTOR_PROP_HDR_INFO,
	CONNECTOR_PROP_EXT_HDR_INFO,
	CONNECTOR_PROP_PP_DITHER,
	CONNECTOR_PROP_HDR_METADATA,
	CONNECTOR_PROP_DEMURA_PANEL_ID,

	/* # of blob properties */
	CONNECTOR_PROP_BLOBCOUNT,

	/* range properties */
	CONNECTOR_PROP_OUT_FB = CONNECTOR_PROP_BLOBCOUNT,
	CONNECTOR_PROP_RETIRE_FENCE,
	CONNECTOR_PROP_DST_X,
	CONNECTOR_PROP_DST_Y,
	CONNECTOR_PROP_DST_W,
	CONNECTOR_PROP_DST_H,
	CONNECTOR_PROP_ROI_V1,
	CONNECTOR_PROP_BL_SCALE,
	CONNECTOR_PROP_SV_BL_SCALE,
	CONNECTOR_PROP_SUPPORTED_COLORSPACES,

	/* enum/bitmask properties */
	CONNECTOR_PROP_TOPOLOGY_NAME,
	CONNECTOR_PROP_TOPOLOGY_CONTROL,
	CONNECTOR_PROP_AUTOREFRESH,
	CONNECTOR_PROP_LP,
	CONNECTOR_PROP_FB_TRANSLATION_MODE,
	CONNECTOR_PROP_QSYNC_MODE,
	CONNECTOR_PROP_CMD_FRAME_TRIGGER_MODE,

	/* total # of properties */
	CONNECTOR_PROP_COUNT
};

#define MSM_GPU_MAX_RINGS 4
#define MAX_H_TILES_PER_DISPLAY 2

/**
 * enum msm_display_compression_type - compression method used for pixel stream
 * @MSM_DISPLAY_COMPRESSION_NONE:     Pixel data is not compressed
 * @MSM_DISPLAY_COMPRESSION_DSC:      DSC compresison is used
 * @MSM_DISPLAY_COMPRESSION_VDC:      VDC compresison is used
 */
enum msm_display_compression_type {
	MSM_DISPLAY_COMPRESSION_NONE,
	MSM_DISPLAY_COMPRESSION_DSC,
	MSM_DISPLAY_COMPRESSION_VDC
};

#define MSM_DISPLAY_COMPRESSION_RATIO_NONE 1
#define MSM_DISPLAY_COMPRESSION_RATIO_MAX 5

/**
 * enum msm_display_spr_pack_type - sub pixel rendering pack patterns supported
 * @MSM_DISPLAY_SPR_TYPE_NONE:	        Bypass, no special packing
 * @MSM_DISPLAY_SPR_TYPE_PENTILE:	pentile pack pattern
 * @MSM_DISPLAY_SPR_TYPE_RGBW:		RGBW pack pattern
 * @MSM_DISPLAY_SPR_TYPE_YYGM:		YYGM pack pattern
 * @MSM_DISPLAY_SPR_TYPE_YYGW:		YYGW pack patterm
 * @MSM_DISPLAY_SPR_TYPE_MAX:		max and invalid
 */
enum msm_display_spr_pack_type {
	MSM_DISPLAY_SPR_TYPE_NONE,
	MSM_DISPLAY_SPR_TYPE_PENTILE,
	MSM_DISPLAY_SPR_TYPE_RGBW,
	MSM_DISPLAY_SPR_TYPE_YYGM,
	MSM_DISPLAY_SPR_TYPE_YYGW,
	MSM_DISPLAY_SPR_TYPE_MAX
};

static const char *msm_spr_pack_type_str[MSM_DISPLAY_SPR_TYPE_MAX] = {
	[MSM_DISPLAY_SPR_TYPE_NONE] = "",
	[MSM_DISPLAY_SPR_TYPE_PENTILE] = "pentile",
	[MSM_DISPLAY_SPR_TYPE_RGBW] = "rgbw",
	[MSM_DISPLAY_SPR_TYPE_YYGM] = "yygm",
	[MSM_DISPLAY_SPR_TYPE_YYGW] = "yygw"
};

/**
 * enum msm_display_caps - features/capabilities supported by displays
 * @MSM_DISPLAY_CAP_VID_MODE:           Video or "active" mode supported
 * @MSM_DISPLAY_CAP_CMD_MODE:           Command mode supported
 * @MSM_DISPLAY_CAP_HOT_PLUG:           Hot plug detection supported
 * @MSM_DISPLAY_CAP_EDID:               EDID supported
 * @MSM_DISPLAY_ESD_ENABLED:            ESD feature enabled
 * @MSM_DISPLAY_CAP_MST_MODE:           Display with MST support
 * @MSM_DISPLAY_SPLIT_LINK:             Split Link enabled
 */
enum msm_display_caps {
	MSM_DISPLAY_CAP_VID_MODE	= BIT(0),
	MSM_DISPLAY_CAP_CMD_MODE	= BIT(1),
	MSM_DISPLAY_CAP_HOT_PLUG	= BIT(2),
	MSM_DISPLAY_CAP_EDID		= BIT(3),
	MSM_DISPLAY_ESD_ENABLED		= BIT(4),
	MSM_DISPLAY_CAP_MST_MODE	= BIT(5),
	MSM_DISPLAY_SPLIT_LINK		= BIT(6),
};

/**
 * enum panel_mode - panel operation mode
 * @MSM_DISPLAY_VIDEO_MODE: video mode panel
 * @MSM_DISPLAY_CMD_MODE:   Command mode panel
 * @MODE_MAX:
 */
enum panel_op_mode {
	MSM_DISPLAY_VIDEO_MODE = 0,
	MSM_DISPLAY_CMD_MODE,
	MSM_DISPLAY_MODE_MAX,
};

/**
 * struct msm_ratio - integer ratio
 * @numer: numerator
 * @denom: denominator
 */
struct msm_ratio {
	uint32_t numer;
	uint32_t denom;
};

/**
 * enum msm_event_wait - type of HW events to wait for
 * @MSM_ENC_COMMIT_DONE - wait for the driver to flush the registers to HW
 * @MSM_ENC_TX_COMPLETE - wait for the HW to transfer the frame to panel
 * @MSM_ENC_VBLANK - wait for the HW VBLANK event (for driver-internal waiters)
 * @MSM_ENC_ACTIVE_REGION - wait for the TG to be in active pixel region
 */
enum msm_event_wait {
	MSM_ENC_COMMIT_DONE = 0,
	MSM_ENC_TX_COMPLETE,
	MSM_ENC_VBLANK,
	MSM_ENC_ACTIVE_REGION,
};

/**
 * struct msm_roi_alignment - region of interest alignment restrictions
 * @xstart_pix_align: left x offset alignment restriction
 * @width_pix_align: width alignment restriction
 * @ystart_pix_align: top y offset alignment restriction
 * @height_pix_align: height alignment restriction
 * @min_width: minimum width restriction
 * @min_height: minimum height restriction
 */
struct msm_roi_alignment {
	uint32_t xstart_pix_align;
	uint32_t width_pix_align;
	uint32_t ystart_pix_align;
	uint32_t height_pix_align;
	uint32_t min_width;
	uint32_t min_height;
};

/**
 * struct msm_roi_caps - display's region of interest capabilities
 * @enabled: true if some region of interest is supported
 * @merge_rois: merge rois before sending to display
 * @num_roi: maximum number of rois supported
 * @align: roi alignment restrictions
 */
struct msm_roi_caps {
	bool enabled;
	bool merge_rois;
	uint32_t num_roi;
	struct msm_roi_alignment align;
};

/**
 * struct msm_display_dsc_info - defines dsc configuration
 * @config                   DSC encoder configuration
 * @scr_rev:                 DSC revision.
 * @initial_lines:           Number of initial lines stored in encoder.
 * @pkt_per_line:            Number of packets per line.
 * @bytes_in_slice:          Number of bytes in slice.
 * @eol_byte_num:            Valid bytes at the end of line.
 * @bytes_per_pkt            Number of bytes in DSI packet
 * @pclk_per_line:           Compressed width.
 * @slice_last_group_size:   Size of last group in pixels.
 * @slice_per_pkt:           Number of slices per packet.
 * @source_color_space:      Source color space of DSC encoder
 * @chroma_format:           Chroma_format of DSC encoder.
 * @det_thresh_flatness:     Flatness threshold.
 * @extra_width:             Extra width required in timing calculations.
 * @pps_delay_ms:            Post PPS command delay in milliseconds.
 * @dsc_4hsmerge_en:         Using DSC 4HS merge topology
 * @dsc_4hsmerge_padding     4HS merge DSC pair padding value in bytes
 * @dsc_4hsmerge_alignment   4HS merge DSC alignment value in bytes
 * @half_panel_pu            True for single and dual dsc encoders if partial
 *                           update sets the roi width to half of mode width
 *                           False in all other cases
 */
struct msm_display_dsc_info {
	struct drm_dsc_config config;
	u8 scr_rev;

	int initial_lines;
	int pkt_per_line;
	int bytes_in_slice;
	int bytes_per_pkt;
	int eol_byte_num;
	int pclk_per_line;
	int slice_last_group_size;
	int slice_per_pkt;
	int source_color_space;
	int chroma_format;
	int det_thresh_flatness;
	u32 extra_width;
	u32 pps_delay_ms;
	bool dsc_4hsmerge_en;
	u32 dsc_4hsmerge_padding;
	u32 dsc_4hsmerge_alignment;
	bool half_panel_pu;
};


/**
 * struct msm_display_vdc_info - defines vdc configuration
 * @version_major:              major version number of VDC encoder.
 * @version_minor:              minor version number of VDC encoder.
 * @source_color_space:         source color space of VDC encoder
 * @chroma_format:              chroma_format of VDC encoder.
 * @mppf_bpc_r_y:               MPPF bpc for R/Y color component
 * @mppf_bpc_g_cb:              MPPF bpc for G/Cb color component
 * @mppf_bpc_b_cr:              MPPF bpc for B/Cr color component
 * @mppf_bpc_y:                 MPPF bpc for Y color component
 * @mppf_bpc_co:                MPPF bpc for Co color component
 * @mppf_bpc_cg:                MPPF bpc for Cg color component
 * @flatqp_vf_fbls:             flatness qp very flat FBLs
 * @flatqp_vf_nbls:             flatness qp very flat NBLs
 * @flatqp_sw_fbls:             flatness qp somewhat flat FBLs
 * @flatqp_sw_nbls:             flatness qp somewhat flat NBLs
 * @chroma_samples:             number of chroma samples
 * @split_panel_enable:         indicates whether split panel is enabled
 * @panel_mode:                 indicates panel is in video or cmd mode
 * @traffic_mode:               indicates burst/non-burst mode
 * @flatness_qp_lut:            LUT used to determine flatness QP
 * @max_qp_lut:                 LUT used to determine maximum QP
 * @tar_del_lut:                LUT used to calculate RC target rate
 * @lbda_brate_lut:             lambda bitrate LUT for encoder
 * @lbda_bf_lut:                lambda buffer fullness lut for encoder
 * @lbda_brate_lut_interp:      interpolated lambda bitrate LUT
 * @lbda_bf_lut_interp:         interpolated lambda buffer fullness lut
 * @num_of_active_ss:           number of active soft slices
 * @bits_per_component:         number of bits per component.
 * @max_pixels_per_line:        maximum pixels per line
 * @max_pixels_per_hs_line:     maximum pixels per hs line
 * @max_lines_per_frame:        maximum lines per frame
 * @max_lines_per_slice:        maximum lines per slice
 * @chunk_size:                 chunk size for encoder
 * @chunk_size_bits:            number of bits in the chunk
 * @avg_block_bits:             average block bits
 * @per_chunk_pad_bits:         number of bits per chunk pad
 * @tot_pad_bits:               total padding bits
 * @rc_stuffing_bits:           rate control stuffing bits
 * @chunk_adj_bits:             number of adjacent bits in the chunk
 * @rc_buf_init_size_temp:      temporary rate control buffer init size
 * @init_tx_delay_temp:         initial tx delay
 * @rc_buffer_init_size:        rate control buffer init size
 * @rc_init_tx_delay:           rate control buffer init tx delay
 * @rc_init_tx_delay_px_times:  rate control buffer init tx
 *                              delay times pixels
 * @rc_buffer_max_size:         max size of rate control buffer
 * @rc_tar_rate_scale_temp_a:   rate control target rate scale parameter
 * @rc_tar_rate_scale_temp_b:	rate control target rate scale parameter
 * @rc_tar_rate_scale:          rate control target rate scale
 * @block_max_bits:             max bits in the block
 * @rc_lambda_bitrate_scale:    rate control lambda bitrate scale
 * @rc_buffer_fullness_scale:   rate control lambda fullness scale
 * @rc_fullness_offset_thresh:  rate control lambda fullness threshold
 * @ramp_blocks:                number of ramp blocks
 * @bits_per_pixel:             number of bits per pixel.
 * @num_extra_mux_bits_init:    initial value of number of extra mux bits
 * @extra_crop_bits:            number of extra crop bits
 * @num_extra_mux_bits:         value of number of extra mux bits
 * @mppf_bits_comp_0:           mppf bits in color component 0
 * @mppf_bits_comp_1:           mppf bits in color component 1
 * @mppf_bits_comp_2:           mppf bits in color component 2
 * @min_block_bits:             min number of block bits
 * @slice_height:               slice height configuration of encoder.
 * @slice_width:                slice width configuration of encoder.
 * @frame_width:                frame width configuration of encoder
 * @frame_height:               frame height configuration of encoder
 * @bytes_in_slice:             Number of bytes in slice.
 * @bytes_per_pkt:              Number of bytes in packet.
 * @eol_byte_num:               Valid bytes at the end of line.
 * @pclk_per_line:              Compressed width.
 * @slice_per_pkt:              Number of slices per packet.
 * @pkt_per_line:               Number of packets per line.
 * @min_ssm_delay:              Min Sub-stream multiplexing delay
 * @max_ssm_delay:              Max Sub-stream multiplexing delay
 * @input_ssm_out_latency:      input Sub-stream multiplexing output latency
 * @input_ssm_out_latency_min:  min input Sub-stream multiplexing output latency
 * @obuf_latency:               Output buffer latency
 * @base_hs_latency:            base hard-slice latency
 * @base_hs_latency_min:        base hard-slice min latency
 * @base_hs_latency_pixels:     base hard-slice latency pixels
 * @base_hs_latency_pixels_min: base hard-slice latency pixels(min)
 * @base_initial_lines:         base initial lines
 * @base_top_up:                base top up
 * @output_rate:                output rate
 * @output_rate_ratio_100:      output rate times 100
 * @burst_accum_pixels:         burst accumulated pixels
 * @ss_initial_lines:           soft-slice initial lines
 * @burst_initial_lines:        burst mode initial lines
 * @initial_lines:              initial lines
 * @obuf_base:                  output buffer base
 * @obuf_extra_ss0:             output buffer extra ss0
 * @obuf_extra_ss1:             output buffer extra ss1
 * @obuf_extra_burst:           output buffer extra burst
 * @obuf_ss0:                   output buffer ss0
 * @obuf_ss1:                   output buffer ss1
 * @obuf_margin_words:          output buffer margin words
 * @ob0_max_addr:               output buffer 0 max address
 * @ob1_max_addr:               output buffer 1 max address
 * @slice_width_orig:           original slice width
 * @r2b0_max_addr:              r2b0 max addr
 * @r2b1_max_addr:              r1b1 max addr
 * @slice_num_px:               number of pixels per slice
 * @rc_target_rate_threshold:   rate control target rate threshold
 * @rc_fullness_offset_slope:   rate control fullness offset slop
 * @pps_delay_ms:               Post PPS command delay in milliseconds.
 * @version_release:            release version of VDC encoder.
 * @slice_num_bits:             number of bits per slice
 * @ramp_bits:                  number of ramp bits
 */
struct msm_display_vdc_info {
	u8 version_major;
	u8 version_minor;

	u8 source_color_space;
	u8 chroma_format;
	u8 mppf_bpc_r_y;
	u8 mppf_bpc_g_cb;
	u8 mppf_bpc_b_cr;
	u8 mppf_bpc_y;
	u8 mppf_bpc_co;
	u8 mppf_bpc_cg;
	u8 flatqp_vf_fbls;
	u8 flatqp_vf_nbls;
	u8 flatqp_sw_fbls;
	u8 flatqp_sw_nbls;
	u8 chroma_samples;
	u8 split_panel_enable;
	u8 panel_mode;
	u8 traffic_mode;

	u16 flatness_qp_lut[8];
	u16 max_qp_lut[8];
	u16 tar_del_lut[16];
	u16 lbda_brate_lut[16];
	u16 lbda_bf_lut[16];
	u16 lbda_brate_lut_interp[64];
	u16 lbda_bf_lut_interp[64];

	u8 num_of_active_ss;
	u8 bits_per_component;

	u16 max_pixels_per_line;
	u16 max_pixels_per_hs_line;
	u16 max_lines_per_frame;
	u16 max_lines_per_slice;

	u16 chunk_size;
	u16 chunk_size_bits;

	u16 avg_block_bits;
	u16 per_chunk_pad_bits;
	u16 tot_pad_bits;
	u16 rc_stuffing_bits;
	u16 chunk_adj_bits;
	u16 rc_buf_init_size_temp;
	u16 init_tx_delay_temp;
	u16 rc_buffer_init_size;
	u16 rc_init_tx_delay;
	u16 rc_init_tx_delay_px_times;
	u16 rc_buffer_max_size;
	u16 rc_tar_rate_scale_temp_a;
	u16 rc_tar_rate_scale_temp_b;
	u16 rc_tar_rate_scale;
	u16 block_max_bits;
	u16 rc_lambda_bitrate_scale;
	u16 rc_buffer_fullness_scale;
	u16 rc_fullness_offset_thresh;
	u16 ramp_blocks;
	u16 bits_per_pixel;
	u16 num_extra_mux_bits_init;
	u16 extra_crop_bits;
	u16 num_extra_mux_bits;
	u16 mppf_bits_comp_0;
	u16 mppf_bits_comp_1;
	u16 mppf_bits_comp_2;
	u16 min_block_bits;

	int slice_height;
	int slice_width;
	int frame_width;
	int frame_height;

	int bytes_in_slice;
	int bytes_per_pkt;
	int eol_byte_num;
	int pclk_per_line;
	int slice_per_pkt;
	int pkt_per_line;

	int min_ssm_delay;
	int max_ssm_delay;
	int input_ssm_out_latency;
	int input_ssm_out_latency_min;
	int obuf_latency;
	int base_hs_latency;
	int base_hs_latency_min;
	int base_hs_latency_pixels;
	int base_hs_latency_pixels_min;
	int base_initial_lines;
	int base_top_up;
	int output_rate;
	int output_rate_ratio_100;
	int burst_accum_pixels;
	int ss_initial_lines;
	int burst_initial_lines;
	int initial_lines;
	int obuf_base;
	int obuf_extra_ss0;
	int obuf_extra_ss1;
	int obuf_extra_burst;
	int obuf_ss0;
	int obuf_ss1;
	int obuf_margin_words;
	int ob0_max_addr;
	int ob1_max_addr;
	int slice_width_orig;
	int r2b0_max_addr;
	int r2b1_max_addr;

	u32 slice_num_px;
	u32 rc_target_rate_threshold;
	u32 rc_fullness_offset_slope;
	u32 pps_delay_ms;
	u32 version_release;

	u64 slice_num_bits;
	u64 ramp_bits;
};

/**
 * Bits/pixel target >> 4  (removing the fractional bits)
 * returns the integer bpp value from the drm_dsc_config struct
 */
#define DSC_BPP(config) ((config).bits_per_pixel >> 4)

/**
 * struct msm_compression_info - defined panel compression
 * @comp_type:        type of compression supported
 * @comp_ratio:       compression ratio
 * @dsc_info:         dsc configuration if the compression
 *                    supported is DSC
 * @vdc_info:         vdc configuration if the compression
 *                    supported is VDC
 */
struct msm_compression_info {
	enum msm_display_compression_type comp_type;
	u32 comp_ratio;

	union{
		struct msm_display_dsc_info dsc_info;
		struct msm_display_vdc_info vdc_info;
	};
};

/**
 * struct msm_display_topology - defines a display topology pipeline
 * @num_lm:       number of layer mixers used
 * @num_enc:      number of compression encoder blocks used
 * @num_intf:     number of interfaces the panel is mounted on
 * @comp_type:    type of compression supported
 */
struct msm_display_topology {
	u32 num_lm;
	u32 num_enc;
	u32 num_intf;
	enum msm_display_compression_type comp_type;
};

/**
 * struct msm_mode_info - defines all msm custom mode info
 * @frame_rate:      frame_rate of the mode
 * @vtotal:          vtotal calculated for the mode
 * @prefill_lines:   prefill lines based on porches.
 * @jitter_numer:	display panel jitter numerator configuration
 * @jitter_denom:	display panel jitter denominator configuration
 * @clk_rate:	     DSI bit clock per lane in HZ.
 * @dfps_maxfps:     max FPS of dynamic FPS
 * @topology:        supported topology for the mode
 * @comp_info:       compression info supported
 * @roi_caps:        panel roi capabilities
 * @wide_bus_en:	wide-bus mode cfg for interface module
 * @mdp_transfer_time_us   Specifies the mdp transfer time for command mode
 *                         panels in microseconds.
 * @allowed_mode_switches: bit mask to indicate supported mode switch.
 */
struct msm_mode_info {
	uint32_t frame_rate;
	uint32_t vtotal;
	uint32_t prefill_lines;
	uint32_t jitter_numer;
	uint32_t jitter_denom;
	uint64_t clk_rate;
	uint32_t dfps_maxfps;
	struct msm_display_topology topology;
	struct msm_compression_info comp_info;
	struct msm_roi_caps roi_caps;
	bool wide_bus_en;
	u32 mdp_transfer_time_us;
	u32 allowed_mode_switches;
};

/**
 * struct msm_resource_caps_info - defines hw resources
 * @num_lm              number of layer mixers available
 * @num_dsc             number of dsc available
 * @num_vdc             number of vdc available
 * @num_ctl             number of ctl available
 * @num_3dmux           number of 3d mux available
 * @max_mixer_width:    max width supported by layer mixer
 */
struct msm_resource_caps_info {
	uint32_t num_lm;
	uint32_t num_dsc;
	uint32_t num_vdc;
	uint32_t num_ctl;
	uint32_t num_3dmux;
	uint32_t max_mixer_width;
};

/**
 * struct msm_display_info - defines display properties
 * @intf_type:          DRM_MODE_CONNECTOR_ display type
 * @capabilities:       Bitmask of display flags
 * @num_of_h_tiles:     Number of horizontal tiles in case of split interface
 * @h_tile_instance:    Controller instance used per tile. Number of elements is
 *                      based on num_of_h_tiles
 * @is_connected:       Set to true if display is connected
 * @width_mm:           Physical width
 * @height_mm:          Physical height
 * @max_width:          Max width of display. In case of hot pluggable display
 *                      this is max width supported by controller
 * @max_height:         Max height of display. In case of hot pluggable display
 *                      this is max height supported by controller
 * @clk_rate:           DSI bit clock per lane in HZ.
 * @display_type:       Enum for type of display
 * @is_te_using_watchdog_timer:  Boolean to indicate watchdog TE is
 *				 used instead of panel TE in cmd mode panels
 * @poms_align_vsync:   poms with vsync aligned
 * @roi_caps:           Region of interest capability info
 * @qsync_min_fps	Minimum fps supported by Qsync feature
 * @has_qsync_min_fps_list True if dsi-supported-qsync-min-fps-list exits
 * @te_source		vsync source pin information
 * @dsc_count:		max dsc hw blocks used by display (only available
 *			for dsi display)
 * @lm_count:		max layer mixer blocks used by display (only available
 *			for dsi display)
 */
struct msm_display_info {
	int intf_type;
	uint32_t capabilities;
	enum panel_op_mode curr_panel_mode;
	uint32_t num_of_h_tiles;
	uint32_t h_tile_instance[MAX_H_TILES_PER_DISPLAY];

	bool is_connected;

	unsigned int width_mm;
	unsigned int height_mm;

	uint32_t max_width;
	uint32_t max_height;
	uint64_t clk_rate;

	uint32_t display_type;
	bool is_te_using_watchdog_timer;
	bool poms_align_vsync;
	struct msm_roi_caps roi_caps;

	uint32_t qsync_min_fps;
	bool has_qsync_min_fps_list;

	uint32_t te_source;

	uint32_t dsc_count;
	uint32_t lm_count;
};

#define MSM_MAX_ROI	4

/**
 * struct msm_roi_list - list of regions of interest for a drm object
 * @num_rects: number of valid rectangles in the roi array
 * @roi: list of roi rectangles
 */
struct msm_roi_list {
	uint32_t num_rects;
	struct drm_clip_rect roi[MSM_MAX_ROI];
};

/**
 * struct - msm_display_kickoff_params - info for display features at kickoff
 * @rois: Regions of interest structure for mapping CRTC to Connector output
 */
struct msm_display_kickoff_params {
	struct msm_roi_list *rois;
	struct drm_msm_ext_hdr_metadata *hdr_meta;
};

/**
 * struct - msm_display_conn_params - info of dpu display features
 * @qsync_mode: Qsync mode, where 0: disabled 1: continuous mode 2: oneshot
 * @qsync_update: Qsync settings were changed/updated
 */
struct msm_display_conn_params {
	uint32_t qsync_mode;
	bool qsync_update;
};

/**
 * struct msm_drm_event - defines custom event notification struct
 * @base: base object required for event notification by DRM framework.
 * @event: event object required for event notification by DRM framework.
 */
struct msm_drm_event {
	struct drm_pending_event base;
	struct drm_msm_event_resp event;
};

/* Commit/Event thread specific structure */
struct msm_drm_thread {
	struct drm_device *dev;
	struct task_struct *thread;
	unsigned int crtc_id;
	struct kthread_worker worker;
};

struct msm_drm_private {

	struct drm_device *dev;

	struct msm_kms *kms;

	struct sde_power_handle phandle;

	/* subordinate devices, if present: */
	struct platform_device *gpu_pdev;

	/* top level MDSS wrapper device (for MDP5 only) */
	struct msm_mdss *mdss;

	/* possibly this should be in the kms component, but it is
	 * shared by both mdp4 and mdp5..
	 */
	struct hdmi *hdmi;

	/* eDP is for mdp5 only, but kms has not been created
	 * when edp_bind() and edp_init() are called. Here is the only
	 * place to keep the edp instance.
	 */
	struct msm_edp *edp;

	/* DSI is shared by mdp4 and mdp5 */
	struct msm_dsi *dsi[2];

	/* when we have more than one 'msm_gpu' these need to be an array: */
	struct msm_gpu *gpu;
	struct msm_file_private *lastctx;

	struct drm_fb_helper *fbdev;

	struct msm_rd_state *rd;       /* debugfs to dump all submits */
	struct msm_rd_state *hangrd;   /* debugfs to dump hanging submits */
	struct msm_perf_state *perf;

	/* list of GEM objects: */
	struct list_head inactive_list;

	struct workqueue_struct *wq;

	/* crtcs pending async atomic updates: */
	uint32_t pending_crtcs;
	uint32_t pending_planes;
	wait_queue_head_t pending_crtcs_event;

	unsigned int num_planes;
	struct drm_plane *planes[MAX_PLANES];

	unsigned int num_crtcs;
	struct drm_crtc *crtcs[MAX_CRTCS];

	struct msm_drm_thread disp_thread[MAX_CRTCS];
	struct msm_drm_thread event_thread[MAX_CRTCS];

	struct task_struct *pp_event_thread;
	struct kthread_worker pp_event_worker;

	unsigned int num_encoders;
	struct drm_encoder *encoders[MAX_ENCODERS];

	unsigned int num_bridges;
	struct drm_bridge *bridges[MAX_BRIDGES];

	unsigned int num_connectors;
	struct drm_connector *connectors[MAX_CONNECTORS];

	/* Properties */
	struct drm_property *plane_property[PLANE_PROP_COUNT];
	struct drm_property *crtc_property[CRTC_PROP_COUNT];
	struct drm_property *conn_property[CONNECTOR_PROP_COUNT];

	/* Color processing properties for the crtc */
	struct drm_property **cp_property;

	/* VRAM carveout, used when no IOMMU: */
	struct {
		unsigned long size;
		dma_addr_t paddr;
		/* NOTE: mm managed at the page level, size is in # of pages
		 * and position mm_node->start is in # of pages:
		 */
		struct drm_mm mm;
		spinlock_t lock; /* Protects drm_mm node allocation/removal */
	} vram;

	struct notifier_block vmap_notifier;
	struct shrinker shrinker;

	struct drm_atomic_state *pm_state;

	/* task holding struct_mutex.. currently only used in submit path
	 * to detect and reject faults from copy_from_user() for submit
	 * ioctl.
	 */
	struct task_struct *struct_mutex_task;

	/* list of clients waiting for events */
	struct list_head client_event_list;

	/* whether registered and drm_dev_unregister should be called */
	bool registered;

	/* msm drv debug root node */
	struct dentry *debug_root;

	/* update the flag when msm driver receives shutdown notification */
	bool shutdown_in_progress;

	struct mutex vm_client_lock;
	struct list_head vm_client_list;
};

/* get struct msm_kms * from drm_device * */
#define ddev_to_msm_kms(D) ((D) && (D)->dev_private ? \
		((struct msm_drm_private *)((D)->dev_private))->kms : NULL)

struct msm_format {
	uint32_t pixel_format;
};

int msm_atomic_prepare_fb(struct drm_plane *plane,
			  struct drm_plane_state *new_state);
void msm_atomic_commit_tail(struct drm_atomic_state *state);
int msm_atomic_commit(struct drm_device *dev,
	struct drm_atomic_state *state, bool nonblock);

/* callback from wq once fence has passed: */
struct msm_fence_cb {
	struct work_struct work;
	uint32_t fence;
	void (*func)(struct msm_fence_cb *cb);
};

void __msm_fence_worker(struct work_struct *work);

#define INIT_FENCE_CB(_cb, _func)  do {                     \
		INIT_WORK(&(_cb)->work, __msm_fence_worker); \
		(_cb)->func = _func;                         \
	} while (0)

struct drm_atomic_state *msm_atomic_state_alloc(struct drm_device *dev);
void msm_atomic_state_clear(struct drm_atomic_state *state);
void msm_atomic_state_free(struct drm_atomic_state *state);

int msm_gem_init_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, int npages);
void msm_gem_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		unsigned int flags);
int msm_gem_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt, int npages,
		unsigned int flags);

struct device *msm_gem_get_aspace_device(struct msm_gem_address_space *aspace);

void msm_gem_address_space_put(struct msm_gem_address_space *aspace);

struct msm_gem_address_space *
msm_gem_address_space_create(struct device *dev, struct iommu_domain *domain,
		const char *name);

/* For SDE  display */
struct msm_gem_address_space *
msm_gem_smmu_address_space_create(struct drm_device *dev, struct msm_mmu *mmu,
		const char *name);

/**
 * msm_gem_add_obj_to_aspace_active_list: adds obj to active obj list in aspace
 */
void msm_gem_add_obj_to_aspace_active_list(
		struct msm_gem_address_space *aspace,
		struct drm_gem_object *obj);

/**
 * msm_gem_remove_obj_from_aspace_active_list: removes obj from  active obj
 * list in aspace
 */
void msm_gem_remove_obj_from_aspace_active_list(
		struct msm_gem_address_space *aspace,
		struct drm_gem_object *obj);

/**
 * msm_gem_smmu_address_space_get: returns the aspace pointer for the requested
 * domain
 */
struct msm_gem_address_space *
msm_gem_smmu_address_space_get(struct drm_device *dev,
		unsigned int domain);
int msm_register_mmu(struct drm_device *dev, struct msm_mmu *mmu);
void msm_unregister_mmu(struct drm_device *dev, struct msm_mmu *mmu);

/**
 * msm_gem_aspace_domain_attach_detach: function to inform the attach/detach
 * of the domain for this aspace
 */
void msm_gem_aspace_domain_attach_detach_update(
		struct msm_gem_address_space *aspace,
		bool is_detach);

/**
 * msm_gem_address_space_register_cb: function to register callback for attach
 * and detach of the domain
 */
int msm_gem_address_space_register_cb(
		struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data);

/**
 * msm_gem_address_space_register_cb: function to unregister callback
 */
int msm_gem_address_space_unregister_cb(
		struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data);

void msm_gem_submit_free(struct msm_gem_submit *submit);
int msm_ioctl_gem_submit(struct drm_device *dev, void *data,
		struct drm_file *file);

void msm_gem_shrinker_init(struct drm_device *dev);
void msm_gem_shrinker_cleanup(struct drm_device *dev);

void msm_gem_sync(struct drm_gem_object *obj);
int msm_gem_mmap_obj(struct drm_gem_object *obj,
			struct vm_area_struct *vma);
int msm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
vm_fault_t msm_gem_fault(struct vm_fault *vmf);
uint64_t msm_gem_mmap_offset(struct drm_gem_object *obj);
int msm_gem_get_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace, uint64_t *iova);
int msm_gem_get_and_pin_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace, uint64_t *iova);
uint64_t msm_gem_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace);
void msm_gem_unpin_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace);
struct page **msm_gem_get_pages(struct drm_gem_object *obj);
void msm_gem_put_pages(struct drm_gem_object *obj);
void msm_gem_put_iova(struct drm_gem_object *obj,
		struct msm_gem_address_space *aspace);
dma_addr_t msm_gem_get_dma_addr(struct drm_gem_object *obj);
int msm_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args);
int msm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		uint32_t handle, uint64_t *offset);
struct sg_table *msm_gem_prime_get_sg_table(struct drm_gem_object *obj);
void *msm_gem_prime_vmap(struct drm_gem_object *obj);
void msm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int msm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
struct drm_gem_object *msm_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sg);
int msm_gem_prime_pin(struct drm_gem_object *obj);
void msm_gem_prime_unpin(struct drm_gem_object *obj);
struct drm_gem_object *msm_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf);
void *msm_gem_get_vaddr(struct drm_gem_object *obj);
void *msm_gem_get_vaddr_active(struct drm_gem_object *obj);
void msm_gem_put_vaddr(struct drm_gem_object *obj);
int msm_gem_madvise(struct drm_gem_object *obj, unsigned madv);
int msm_gem_cpu_prep(struct drm_gem_object *obj, uint32_t op, ktime_t *timeout);
int msm_gem_cpu_fini(struct drm_gem_object *obj);
void msm_gem_free_object(struct drm_gem_object *obj);
int msm_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		uint32_t size, uint32_t flags, uint32_t *handle, char *name);
struct drm_gem_object *msm_gem_new(struct drm_device *dev,
		uint32_t size, uint32_t flags);
struct drm_gem_object *msm_gem_new_locked(struct drm_device *dev,
		uint32_t size, uint32_t flags);
void *msm_gem_kernel_new(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova);
void *msm_gem_kernel_new_locked(struct drm_device *dev, uint32_t size,
		uint32_t flags, struct msm_gem_address_space *aspace,
		struct drm_gem_object **bo, uint64_t *iova);
struct drm_gem_object *msm_gem_import(struct drm_device *dev,
		struct dma_buf *dmabuf, struct sg_table *sgt);

__printf(2, 3)
void msm_gem_object_set_name(struct drm_gem_object *bo, const char *fmt, ...);

int msm_gem_delayed_import(struct drm_gem_object *obj);

void msm_framebuffer_set_keepattrs(struct drm_framebuffer *fb, bool enable);
int msm_framebuffer_prepare(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace);
void msm_framebuffer_cleanup(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace);
uint32_t msm_framebuffer_iova(struct drm_framebuffer *fb,
		struct msm_gem_address_space *aspace, int plane);
uint32_t msm_framebuffer_phys(struct drm_framebuffer *fb, int plane);
struct drm_gem_object *msm_framebuffer_bo(struct drm_framebuffer *fb, int plane);
const struct msm_format *msm_framebuffer_format(struct drm_framebuffer *fb);
struct drm_framebuffer *msm_framebuffer_init(struct drm_device *dev,
		const struct drm_mode_fb_cmd2 *mode_cmd,
		struct drm_gem_object **bos);
struct drm_framebuffer *msm_framebuffer_create(struct drm_device *dev,
		struct drm_file *file, const struct drm_mode_fb_cmd2 *mode_cmd);
struct drm_framebuffer * msm_alloc_stolen_fb(struct drm_device *dev,
		int w, int h, int p, uint32_t format);

struct drm_fb_helper *msm_fbdev_init(struct drm_device *dev);
void msm_fbdev_free(struct drm_device *dev);

struct hdmi;
#if IS_ENABLED(CONFIG_DRM_MSM_HDMI)
int msm_hdmi_modeset_init(struct hdmi *hdmi, struct drm_device *dev,
		struct drm_encoder *encoder);
void __init msm_hdmi_register(void);
void __exit msm_hdmi_unregister(void);
#else
static inline void __init msm_hdmi_register(void)
{
}
static inline void __exit msm_hdmi_unregister(void)
{
}
#endif /* CONFIG_DRM_MSM_HDMI */

struct msm_edp;
#if IS_ENABLED(CONFIG_DRM_MSM_EDP)
void __init msm_edp_register(void);
void __exit msm_edp_unregister(void);
int msm_edp_modeset_init(struct msm_edp *edp, struct drm_device *dev,
		struct drm_encoder *encoder);
#else
static inline void __init msm_edp_register(void)
{
}
static inline void __exit msm_edp_unregister(void)
{
}

static inline int msm_edp_modeset_init(struct msm_edp *edp,
		struct drm_device *dev, struct drm_encoder *encoder)
{
	return -EINVAL;
}
#endif /* CONFIG_DRM_MSM_EDP */

struct msm_dsi;

/* *
 * msm_mode_object_event_notify - notify user-space clients of drm object
 *                                events.
 * @obj: mode object (crtc/connector) that is generating the event.
 * @event: event that needs to be notified.
 * @payload: payload for the event.
 */
void msm_mode_object_event_notify(struct drm_mode_object *obj,
		struct drm_device *dev, struct drm_event *event, u8 *payload);
#if IS_ENABLED(CONFIG_DRM_MSM_DSI)
static inline void __init msm_dsi_register(void)
{
}
static inline void __exit msm_dsi_unregister(void)
{
}
static inline int msm_dsi_modeset_init(struct msm_dsi *msm_dsi,
				       struct drm_device *dev,
				       struct drm_encoder *encoder)
{
	return -EINVAL;
}
#else
void __init msm_dsi_register(void);
void __exit msm_dsi_unregister(void);
int msm_dsi_modeset_init(struct msm_dsi *msm_dsi, struct drm_device *dev,
			 struct drm_encoder *encoder);
#endif /* CONFIG_DRM_MSM_DSI */

#if IS_ENABLED(CONFIG_DRM_MSM_MDP5)
void __init msm_mdp_register(void);
void __exit msm_mdp_unregister(void);
#else
static inline void __init msm_mdp_register(void)
{
}
static inline void __exit msm_mdp_unregister(void)
{
}
#endif /* CONFIG_DRM_MSM_MDP5 */

#ifdef CONFIG_DEBUG_FS
void msm_gem_describe(struct drm_gem_object *obj, struct seq_file *m);
void msm_gem_describe_objects(struct list_head *list, struct seq_file *m);
void msm_framebuffer_describe(struct drm_framebuffer *fb, struct seq_file *m);
int msm_debugfs_late_init(struct drm_device *dev);
int msm_rd_debugfs_init(struct drm_minor *minor);
void msm_rd_debugfs_cleanup(struct msm_drm_private *priv);
__printf(3, 4)
void msm_rd_dump_submit(struct msm_rd_state *rd, struct msm_gem_submit *submit,
		const char *fmt, ...);
int msm_perf_debugfs_init(struct drm_minor *minor);
void msm_perf_debugfs_cleanup(struct msm_drm_private *priv);
#else
static inline int msm_debugfs_late_init(struct drm_device *dev) { return 0; }
__printf(3, 4)
static inline void msm_rd_dump_submit(struct msm_rd_state *rd, struct msm_gem_submit *submit,
		const char *fmt, ...) {}
static inline void msm_rd_debugfs_cleanup(struct msm_drm_private *priv) {}
static inline void msm_perf_debugfs_cleanup(struct msm_drm_private *priv) {}
#endif

#if IS_ENABLED(CONFIG_DRM_MSM_DSI)
void __init dsi_display_register(void);
void __exit dsi_display_unregister(void);
#else
static inline void __init dsi_display_register(void)
{
}
static inline void __exit dsi_display_unregister(void)
{
}
#endif /* CONFIG_DRM_MSM_DSI */

#if IS_ENABLED(CONFIG_HDCP_QSEECOM)
void __init msm_hdcp_register(void);
void __exit msm_hdcp_unregister(void);
#else
static inline void __init msm_hdcp_register(void)
{
}
static inline void __exit msm_hdcp_unregister(void)
{
}
#endif /* CONFIG_HDCP_QSEECOM */

#if IS_ENABLED(CONFIG_DRM_MSM_DP)
void __init dp_display_register(void);
void __exit dp_display_unregister(void);
#else
static inline void __init dp_display_register(void)
{
}
static inline void __exit dp_display_unregister(void)
{
}
#endif /* CONFIG_DRM_MSM_DP */

#if IS_ENABLED(CONFIG_DRM_SDE_RSC)
void __init sde_rsc_register(void);
void __exit sde_rsc_unregister(void);
void __init sde_rsc_rpmh_register(void);
#else
static inline void __init sde_rsc_register(void)
{
}
static inline void __exit sde_rsc_unregister(void)
{
}
static inline void __init sde_rsc_rpmh_register(void)
{
}
#endif /* CONFIG_DRM_SDE_RSC */

#if IS_ENABLED(CONFIG_DRM_SDE_WB)
void __init sde_wb_register(void);
void __exit sde_wb_unregister(void);
#else
static inline void __init sde_wb_register(void)
{
}
static inline void __exit sde_wb_unregister(void)
{
}
#endif /* CONFIG_DRM_SDE_WB */

#if IS_ENABLED(CONFIG_MSM_SDE_ROTATOR)
void sde_rotator_register(void);
void sde_rotator_unregister(void);
#else
static inline void sde_rotator_register(void)
{
}
static inline void sde_rotator_unregister(void)
{
}
#endif /* CONFIG_MSM_SDE_ROTATOR */

#if IS_ENABLED(CONFIG_MSM_SDE_ROTATOR)
void sde_rotator_smmu_driver_register(void);
void sde_rotator_smmu_driver_unregister(void);
#else
static inline void sde_rotator_smmu_driver_register(void)
{
}
static inline void sde_rotator_smmu_driver_unregister(void)
{
}
#endif /* CONFIG_MSM_SDE_ROTATOR */

struct clk *msm_clk_get(struct platform_device *pdev, const char *name);
int msm_clk_bulk_get(struct device *dev, struct clk_bulk_data **bulk);

struct clk *msm_clk_bulk_get_clock(struct clk_bulk_data *bulk, int count,
	const char *name);
void __iomem *msm_ioremap(struct platform_device *pdev, const char *name,
		const char *dbgname);
unsigned long msm_iomap_size(struct platform_device *pdev, const char *name);
void msm_iounmap(struct platform_device *dev, void __iomem *addr);
void msm_writel(u32 data, void __iomem *addr);
u32 msm_readl(const void __iomem *addr);

#define DBG(fmt, ...) DRM_DEBUG_DRIVER(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt, ...) if (0) DRM_DEBUG_DRIVER(fmt"\n", ##__VA_ARGS__)

static inline int align_pitch(int width, int bpp)
{
	int bytespp = (bpp + 7) / 8;
	/* adreno needs pitch aligned to 32 pixels: */
	return bytespp * ALIGN(width, 32);
}

/* for the generated headers: */
#define INVALID_IDX(idx) ({BUG(); 0;})
#define fui(x)                ({BUG(); 0;})
#define util_float_to_half(x) ({BUG(); 0;})


#define FIELD(val, name) (((val) & name ## __MASK) >> name ## __SHIFT)

/* for conditionally setting boolean flag(s): */
#define COND(bool, val) ((bool) ? (val) : 0)

static inline unsigned long timeout_to_jiffies(const ktime_t *timeout)
{
	ktime_t now = ktime_get();
	unsigned long remaining_jiffies;

	if (ktime_compare(*timeout, now) < 0) {
		remaining_jiffies = 0;
	} else {
		ktime_t rem = ktime_sub(*timeout, now);
		struct timespec ts = ktime_to_timespec(rem);
		remaining_jiffies = timespec_to_jiffies(&ts);
	}

	return remaining_jiffies;
}

int msm_get_mixer_count(struct msm_drm_private *priv,
		const struct drm_display_mode *mode,
		const struct msm_resource_caps_info *res, u32 *num_lm);

int msm_get_dsc_count(struct msm_drm_private *priv,
		u32 hdisplay, u32 *num_dsc);

int msm_get_src_bpc(int chroma_format, int bpc);

#endif /* __MSM_DRV_H__ */
