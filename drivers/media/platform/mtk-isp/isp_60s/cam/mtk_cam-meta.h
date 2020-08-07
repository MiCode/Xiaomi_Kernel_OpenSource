/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __MTK_CAM_META_H__
#define __MTK_CAM_META_H__

/**
 * Common stuff for all statistics
 */

/**
 * struct mtk_cam_uapi_meta_hw_buf - hardware buffer info
 *
 * @offset:	offset from the start of the device memory associated to the
 *		v4l2 meta buffer
 * @size:	size of the buffer
 *
 * Some part of the meta buffers are read or written by statistic related
 * hardware DMAs. The hardware buffers may have different size among
 * difference pipeline.
 */
struct mtk_cam_uapi_meta_hw_buf {
	__u32	offset;
	__u32	size;
};

/**
 * struct mtk_cam_uapi_meta_rect - rect info
 *
 * @left:	The X coordinate of the left side of the rectangle
 * @top:	The Y coordinate of the left side of the rectangle
 * @width:	The width of the rectangle
 * @height:	The height of the rectangle
 *
 * rect containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_rect {
	__s32   left;
	__s32   top;
	__u32   width;
	__u32   height;
};

/**
 * struct mtk_cam_uapi_meta_area - area info
 *
 * @width:	The width of the area
 * @height:	The height of the area
 *
 * area containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_area {
	__u32   width;
	__u32   height;
};

/**
 *	A U T O  E X P O S U R E
 */

/*
 *  struct mtk_cam_uapi_ae_hist_cfg - histogram info for AE
 *
 *  @hist_en:	 enable bit for current histogram, each histogram can
 *		 be 0/1 (disabled/enabled) separately
 *  @hist_opt:	 color mode config for current histogram (0/1/2/3/4:
 *		 R/G/B/RGB mix/Y)
 *  @hist_bin:	 bin mode config for current histogram (1/4: 256/1024 bin)
 *  @hist_y_hi:  ROI Y range high bound ratio for current histogram,
 *		 range [0,100]
 *  @hist_y_low: ROI Y range low bound ratio for current histogram,
 *		 range [0,100]
 *  @hist_x_hi:	 ROI X range high bound ratio for current histogram,
 *               range [0,100]
 *  @hist_x_low: ROI X range low bound ratio for current histogram,
 *		 range [0,100]
 */
struct mtk_cam_uapi_ae_hist_cfg {
	__s32	hist_en;
	__u8	hist_opt;
	__u8	hist_bin;
	__u16	hist_y_hi;
	__u16	hist_y_low;
	__u16	hist_x_hi;
	__u16	hist_x_low;
};

/*
 *  struct mtk_cam_uapi_ae_param - parameters for AE configurtion
 *
 *  @stats_src:		    source width and height of the statistics
 *  @pixel_hist_win_cfg_le: window config for le histogram 0~5
 *			    separately, uAEHistBin shold be the same
 *			    for these 6 histograms
 *  @pixel_hist_win_cfg_se: window config for se histogram 0~5
 *			    separately, uAEHistBin shold be the same
 *			    for these 6 histograms
 *  @hdr_ratio:		    in HDR scenario, AE calculated hdr ratio
 *			    (LE exp*iso/SE exp*iso*100) for current frame,
 *			    default non-HDR scenario ratio=100
 */
struct mtk_cam_uapi_ae_param {
	struct	mtk_cam_uapi_meta_area stats_src;
	struct	mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_le[6];
	struct	mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_se[6];
	__u16	hdr_ratio; /* base 1 x= 100 */
};

/* maximum width/height in pixel that platform supports */
#define MTK_CAM_UAPI_AE_STATS_MAX_WIDTH		120
#define MTK_CAM_UAPI_AE_STATS_MAX_HEIGHT	90

/* TODO: Need to check the size of MTK_CAM_AE_HIST_MAX_BIN*/
#define MTK_CAM_UAPI_AE_STATS_HIST_MAX_BIN	1024

/**
 *	A U T O  W H I T E  B A L A N C E
 */

/* Maximum blocks that Mediatek AWB supports */
#define MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM	 10

/*
 *  struct mtk_cam_uapi_ae_awb_stats - parameters for AE configurtion
 *
 *  @windownum_x:	       Number of horizontal AWB windows
 *  @windownum_y:	       Number of vertical AWB windows
 *  @lowthreshold_r:	       Low threshold of R
 *  @lowthreshold_g:	       Low threshold of G
 *  @lowthreshold_b:	       Low threshold of B
 *  @highthreshold_r:	       High threshold of R
 *  @highthreshold_g:	       High threshold of G
 *  @highthreshold_b:	       High threshold of B
 *  @lightsrc_lowthreshold_r:  Low threshold of R for light source estimation
 *  @lightsrc_lowthreshold_g:  Low threshold of G for light source estimation
 *  @lightsrc_lowthreshold_b:  Low threshold of B for light source estimation
 *  @lightsrc_highthreshold_r: High threshold of R for light source estimation
 *  @lightsrc_highthreshold_g: High threshold of G for light source estimation
 *  @lightsrc_highthreshold_b: High threshold of B for light source estimation
 *  @pregainlimit_r:	       Maximum limit clipping for R color
 *  @pregainlimit_g:	       Maximum limit clipping for G color
 *  @pregainlimit_b:	       Maximum limit clipping for B color
 *  @pregain_r:		       unit module compensation gain for R color
 *  @pregain_g:		       unit module compensation gain for G color
 *  @pregain_b:		       unit module compensation gain for B color
 *  @valid_datawidth:	       valid bits of statistic data
 *  @hdr_support_en:	       support HDR mode
 *  @stat_mode:		       Output format select <1>sum mode <0>average mode
 *  @error_threshold:	       error pixel threshold for the allowed total
 *  @mo_error_threshold:       motion error pixel threshold for the allowed
 *			       total
 *  @error_shift_bits:	       Programmable error count shift bits: 0 ~ 7. AWB
 *			       statistics provide 4-bits error count output only
 *  @error_ratio:	       Programmable error pixel count by AWB window size
 *			       (base : 256)
 *  @mo_error_ratio:	       Programmable motion error pixel count by AWB
 *			       window size (base : 256)
 *  @awbxv_win_r:	       light area of right bound, the size is defined in
 *			       MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_l:	       light area of left bound the size is defined in
 *			       MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_d:	       light area of lower bound the size is defined in
 *			       MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_u:	       light area of upper bound the size is defined in
 *			       MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @pregain2_r:	       white balance gain of R color
 *  @pregain2_g:	       white balance gain of G color
 *  @pregain2_b:	       white balance gain of B color
 */
struct mtk_cam_uapi_awb_param {
	__u32 windownum_x;
	__u32 windownum_y;
	__u32 lowthreshold_r;
	__u32 lowthreshold_g;
	__u32 lowthreshold_b;
	__u32 highthreshold_r;
	__u32 highthreshold_g;
	__u32 highthreshold_b;
	__u32 lightsrc_lowthreshold_r;
	__u32 lightsrc_lowthreshold_g;
	__u32 lightsrc_lowthreshold_b;
	__u32 lightsrc_highthreshold_r;
	__u32 lightsrc_highthreshold_g;
	__u32 lightsrc_highthreshold_b;
	__u32 pregainlimit_r;
	__u32 pregainlimit_g;
	__u32 pregainlimit_b;
	__u32 pregain_r;
	__u32 pregain_g;
	__u32 pregain_b;
	__u32 valid_datawidth;
	__u32 hdr_support_en;
	__u32 stat_mode;
	__u32 error_threshold;
	__u32 mo_error_threshold;
	__u32 error_shift_bits;
	__u32 error_ratio;
	__u32 mo_error_ratio;
	__u32 awbxv_win_r[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__u32 awbxv_win_l[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__u32 awbxv_win_d[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__u32 awbxv_win_u[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__u32 pregain2_r;
	__u32 pregain2_g;
	__u32 pregain2_b;
};

/**
 *	A E  A N D   A W B  C O M M O N
 */

#define MTK_CAM_UAPI_AAO_BLK_SIZE	32
#define MTK_CAM_UAPI_AAO_MAX_BLK_NUM	(128 * 128)
#define MTK_CAM_UAPI_AAO_MAX_BUF_SIZE	(MTK_CAM_UAPI_AAO_BLK_SIZE \
					 * MTK_CAM_UAPI_AAO_MAX_BLK_NUM)

#define MTK_CAM_UAPI_AHO_BLK_SIZE	3
#define MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE	(6 * 1024 * MTK_CAM_UAPI_AHO_BLK_SIZE \
					 + 6 * 256 * MTK_CAM_UAPI_AHO_BLK_SIZE)

/**
 * struct mtk_cam_uapi_ae_awb_stats - statistics of ae and awb
 *
 * @window_width:  source width and heitgh of the statistics. The final input
 *		   size to ae and awb module is determined by driver according
 *		   the crop, resize, bining configuration in the pipeline for
 *		   sensor to AE/AWB block. This field let user get the
 *		   real input size.
 * @aao_buf:	   The buffer for AAHO statistic hardware output.
 *		   The maximum size of the buffer is defined with
 *		   MTK_CAM_UAPI_AAO_MAX_BUF_SIZE
 * @aaoh_buf:	   The buffer for AAHO statistic hardware output.
 *		   The maximum size of the buffer is defined with
 *		   MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE.
 *
 * This is the AE and AWB statistic returned to user. From  our hardware's
 * point of view, we can't separate the AE and AWB output result, so I use
 * a struct to retutn them.
 */
struct mtk_cam_uapi_ae_awb_stats {
	struct	mtk_cam_uapi_meta_area stats_src;
	struct	mtk_cam_uapi_meta_hw_buf aao_buf;
	struct	mtk_cam_uapi_meta_hw_buf aaho_buf;
};

/*
 * struct mtk_cam_uapi_dgn_param
 *
 *  @gain: digital gain to increase image brightness, 1 x= 1024
 */
struct mtk_cam_uapi_dgn_param {
	__u16	gain;
};

/*
 * struct mtk_cam_uapi_wb_param
 *
 *  @r_gain: white balance gain of R channel
 *  @g_gain: white balance gain of G channel
 *  @b_gain: white balance gain of B channel
 */
struct mtk_cam_uapi_wb_param {
	__u32 gain_r;
	__u32 gain_g;
	__u32 gain_b;
};

/**
 *	A U T O  F O C U S
 */

#define MTK_CAM_UAPI_AFO_BLK_SIZ		44
#define MTK_CAM_UAPI_AFO_MAX_BLK_NUM		(128 * 128)
#define MTK_CAM_UAPI_AFO_MAX_BUF_SIZE		(MTK_CAM_UAPI_AFO_BLK_SIZ \
						* MTK_CAM_UAPI_AFO_MAX_BLK_NUM)

/**
 * struct mtk_cam_uapi_af_param - af statistic parameters
 *  @roi:	AF roi rectangle (in pixel) for AF statistic covered, including
 *		x, y, width, height
 *  @th_sat_r:	red channel pixel value saturation threshold (0~255)
 *  @th_sat_g:	green channel pixel value saturation threshold (0~255)
 *  @th_sat_b:	blue channel pixel value saturation threshold (0~255)
 *  @th_h[3]:	horizontal AF filters response threshold (0~50) for H0, H1,
 *		and H2
 *  @th_v:	vertical AF filter response threshold (0~50)
 */
struct mtk_cam_uapi_af_param {
	struct mtk_cam_uapi_meta_rect roi;
	__u32 th_sat_r;
	__u32 th_sat_g;
	__u32 th_sat_b;
	__u32 th_h[3];
	__u32 th_v;
};

/**
 * struct mtk_cam_uapi_af_stats - af statistics
 *
 * @blk_xnum: block number of horizontal direction
 * @blk_ynum: block number of vertical direction
 * @buf:	 the buffer for AAHO statistic hardware output. The maximum
 *		 size of the buffer is defined with
 *		 MTK_CAM_UAPI_AFO_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_af_stats {
	__u32	blk_num_x;
	__u32	blk_num_y;
	struct	mtk_cam_uapi_meta_hw_buf afo_buf;
};

/**
 *	F L I C K E R
 */

/* FLK's hardware output block size: 48 bits */
#define MTK_CAM_UAPI_FLK_BLK_SIZE		6

/* Maximum block size (each line) of Mediatek flicker statistic */
#define MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM	6

/* Maximum height (in pixel) that driver can support */
#define MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT	6000
#define MTK_CAM_UAPI_FLK_MAX_BUF_SIZE	(MTK_CAM_UAPI_FLK_BLK_SIZE \
					 * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM \
					 * MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT)

/*
 *  struct mtk_cam_uapi_flk_param
 *
 *  @input_bit_sel: maximum pixel value of flicker statistic input
 *  @offset_x: initial position for flicker statistic calculation in x direction
 *  @offset_y: initial position for flicker statistic calculation in y direction
 *  @crop_x: number of columns which will be cropped from right
 *  @crop_y: number of rows which will be cropped from bottom
 *  @num_x: number of blocks in x direction
 *  @num_y: number of blocks in y direction
 *  @sgg_val[8]: Simple Gain and Gamma for noise reduction, sgg_val[0] is
 *               gain and sgg_val[1] - sgg_val[7] are gamma table
 *  @noise_thr: the noise threshold of pixel value, pixel value lower than
 *              this value is considered as noise
 *  @saturate_thr: the saturation threshold of pixel value, pixel value
 *                 higher than this value is considered as saturated
 */
struct mtk_cam_uapi_flk_param {
	__u32 input_bit_sel;
	__u32 offset_x;
	__u32 offset_y;
	__u32 crop_x;
	__u32 crop_y;
	__u32 num_x;
	__u32 num_y;
	__u32 sgg_val[8];
	__u32 noise_thr;
	__u32 saturate_thr;
};

/**
 * struct mtk_cam_uapi_flk_stats
 *
 * @flko_buf: the buffer for FLKO statistic hardware output. The maximum
 *	      size of the buffer is defined with MTK_CAM_UAPI_FLK_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_flk_stats {
	struct	mtk_cam_uapi_meta_hw_buf flko_buf;
};

/**
 *	T S F
 */

#define MTK_CAM_UAPI_TSFSO_SIZE		(40 * 30 * 3 * 4)

/*
 * struct mtk_cam_uapi_tsf_param
 *
 *  @horizontal_num: block number of horizontal direction
 *  @vertical_num:   block number of vertical direction
 */
struct mtk_cam_uapi_tsf_param {
	__u32 horizontal_num;
	__u32 vertical_num;
};

/**
 * struct mtk_cam_uapi_tsf_stats - TSF statistic data
 *
 * @tsfo_buf: The buffer for tsf statistic hardware output. The buffer size
 *			 is defined in MTK_CAM_UAPI_TSFSO_SIZE.
 *
 * This output is for Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_tsf_stats {
	struct mtk_cam_uapi_meta_hw_buf tsfo_buf;
};

/**
 *	T O N E
 */
#define MTK_CAM_UAPI_LTMSO_SIZE		18036
#define MTK_CAM_UAPI_LCESO_SIZE		(680 * 510 * 2)
#define MTK_CAM_UAPI_LCESHO_SIZE	1548

/**
 * struct mtk_cam_uapi_ltm_stats - Tone1 statistic data for
 *				   Mediatek proprietary algorithm
 *
 * @ltmso_buf:	The buffer for ltm statistic hardware output. The buffer size
 *		is defined in MTK_CAM_UAPI_LTMSO_SIZE.
 *
 * For Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_ltm_stats {
	struct mtk_cam_uapi_meta_hw_buf ltmso_buf;
};

/**
 * struct mtk_cam_uapi_lce_stats - Tone2 statistic data
 *
 * @stats_out:	The x_size and y_size of the lcseo output in lceso_buf
 * @stride:	The stride of the lcseo output in lceso_buf
 * @lceso_buf:	The buffer for lceso statistic hardware output. The buffer size
 *		is defined in MTK_CAM_UAPI_LCESO_SIZE.
 *
 * For Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_lce_stats {
	struct mtk_cam_uapi_meta_area stats_out;
	__u32  stride;
	struct mtk_cam_uapi_meta_hw_buf lceso_buf;
};

/**
 * struct mtk_cam_uapi_lceh_stats - Tone3 statistic data
 *
 * @lcesho_buf:	The buffer for lceho statistic hardware output. The buffer size
 *		is defined in MTK_CAM_UAPI_LCESHO_SIZE.
 *
 * For Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_lceh_stats {
	struct mtk_cam_uapi_meta_hw_buf lcesho_buf;
};

/**
 *	L M V
 */
#define MTK_CAM_UAPI_LMVO_SIZE		256

/**
 * struct mtk_cam_uapi_lmv_stats - LMV statistic
 *
 * @lmvo_buf:	The buffer for lvm statistic hardware output. The buffer size
 *		is defined in MTK_CAM_UAPI_LMVO_SIZE.
 */
struct mtk_cam_uapi_lmv_stats {
	struct mtk_cam_uapi_meta_hw_buf lmvo_buf;
};



/**
 *	V 4 L 2  M E T A  B U F F E R  L A Y O U T
 */

/*
 *  struct mtk_cam_uapi_meta_raw_stats_cfg
 *
 *  @ae_awb_enable: To indicate if AE and AWB should be enblaed or not. If
 *		    it is 1, it means that we enable the following parts of
 *		    hardware:
 *		    (1) AE/AWB
 *		    (2) aao
 *		    (3) aaho
 *  @af_enable:	    To indicate if AF should be enabled or not. If it is 1,
 *		    it means that the AF and afo is enabled.
 *  @dgn_enable:    To indicate if dgn module should be enabled or not.
 *  @flk_enable:    If it is 1, it means flk and flko is enable. If ie is 0,
 *		    both flk and flko is disabled.
 *  @tsf_enable:    If it is 1, it means tsfs and tsfso is enable. If ie is 0,
 *		    both tsfs and tsfso is disabled.
 *  @wb_enable:	    To indicate if wb module should be enabled or not.
 *  @ae_param:	    AE Statistic window config
 *  @awb_param:	    AWB statistic configuration control
 *  @dgn_param:	    DGN settings
 *  @flk_param:	    Flicker statistic configuration
 *  @tsf_param:	    tsf statistic configuration
 *  @wb_param:	    WB settings
 */
struct mtk_cam_uapi_meta_raw_stats_cfg {
	__s8   ae_awb_enable;
	__s8   af_enable;
	__s8   dgn_enable;
	__s8   flk_enable;
	__s8   tsf_enable;
	__s8   wb_enable;

	struct mtk_cam_uapi_ae_param  ae_param;
	struct mtk_cam_uapi_awb_param awb_param;
	struct mtk_cam_uapi_af_param  af_param;
	struct mtk_cam_uapi_dgn_param dgn_param;
	struct mtk_cam_uapi_flk_param flk_param;
	struct mtk_cam_uapi_tsf_param tsf_param;
	struct mtk_cam_uapi_wb_param  wb_param;

	__u8 bytes[1024 * 40];
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_0 - capture buffer returns from camsys
 *	 after the frame is done. The buffer are not be pushed the other
 *	 driver such as dip.
 *
 * @ae_awb_stats_enabled: indicate that ae_awb_stats is ready or not in
 *			 this buffer
 * @ltm_stats_enabled:	 indicate that ltm_stats is ready or not in
 *			 this buffer
 * @flk_stats_enabled:	 indicate that flk_stats is ready or not in
 *			 this buffer
 * @tsf_stats_enabled:	 indicate that tsf_stats is ready or not in
 *			 this buffer
 *
 */
struct mtk_cam_uapi_meta_raw_stats_0 {
	__u8   ae_awb_stats_enabled;
	__u8   ltm_stats_enabled;
	__u8   flk_stats_enabled;
	__u8   tsf_stats_enabled;

	struct mtk_cam_uapi_ae_awb_stats ae_awb_stats;
	struct mtk_cam_uapi_ltm_stats ltm_stats;
	struct mtk_cam_uapi_flk_stats flk_stats;
	struct mtk_cam_uapi_tsf_stats tsf_stats;
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_1 - statistics before frame done
 *
 * @af_stats_enabled: indicate that lce_stats is ready or not in this buffer
 * @af_stats:	      AF statistics
 *
 * Any statistic output put in this structure should be careful.
 * The meta buffer needs copying overhead to return the buffer before the
 * all the ISP hardware's processing is finished.
 */
struct mtk_cam_uapi_meta_raw_stats_1 {
	__u8 af_stats_enabled;
	struct mtk_cam_uapi_af_stats af_stats;
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_2 - shared statistics buffer
 *
 * @lce_stats_enabled:	indicate that lce_stats is ready or not in this buffer
 * @lceh_stats_enabled:	indicate that the lceh_stats is ready or not in this
 *			buffer
 * @lmv_stats_enabled:	indicate that the lmv_stats is ready or not in this
 *			buffer
 * @lcs_stats:	lcs statistics
 * @lceh_stats:	lceh statistics
 * @lmv_stats:	lmv statistics
 *
 * The statistic output in this structure may be pushed to the other
 * driver such as dip.
 *
 */
struct mtk_cam_uapi_meta_raw_stats_2 {
	__u8 lce_stats_enabled;
	__u8 lceh_stats_enabled;
	__u8 lmv_stats_enabled;

	struct mtk_cam_uapi_lce_stats lce_stats;
	struct mtk_cam_uapi_lceh_stats lceh_stats;
	struct mtk_cam_uapi_lmv_stats lmv_stats;
};

#endif /* __MTK_CAM_META_H__ */
