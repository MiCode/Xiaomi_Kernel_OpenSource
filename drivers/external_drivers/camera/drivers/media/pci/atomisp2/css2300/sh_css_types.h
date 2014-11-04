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

#ifndef _SH_CSS_TYPES_H_
#define _SH_CSS_TYPES_H_

/*! \file */

/** @file ia_css_types.h
 * This file contains types used for the ia_css parameters.
 * These types are in a separate file because they are expected
 * to be used in software layers that do not access the CSS API
 * directly but still need to forward parameters for it.
 */

/* This code is also used by Silicon Hive in a simulation environment
 * Therefore, the following macro is used to differentiate when this
 * code is being included from within the Linux kernel source
 */
#include <stdbool.h>
#include <system_types.h>	/* hrt_vaddress, HAS_IRQ_MAP_VERSION_# */

/*#include "vamem.h"*/ /* Canot include for VAMEM properties this file is visible on ISP -> pipeline generator */
/*#include "isp.h"*/	/* surrogate to get VAMEM "HAS" properties, causes other circular include issues */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>       /* memcpy() */
#else
#include <stdarg.h>             /* printf() */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memcpy() */
#include "math_support.h"		/* min(), max() */
#endif

#define SH_CSS_MAJOR    0
#define SH_CSS_MINOR    2
#define SH_CSS_REVISION 5

/** Number of axes in the MACC table. */
#define SH_CSS_MACC_NUM_AXES           16
/** Number of coefficients per MACC axes. */
#define SH_CSS_MACC_NUM_COEFS          4
/** The number of planes in the morphing table. */
#define SH_CSS_MORPH_TABLE_NUM_PLANES  6
/** Number of color planes in the shading table. */
#define SH_CSS_SC_NUM_COLORS           4
/** Number of DIS coefficient types (TBD) */
#define SH_CSS_DIS_NUM_COEF_TYPES      6
#define SH_CSS_DIS_COEF_TYPES_ON_DMEM  2

/** Fractional bits for CTC gain */
#define SH_CSS_CTC_COEF_SHIFT          13
/** Fractional bits for GAMMA gain */
#define SH_CSS_GAMMA_GAIN_K_SHIFT      13

/*
 * MW: Note that there is a duplicate copy in "sh_css_defs.h"
 * This version is supposedly platform independent. Unfortunately
 * the support file IspConfig depends 
 */
#if defined(HAS_VAMEM_VERSION_2)
#define SH_CSS_CTC_TABLE_SIZE_LOG2      8
#define SH_CSS_CTC_TABLE_SIZEM1         (1U<<SH_CSS_CTC_TABLE_SIZE_LOG2)
#define SH_CSS_CTC_TABLE_SIZE           (SH_CSS_CTC_TABLE_SIZEM1 + 1)
#define SH_CSS_GAMMA_TABLE_SIZE_LOG2    8
#define SH_CSS_GAMMA_TABLE_SIZEM1	(1U<<SH_CSS_GAMMA_TABLE_SIZE_LOG2)
#define SH_CSS_GAMMA_TABLE_SIZE         (SH_CSS_GAMMA_TABLE_SIZEM1 + 1)
#define SH_CSS_XNR_TABLE_SIZE_LOG2      6
#define SH_CSS_XNR_TABLE_SIZE	        (1U<<SH_CSS_XNR_TABLE_SIZE_LOG2)
/*#define SH_CSS_XNR_TABLE_SIZE	        (SH_CSS_XNR_TABLE_SIZEM1 + 1)*/
#define SH_CSS_RGB_GAMMA_TABLE_SIZE_LOG2    8
#define SH_CSS_RGB_GAMMA_TABLE_SIZEM1	(1U<<SH_CSS_RGB_GAMMA_TABLE_SIZE_LOG2)
#define SH_CSS_RGB_GAMMA_TABLE_SIZE     (SH_CSS_RGB_GAMMA_TABLE_SIZEM1 + 1)
#elif defined(HAS_VAMEM_VERSION_1)
/** Number of elements in the CTC table. */
#define SH_CSS_CTC_TABLE_SIZE_LOG2      10
#define SH_CSS_CTC_TABLE_SIZE           (1U<<SH_CSS_CTC_TABLE_SIZE_LOG2)
/** Number of elements in the gamma table. */
#define SH_CSS_GAMMA_TABLE_SIZE_LOG2    10
#define SH_CSS_GAMMA_TABLE_SIZE         (1U<<SH_CSS_GAMMA_TABLE_SIZE_LOG2)
/** Number of elements in the xnr table. */
#define SH_CSS_XNR_TABLE_SIZE_LOG2      6
#define SH_CSS_XNR_TABLE_SIZE           (1U<<SH_CSS_XNR_TABLE_SIZE_LOG2)
/** Number of elements in the sRGB gamma table. */
#define SH_CSS_RGB_GAMMA_TABLE_SIZE_LOG2 8
#define SH_CSS_RGB_GAMMA_TABLE_SIZE      (1U<<SH_CSS_RGB_GAMMA_TABLE_SIZE_LOG2)
#else
#error "sh_css_types: Unknown VAMEM version"
#endif

/* Fixed point types.
 * NOTE: the 16 bit fixed point types actually occupy 32 bits
 * to save on extension operations in the ISP code.
 */
/** Unsigned fixed point value, 0 integer bits, 16 fractional bits */
#define u0_16 unsigned int
/** Unsigned fixed point value, 2 integer bits, 14 fractional bits */
#define u2_14 unsigned int
/** Unsigned fixed point value, 5 integer bits, 11 fractional bits */
#define u5_11 unsigned int
/** Unsigned fixed point value, 8 integer bits, 8 fractional bits */
#define u8_8  unsigned int
/** Signed fixed point value, 0 integer bits, 15 fractional bits */
#define s0_15 signed int

typedef uint32_t ia_css_ptr;
/** Frame formats, some of these come from fourcc.org, others are
   better explained by video4linux2. The NV11 seems to be described only
   on MSDN pages, but even those seem to be gone now.
   Frames can come in many forms, the main categories are RAW, RGB and YUV
   (or YCbCr). The YUV frames come in 4 flavors, determined by how the U and V
   values are subsampled:
   1. YUV420: hor = 2, ver = 2
   2. YUV411: hor = 4, ver = 1
   3. YUV422: hor = 2, ver = 1
   4. YUV444: hor = 1, ver = 1
 */
enum sh_css_frame_format {
	SH_CSS_FRAME_FORMAT_NV11,       /**< 12 bit YUV 411, Y, UV plane */
	SH_CSS_FRAME_FORMAT_NV12,       /**< 12 bit YUV 420, Y, UV plane */
	SH_CSS_FRAME_FORMAT_NV16,       /**< 16 bit YUV 422, Y, UV plane */
	SH_CSS_FRAME_FORMAT_NV21,       /**< 12 bit YUV 420, Y, VU plane */
	SH_CSS_FRAME_FORMAT_NV61,       /**< 16 bit YUV 422, Y, VU plane */
	SH_CSS_FRAME_FORMAT_YV12,       /**< 12 bit YUV 420, Y, V, U plane */
	SH_CSS_FRAME_FORMAT_YV16,       /**< 16 bit YUV 422, Y, V, U plane */
	SH_CSS_FRAME_FORMAT_YUV420,     /**< 12 bit YUV 420, Y, U, V plane */
	SH_CSS_FRAME_FORMAT_YUV420_16,  /**< yuv420, 16 bits per subpixel */
	SH_CSS_FRAME_FORMAT_YUV422,     /**< 16 bit YUV 422, Y, U, V plane */
	SH_CSS_FRAME_FORMAT_YUV422_16,  /**< yuv422, 16 bits per subpixel */
	SH_CSS_FRAME_FORMAT_UYVY,       /**< 16 bit YUV 422, UYVY interleaved */
	SH_CSS_FRAME_FORMAT_YUYV,       /**< 16 bit YUV 422, YUYV interleaved */
	SH_CSS_FRAME_FORMAT_YUV444,     /**< 24 bit YUV 444, Y, U, V plane */
	SH_CSS_FRAME_FORMAT_YUV_LINE,   /**< Internal format, 2 y lines followed
					     by a uvinterleaved line */
	SH_CSS_FRAME_FORMAT_RAW,	/**< RAW, 1 plane */
	SH_CSS_FRAME_FORMAT_RGB565,     /**< 16 bit RGB, 1 plane. Each 3 sub
					     pixels are packed into one 16 bit
					     value, 5 bits for R, 6 bits for G
					     and 5 bits for B. */
	SH_CSS_FRAME_FORMAT_PLANAR_RGB888, /**< 24 bit RGB, 3 planes */
	SH_CSS_FRAME_FORMAT_RGBA888,	/**< 32 bit RGBA, 1 plane, A=Alpha
					     (alpha is unused) */
	SH_CSS_FRAME_FORMAT_QPLANE6, /**< Internal, for advanced ISP */
	SH_CSS_FRAME_FORMAT_BINARY_8,	/**< byte stream, used for jpeg. For
					     frames of this type, we set the
					     height to 1 and the width to the
					     number of allocated bytes. */
	N_SH_CSS_FRAME_FORMAT
};
/* The maximum number of different frame formats any binary can support */
#define SH_CSS_MAX_NUM_FRAME_FORMATS 19

/** Vector with signed values. This is used to indicate motion for
 * Digital Image Stabilization.
 */
struct sh_css_vector {
	int x; /**< horizontal motion (in pixels) */
	int y; /**< vertical motion (in pixels) */
};

struct sh_css_uds_info {
	uint16_t curr_dx;
	uint16_t curr_dy;
	uint16_t xc;
	uint16_t yc;
};

struct sh_css_zoom {
	uint16_t dx;
	uint16_t dy;
};

struct sh_css_crop_pos {
	uint16_t x;
	uint16_t y;
};

struct sh_css_dvs_envelope {
	uint16_t width;
	uint16_t height;
};

/** 3A statistics grid */
struct sh_css_3a_grid_info {
	unsigned int enable;            /**< 3A statistics enabled */
	unsigned int use_dmem;          /**< DMEM or VMEM determines layout */
	unsigned int width;		/**< Width of 3A grid */
	unsigned int height;	        /**< Height of 3A grid */
	unsigned int aligned_width;     /**< Horizontal stride (for alloc) */
	unsigned int aligned_height;    /**< Vertical stride (for alloc) */
	unsigned int bqs_per_grid_cell; /**< Grid cell size */
};

/** DVS statistics grid */
struct sh_css_dvs_grid_info {
	unsigned int enable;        /**< DVS statistics enabled */
	unsigned int width;	    /**< Width of DVS grid, this is equal to the
					 the number of vertical statistics. */
	unsigned int aligned_width; /**< Stride of each grid line */
	unsigned int height;	    /**< Height of DVS grid, this is equal
					 to the number of horizontal statistics.
				     */
	unsigned int aligned_height;/**< Stride of each grid column */
	unsigned int bqs_per_grid_cell; /**< Grid cell size */
};

/** structure that describes the 3A and DIS grids */
struct sh_css_grid_info {
	/** \name ISP input size
	  * that is visible for user
	  * @{
	  */
	unsigned int isp_in_width;
	unsigned int isp_in_height;
	/** @}*/

	struct sh_css_3a_grid_info  s3a_grid; /**< 3A grid info */
	struct sh_css_dvs_grid_info dvs_grid; /**< DVS grid info */

	unsigned int dvs_hor_coef_num;	/**< Number of horizontal coefficients. */
	unsigned int dvs_ver_coef_num;	/**< Number of vertical coefficients. */
};

/** Optical black mode.
 */
enum sh_css_ob_mode {
	sh_css_ob_mode_none,
	sh_css_ob_mode_fixed,
	sh_css_ob_mode_raster
};

/** The 4 colors that a shading table consists of.
 *  For each color we store a grid of values.
 */
enum sh_css_sc_color {
	SH_CSS_SC_COLOR_GR, /**< Green on a green-red line */
	SH_CSS_SC_COLOR_R,  /**< Red */
	SH_CSS_SC_COLOR_B,  /**< Blue */
	SH_CSS_SC_COLOR_GB  /**< Green on a green-blue line */
};

/** White Balance configuration (Gain Adjust).
 *  All values are uinteger_bits.16-integer_bits fixed point values.
 */
struct sh_css_wb_config {
	unsigned int integer_bits; /**< */
	unsigned int gr;	/* unsigned <integer_bits>.<16-integer_bits> */
	unsigned int r;		/* unsigned <integer_bits>.<16-integer_bits> */
	unsigned int b;		/* unsigned <integer_bits>.<16-integer_bits> */
	unsigned int gb;	/* unsigned <integer_bits>.<16-integer_bits> */
};

/** Color Space Conversion settings.
 *  The data is s13-fraction_bits.fraction_bits fixed point.
 */
struct sh_css_cc_config {
	unsigned int fraction_bits;
	int matrix[3 * 3]; /**< RGB2YUV conversion matrix, signed
				   <13-fraction_bits>.<fraction_bits> */
};

/** Morping table, used for geometric distortion and chromatic abberration
 *  correction (GDCAC, also called GDC).
 *  This table describes the imperfections introduced by the lens, the
 *  advanced ISP can correct for these imperfections using this table.
 */
struct sh_css_morph_table {
	unsigned int height; /**< Table height */
	unsigned int width;  /**< Table width */
	unsigned short *coordinates_x[SH_CSS_MORPH_TABLE_NUM_PLANES];
	/**< X coordinates that describe the sensor imperfection */
	unsigned short *coordinates_y[SH_CSS_MORPH_TABLE_NUM_PLANES];
	/**< Y coordinates that describe the sensor imperfection */
};

/** Fixed pattern noise table. This contains the fixed patterns noise values
 *  obtained from a black frame capture.
 */
struct sh_css_fpn_table {
	short *data;		/**< Tbale content */
	unsigned int width;	/**< Table height */
	unsigned int height;	/**< Table width */
	unsigned int shift;	/**< */
};

/** Lens color shading table. This describes the color shading artefacts
 *  introduced by lens imperfections.
 */
struct sh_css_shading_table {
	unsigned int sensor_width;  /**< Native sensor width in pixels */
	unsigned int sensor_height; /**< Native sensor height in lines */
	unsigned int width;  /**< Number of data points per line per color */
	unsigned int height; /**< Number of lines of data points per color */
	unsigned int fraction_bits; /**< Bits of fractional part in the data
					    points */
	unsigned short *data[SH_CSS_SC_NUM_COLORS];
	/**< Table data, one array for each color. Use ia_css_sc_color to
	     index this array */
};

/** Gamma table, used for gamma correction.
 */
struct sh_css_gamma_table {
	unsigned short data[SH_CSS_GAMMA_TABLE_SIZE];
};

/** CTC table (need to explain CTC)
 */
struct sh_css_ctc_table {
	unsigned short data[SH_CSS_CTC_TABLE_SIZE];
};

/** sRGB Gamma table, used for sRGB gamma correction.
 */
struct sh_css_rgb_gamma_table {
	unsigned short data[SH_CSS_RGB_GAMMA_TABLE_SIZE];
};

/** XNR table
 */
struct sh_css_xnr_table {
	unsigned short data[SH_CSS_XNR_TABLE_SIZE];
};

/** Multi-Axes Color Correction (MACC) table. */
struct sh_css_macc_table {
	short data[SH_CSS_MACC_NUM_COEFS * SH_CSS_MACC_NUM_AXES];
};

/** Temporal noise reduction (TNR) configuration.
 */
struct sh_css_tnr_config {
	u0_16 gain;		/**< Gain (strength) of NR */
	u0_16 threshold_y;	/**< Motion sensitivity for Y */
	u0_16 threshold_uv;	/**< Motion sensitivity for U/V */
};

/** Optical black level configuration.
 */
struct sh_css_ob_config {
	enum sh_css_ob_mode mode; /**< Mode (Fixed / Raster) */
	u0_16 level_gr;    /**< Black level for GR pixels */
	u0_16 level_r;     /**< Black level for R pixels */
	u0_16 level_b;     /**< Black level for B pixels */
	u0_16 level_gb;    /**< Black level for GB pixels */
	unsigned short start_position; /**< Start position of OB area (used for
				      raster mode only). Valid range is [0..63]. */
	unsigned short end_position;  /**< End position of OB area (used for
				      raster mode only).
				      Valid range is [start_pos..64]. */
};

/** Defect pixel correction configuration.
 */
struct sh_css_dp_config {
	u0_16 threshold; /**< The threshold of defect Pixel Correction,
			      representing the permissible difference of
			      intensity between one pixel and its
			      surrounding pixels. Smaller values result
			      in more frequent pixel corrections. */
	u8_8 gain;	 /**< The sensitivity of mis-correction. ISP will
			      miss a lot of defects if the value is set
			      too large. */
};

/** Configuration used by Bayer Noise Reduction (BNR) and
 *  YCC noise reduction (YNR).
 */
struct sh_css_nr_config {
	u0_16 bnr_gain;	    /**< Strength of noise reduction (BNR) */
	u0_16 ynr_gain;	    /**< Strength of noise reduction (YNR */
	u0_16 direction;    /**< Sensitivity of Edge (BNR) */
	u0_16 threshold_cb; /**< Coring threshold for Cb (YNR) */
	u0_16 threshold_cr; /**< Coring threshold for Cr (YNR) */
};

/** Edge Enhancement (sharpen) configuration.
 */
struct sh_css_ee_config {
	u5_11 gain;	   /**< The strength of sharpness. */
	u8_8 threshold;    /**< The threshold that divides noises from
				       edge. */
	u5_11 detail_gain; /**< The strength of sharpness in pell-mell
				       area. */
};

/** Demosaic (bayer-to-rgb) configuration.
 */
struct sh_css_de_config {
	u0_16 pixelnoise;	   /**< Pixel noise used in moire elimination */
	u0_16 c1_coring_threshold; /**< Coring threshold for C1 */
	u0_16 c2_coring_threshold; /**< Coring threshold for C2 */
};

/** Gamma Correction configuration.
  */
struct sh_css_gc_config {
	unsigned short gain_k1; /**< */
	unsigned short gain_k2; /**< */
};



/* 
 * NOTE: 
 * Temporary code until the proper mechanism for
 * host to receive tetragon coordinates is implemented.
 * Important:
 *   Enable only one of the following defines.
*/
#define DVS_6AXIS_COORDS_1080P_UNITY
//#define DVS_6AXIS_COORDS_SMALL_UNITY
//#define DVS_6AXIS_COORDS_SMALL_WARPED

/* @GC: Comment on this & Remove the hard-coded values */
#if defined(DVS_6AXIS_COORDS_1080P_UNITY)
struct sh_css_dvs_6axis_config {
	unsigned int xcoords[18][31];
	unsigned int ycoords[18][31];
};
#elif defined(DVS_6AXIS_COORDS_SMALL_UNITY) ||\
	defined(DVS_6AXIS_COORDS_SMALL_WARPED)
struct sh_css_dvs_6axis_config {
	unsigned int xcoords[10][13];
	unsigned int ycoords[10][13];
};
#else
#error No default DVS coordinate table specified.
#endif


/** Advanced Noise Reduction configuration.
 *  This is also known as Low-Light.
 */
struct sh_css_anr_config {
	int threshold; /**< Threshold */
};

/** Eigen Color Demosaicing configuration.
 */
struct sh_css_ecd_config {
	unsigned short ecd_zip_strength;
	unsigned short ecd_fc_strength;
	unsigned short ecd_fc_debias;
};

/** Y(Luma) Noise Reduction configuration.
 */
struct sh_css_ynr_config {
	unsigned short edge_sense_gain_0;
	unsigned short edge_sense_gain_1;
	unsigned short corner_sense_gain_0;
	unsigned short corner_sense_gain_1;
};

/** Fringe Control configuration.
 */
struct sh_css_fc_config {
	unsigned char gain_exp;
	unsigned short gain_pos_0;
	unsigned short gain_pos_1;
	unsigned short gain_neg_0;
	unsigned short gain_neg_1;
	unsigned short crop_pos_0;
	unsigned short crop_pos_1;
	unsigned short crop_neg_0;
	unsigned short crop_neg_1;
};

/** Chroma Noise Reduction configuration.
 */
struct sh_css_cnr_config {
	unsigned char coring_u;
	unsigned char coring_v;
	unsigned char sense_gain_vy;
	unsigned char sense_gain_vu;
	unsigned char sense_gain_vv;
	unsigned char sense_gain_hy;
	unsigned char sense_gain_hu;
	unsigned char sense_gain_hv;
};

/** MACC
 */
struct sh_css_macc_config {
	unsigned char exp; /**< */
};

/** Chroma Tone Control configuration.
 */
struct sh_css_ctc_config {
	unsigned short y0;
	unsigned short y1;
	unsigned short y2;
	unsigned short y3;
	unsigned short y4;
	unsigned short y5;
	unsigned short ce_gain_exp;
	unsigned short x1;
	unsigned short x2;
	unsigned short x3;
	unsigned short x4;
};

/** Anti-Aliasing configuration.
 */
struct sh_css_aa_config {
	unsigned short scale;
};

/** Chroma Enhancement configuration.
 */
struct sh_css_ce_config {
	u0_16 uv_level_min; /**< */
	u0_16 uv_level_max; /**< */
};

/** Color Correction Matrix (YCgCo to RGB) settings.
 *  The data is
 *  s(13-SH_CSS_YUV2RGB_CCM_COEF_SHIFT).SH_CSS_YUV2RGB_CCM_COEF_SHIFT
 *  fixed point.
 */
struct sh_css_yuv2rgb_cc_config {
	int matrix[3 * 3]; /**< YUV2RGB conversion matrix, signed
	<13-SH_CSS_YUV2RGB_CCM_COEF_SHIFT>.<SH_CSS_YUV2RGB_CCM_COEF_SHIFT> */
};

/** Color Space Conversion (RGB to YUV) settings.
 *  The data is
 *  s(13-SH_CSS_RGB2YUV_CSC_COEF_SHIFT).SH_CSS_RGB2YUV_CSC_COEF_SHIFT
 *  fixed point.
 */
struct sh_css_rgb2yuv_cc_config {
	int matrix[3 * 3]; /**< RGB2YUV conversion matrix, signed
	<13-SH_CSS_RGB2YUV_CSC_COEF_SHIFT>.<SH_CSS_RGB2YUV_CSC_COEF_SHIFT> */
};

/** 3A configuration. This configures the 3A statistics collection
 *  module.
 */
struct sh_css_3a_config {
	u0_16 ae_y_coef_r;	/**< Weight of R for Y */
	u0_16 ae_y_coef_g;	/**< Weight of G for Y */
	u0_16 ae_y_coef_b;	/**< Weight of B for Y */
	u0_16 awb_lg_high_raw;	/**< AWB level gate high for raw */
	u0_16 awb_lg_low;	/**< AWB level gate low */
	u0_16 awb_lg_high;	/**< AWB level gate high */
	s0_15 af_fir1_coef[7];	/**< AF FIR coefficients of fir1 */
	s0_15 af_fir2_coef[7];	/**< AF FIR coefficients of fir2 */
};

/** eXtra Noise Reduction configuration.
 */
struct sh_css_xnr_config {
	unsigned int threshold;  /**< Threshold */
};

/** ISP filter configuration. This is a collection of configurations
 *  for each of the ISP filters (modules).
 *  We do not allow individual configurations to be set, only the
 *  complete grouping can be set. This is required to make sure all
 *  settings are always applied to the same frame.
 */
struct sh_css_isp_config {
	struct sh_css_wb_config  wb_config;  /**< White Balance config */
	struct sh_css_cc_config  cc_config;  /**< Color Correction config */
	struct sh_css_tnr_config tnr_config; /**< Temporal Noise Reduction */
	struct sh_css_ecd_config ecd_config; /**< Eigen Color Demosaicing */
	struct sh_css_ynr_config ynr_config; /**< Y(Luma) Noise Reduction */
	struct sh_css_fc_config  fc_config;  /**< Fringe Control */
	struct sh_css_cnr_config cnr_config; /**< Chroma Noise Reduction */
	struct sh_css_macc_config  macc_config;  /**< MACC */
	struct sh_css_ctc_config ctc_config; /**< Chroma Tone Control */
	struct sh_css_aa_config  aa_config;  /**< Anti-Aliasing */
	struct sh_css_ob_config  ob_config;  /**< Objective Black config */
	struct sh_css_dp_config  dp_config;  /**< Dead Pixel config */
	struct sh_css_nr_config  nr_config;  /**< Noise Reduction config */
	struct sh_css_ee_config  ee_config;  /**< Edge Enhancement config */
	struct sh_css_de_config  de_config;  /**< Demosaic config */
	struct sh_css_gc_config  gc_config;  /**< Gamma Correction config */
	struct sh_css_anr_config anr_config; /**< Advanced Noise Reduction */
	struct sh_css_yuv2rgb_cc_config yuv2rgb_cc_config; /**< Color
							Correction config */
	struct sh_css_rgb2yuv_cc_config rgb2yuv_cc_config; /**< Color
							Correction config */
	struct sh_css_3a_config  s3a_config; /**< 3A Statistics config */
	struct sh_css_xnr_config xnr_config; /**< eXtra Noise Reduction */
};

/* Guard this declaration, because this struct is also defined by
 * Sh3a_Types.h now
 */
#ifndef __SH_CSS_3A_OUTPUT__
#define __SH_CSS_3A_OUTPUT__

/* Workaround: hivecc complains about "tag "sh_css_3a_output" already declared"
   without this extra decl. */
struct sh_css_3a_output;

/** 3A statistics point. This structure describes the data stored
 *  in each 3A grid point.
 */
struct sh_css_3a_output {
	int ae_y;    /**< */
	int awb_cnt; /**< */
	int awb_gr;  /**< */
	int awb_r;   /**< */
	int awb_b;   /**< */
	int awb_gb;  /**< */
	int af_hpf1; /**< */
	int af_hpf2; /**< */
};

#endif /* End of guard __SH_CSS_3A_OUTPUT__ */

/* Types for the acceleration API.
 * These should be moved to sh_css_internal.h once the old acceleration
 * argument handling has been completed.
 * After that, interpretation of these structures is no longer needed
 * in the kernel and HAL.
*/

/** Blob descriptor.
 * This structure describes an SP or ISP blob.
 * It describes the test, data and bss sections as well as position in a
 * firmware file.
 * For convenience, it contains dynamic data after loading.
 */
struct sh_css_blob_info {
	/**< Static blob data */
	unsigned int offset;		/**< Blob offset in fw file */
	unsigned int size;		/**< Size of blob */
	unsigned int prog_name_offset;  /**< offset wrt hdr in bytes */
	unsigned int text_source;	/**< Position of text in blob */
	unsigned int text_size;		/**< Size of text section */
	unsigned int icache_source;	/**< Position of icache in blob */
	unsigned int icache_size;		/**< Size of icache section */
	unsigned int data_source;	/**< Position of data in blob */
	unsigned int data_target;	/**< Start of data in SP dmem */
	unsigned int data_size;		/**< Size of text section */
	unsigned int bss_target;	/**< Start position of bss in SP dmem */
	unsigned int bss_size;		/**< Size of bss section */
	/**< Dynamic data filled by loader */
	const void  *text;		/**< Text section within fw */
	const void  *data;		/**< Sp data section */
};

/** Type of acceleration.
 */
enum sh_css_acc_type {
	SH_CSS_ACC_NONE,	/**< Normal binary */
	SH_CSS_ACC_OUTPUT,	/**< Accelerator stage on output frame */
	SH_CSS_ACC_VIEWFINDER,	/**< Accelerator stage on viewfinder frame */
	SH_CSS_ACC_STANDALONE,	/**< Stand-alone acceleration */
};

/** Firmware types.
 */
enum sh_css_fw_type {
	sh_css_sp_firmware,	/**< Firmware for the SP */
	sh_css_isp_firmware,	/**< Firmware for the ISP */
	sh_css_acc_firmware	/**< Firmware for accelrations */
};

/** Isp memory section descriptor */
struct sh_css_isp_memory_section {
	unsigned int address;  /* In ISP mem */
	size_t       size;     /* In ISP mem */
};

struct sh_css_isp_memory_interface {
	struct sh_css_isp_memory_section parameters;
};

#if defined(IS_ISP_2300_SYSTEM)
enum sh_css_isp_memories {
	SH_CSS_ISP_PMEM0 = 0,
	SH_CSS_ISP_DMEM0,
	SH_CSS_ISP_VMEM0,
	SH_CSS_ISP_VAMEM0,
	SH_CSS_ISP_VAMEM1,
	N_SH_CSS_ISP_MEMORIES
};

#define SH_CSS_NUM_ISP_MEMORIES 5

#elif defined(IS_ISP_2400_SYSTEM)
enum sh_css_isp_memories {
	SH_CSS_ISP_PMEM0 = 0,
	SH_CSS_ISP_DMEM0,
	SH_CSS_ISP_VMEM0,
	SH_CSS_ISP_VAMEM0,
	SH_CSS_ISP_VAMEM1,
	SH_CSS_ISP_VAMEM2,
	SH_CSS_ISP_HMEM0,
	N_SH_CSS_ISP_MEMORIES
};

#define SH_CSS_NUM_ISP_MEMORIES 7

#else
#error "sh_css_types.h:  SYSTEM must be one of {ISP_2300_SYSTEM, ISP_2400_SYSTEM}"
#endif

/** Structure describing an ISP binary.
 * It describes the capabilities of a binary, like the maximum resolution,
 * support features, dma channels, uds features, etc.
 */
struct sh_css_binary_info {
	unsigned int		id; /* SH_CSS_BINARY_ID_* */
	unsigned int		mode;
	enum sh_css_acc_type	 type;
	const struct sh_css_blob_descr *blob;
	int			 num_output_formats;
	enum sh_css_frame_format output_formats[SH_CSS_MAX_NUM_FRAME_FORMATS];
	uint32_t		min_input_width;
	uint32_t		min_input_height;
	uint32_t		max_input_width;
	uint32_t		max_input_height;
	uint32_t		min_output_width;
	uint32_t		min_output_height;
	uint32_t		max_output_width;
	uint32_t		max_output_height;
	uint32_t		max_internal_width;
	uint32_t		max_internal_height;
	unsigned int		max_dvs_envelope_width;
	unsigned int		max_dvs_envelope_height;
	unsigned int		variable_resolution;
	unsigned int		variable_output_format;
	unsigned int		variable_vf_veceven;
	unsigned int		max_vf_log_downscale;
	unsigned int		top_cropping;
	unsigned int		left_cropping;
	unsigned int		s3atbl_use_dmem;
	int                      input;
	unsigned int		xmem_addr; /* hrt_vaddress */
	unsigned int		c_subsampling;
	unsigned int		output_num_chunks;
	unsigned int		input_num_chunks;
	unsigned int		num_stripes;
	unsigned int		pipelining;
	unsigned int		fixed_s3a_deci_log;
	unsigned int		isp_addresses; /* Address in ISP dmem */
	unsigned int		main_entry;    /* Address of entry fct */
	unsigned int		in_frame;  /* Address in ISP dmem */
	unsigned int		out_frame; /* Address in ISP dmem */
	unsigned int		in_data;  /* Address in ISP dmem */
	unsigned int		out_data; /* Address in ISP dmem */
	unsigned int		block_width;
	unsigned int		block_height;
	unsigned int		output_block_height;
	unsigned int		dvs_in_block_width;
	unsigned int		dvs_in_block_height;
	struct sh_css_isp_memory_interface
				 memory_interface[SH_CSS_NUM_ISP_MEMORIES];
	unsigned int		sh_dma_cmd_ptr;     /* In ISP dmem */
	unsigned int		isp_pipe_version;
	struct {
		unsigned char	 ctc;   /* enum sh_css_isp_memories */
		unsigned char	 gamma; /* enum sh_css_isp_memories */
		unsigned char	 xnr;   /* enum sh_css_isp_memories */
		unsigned char	 r_gamma; /* enum sh_css_isp_memories */
		unsigned char	 g_gamma; /* enum sh_css_isp_memories */
		unsigned char	 b_gamma; /* enum sh_css_isp_memories */
	} memories;
/* MW: Packing (related) bools in an integer ?? */
	struct {
		unsigned char     reduced_pipe;
		unsigned char     vf_veceven;
		unsigned char     dis;
		unsigned char     dvs_envelope;
		unsigned char     uds;
		unsigned char     dvs_6axis;
		unsigned char     block_output;
		unsigned char     ds;
		unsigned char     fixed_bayer_ds;
		unsigned char     bayer_fir_6db;
		unsigned char     raw_binning;
		unsigned char     continuous;
		unsigned char     s3a;
		unsigned char     fpnr;
		unsigned char     sc;
		unsigned char     dis_crop;
		unsigned char     dp_2adjacent;
		unsigned char     macc;
		unsigned char     ss;
		unsigned char     output;
		unsigned char     ref_frame;
		unsigned char     tnr;
		unsigned char     xnr;
		unsigned char     raw;
		unsigned char     params;
		unsigned char     gamma;
		unsigned char     ctc;
		unsigned char     ca_gdc;
		unsigned char     isp_addresses;
		unsigned char     in_frame;
		unsigned char     out_frame;
		unsigned char     high_speed;
		unsigned char     input_chunking;
		/* unsigned char	  padding[2]; */
	} enable;
	struct {
/* DMA channel ID: [0,...,HIVE_ISP_NUM_DMA_CHANNELS> */
		uint8_t		crop_channel;
		uint8_t		fpntbl_channel;
		uint8_t		multi_channel;
		uint8_t		raw_out_channel;
		uint8_t		sctbl_channel;
		uint8_t		ref_y_channel;
		uint8_t		ref_c_channel;
		uint8_t		tnr_channel;
		uint8_t		tnr_out_channel;
		uint8_t		dvs_in_channel;
		uint8_t		dvs_coords_channel;
		uint8_t		output_channel;
		uint8_t		c_channel;
		uint8_t		vfout_channel;
		uint8_t		claimed_by_isp;
		/* uint8_t	padding[0]; */
		struct {
			uint8_t	 channel;  /* Dma channel used */
			uint8_t	 height;   /* Buffer height */
			uint16_t stride;   /* Buffer stride */
		} raw;
	} dma;
	struct {
		unsigned short	 bpp;
		unsigned short	 use_bci;
		unsigned short	 woix;
		unsigned short	 woiy;
		unsigned short   extra_out_vecs;
		unsigned short	 vectors_per_line_in;
		unsigned short	 vectors_per_line_out;
		unsigned short	 vectors_c_per_line_in;
		unsigned short	 vectors_c_per_line_out;
		unsigned short	 vmem_gdc_in_block_height_y;
		unsigned short	 vmem_gdc_in_block_height_c;
		/* unsigned short	 padding; */
	} uds;
	unsigned		   blob_index;
	struct sh_css_binary_info *next;
};

/** Structure describing the SP binary.
 * It contains several address, either in ddr, sp_dmem or
 * the entry function in pmem.
 */
struct sh_css_sp_info {
	unsigned int	init_dmem_data; /**< data sect config, stored to dmem */
	unsigned int	per_frame_data; /**< Per frame data, stored to dmem */
	unsigned int	group;		/**< Per pipeline data, loaded by dma */
	unsigned int	output;		/**< SP output data, loaded by dmem */
	unsigned int	host_sp_queue;	/**< Host <-> SP queues */
	unsigned int	host_sp_com;/**< Host <-> SP commands */
	unsigned int	isp_started;	/**< Polled from sensor thread, csim only */
	unsigned int	sw_state;	/**< Polled from css */
	unsigned int	host_sp_queues_initialized; /**< Polled from the SP */
	unsigned int	sleep_mode;  /**< different mode to halt SP */
	unsigned int	invalidate_tlb;		/**< inform SP to invalidate mmu TLB */
	unsigned int	request_flash;	/**< inform SP to switch on flash for next frame */
	unsigned int	stop_copy_preview;	/**< suspend copy and preview pipe when capture */
	unsigned int	copy_preview_overlap; /**< indicate when to start preview pipe in continuous mode */
	unsigned int	copy_pack;	/**< use packed memory layout for raw data */
	unsigned int	debug_buffer_ddr_address;	/**< inform SP the address
	of DDR debug queue */
	unsigned int	ddr_parameter_address; /**< acc param ddrptr, sp dmem */
	unsigned int	ddr_parameter_size;    /**< acc param size, sp dmem */
	/* Entry functions */
	unsigned int	sp_entry;	/**< The SP entry function */
};

/** Accelerator firmware information.
 */
struct sh_css_acc_info {
	unsigned int	per_frame_data; /**< Dummy for now */
};

/** Firmware information.
 */
union sh_css_fw_union {
	struct sh_css_binary_info	isp; /**< ISP info */
	struct sh_css_sp_info		sp;  /**< SP info */
	struct sh_css_acc_info		acc; /**< Accelerator info */
};

/** Hmm section descriptor */
struct sh_css_hmm_section {
	hrt_vaddress			ddr_address;
	size_t				ddr_size; /* Disabled if 0 */
};

/** Hmm section descriptor */
struct sh_css_hmm_isp_interface {
	struct sh_css_hmm_section	parameters;
};

/** Firmware information.
 */
struct sh_css_fw_info {
	size_t			header_size; /**< size of fw header */
	enum sh_css_fw_type	type; /**< FW type */
	union sh_css_fw_union	info; /**< Binary info */
	struct sh_css_blob_info blob; /**< Blob info */
	/* Dynamic part */
	struct sh_css_fw_info  *next;
	unsigned		loaded;    /**< Firmware has been loaded */
	const unsigned char    *isp_code;  /**< ISP pointer to code */
	/**< Firmware handle between user space and kernel */
	unsigned int		handle;
	/**< Sections to copy from/to ISP */
	struct sh_css_hmm_isp_interface
				memory_interface[SH_CSS_NUM_ISP_MEMORIES];
};

struct sh_css_acc_fw;

/** Structure describing the SP binary of a stand-alone accelerator.
 */
 struct sh_css_acc_sp {
	void (*init) (struct sh_css_acc_fw *); /**< init for crun */
	unsigned      sp_prog_name_offset; /**< program name offset wrt hdr
						in bytes */
	unsigned      sp_blob_offset;	   /**< blob offset wrt hdr in bytes */
	void	     *entry;		   /**< Address of sp entry point */
	unsigned int *css_abort;	   /**< SP dmem abort flag */
	void	     *isp_code;		   /**< SP dmem address holding xmem
						address of isp code */
	struct sh_css_fw_info fw;	   /**< SP fw descriptor */
	const unsigned char *code;	   /**< ISP pointer of allocated
						SP code */
};

/** Acceleration firmware descriptor.
  * This descriptor descibes either SP code (stand-alone), or
  * ISP code (a separate pipeline stage).
  */
struct sh_css_acc_fw_hdr {
	enum sh_css_acc_type type;	/**< Type of accelerator */
	unsigned	isp_prog_name_offset; /**< program name offset wrt
						   header in bytes */
	unsigned	isp_blob_offset;      /**< blob offset wrt header
						   in bytes */
	unsigned int	isp_size;	      /**< Size of isp blob */
	const unsigned
	      char     *isp_code;	      /**< ISP pointer to code */
	struct sh_css_acc_sp  sp;  /**< Standalone sp code */
	unsigned	     loaded;    /**< Firmware has been loaded */
	/**< Firmware handle between user space and kernel */
	unsigned int	handle;
	struct sh_css_hmm_section parameters; /**< Current SP parameters */
};

/** Firmware structure.
  * This contains the header and actual blobs.
  * For standalone, it contains SP and ISP blob.
  * For a pipeline stage accelerator, it contains ISP code only.
  * Since its members are variable size, their offsets are described in the
  * header and computed using the access macros below.
  */
struct sh_css_acc_fw {
	struct sh_css_acc_fw_hdr header; /**< firmware header */
	/*
	char   isp_progname[];	  **< ISP program name
	char   sp_progname[];	  **< SP program name, stand-alone only
	unsigned char sp_code[];  **< SP blob, stand-alone only
	unsigned char isp_code[]; **< ISP blob
	*/
};

/* Access macros for firmware */
#define SH_CSS_ACC_OFFSET(t, f, n) ((t)((unsigned char *)(f)+(f->header.n)))
#define SH_CSS_ACC_SP_PROG_NAME(f) SH_CSS_ACC_OFFSET(const char *, f, \
						 sp.sp_prog_name_offset)
#define SH_CSS_ACC_ISP_PROG_NAME(f) SH_CSS_ACC_OFFSET(const char *, f, \
						 isp_prog_name_offset)
#define SH_CSS_ACC_SP_CODE(f)      SH_CSS_ACC_OFFSET(unsigned char *, f, \
						 sp.sp_blob_offset)
#define SH_CSS_ACC_SP_DATA(f)      (SH_CSS_ACC_SP_CODE(f) + \
					(f)->header.sp.fw.blob.data_source)
#define SH_CSS_ACC_ISP_CODE(f)     SH_CSS_ACC_OFFSET(unsigned char*, f,\
						 isp_blob_offset)
#define SH_CSS_ACC_ISP_SIZE(f)     ((f)->header.isp_size)

/* Binary name follows header immediately */
#define SH_CSS_EXT_ISP_PROG_NAME(f) ((const unsigned char *)(f)+sizeof(*f))

/** Structure to encapsulate required arguments for
 * initialization of SP DMEM using the SP itself
 * This is exported for accelerators implementing their own SP code.
 */
struct sh_css_sp_init_dmem_cfg {
	unsigned      done;	      /**< Init has been done */
	/* Next three should be hrt_vaddress */
	unsigned      ddr_data_addr;  /**< data segment address in ddr  */
	unsigned      dmem_data_addr; /**< data segment address in dmem */
	unsigned      dmem_bss_addr;  /**< bss segment address in dmem  */
	unsigned int  data_size;      /**< data segment size            */
	unsigned int  bss_size;       /**< bss segment size             */
};

enum sh_css_sp_sleep_mode {
	SP_DISABLE_SLEEP_MODE = 0,
	SP_SLEEP_AFTER_FRAME = 1 << 0,
	SP_SLEEP_AFTER_IRQ = 1 << 1
};

enum sh_css_sp_sw_state {
	SP_READY_TO_START = 0,
	SP_BOOTED,
	SP_TERMINATED
};

#endif /* _SH_CSS_TYPES_H_ */
