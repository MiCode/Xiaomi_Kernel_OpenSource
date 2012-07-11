/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_VFE7X_H__
#define __MSM_VFE7X_H__
#include <media/msm_camera.h>
#include <mach/camera.h>
#include <linux/list.h>
#include "msm.h"
#include "msm_vfe_stats_buf.h"

/*8 DSP buffers, 3 - ping, pong, free*/
#define FREE_BUF_ARR_SIZE 5

struct cmd_id_map {
	uint32_t isp_id;
	uint32_t vfe_id;
	uint32_t queue;
	char isp_id_name[64];
	char vfe_id_name[64];
} __packed;

struct msg_id_map {
	uint32_t vfe_id;
	uint32_t isp_id;
} __packed;

struct table_cmd {
	struct list_head list;
	void *cmd;
	int size;
	int queue;
} __packed;

struct vfe_msg {
	struct list_head list;
	void *cmd;
	int len;
	int id;
} __packed;

struct buf_info {
	/* Buffer */
	struct msm_free_buf ping;
	struct msm_free_buf pong;
	struct msm_free_buf free_buf;
	/*Array for holding the free buffer if more than one*/
	struct msm_free_buf free_buf_arr[FREE_BUF_ARR_SIZE];
	int free_buf_cnt;
	int frame_cnt;
} __packed;

struct prev_free_buf_info {
	struct msm_free_buf buf[3];
};

struct vfe_cmd_start {
	uint32_t input_source:1;
	uint32_t mode_of_operation:1;
	uint32_t snap_number:4;
	uint32_t /* reserved */ : 26;

	/* Image Pipeline Modules */
	uint32_t blacklevel_correction_enable:1;
	uint32_t lens_rolloff_correction_enable:1;
	uint32_t white_balance_enable:1;
	uint32_t rgb_gamma_enable:1;
	uint32_t luma_noise_reductionpath_enable:1;
	uint32_t adaptive_spatialfilter_enable:1;
	uint32_t chroma_subsample_enable:1;
	uint32_t /* reserved */ : 25;

	/* The dimension fed to the statistics module */
	uint32_t last_pixel:12;
	uint32_t /* reserved */ : 4;
	uint32_t last_line:12;
	uint32_t /* reserved */ : 4;
} __packed;

struct vfe2x_ctrl_type {
	struct buf_info prev;
	struct buf_info video;
	struct buf_info snap;
	struct buf_info raw;
	struct buf_info thumb;
	struct prev_free_buf_info free_buf;
	struct buf_info zsl_prim;
	struct buf_info zsl_sec;
	struct prev_free_buf_info zsl_free_buf[2];


	spinlock_t  table_lock;
	struct list_head table_q;
	uint32_t tableack_pending;
	uint32_t vfeFrameId;

	spinlock_t vfe_msg_lock;
	struct list_head vfe_msg_q;

	struct vfe_cmd_start start_cmd;
	uint32_t start_pending;
	uint32_t vfe_started;
	uint32_t stop_pending;
	uint32_t update_pending;

	spinlock_t liveshot_enabled_lock;
	uint32_t liveshot_enabled;

	/* v4l2 subdev */
	struct v4l2_subdev subdev;
	struct platform_device *pdev;
	struct clk *vfe_clk[3];
	spinlock_t  sd_notify_lock;
	uint32_t    reconfig_vfe;
	uint32_t    zsl_mode;
	spinlock_t  stats_bufq_lock;
	struct msm_stats_bufq_ctrl stats_ctrl;
	struct msm_stats_ops stats_ops;
	unsigned long stats_we_buf_ptr[3];
	unsigned long stats_af_buf_ptr[3];
	int num_snap;
} __packed;

struct vfe_frame_extra {
	uint32_t	bl_evencol:23;
	uint32_t	rvd1:9;
	uint32_t	bl_oddcol:23;
	uint32_t	rvd2:9;

	uint32_t	d_dbpc_stats_hot:16;
	uint32_t	d_dbpc_stats_cold:16;

	uint32_t	d_dbpc_stats_0_hot:10;
	uint32_t	rvd3:6;
	uint32_t	d_dbpc_stats_0_cold:10;
	uint32_t	rvd4:6;
	uint32_t	d_dbpc_stats_1_hot:10;
	uint32_t	rvd5:6;
	uint32_t	d_dbpc_stats_1_cold:10;
	uint32_t	rvd6:6;

	uint32_t	asf_max_edge;

	uint32_t	e_y_wm_pm_stats_0:21;
	uint32_t	rvd7:11;
	uint32_t	e_y_wm_pm_stats_1_bl:8;
	uint32_t	rvd8:8;
	uint32_t	e_y_wm_pm_stats_1_nl:12;
	uint32_t	rvd9:4;

	uint32_t	e_cbcr_wm_pm_stats_0:21;
	uint32_t	rvd10:11;
	uint32_t	e_cbcr_wm_pm_stats_1_bl:8;
	uint32_t	rvd11:8;
	uint32_t	e_cbcr_wm_pm_stats_1_nl:12;
	uint32_t	rvd12:4;

	uint32_t	v_y_wm_pm_stats_0:21;
	uint32_t	rvd13:11;
	uint32_t	v_y_wm_pm_stats_1_bl:8;
	uint32_t	rvd14:8;
	uint32_t	v_y_wm_pm_stats_1_nl:12;
	uint32_t	rvd15:4;

	uint32_t	v_cbcr_wm_pm_stats_0:21;
	uint32_t	rvd16:11;
	uint32_t	v_cbcr_wm_pm_stats_1_bl:8;
	uint32_t	rvd17:8;
	uint32_t	v_cbcr_wm_pm_stats_1_nl:12;
	uint32_t	rvd18:4;

	uint32_t      frame_id;
} __packed;

struct vfe_endframe {
	uint32_t      y_address;
	uint32_t      cbcr_address;

	struct vfe_frame_extra extra;
} __packed;

struct vfe_outputack {
	uint32_t  header;
	void      *output2newybufferaddress;
	void      *output2newcbcrbufferaddress;
} __packed;

struct vfe_stats_ack {
	uint32_t header;
	/* MUST BE 64 bit ALIGNED */
	void     *bufaddr;
} __packed;

/* AXI Output Config Command sent to DSP */
struct axiout {
	uint32_t            cmdheader:32;
	int                 outputmode:3;
	uint8_t             format:2;
	uint32_t            /* reserved */ : 27;

	/* AXI Output 1 Y Configuration, Part 1 */
	uint32_t            out1yimageheight:12;
	uint32_t            /* reserved */ : 4;
	uint32_t            out1yimagewidthin64bitwords:10;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 1 Y Configuration, Part 2 */
	uint8_t             out1yburstlen:2;
	uint32_t            out1ynumrows:12;
	uint32_t            out1yrowincin64bitincs:12;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 1 CbCr Configuration, Part 1 */
	uint32_t            out1cbcrimageheight:12;
	uint32_t            /* reserved */ : 4;
	uint32_t            out1cbcrimagewidthin64bitwords:10;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 1 CbCr Configuration, Part 2 */
	uint8_t             out1cbcrburstlen:2;
	uint32_t            out1cbcrnumrows:12;
	uint32_t            out1cbcrrowincin64bitincs:12;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 2 Y Configuration, Part 1 */
	uint32_t            out2yimageheight:12;
	uint32_t            /* reserved */ : 4;
	uint32_t            out2yimagewidthin64bitwords:10;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 2 Y Configuration, Part 2 */
	uint8_t             out2yburstlen:2;
	uint32_t            out2ynumrows:12;
	uint32_t            out2yrowincin64bitincs:12;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 2 CbCr Configuration, Part 1 */
	uint32_t            out2cbcrimageheight:12;
	uint32_t            /* reserved */ : 4;
	uint32_t            out2cbcrimagewidtein64bitwords:10;
	uint32_t            /* reserved */ : 6;

	/* AXI Output 2 CbCr Configuration, Part 2 */
	uint8_t             out2cbcrburstlen:2;
	uint32_t            out2cbcrnumrows:12;
	uint32_t            out2cbcrrowincin64bitincs:12;
	uint32_t            /* reserved */ : 6;

	/* Address configuration:
	 * output1 phisycal address */
	unsigned long   output1buffer1_y_phy;
	unsigned long   output1buffer1_cbcr_phy;
	unsigned long   output1buffer2_y_phy;
	unsigned long   output1buffer2_cbcr_phy;
	unsigned long   output1buffer3_y_phy;
	unsigned long   output1buffer3_cbcr_phy;
	unsigned long   output1buffer4_y_phy;
	unsigned long   output1buffer4_cbcr_phy;
	unsigned long   output1buffer5_y_phy;
	unsigned long   output1buffer5_cbcr_phy;
	unsigned long   output1buffer6_y_phy;
	unsigned long   output1buffer6_cbcr_phy;
	unsigned long   output1buffer7_y_phy;
	unsigned long   output1buffer7_cbcr_phy;
	unsigned long   output1buffer8_y_phy;
	unsigned long   output1buffer8_cbcr_phy;

	/* output2 phisycal address */
	unsigned long   output2buffer1_y_phy;
	unsigned long   output2buffer1_cbcr_phy;
	unsigned long   output2buffer2_y_phy;
	unsigned long   output2buffer2_cbcr_phy;
	unsigned long   output2buffer3_y_phy;
	unsigned long   output2buffer3_cbcr_phy;
	unsigned long   output2buffer4_y_phy;
	unsigned long   output2buffer4_cbcr_phy;
	unsigned long   output2buffer5_y_phy;
	unsigned long   output2buffer5_cbcr_phy;
	unsigned long   output2buffer6_y_phy;
	unsigned long   output2buffer6_cbcr_phy;
	unsigned long   output2buffer7_y_phy;
	unsigned long   output2buffer7_cbcr_phy;
	unsigned long   output2buffer8_y_phy;
	unsigned long   output2buffer8_cbcr_phy;
} __packed;

struct vfe_stats_we_cfg {
	uint32_t       header;

	/* White Balance/Exposure Statistic Selection */
	uint8_t        wb_expstatsenable:1;
	uint8_t        wb_expstatbuspriorityselection:1;
	unsigned int   wb_expstatbuspriorityvalue:4;
	unsigned int   /* reserved */ : 26;

	/* White Balance/Exposure Statistic Configuration, Part 1 */
	uint8_t        exposurestatregions:1;
	uint8_t        exposurestatsubregions:1;
	unsigned int   /* reserved */ : 14;

	unsigned int   whitebalanceminimumy:8;
	unsigned int   whitebalancemaximumy:8;

	/* White Balance/Exposure Statistic Configuration, Part 2 */
	uint8_t wb_expstatslopeofneutralregionline[
		NUM_WB_EXP_NEUTRAL_REGION_LINES];

	/* White Balance/Exposure Statistic Configuration, Part 3 */
	unsigned int   wb_expstatcrinterceptofneutralregionline2:12;
	unsigned int   /* reserved */ : 4;
	unsigned int   wb_expstatcbinterceptofneutralreginnline1:12;
	unsigned int    /* reserved */ : 4;

	/* White Balance/Exposure Statistic Configuration, Part 4 */
	unsigned int   wb_expstatcrinterceptofneutralregionline4:12;
	unsigned int   /* reserved */ : 4;
	unsigned int   wb_expstatcbinterceptofneutralregionline3:12;
	unsigned int   /* reserved */ : 4;

	/* White Balance/Exposure Statistic Output Buffer Header */
	unsigned int   wb_expmetricheaderpattern:8;
	unsigned int   /* reserved */ : 24;

	/* White Balance/Exposure Statistic Output Buffers-MUST
	* BE 64 bit ALIGNED */
	void  *wb_expstatoutputbuffer[NUM_WB_EXP_STAT_OUTPUT_BUFFERS];
} __packed;

struct vfe_stats_af_cfg {
	uint32_t header;

	/* Autofocus Statistic Selection */
	uint8_t       af_enable:1;
	uint8_t       af_busprioritysel:1;
	unsigned int  af_buspriorityval:4;
	unsigned int  /* reserved */ : 26;

	/* Autofocus Statistic Configuration, Part 1 */
	unsigned int  af_singlewinvoffset:12;
	unsigned int  /* reserved */ : 4;
	unsigned int  af_singlewinhoffset:12;
	unsigned int  /* reserved */ : 3;
	uint8_t       af_winmode:1;

	/* Autofocus Statistic Configuration, Part 2 */
	unsigned int  af_singglewinvh:11;
	unsigned int  /* reserved */ : 5;
	unsigned int  af_singlewinhw:11;
	unsigned int  /* reserved */ : 5;

	/* Autofocus Statistic Configuration, Parts 3-6 */
	uint8_t       af_multiwingrid[NUM_AUTOFOCUS_MULTI_WINDOW_GRIDS];

	/* Autofocus Statistic Configuration, Part 7 */
	signed int    af_metrichpfcoefa00:5;
	signed int    af_metrichpfcoefa04:5;
	unsigned int  af_metricmaxval:11;
	uint8_t       af_metricsel:1;
	unsigned int  /* reserved */ : 10;

	/* Autofocus Statistic Configuration, Part 8 */
	signed int    af_metrichpfcoefa20:5;
	signed int    af_metrichpfcoefa21:5;
	signed int    af_metrichpfcoefa22:5;
	signed int    af_metrichpfcoefa23:5;
	signed int    af_metrichpfcoefa24:5;
	unsigned int  /* reserved */ : 7;

	/* Autofocus Statistic Output Buffer Header */
	unsigned int  af_metrichp:8;
	unsigned int  /* reserved */ : 24;

	/* Autofocus Statistic Output Buffers - MUST BE 64 bit ALIGNED!!! */
	void *af_outbuf[NUM_AF_STAT_OUTPUT_BUFFERS];
} __packed; /* VFE_StatsAutofocusConfigCmdType */

struct msm_camera_frame_msg {
	unsigned long   output_y_address;
	unsigned long   output_cbcr_address;

	unsigned int    blacklevelevenColumn:23;
	uint16_t        reserved1:9;
	unsigned int    blackleveloddColumn:23;
	uint16_t        reserved2:9;

	uint16_t        greendefectpixelcount:8;
	uint16_t        reserved3:8;
	uint16_t        redbluedefectpixelcount:8;
	uint16_t        reserved4:8;
} __packed;

/* New one for 7k */
struct msm_vfe_command_7k {
	uint16_t queue;
	uint16_t length;
	void     *value;
};

struct stop_event {
	wait_queue_head_t wait;
	int state;
	int timeout;
};
struct vfe_error_msg {
	unsigned int camif_error:1;
	unsigned int output1ybusoverflow:1;
	unsigned int output1cbcrbusoverflow:1;
	unsigned int output2ybusoverflow:1;
	unsigned int output2cbcrbusoverflow:1;
	unsigned int autofocusstatbusoverflow:1;
	unsigned int wb_expstatbusoverflow:1;
	unsigned int axierror:1;
	unsigned int /* reserved */ : 24;
	unsigned int camif_staus:1;
	unsigned int pixel_count:14;
	unsigned int line_count:14;
	unsigned int /*reserved */ : 3;
} __packed;

static struct msm_free_buf *vfe2x_check_free_buffer(int id, int path);

#endif /* __MSM_VFE7X_H__ */
