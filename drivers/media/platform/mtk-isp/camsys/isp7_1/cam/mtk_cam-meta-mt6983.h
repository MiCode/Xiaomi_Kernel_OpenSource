#ifndef __MTK_CAM_META_H__
#define __MTK_CAM_META_H__


/**
 * struct mtk_cam_uapi_meta_rect - rect info
 *
 * @left: The X coordinate of the left side of the rectangle
 * @top:  The Y coordinate of the left side of the rectangle
 * @width:  The width of the rectangle
 * @height: The height of the rectangle
 *
 * rect containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_rect {
	__s32 left;
	__s32 top;
	__u32 width;
	__u32 height;
};

/**
 * struct mtk_cam_uapi_meta_size - size info
 *
 * @width:  The width of the size
 * @height: The height of the size
 *
 * size containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_size {
	__u32 width;
	__u32 height;
};

/**
 *  A U T O  E X P O S U R E
 */

/*
 *  struct mtk_cam_uapi_ae_hist_cfg - histogram info for AE
 *
 *  @hist_en:    enable bit for current histogram, each histogram can
 *      be 0/1 (disabled/enabled) separately
 *  @hist_opt:   color mode config for current histogram (0/1/2/3/4:
 *      R/G/B/RGB mix/Y)
 *  @hist_bin:   bin mode config for current histogram (1/4: 256/1024 bin)
 *  @hist_y_hi:  ROI Y range high bound for current histogram
 *  @hist_y_low: ROI Y range low bound for current histogram
 *  @hist_x_hi:  ROI X range high bound for current histogram
 *  @hist_x_low: ROI X range low bound for current histogram
 */
struct mtk_cam_uapi_ae_hist_cfg {
	__s32 hist_en;
	__u8 hist_opt;
	__u8 hist_bin;
	__u16 hist_y_hi;
	__u16 hist_y_low;
	__u16 hist_x_hi;
	__u16 hist_x_low;
};

#define MTK_CAM_UAPI_ROI_MAP_BLK_NUM (128*128)
/*
 *  struct mtk_cam_uapi_ae_param - parameters for AE configurtion
 *
 *  @pixel_hist_win_cfg_le: window config for le histogram 0~5
 *           separately, uAEHistBin shold be the same
 *           for these 6 histograms
 *  @pixel_hist_win_cfg_se: window config for se histogram 0~5
 *           separately, uAEHistBin shold be the same
 *           for these 6 histograms
 *  @roi_hist_cfg_le : config for roi le histogram 0~3
 *           color mode/enable
 *  @roi_hist_cfg_se : config for roi se histogram 0~3
 *           color mode/enable
 *  @hdr_ratio: in HDR scenario, AE calculated hdr ratio
 *           (LE exp*iso/SE exp*iso*100) for current frame,
 *           default non-HDR scenario ratio=1000
 */
struct mtk_cam_uapi_ae_param {
	struct mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_le[6];
	struct mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_se[6];
	struct mtk_cam_uapi_ae_hist_cfg roi_hist_cfg_le[4];
	struct mtk_cam_uapi_ae_hist_cfg roi_hist_cfg_se[4];
	__u8  aai_r1_enable;
	__u8  aai_roi_map[MTK_CAM_UAPI_ROI_MAP_BLK_NUM];
	__u16 hdr_ratio; /* base 1 x= 1000 */
	__u32 act_win_x_start;
	__u32 act_win_x_end;
	__u32 act_win_y_start;
	__u32 act_win_y_end;
};

/**
 *  A U T O  W H I T E  B A L A N C E
 */

/* Maximum blocks that Mediatek AWB supports */
#define MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM (10)

/*
 *  struct mtk_cam_uapi_awb_param - parameters for AWB configurtion
 *
 *  @stat_en:                  AWB stat enable
 *  @windownum_x:              Number of horizontal AWB windows
 *  @windownum_y:              Number of vertical AWB windows
 *  @lowthreshold_r:           Low threshold of R
 *  @lowthreshold_g:           Low threshold of G
 *  @lowthreshold_b:           Low threshold of B
 *  @highthreshold_r:          High threshold of R
 *  @highthreshold_g:          High threshold of G
 *  @highthreshold_b:          High threshold of B
 *  @lightsrc_lowthreshold_r:  Low threshold of R for light source estimation
 *  @lightsrc_lowthreshold_g:  Low threshold of G for light source estimation
 *  @lightsrc_lowthreshold_b:  Low threshold of B for light source estimation
 *  @lightsrc_highthreshold_r: High threshold of R for light source estimation
 *  @lightsrc_highthreshold_g: High threshold of G for light source estimation
 *  @lightsrc_highthreshold_b: High threshold of B for light source estimation
 *  @pregainlimit_r:           Maximum limit clipping for R color
 *  @pregainlimit_g:           Maximum limit clipping for G color
 *  @pregainlimit_b:           Maximum limit clipping for B color
 *  @pregain_r:                unit module compensation gain for R color
 *  @pregain_g:                unit module compensation gain for G color
 *  @pregain_b:                unit module compensation gain for B color
 *  @valid_datawidth:          valid bits of statistic data
 *  @hdr_support_en:           support HDR mode
 *  @stat_mode:                Output format select <1>sum mode <0>average mode
 *  @error_ratio:              Programmable error pixel count by AWB window size
 *              (base : 256)
 *  @awbxv_win_r:              light area of right bound, the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_l:              light area of left bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_d:              light area of lower bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_u:              light area of upper bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @pregain2_r:               white balance gain of R color
 *  @pregain2_g:               white balance gain of G color
 *  @pregain2_b:               white balance gain of B color
 */
struct mtk_cam_uapi_awb_param {
	__u32 stat_en;
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
	__u32 format_shift;
	__u32 error_ratio;
	__u32 postgain_r;
	__u32 postgain_g;
	__u32 postgain_b;
	__u32 postgain2_hi_r;
	__u32 postgain2_hi_g;
	__u32 postgain2_hi_b;
	__u32 postgain2_med_r;
	__u32 postgain2_med_g;
	__u32 postgain2_med_b;
	__u32 postgain2_low_r;
	__u32 postgain2_low_g;
	__u32 postgain2_low_b;
	__s32 awbxv_win_r[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__s32 awbxv_win_l[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__s32 awbxv_win_d[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__s32 awbxv_win_u[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	__u32 csc_ccm[9];
	__u32 acc;
	__u32 med_region[4];
	__u32 low_region[4];
	__u32 pregain2_r;
	__u32 pregain2_g;
	__u32 pregain2_b;
};

/*
 * struct mtk_cam_uapi_dgn_param
 *
 *  @gain: digital gain to increase image brightness, 1 x= 1024
 */
struct mtk_cam_uapi_dgn_param {
	__u32 gain;
};

/*
 * struct mtk_cam_uapi_wb_param
 *
 *  @gain_r: white balance gain of R channel
 *  @gain_g: white balance gain of G channel
 *  @gain_b: white balance gain of B channel
 */
struct mtk_cam_uapi_wb_param {
	__u32 gain_r;
	__u32 gain_g;
	__u32 gain_b;
	__u32 clip;
};

/**
 *  A U T O  F O C U S
 */

/**
 * struct mtk_cam_uapi_af_param - af statistic parameters
 *  @roi: AF roi rectangle (in pixel) for AF statistic covered, including
 *    x, y, width, height
 *  @th_sat_g:  green channel pixel value saturation threshold (0~255)
 *  @th_h[3]: horizontal AF filters response threshold (0~50) for H0, H1,
 *    and H2
 *  @th_v:  vertical AF filter response threshold (0~50)
 *  @blk_pixel_xnum: horizontal number of pixel per block
 *  @blk_pixel_ynum: vertical number of pixel per block
 *  @fir_type: to select FIR filter by AF target type (0,1,2,3)
 *  @iir_type: to select IIR filter by AF target type (0,1,2,3)
 *  @data_gain[7]: gamma curve gain for AF source data
 */
struct mtk_cam_uapi_af_param {
	struct mtk_cam_uapi_meta_rect roi;
	__u32 th_sat_g;
	__u32 th_h[3];
	__u32 th_v;
	__u32 blk_pixel_xnum;
	__u32 blk_pixel_ynum;
	__u32 fir_type;
	__u32 iir_type;
	__u32 data_gain[7];
};

enum mtk_cam_uapi_flk_hdr_path_control {
	MTKCAM_UAPI_FKLO_HDR_1ST_FRAME = 0,
	MTKCAM_UAPI_FKLO_HDR_2ND_FRAME,
	MTKCAM_UAPI_FKLO_HDR_3RD_FRAME,
};

/*
 *  struct mtk_cam_uapi_flk_param
 *
 *  @input_bit_sel: maximum pixel value of flicker statistic input
 *  @offset_y: initial position for flicker statistic calculation in y direction
 *  @crop_y: number of rows which will be cropped from bottom
 *  @sgg_val[8]: Simple Gain and Gamma for noise reduction, sgg_val[0] is
 *               gain and sgg_val[1] - sgg_val[7] are gamma table
 *  @noise_thr: the noise threshold of pixel value, pixel value lower than
 *              this value is considered as noise
 *  @saturate_thr: the saturation threshold of pixel value, pixel value
 *                 higher than this value is considered as saturated
 *  @hdr_flk_src: flk source tap point selection
 */
struct mtk_cam_uapi_flk_param {
	__u32 input_bit_sel;
	__u32 offset_y;
	__u32 crop_y;
	__u32 sgg_val[8];
	__u32 noise_thr;
	__u32 saturate_thr;
	__u32 hdr_flk_src;
};

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

/*
 * struct mtk_cam_uapi_pde_param
 *
 * @pdi_max_size: the max required memory size for pd table
 * @pdo_max_size: the max required memory size for pd point output
 * @pdo_x_size: the pd points out x size
 * @pdo_y_size: the pd points out y size
 * @pd_table_offset: the offset of pd table in the meta_cfg
 */
struct mtk_cam_uapi_pde_param {
	__u32 pdi_max_size;
	__u32 pdo_max_size;
	__u32 pdo_x_size;
	__u32 pdo_y_size;
	__u32 pd_table_offset;
};

/**
 * struct mtk_cam_uapi_meta_hw_buf - hardware buffer info
 *
 * @offset: offset from the start of the device memory associated to the
 *    v4l2 meta buffer
 * @size: size of the buffer
 *
 * Some part of the meta buffers are read or written by statistic related
 * hardware DMAs. The hardware buffers may have different size among
 * difference pipeline.
 */
struct mtk_cam_uapi_meta_hw_buf {
	__u32 offset;
	__u32 size;
};

/**
 * struct mtk_cam_uapi_pdp_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:     stride value used by
 * @pdo_buf:     The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_pdp_stats {
	struct  mtk_cam_uapi_meta_size stats_src;
	__u32   stride;
	struct  mtk_cam_uapi_meta_hw_buf pdo_buf;
};

/**
 * struct mtk_cam_uapi_cpi_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:     stride value used by
 * @pdo_buf:     The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_cpi_stats {
	struct  mtk_cam_uapi_meta_size stats_src;
	__u32   stride;
	struct  mtk_cam_uapi_meta_hw_buf cpio_buf;
};

/*
 * struct mtk_cam_uapi_mqe_param
 *
 * @mqe_mode:
 */
struct mtk_cam_uapi_mqe_param {
	__u32 mqe_mode;
};

/*
 * struct mtk_cam_uapi_mobc_param
 *
 *
 */
struct mtk_cam_uapi_mobc_param {
	__u32 mobc_offst0;
	__u32 mobc_offst1;
	__u32 mobc_offst2;
	__u32 mobc_offst3;
	__u32 mobc_gain0;
	__u32 mobc_gain1;
	__u32 mobc_gain2;
	__u32 mobc_gain3;
};

/*
 * struct mtk_cam_uapi_lsc_param
 *
 *
 */
struct mtk_cam_uapi_lsc_param {
	__u32 lsc_ctl1;
	__u32 lsc_ctl2;
	__u32 lsc_ctl3;
	__u32 lsc_lblock;
	__u32 lsc_fblock;
	__u32 lsc_ratio;
	__u32 lsc_tpipe_ofst;
	__u32 lsc_tpipe_size;
};

/*
 * struct mtk_cam_uapi_sgg_param
 *
 *
 */
struct mtk_cam_uapi_sgg_param {
	__u32 sgg_pgn;
	__u32 sgg_gmrc_1;
	__u32 sgg_gmrc_2;
};

/*
 * struct mtk_cam_uapi_mbn_param
 *
 *
 */
struct mtk_cam_uapi_mbn_param {
	__u32 mbn_pow;
	__u32 mbn_dir;
	__u32 mbn_spar_hei;
	__u32 mbn_spar_pow;
	__u32 mbn_spar_fac;
	__u32 mbn_spar_con1;
	__u32 mbn_spar_con0;
};

/*
 * struct mtk_cam_uapi_cpi_param
 *
 *
 */
struct mtk_cam_uapi_cpi_param {
	__u32 cpi_th;
	__u32 cpi_pow;
	__u32 cpi_dir;
	__u32 cpi_spar_hei;
	__u32 cpi_spar_pow;
	__u32 cpi_spar_fac;
	__u32 cpi_spar_con1;
	__u32 cpi_spar_con0;
};

/*
 * struct mtk_cam_uapi_lsci_param
 *
 *
 */
struct mtk_cam_uapi_lsci_param {
	__u32 lsci_xsize;
	__u32 lsci_ysize;
};

/*
 *  struct mtk_cam_uapi_meta_mraw_stats_cfg
 *
 */
struct mtk_cam_uapi_meta_mraw_stats_cfg {
	__s8 mqe_enable;
	__s8 mobc_enable;
	__s8 lsc_enable;
	__s8 lsci_enable;

	struct mtk_cam_uapi_mqe_param mqe_param;
	struct mtk_cam_uapi_mobc_param mobc_param;
	struct mtk_cam_uapi_lsc_param lsc_param;
	struct mtk_cam_uapi_sgg_param sgg_param;
	struct mtk_cam_uapi_mbn_param mbn_param;
	struct mtk_cam_uapi_cpi_param cpi_param;
	struct mtk_cam_uapi_lsci_param lsci_param;

	// __u8 bytes[1024 * 94]; //lsci table(todo: size)
};

/**
 * struct mtk_cam_uapi_meta_mraw_stats_0 - capture buffer returns from
 *   camsys's mraw module after the frame is done. The buffer are
 *   not be pushed the other driver such as dip.
 *
 * @stats_enabled:   indicate that stats is ready or not in
 *       this buffer
 */
struct mtk_cam_uapi_meta_mraw_stats_0 {
	__u8   pdp_0_stats_enabled;
	__u8   pdp_1_stats_enabled;
	__u8   cpi_stats_enabled;

	struct mtk_cam_uapi_pdp_stats pdp_0_stats;
	struct mtk_cam_uapi_pdp_stats pdp_1_stats;
	struct mtk_cam_uapi_cpi_stats cpi_stats;
};

/**
 * Common stuff for all statistics
 */

#define MTK_CAM_UAPI_MAX_CORE_NUM (2)

/**
 * struct mtk_cam_uapi_pipeline_config - pipeline configuration
 *
 * @num_of_core: The number of isp cores
 */
struct mtk_cam_uapi_pipeline_config {
	__u32	num_of_core;
	struct	mtk_cam_uapi_meta_size core_data_size;
	__u32	core_pxl_mode_lg2;
};

/**
 *  A U T O  E X P O S U R E
 */

/* TODO: Need to check the size of MTK_CAM_AE_HIST_MAX_BIN*/
#define MTK_CAM_UAPI_AE_STATS_HIST_MAX_BIN (1024)

/**
 *  A E  A N D   A W B
 */

#define MTK_CAM_UAPI_AAO_BLK_SIZE (32)
#define MTK_CAM_UAPI_AAO_MAX_BLK_X (128)
#define MTK_CAM_UAPI_AAO_MAX_BLK_Y (128)
#define MTK_CAM_UAPI_AAO_MAX_BUF_SIZE (MTK_CAM_UAPI_AAO_BLK_SIZE \
					* MTK_CAM_UAPI_AAO_MAX_BLK_X \
					* MTK_CAM_UAPI_AAO_MAX_BLK_Y)

#define MTK_CAM_UAPI_AHO_BLK_SIZE (3)
#define MTK_CAM_UAPI_AAHO_HIST_SIZE  (6 * 1024 * MTK_CAM_UAPI_AHO_BLK_SIZE \
					+ 14 * 256 * MTK_CAM_UAPI_AHO_BLK_SIZE)
#define MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE  (MTK_CAM_UAPI_MAX_CORE_NUM * \
					MTK_CAM_UAPI_AAHO_HIST_SIZE)

/**
 * struct mtk_cam_uapi_ae_awb_stats - statistics of ae and awb
 *
 * @aao_buf:       The buffer for AAHO statistic hardware output.
 *        The maximum size of the buffer is defined with
 *        MTK_CAM_UAPI_AAO_MAX_BUF_SIZE
 * @aaho_buf:      The buffer for AAHO statistic hardware output.
 *        The maximum size of the buffer is defined with
 *        MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE.
 *
 * This is the AE and AWB statistic returned to user. From  our hardware's
 * point of view, we can't separate the AE and AWB output result, so I use
 * a struct to retutn them.
 */
struct mtk_cam_uapi_ae_awb_stats {
	__u32 awb_stat_en_status;
	__u32 awb_qbn_acc;
	__u32 ae_stat_en_status;
	struct mtk_cam_uapi_meta_hw_buf aao_buf;
	struct mtk_cam_uapi_meta_hw_buf aaho_buf;
};

/**
 *  A U T O  F O C U S
 */

#define MTK_CAM_UAPI_AFO_BLK_SIZ    (32)
#define MTK_CAM_UAPI_AFO_MAX_BLK_NUM (128 * 128)
#define MTK_CAM_UAPI_AFO_MAX_BUF_SIZE   (MTK_CAM_UAPI_AFO_BLK_SIZ \
						* MTK_CAM_UAPI_AFO_MAX_BLK_NUM)

/**
 * struct mtk_cam_uapi_af_stats - af statistics
 *
 * @blk_num_x: block number of horizontal direction
 * @blk_num_y: block number of vertical direction
 * @afo_buf:    the buffer for AAHO statistic hardware output. The maximum
 *      size of the buffer is defined with
 *      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_af_stats {
	__u32 blk_num_x;
	__u32 blk_num_y;
	struct mtk_cam_uapi_meta_hw_buf afo_buf;
};

/**
 *  F L I C K E R
 */

/* FLK's hardware output block size: 64 bits */
#define MTK_CAM_UAPI_FLK_BLK_SIZE (8)

/* Maximum block size (each line) of Mediatek flicker statistic */
#define MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM (6)

/* Maximum height (in pixel) that driver can support */
#define MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT (9000)
#define MTK_CAM_UAPI_FLK_MAX_BUF_SIZE                              \
	(MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM * \
	MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT)

/**
 * struct mtk_cam_uapi_flk_stats
 *
 * @flko_buf: the buffer for FLKO statistic hardware output. The maximum
 *         size of the buffer is defined with MTK_CAM_UAPI_FLK_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_flk_stats {
	struct mtk_cam_uapi_meta_hw_buf flko_buf;
};

/**
 *  T S F
 */

#define MTK_CAM_UAPI_TSFSO_SIZE (40 * 30 * 3 * 4)

/**
 * struct mtk_cam_uapi_tsf_stats - TSF statistic data
 *
 * @tsfo_buf: The buffer for tsf statistic hardware output. The buffer size
 *        is defined in MTK_CAM_UAPI_TSFSO_SIZE.
 *
 * This output is for Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_tsf_stats {
	struct mtk_cam_uapi_meta_hw_buf tsfo_r1_buf;
	struct mtk_cam_uapi_meta_hw_buf tsfo_r2_buf;
};

/**
 * struct mtk_cam_uapi_pd_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:	   stride value used by
 * @pdo_buf:	   The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_pd_stats {
	struct	mtk_cam_uapi_meta_size stats_src;
	__u32	stride;
	struct	mtk_cam_uapi_meta_hw_buf pdo_buf;
};

struct mtk_cam_uapi_timestamp {
	__u64 timestamp_buf[128];
};

/**
 *  T O N E
 */
#define MTK_CAM_UAPI_LTMSO_SIZE ((37*12*9 + 258) * 8)
#define MTK_CAM_UAPI_TNCSO_SIZE (680*510*2)
#define MTK_CAM_UAPI_TNCSHO_SIZE (1544)
#define MTK_CAM_UAPI_TNCSBO_SIZE (3888)
#define MTK_CAM_UAPI_TNCSYO_SIZE (68)

/**
 * struct mtk_cam_uapi_ltm_stats - Tone1 statistic data for
 *            Mediatek proprietary algorithm
 *
 * @ltmso_buf:  The buffer for ltm statistic hardware output. The buffer size
 *    is defined in MTK_CAM_UAPI_LTMSO_SIZE.
 * @blk_num_x: block number of horizontal direction
 * @blk_num_y:  block number of vertical direction
 *
 * For Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_ltm_stats {
	struct mtk_cam_uapi_meta_hw_buf ltmso_buf;
	__u8  blk_num_x;
	__u8  blk_num_y;
};

/**
 * struct mtk_cam_uapi_tnc_stats - Tone2 statistic data for
 *                 Mediatek proprietary algorithm
 *
 * @tncso_buf: The buffer for tnc statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSO_SIZE (680*510*2)
 */
struct mtk_cam_uapi_tnc_stats {
	struct mtk_cam_uapi_meta_hw_buf tncso_buf;
};

/**
 * struct mtk_cam_uapi_tnch_stats - Tone3 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsho_buf: The buffer for tnch statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSHO_SIZE (1544)
 */
struct mtk_cam_uapi_tnch_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsho_buf;
};

/**
 * struct mtk_cam_uapi_tncb_stats - Tone4 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsbo_buf: The buffer for tncb statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSBO_SIZE (3888)
 */
struct mtk_cam_uapi_tncb_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsbo_buf;
};

/**
 * struct mtk_cam_uapi_tncy_stats - Tone3 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsyo_buf: The buffer for tncy statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSYO_SIZE (68)
 */
struct mtk_cam_uapi_tncy_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsyo_buf;
};

/**
 * struct mtk_cam_uapi_act_stats - act statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @actso_buf: The buffer for tncy statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_ACTSO_SIZE (768)
 */
#define MTK_CAM_UAPI_ACTSO_SIZE (768)
struct mtk_cam_uapi_act_stats {
	struct mtk_cam_uapi_meta_hw_buf actso_buf;
};


/**
 *  V 4 L 2  M E T A  B U F F E R  L A Y O U T
 */

/*
 *  struct mtk_cam_uapi_meta_raw_stats_cfg
 *
 *  @ae_awb_enable: To indicate if AE and AWB should be enblaed or not. If
 *        it is 1, it means that we enable the following parts of
 *        hardware:
 *        (1) AE/AWB
 *        (2) aao
 *        (3) aaho
 *  @af_enable:     To indicate if AF should be enabled or not. If it is 1,
 *        it means that the AF and afo is enabled.
 *  @dgn_enable:    To indicate if dgn module should be enabled or not.
 *  @flk_enable:    If it is 1, it means flk and flko is enable. If ie is 0,
 *        both flk and flko is disabled.
 *  @tsf_enable:    If it is 1, it means tsfs and tsfso is enable. If ie is 0,
 *        both tsfs and tsfso is disabled.
 *  @wb_enable:     To indicate if wb module should be enabled or not.
 *  @pde_enable:    To indicate if pde module should be enabled or not.
 *  @ae_param:  AE Statistic window config
 *  @awb_param: AWB statistic configuration control
 *  @dgn_param: DGN settings
 *  @flk_param: Flicker statistic configuration
 *  @tsf_param: tsf statistic configuration
 *  @wb_param:  WB settings
 *  @pde_param: pde settings
 */
struct mtk_cam_uapi_meta_raw_stats_cfg {
	__s8 ae_awb_enable;
	__s8 af_enable;
	__s8 dgn_enable;
	__s8 flk_enable;
	__s8 tsf_enable;
	__s8 wb_enable;
	__s8 pde_enable;

	struct mtk_cam_uapi_ae_param ae_param;
	struct mtk_cam_uapi_awb_param awb_param;
	struct mtk_cam_uapi_af_param af_param;
	struct mtk_cam_uapi_dgn_param dgn_param;
	struct mtk_cam_uapi_flk_param flk_param;
	struct mtk_cam_uapi_tsf_param tsf_param;
	struct mtk_cam_uapi_wb_param wb_param;
	struct mtk_cam_uapi_pde_param pde_param;

	__u8 bytes[1024 * 94];
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_0 - capture buffer returns from camsys
 *    after the frame is done. The buffer are not be pushed the other
 *    driver such as dip.
 *
 * @ae_awb_stats_enabled: indicate that ae_awb_stats is ready or not in
 *       this buffer
 * @ltm_stats_enabled:    indicate that ltm_stats is ready or not in
 *       this buffer
 * @flk_stats_enabled:    indicate that flk_stats is ready or not in
 *       this buffer
 * @tsf_stats_enabled:    indicate that tsf_stats is ready or not in
 *       this buffer
 * @pde_stats_enabled:    indicate that pde_stats is ready or not in
 *       this buffer
 * @pipeline_config:      the pipeline configuration during processing
 * @pde_stats: the pde module stats
 */
struct mtk_cam_uapi_meta_raw_stats_0 {
	__u8 ae_awb_stats_enabled;
	__u8 ltm_stats_enabled;
	__u8 flk_stats_enabled;
	__u8 tsf_stats_enabled;
	__u8 tncy_stats_enabled;
	__u8 pde_stats_enabled;

	struct mtk_cam_uapi_pipeline_config pipeline_config;

	struct mtk_cam_uapi_ae_awb_stats ae_awb_stats;
	struct mtk_cam_uapi_ltm_stats ltm_stats;
	struct mtk_cam_uapi_flk_stats flk_stats;
	struct mtk_cam_uapi_tsf_stats tsf_stats;
	struct mtk_cam_uapi_tncy_stats tncy_stats;
	struct mtk_cam_uapi_pd_stats pde_stats;
	struct mtk_cam_uapi_timestamp timestamp;
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_1 - statistics before frame done
 *
 * @af_stats_enabled: indicate that lce_stats is ready or not in this buffer
 * @af_stats: AF statistics
 *
 * Any statistic output put in this structure should be careful.
 * The meta buffer needs copying overhead to return the buffer before the
 * all the ISP hardware's processing is finished.
 */
struct mtk_cam_uapi_meta_raw_stats_1 {
	__u8 af_stats_enabled;
	__u8 af_qbn_r6_enabled;
	struct mtk_cam_uapi_af_stats af_stats;
};

/**
 * struct mtk_cam_uapi_meta_raw_stats_2 - shared statistics buffer
 *
 * @act_stats_enabled:  indicate that act_stats is ready or not in this
 * buffer
 * @act_stats:  act statistics
 *
 * The statistic output in this structure may be pushed to the other
 * driver such as dip.
 *
 */
struct mtk_cam_uapi_meta_raw_stats_2 {
	__u8 act_stats_enabled;

	struct mtk_cam_uapi_act_stats act_stats;
};

/**
 * struct mtk_cam_uapi_meta_camsv_stats_0 - capture buffer returns from
 *	 camsys's camsv module after the frame is done. The buffer are
 *	 not be pushed the other driver such as dip.
 *
 * @pd_stats_enabled:	 indicate that pd_stats is ready or not in
 *			 this buffer
 */
struct mtk_cam_uapi_meta_camsv_stats_0 {
	__u8   pd_stats_enabled;

	struct mtk_cam_uapi_pd_stats pd_stats;
};

#define MTK_CAM_META_VERSION_MAJOR 1
#define MTK_CAM_META_VERSION_MINOR 11
#define MTK_CAM_META_PLATFORM_NAME "isp71"
#define MTK_CAM_META_CHIP_NAME "mt6983"


#endif /* __MTK_CAM_META_H__ */
