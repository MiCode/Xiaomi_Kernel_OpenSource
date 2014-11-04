/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#ifndef _SH_CSS_INTERNAL_H_
#define _SH_CSS_INTERNAL_H_

#include <stdint.h>
#include "input_formatter.h"
#include "input_system.h"

#include "sh_css_types.h"
#include "sh_css_binary.h"
#include "sh_css_firmware.h"
/* #include "sh_css_debug.h"*/

#include "dma.h"	/* N_DMA_CHANNEL_ID */

#define sh_css_print(fmt, s...) \
	do { \
		if (sh_css_printf) { \
			sh_css_printf(fmt, ## s); \
	   } \
	} while (0)

#define SH_CSS_MAX_BINARY_NAME	32

/* this is an internal timeout value for CSS
   it should apply to all busy-wait inside CSS system */
#define CSS_TIMEOUT_US 200000

#define SP_DEBUG_NONE	(0)
#define SP_DEBUG_DUMP	(1)
#define SP_DEBUG_COPY	(2)
#define SP_DEBUG_TRACE	(3)
#define SP_DEBUG_STALL	(4)

#define SP_DEBUG SP_DEBUG_NONE

#define SH_CSS_MAX_SP_THREADS	4 /* raw_copy, preview, capture, acceleration */

#define NUM_REF_FRAMES		2

/* Determines MAX_CB_ELEMS_FOR_TAGGER in tagger.sp.c */
/*
#if defined(HAS_SP_2400A0)
#define NUM_CONTINUOUS_FRAMES	5
#else
#define NUM_CONTINUOUS_FRAMES	10
#endif
*/

/* The maximum depth of the frame buffer queue */
#define NUM_CONTINUOUS_FRAMES	15

#define NUM_OFFLINE_INIT_CONTINUOUS_FRAMES	3
#define NUM_ONLINE_INIT_CONTINUOUS_FRAMES	2

#define NUM_TNR_FRAMES		2

#define NUM_VIDEO_REF_FRAMES	2
#define NUM_VIDEO_TNR_FRAMES	2
#define NR_OF_PIPELINES		5 /* Must match with SH_CSS_NR_OF_PIPELINES */

#define SH_CSS_MAX_STAGES 6 /* copy, preisp, anr, postisp, capture_pp, vf_pp */

/* ISP parameter versions
 * If ISP_PIPE_VERSION is defined as 2 in isp_defs_for_hive.h,
 * SH_CSS_ISP_PARAMS_VERSION should be 2.
 * If ISP_PIPE_VERSION is defined as 1,
 * SH_CSS_ISP_PARAMS_VERSION can be either 1 or 2.
 * ISP_PIPE_VRSION is always defined as 1 for ISP2300,
 * so SH_CSS_ISP_PARAMS_VERSION can be defined as 1 for ISP2300
 * to avoid sched error by increase of the number of paramaters.
*/
#if defined(IS_ISP_2400_SYSTEM)
#define SH_CSS_ISP_PARAMS_VERSION	2
#else
#define SH_CSS_ISP_PARAMS_VERSION	1
#endif

/*
 * JB: keep next enum in sync with thread id's
 * and pipe id's
 */
enum sh_css_pipe_config_override {
	SH_CSS_PIPE_CONFIG_OVRD_NONE     = 0,
	SH_CSS_PIPE_CONFIG_OVRD_THRD_0   = 1,
	SH_CSS_PIPE_CONFIG_OVRD_THRD_1   = 2,
	SH_CSS_PIPE_CONFIG_OVRD_THRD_2   = 4,
	SH_CSS_PIPE_CONFIG_OVRD_THRD_1_2 = 6,
	SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD  = 0xffff
};

enum host2sp_commands {
	host2sp_cmd_error = 0,
	/*
	 * The host2sp_cmd_ready command is the only command written by the SP
	 * It acknowledges that is previous command has been received.
	 * (this does not mean that the command has been executed)
	 * It also indicates that a new command can be send (it is a queue
	 * with depth 1).
	 */
	host2sp_cmd_ready = 1,
	/* Command written by the Host */
	host2sp_cmd_dummy,		/* No action, can be used as watchdog */
	host2sp_cmd_terminate,		/* SP should terminate itself */
	N_host2sp_cmd
};

/** Enumeration used to indicate the events that are produced by
 *  the SP and consumed by the Host.
 */
enum sh_css_sp_event_type {
	SH_CSS_SP_EVENT_NULL,
	SH_CSS_SP_EVENT_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_VF_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_3A_STATISTICS_DONE,
	SH_CSS_SP_EVENT_DIS_STATISTICS_DONE,
	SH_CSS_SP_EVENT_PIPELINE_DONE,
	SH_CSS_SP_EVENT_NR_OF_TYPES,		/* must be last */
};

/* Data structures shared with ISP */
struct sh_css_isp_params {
	/* FPNR (Fixed Pattern Noise Reduction) */
	int fpn_shift;
	int fpn_enabled;

	/* OB (Optical Black) */
	int ob_blacklevel_gr;
	int ob_blacklevel_r;
	int ob_blacklevel_b;
	int ob_blacklevel_gb;
	int obarea_start_bq;
	int obarea_length_bq;
	int obarea_length_bq_inverse;

	/* SC (Shading Corrction) */
	int sc_gain_shift;

	/* WB (White Balance) */
	int wb_gain_shift;
	int wb_gain_gr;
	int wb_gain_r;
	int wb_gain_b;
	int wb_gain_gb;

	/* DP (Defect Pixel Correction) */
	int dp_threshold_single_when_2adjacent_on;
	int dp_threshold_2adjacent_when_2adjacent_on;
	int dp_threshold_single_when_2adjacent_off;
	int dp_threshold_2adjacent_when_2adjacent_off;
	int dp_gain;

	/* BNR (Bayer Noise Reduction) */
	int bnr_gain_all;
	int bnr_gain_dir;
	int bnr_threshold_low;
	int bnr_threshold_width_log2;
	int bnr_threshold_width;
	int bnr_clip;

	/* S3A (3A Support): coefficients to calculate Y */
	int ae_y_coef_r;
	int ae_y_coef_g;
	int ae_y_coef_b;

	/* S3A (3A Support): AWB level gate */
	int awb_lg_high_raw;
	int awb_lg_low;
	int awb_lg_high;

	/* S3A (3A Support): af fir coefficients */
	int af_fir1[7];
	int af_fir2[7];

	/* DE (Demosaic) */
	int de_pixelnoise;
	int de_c1_coring_threshold;
	int de_c2_coring_threshold;

	/* YNR (Y Noise Reduction), YEE (Y Edge Enhancement) */
	int ynr_threshold;
	int ynr_gain_all;
	int ynr_gain_dir;
	int ynryee_dirthreshold_s;
	int ynryee_dirthreshold_g;
	int ynryee_dirthreshold_width_log2;
	int ynryee_dirthreshold_width;
	int yee_detailgain;
	int yee_coring_s;
	int yee_coring_g;
	int yee_scale_plus_s;
	int yee_scale_plus_g;
	int yee_scale_minus_s;
	int yee_scale_minus_g;
	int yee_clip_plus_s;
	int yee_clip_plus_g;
	int yee_clip_minus_s;
	int yee_clip_minus_g;
	int ynryee_Yclip;

	/* CSC (Color Space Conversion) */
	/* YC1C2->YCbCr */
	int csc_coef_shift;
	int yc1c2_to_ycbcr_00;
	int yc1c2_to_ycbcr_01;
	int yc1c2_to_ycbcr_02;
	int yc1c2_to_ycbcr_10;
	int yc1c2_to_ycbcr_11;
	int yc1c2_to_ycbcr_12;
	int yc1c2_to_ycbcr_20;
	int yc1c2_to_ycbcr_21;
	int yc1c2_to_ycbcr_22;

	/* GC (Gamma Correction) */
	int gamma_gain_k1;
	int gamma_gain_k2;

	/* TNR (Temporal Noise Reduction) */
	int tnr_coef;
	int tnr_threshold_Y;
	int tnr_threshold_C;

	/* ANR (Advance Noise Reduction) */
	int anr_threshold;

	/* CE (Chroma Enhancement) */
	int ce_uv_level_min;
	int ce_uv_level_max;

	struct sh_css_crop_pos sp_out_crop_pos[SH_CSS_MAX_STAGES];
	struct sh_css_uds_info uds[SH_CSS_MAX_STAGES];

/* parameters for ISP pipe version 2 */
#if SH_CSS_ISP_PARAMS_VERSION == 2
	/* DE (Demosaic) */
	int ecd_zip_strength;
	int ecd_fc_strength;
	int ecd_fc_debias;

	/* YNR (Y Noise Reduction), YEE (Y Edge Enhancement) */
	int yee_edge_sense_gain_0;
	int yee_edge_sense_gain_1;
	int yee_corner_sense_gain_0;
	int yee_corner_sense_gain_1;

	/* Fringe Control */
	int fc_gain_exp;
	int fc_gain_pos_0;
	int fc_gain_pos_1;
	int fc_gain_neg_0;
	int fc_gain_neg_1;
	int fc_crop_pos_0;
	int fc_crop_pos_1;
	int fc_crop_neg_0;
	int fc_crop_neg_1;

	/* CNR */
	int cnr_coring_u;
	int cnr_coring_v;
	int cnr_sense_gain_vy;
	int cnr_sense_gain_vu;
	int cnr_sense_gain_vv;
	int cnr_sense_gain_hy;
	int cnr_sense_gain_hu;
	int cnr_sense_gain_hv;
#endif /* SH_CSS_ISP_PARAMS_VERSION == 2 */

	/* MACC */
	int exp;

/* parameters for ISP pipe version 2 */
#if SH_CSS_ISP_PARAMS_VERSION == 2
	/* CTC */
	int ctc_y0;
	int ctc_y1;
	int ctc_y2;
	int ctc_y3;
	int ctc_y4;
	int ctc_y5;
	int ctc_ce_gain_exp;
	int ctc_x1;
	int ctc_x2;
	int ctc_x3;
	int ctc_x4;
	int ctc_dydx0;
	int ctc_dydx0_shift;
	int ctc_dydx1;
	int ctc_dydx1_shift;
	int ctc_dydx2;
	int ctc_dydx2_shift;
	int ctc_dydx3;
	int ctc_dydx3_shift;
	int ctc_dydx4;
	int ctc_dydx4_shift;

	/* Anti-Aliasing */
        int aa_scale;

	/* CCM before sRGB Gamma: YCgCo->RGB */
	int ycgco_to_rgb_00;
	int ycgco_to_rgb_01;
	int ycgco_to_rgb_02;
	int ycgco_to_rgb_10;
	int ycgco_to_rgb_11;
	int ycgco_to_rgb_12;
	int ycgco_to_rgb_20;
	int ycgco_to_rgb_21;
	int ycgco_to_rgb_22;

	/* CSC after sRGB Gamma: RGB->YUV */
	int rgb_to_yuv_00;
	int rgb_to_yuv_01;
	int rgb_to_yuv_02;
	int rgb_to_yuv_10;
	int rgb_to_yuv_11;
	int rgb_to_yuv_12;
	int rgb_to_yuv_20;
	int rgb_to_yuv_21;
	int rgb_to_yuv_22;
#else
	int aa_scale;
#endif /* SH_CSS_ISP_PARAMS_VERSION == 2 */

	/* XNR threshold */
	int xnr_threshold;
};

/* xmem address map allocation */
struct sh_css_ddr_address_map {
	hrt_vaddress isp_param;
	hrt_vaddress ctc_tbl;
	hrt_vaddress xnr_tbl;
	hrt_vaddress gamma_tbl;
	hrt_vaddress macc_tbl;
	hrt_vaddress fpn_tbl;
	hrt_vaddress sc_tbl;
	hrt_vaddress sdis_hor_coef;
	hrt_vaddress sdis_ver_coef;
	hrt_vaddress tetra_r_x;
	hrt_vaddress tetra_r_y;
	hrt_vaddress tetra_gr_x;
	hrt_vaddress tetra_gr_y;
	hrt_vaddress tetra_gb_x;
	hrt_vaddress tetra_gb_y;
	hrt_vaddress tetra_b_x;
	hrt_vaddress tetra_b_y;
	hrt_vaddress tetra_ratb_x;
	hrt_vaddress tetra_ratb_y;
	hrt_vaddress tetra_batr_x;
	hrt_vaddress tetra_batr_y;
	hrt_vaddress dvs_6axis_params_y;
	hrt_vaddress r_gamma_tbl;
	hrt_vaddress g_gamma_tbl;
	hrt_vaddress b_gamma_tbl;
};

/* xmem address map allocation */
struct sh_css_ddr_address_map_size {
	size_t isp_param;
	size_t ctc_tbl;
	size_t gamma_tbl;
	size_t xnr_tbl;
	size_t macc_tbl;
	size_t fpn_tbl;
	size_t sc_tbl;
	size_t sdis_hor_coef;
	size_t sdis_ver_coef;
	size_t tetra_r_x;
	size_t tetra_r_y;
	size_t tetra_gr_x;
	size_t tetra_gr_y;
	size_t tetra_gb_x;
	size_t tetra_gb_y;
	size_t tetra_b_x;
	size_t tetra_b_y;
	size_t tetra_ratb_x;
	size_t tetra_ratb_y;
	size_t tetra_batr_x;
	size_t tetra_batr_y;
	size_t dvs_6axis_params_y;
	size_t r_gamma_tbl;
	size_t g_gamma_tbl;
	size_t b_gamma_tbl;
};

struct sh_css_ddr_address_map_compound {
	struct sh_css_ddr_address_map		map;
	struct sh_css_ddr_address_map_size	size;
};

struct sh_css_envelope {
	unsigned int width;
	unsigned int height;
};

/* this struct contains all arguments that can be passed to
   a binary. It depends on the binary which ones are used. */
struct sh_css_binary_args {
	struct sh_css_frame *cc_frame;       /* continuous capture frame */
	struct sh_css_frame *in_frame;	     /* input frame */
	struct sh_css_frame *in_ref_frame;   /* reference input frame */
	struct sh_css_frame *in_tnr_frame;   /* tnr input frame */
	struct sh_css_frame *out_frame;      /* output frame */
	struct sh_css_frame *out_ref_frame;  /* reference output frame */
	struct sh_css_frame *out_tnr_frame;  /* tnr output frame */
	struct sh_css_frame *extra_frame;    /* intermediate frame */
	struct sh_css_frame *out_vf_frame;   /* viewfinder output frame */
	bool                 copy_vf;
	bool                 copy_output;
	unsigned             vf_downscale_log2;
};

/* Pipeline stage to be executed on SP/ISP */
struct sh_css_pipeline_stage {
	unsigned int		     stage_num;
	struct sh_css_binary        *binary;      /* built-in binary */
	struct sh_css_binary_info   *binary_info;
	const struct sh_css_fw_info *firmware; /* acceleration binary */
	struct sh_css_binary_args    args;
	int                          mode;
	bool                         out_frame_allocated;
	bool                         vf_frame_allocated;
	/* Indicate which buffers require an IRQ */
	unsigned int		     irq_buf_flags;
	struct sh_css_pipeline_stage *next;
};

/* Pipeline of n stages to be executed on SP/ISP per stage */
struct sh_css_pipeline {
	enum sh_css_pipe_id pipe_id;
	struct sh_css_pipeline_stage *stages;
	bool reload;
	struct sh_css_pipeline_stage *current_stage;
	unsigned num_stages;
	struct sh_css_frame in_frame;
	struct sh_css_frame out_frame;
	struct sh_css_frame vf_frame;
};


#if (SP_DEBUG == SP_DEBUG_DUMP) || (SP_DEBUG == SP_DEBUG_STALL)

#define SH_CSS_NUM_SP_DEBUG 48

struct sh_css_sp_debug_state {
	unsigned int error;
	unsigned int debug[SH_CSS_NUM_SP_DEBUG];
};

#elif SP_DEBUG == SP_DEBUG_COPY

#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)

struct sh_css_sp_debug_trace {
	uint16_t frame;
	uint16_t line;
	uint16_t pixel_distance;
	uint16_t mipi_used_dword;
	uint16_t sp_index;
};

struct sh_css_sp_debug_state {
	uint16_t if_start_line;
	uint16_t if_start_column;
	uint16_t if_cropped_height;
	uint16_t if_cropped_width;
	unsigned int index;
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_TRACE_DEPTH];
};

#elif SP_DEBUG == SP_DEBUG_TRACE

#define SH_CSS_SP_DBG_NR_OF_TRACES	(1)
#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)

#define SH_CSS_SP_DBG_TRACE_FILE_ID_BIT_POS (13)

/* trace id 0..3 are used by the SP threads */
#define SH_CSS_SP_DBG_TRACE_ID_CONTROL (3) /* Re-use accl thread */
#define SH_CSS_SP_DBG_TRACE_ID_TBD  (5)

struct sh_css_sp_debug_trace {
	uint16_t time_stamp;
	uint16_t location;	/* bit 15..13 = file_id, 12..0 = line */
	uint32_t data;
};

struct sh_css_sp_debug_state {
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_NR_OF_TRACES][SH_CSS_SP_DBG_TRACE_DEPTH];
	uint16_t index_last[SH_CSS_SP_DBG_NR_OF_TRACES];
	uint8_t index[SH_CSS_SP_DBG_NR_OF_TRACES];
};

#endif


struct sh_css_sp_debug_command {
	/*
	 * The DMA software-mask,
	 *	Bit 31...24: unused.
	 *	Bit 23...16: unused.
	 *	Bit 15...08: reading-request enabling bits for DMA channel 7..0
	 *	Bit 07...00: writing-reqeust enabling bits for DMA channel 7..0
	 *
	 * For example, "0...0 0...0 11111011 11111101" indicates that the
	 * writing request through DMA Channel 1 and the reading request
	 * through DMA channel 2 are both disabled. The others are enabled.
	 */
	uint32_t dma_sw_reg;
};

/* SP configuration information */
struct sh_css_sp_config {
	uint8_t			is_offline;  /* Run offline, with continuous copy */
	uint8_t			input_needs_raw_binning;
	uint8_t			no_isp_sync; /* Signal host immediately after start */
	struct {
		uint8_t					a_changed;
		uint8_t					b_changed;
		uint8_t					isp_2ppc;
		uint32_t				stream_format;
		input_formatter_cfg_t	config_a;
		input_formatter_cfg_t	config_b;
	} input_formatter;
	sync_generator_cfg_t	sync_gen;
	tpg_cfg_t				tpg;
	prbs_cfg_t				prbs;
	input_system_cfg_t		input_circuit;
	uint8_t					input_circuit_cfg_changed;
};

enum sh_css_stage_type {
  SH_CSS_SP_STAGE_TYPE  = 0,
  SH_CSS_ISP_STAGE_TYPE = 1
};
#define SH_CSS_NUM_STAGE_TYPES 2

enum sh_css_sp_stage_func {
  SH_CSS_SP_RAW_COPY = 0,
  SH_CSS_SP_BIN_COPY = 1
};
#define SH_CSS_NUM_STAGE_FUNCS 2

#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS 	(1 << 0)
#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS_MASK \
	((SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS << SH_CSS_MAX_SP_THREADS)-1)

/* Information for a pipeline */
struct sh_css_sp_pipeline {
	uint32_t	pipe_id;	/* the pipe ID */
	uint32_t	thread_id;	/* the sp thread ID */
	uint32_t	pipe_config;	/* the pipe config */
	uint32_t	num_stages;
	uint32_t	running;
	hrt_vaddress	sp_stage_addr[SH_CSS_MAX_STAGES];
	struct sh_css_sp_stage *stage; /* Current stage for this pipeline */
	union {
		struct {
			unsigned int	bytes_available;
		} bin;
		struct {
			unsigned int	height;
			unsigned int	width;
			unsigned int	padded_width;
			unsigned int	max_input_width;
			unsigned int	raw_bit_depth;
		} raw;
	} copy;
};

/*
 * These structs are derived from structs defined in sh_css_types.h
 * (just take out the "_sp" from the struct name to get the "original")
 * All the fields that are not needed by the SP are removed.
 */
struct sh_css_sp_frame_plane {
	unsigned int offset;	/* offset in bytes to start of frame data */
				/* offset is wrt data in sh_css_sp_sp_frame */
};

struct sh_css_sp_frame_binary_plane {
	unsigned int size;
	struct sh_css_sp_frame_plane data;
};

struct sh_css_sp_frame_yuv_planes {
	struct sh_css_sp_frame_plane y;
	struct sh_css_sp_frame_plane u;
	struct sh_css_sp_frame_plane v;
};

struct sh_css_sp_frame_nv_planes {
	struct sh_css_sp_frame_plane y;
	struct sh_css_sp_frame_plane uv;
};

struct sh_css_sp_frame_rgb_planes {
	struct sh_css_sp_frame_plane r;
	struct sh_css_sp_frame_plane g;
	struct sh_css_sp_frame_plane b;
};

struct sh_css_sp_frame_plane6_planes {
	struct sh_css_sp_frame_plane r;
	struct sh_css_sp_frame_plane r_at_b;
	struct sh_css_sp_frame_plane gr;
	struct sh_css_sp_frame_plane gb;
	struct sh_css_sp_frame_plane b;
	struct sh_css_sp_frame_plane b_at_r;
};

enum sh_css_frame_id {
	sh_css_frame_in,		/* Dynamic */
	sh_css_frame_out,		/* Dynamic */
	sh_css_frame_out_vf,		/* Dynamic */
	sh_css_frame_s3a,		/* Dynamic */
	sh_css_frame_dis,		/* Dynamic */
	sh_css_frame_ref_in,
	sh_css_frame_ref_out,
	sh_css_frame_tnr_in,
	sh_css_frame_tnr_out,
	sh_css_frame_extra,
	sh_css_frame_raw_out,
	sh_css_frame_cust_in,
	sh_css_frame_cust_out,
};
/*
 * The first frames (with comment Dynamic) can be dynamic or static
 * The other frames (ref_in and below) can only be static
 * Static means that the data addres will not change during the life time
 * of the associated pipe. Dynamic means that the data address can
 * change with every (frame) iteration of the associated pipe
 *
 * s3a and dis are now also dynamic but (stil) handled seperately
 */
#define SH_CSS_NUM_FRAME_IDS (13)
#define SH_CSS_NUM_DYNAMIC_BUFFER_IDS (5)
#define SH_CSS_NUM_DYNAMIC_FRAME_IDS (3)
#define SH_CSS_INVALID_FRAME_ID (-1)


/** Frame info struct. This describes the contents of an image frame buffer.
  */
struct sh_css_sp_frame_info {
	uint16_t width;  /**< width of valid data in pixels */
	uint16_t height; /**< Height of valid data in lines */
	uint16_t padded_width; /**< stride of line in memory (in pixels) */
	unsigned char format; /**< format of the frame data */
	unsigned char raw_bit_depth; /**< number of valid bits per pixel,
					 only valid for RAW bayer frames */
	unsigned char raw_bayer_order; /**< bayer order, only valid
						      for RAW bayer frames */
	unsigned char padding;
};


struct sh_css_sp_frame {
	struct sh_css_sp_frame_info info;
	union {
		struct sh_css_sp_frame_plane raw;
		struct sh_css_sp_frame_plane rgb;
		struct sh_css_sp_frame_rgb_planes planar_rgb;
		struct sh_css_sp_frame_plane yuyv;
		struct sh_css_sp_frame_yuv_planes yuv;
		struct sh_css_sp_frame_nv_planes nv;
		struct sh_css_sp_frame_plane6_planes plane6;
		struct sh_css_sp_frame_binary_plane binary;
	} planes;
};

struct sh_css_sp_frames {
	struct sh_css_sp_frame	in;
	struct sh_css_sp_frame	out;
	struct sh_css_sp_frame	out_vf;
	struct sh_css_sp_frame	ref_in;
	/* ref_out_frame is same as ref_in_frame */
	struct sh_css_sp_frame	tnr_in;
	/* trn_out_frame is same as tnr_in_frame */
	struct sh_css_sp_frame	extra;
	struct sh_css_sp_frame_info internal_frame_info;
	hrt_vaddress static_frame_data[SH_CSS_NUM_FRAME_IDS];
};

/* Information for a single pipeline stage for an ISP */
struct sh_css_isp_stage {
	/*
	 * For compatability and portabilty, only types
	 * from "stdint.h" are allowed
	 *
	 * Use of "enum" and "bool" is prohibited
	 * Multiple boolean flags can be stored in an
	 * integer
	 */
	struct sh_css_blob_info		blob_info;
	struct sh_css_binary_info	binary_info;
	char				binary_name[SH_CSS_MAX_BINARY_NAME];
	struct sh_css_hmm_isp_interface mem_interface
						[SH_CSS_NUM_ISP_MEMORIES];
};

/* Information for a single pipeline stage */
struct sh_css_sp_stage {
	/*
	 * For compatability and portabilty, only types
	 * from "stdint.h" are allowed
	 *
	 * Use of "enum" and "bool" is prohibited
	 * Multiple boolean flags can be stored in an
	 * integer
	 */
	uint8_t			num; /* Stage number */
	uint8_t			isp_online;
	uint8_t			isp_copy_vf;
	uint8_t			isp_copy_output;
	uint8_t			sp_enable_xnr;
	uint8_t			isp_deci_log_factor;
	uint8_t			isp_vf_downscale_bits;
	uint8_t			anr;
	uint8_t			deinterleaved;
/*
 * NOTE: Programming the input circuit can only be done at the
 * start of a session. It is illegal to program it during execution
 * The input circuit defines the connectivity
 */
	uint8_t			program_input_circuit;
/* enum sh_css_sp_stage_func	func; */
	uint8_t			func;
	/* The type of the pipe-stage */
	/* enum sh_css_stage_type	stage_type; */
	uint8_t			stage_type;
	uint8_t			num_stripes;
	struct {
		uint8_t		vf_veceven;
		uint8_t		s3a;
		uint8_t		sdis;
	} enable;
	/* Add padding to come to a word boundary */
	/* unsigned char			padding[0]; */

	struct sh_css_crop_pos		sp_out_crop_pos;
	/* Indicate which buffers require an IRQ */
	uint32_t					irq_buf_flags;
	struct sh_css_sp_frames		frames;
	struct sh_css_dvs_envelope	dvs_envelope;
	struct sh_css_uds_info		uds;
	hrt_vaddress			isp_stage_addr;
	hrt_vaddress			xmem_bin_addr;
	hrt_vaddress			xmem_map_addr;
};

/*
 * Time: 2012-07-19, 17:40.
 * Author: zhengjie.lu@intel.com
 * Note: Add a new data memeber "debug" in "sh_css_sp_group". This
 * data member is used to pass the debugging command from the
 * Host to the SP.
 *
 * Time: Before 2012-07-19.
 * Author: unknown
 * Note:
 * Group all host initialized SP variables into this struct.
 * This is initialized every stage through dma.
 * The stage part itself is transfered through sh_css_sp_stage.
*/
struct sh_css_sp_group {
	struct sh_css_sp_config		config;
	struct sh_css_sp_pipeline	pipe[SH_CSS_MAX_SP_THREADS];

	struct sh_css_sp_debug_command	debug;
};

/* Data in SP dmem that is set from the host every stage. */
struct sh_css_sp_per_frame_data {
	/* ddr address of sp_group and sp_stage */
	hrt_vaddress			sp_group_addr;
};

#define SH_CSS_NUM_SDW_IRQS 3

/* Output data from SP to css */
struct sh_css_sp_output {
	unsigned int			bin_copy_bytes_copied;
#if SP_DEBUG != SP_DEBUG_NONE
	struct sh_css_sp_debug_state	debug;
#endif
	unsigned int		sw_interrupt_value[SH_CSS_NUM_SDW_IRQS];
};

/**
 * @brief Data structure for the circular buffer.
 * The circular buffer is empty if "start == end". The
 * circular buffer is full if "(end + 1) % size == start".
 */
#define  SH_CSS_CIRCULAR_BUF_NUM_ELEMS	6
struct sh_css_circular_buf {
	/*
	 * WARNING: Do NOT change the memeber orders below,
	 * unless you are the expert of either the CSS API
	 * or the SP code.
	 */

	uint8_t size;  /* maximum number of elements */
	uint8_t step;  /* number of bytes per element */
	uint8_t start; /* index of the oldest element */
	uint8_t end;   /* index at which to write the new element */

	uint32_t elems[SH_CSS_CIRCULAR_BUF_NUM_ELEMS]; /* array of elements */
};

struct sh_css_hmm_buffer {
	hrt_vaddress kernel_ptr;
	union {
		union sh_css_s3a_data s3a;
		struct sh_css_dis_data dis;
//		hrt_vaddress frame_data;
		struct {
			hrt_vaddress	frame_data;
			unsigned int	flashed;
			unsigned int	exp_id;
		} frame;
		hrt_vaddress ddr_ptrs;
	} payload;
};

enum sh_css_buffer_queue_id {
	sh_css_invalid_buffer_queue = -1,
	sh_css_input_buffer_queue,
	sh_css_output_buffer_queue,
	sh_css_vf_output_buffer_queue,
	sh_css_s3a_buffer_queue,
	sh_css_dis_buffer_queue,
	sh_css_param_buffer_queue,
	sh_css_tag_cmd_queue
};

struct sh_css_event_irq_mask {
	uint16_t or_mask;
	uint16_t and_mask;
};

#define SH_CSS_NUM_BUFFER_QUEUES 7

struct host_sp_communication {
	/*
	 * Don't use enum host2sp_commands, because the sizeof an enum is
	 * compiler dependant and thus non-portable
	 */
	unsigned int host2sp_command;

	/*
	 * The frame buffers that are reused by the
	 * copy pipe in the offline preview mode.
	 *
	 * host2sp_offline_frames[0]: the input frame of the preview pipe.
	 * host2sp_offline_frames[1]: the output frame of the copy pipe.
	 *
	 * TODO:
	 *   Remove it when the Host and the SP is decoupled.
	 */
	hrt_vaddress host2sp_offline_frames[NUM_CONTINUOUS_FRAMES];
	unsigned int host2sp_cont_avail_num_raw_frames;
	unsigned int host2sp_cont_extra_num_raw_frames;
	unsigned int host2sp_cont_target_num_raw_frames;
	struct sh_css_event_irq_mask host2sp_event_irq_mask[NR_OF_PIPELINES];

};

struct host_sp_queues {
	/*
	 * Queues for the dynamic frame information,
	 * i.e. the "in_frame" buffer, the "out_frame"
	 * buffer and the "vf_out_frame" buffer.
	 */
	struct sh_css_circular_buf host2sp_buffer_queues
		[SH_CSS_MAX_SP_THREADS][SH_CSS_NUM_BUFFER_QUEUES];
	struct sh_css_circular_buf sp2host_buffer_queues
		[SH_CSS_NUM_BUFFER_QUEUES];

	/*
	 * The queue for the events.
	 */
	struct sh_css_circular_buf host2sp_event_queue;
	struct sh_css_circular_buf sp2host_event_queue;
};

extern int (*sh_css_printf) (const char *fmt, ...);

hrt_vaddress
sh_css_params_ddr_address_map(void);

void
sh_css_update_isp_params_to_ddr(hrt_vaddress ddr_ptr);

enum sh_css_err
sh_css_params_write_to_ddr(const struct sh_css_binary *binary_info);

void
sh_css_params_set_current_binary(const struct sh_css_binary *binary);

/* swap 3a double buffers. This should be called when handling an
   interrupt that indicates that statistics are ready.
   This also swaps the DIS buffers. */
#if 0
void
sh_css_params_swap_3a_buffers(void);

enum sh_css_err
sh_css_params_get_free_stat_bufs(const struct sh_css_binary *binary,
					union sh_css_s3a_data *s3a_data,
					struct sh_css_dis_data *sdis_data);

enum sh_css_err
sh_css_params_get_free_stat_bufs2(union sh_css_s3a_data *s3a_data,
					struct sh_css_dis_data *sdis_data);
#endif

enum sh_css_err
sh_css_params_init(void);

void
sh_css_params_uninit(void);

void
sh_css_params_reconfigure_gdc_lut(void);

void *
sh_css_malloc(size_t size);

void
sh_css_free(void *ptr);

/* For Acceleration API: Flush FW (shared buffer pointer) arguments */
extern void
sh_css_flush(struct sh_css_acc_fw *fw);

/* Check two frames for equality (format, resolution, bits per element) */
bool
sh_css_frame_equal_types(const struct sh_css_frame *frame_a,
			 const struct sh_css_frame *frame_b);

bool
sh_css_frame_info_equal_resolution(const struct sh_css_frame_info *info_a,
				   const struct sh_css_frame_info *info_b);

unsigned int
sh_css_input_format_bits_per_pixel(enum sh_css_input_format format,
				   bool two_ppc);

enum sh_css_err
sh_css_vf_downscale_log2(const struct sh_css_frame_info *out_info,
			 const struct sh_css_frame_info *vf_info,
			 unsigned int *downscale_log2);

void
sh_css_capture_enable_bayer_downscaling(bool enable);

void
sh_css_binary_print(const struct sh_css_binary *binary);

#if SP_DEBUG !=SP_DEBUG_NONE

void
sh_css_print_sp_debug_state(const struct sh_css_sp_debug_state *state);

#endif

void
sh_css_frame_info_set_width(struct sh_css_frame_info *info,
			    unsigned int width, unsigned int min_padded_width);

/* Return whether the sp copy process should be started */
bool
sh_css_continuous_start_sp_copy(void);

bool
sh_css_pipe_uses_params(struct sh_css_pipeline *me);

/* The following functions are used for testing purposes only */
const struct sh_css_fpn_table *
sh_css_get_fpn_table(void);

struct sh_css_shading_table *
sh_css_get_shading_table(void);

const struct sh_css_isp_params *
sh_css_get_isp_params(void);

void
sh_css_invalidate_morph_table(void);

const struct sh_css_binary *
sh_css_get_3a_binary(void);

void
sh_css_get_isp_dis_coefficients(short *horizontal_coefficients,
				short *vertical_coefficients);

void
sh_css_get_isp_dis_projections(int *horizontal_projections,
			       int *vertical_projections);

hrt_vaddress
sh_css_store_sp_group_to_ddr(void);

hrt_vaddress
sh_css_store_sp_stage_to_ddr(unsigned pipe, unsigned stage);

hrt_vaddress
sh_css_store_isp_stage_to_ddr(unsigned pipe, unsigned stage);

void
sh_css_frame_info_init(struct sh_css_frame_info *info,
		       unsigned int width,
		       unsigned int height,
		       unsigned int min_padded_width,
		       enum sh_css_frame_format format);

bool
sh_css_enqueue_frame(unsigned int pipe_num,
		     enum sh_css_frame_id frame_id,
		     struct sh_css_frame *frame);

/**
 * @brief Query the SP thread ID.
 *
 * @param[in]	key	The query key.
 * @param[out]	val	The query value.
 *
 * @return
 *	true, if the query succeeds;
 *	false, if the query fails.
 */
bool
sh_css_query_sp_thread_id(enum sh_css_pipe_id key,
		unsigned int *val);

/**
 * @brief Query the internal frame ID.
 *
 * @param[in]	key	The query key.
 * @param[out]	val	The query value.
 *
 * @return
 *	true, if the query succeeds;
 *	false, if the query fails.
 */
bool
sh_css_query_internal_queue_id(enum sh_css_buffer_type key,
		enum sh_css_buffer_queue_id *val);


bool
input_format_is_yuv_8(enum sh_css_input_format format);

#endif /* _SH_CSS_INTERNAL_H_ */
