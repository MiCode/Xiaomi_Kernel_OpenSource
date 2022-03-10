/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __SWPM_ISP_V6789_H__
#define __SWPM_ISP_V6789_H__

#define ISP_SWPM_RESERVED_SIZE 256

struct ParameterCam {
	unsigned int operating_voltage;		/* (mv) */
	unsigned int operating_frequency;
	unsigned int operating_temperature;
	unsigned int mcl;
	unsigned int fps;
	unsigned int ccu_on_off;
	unsigned int camsv_on_off;
	unsigned int camsv_process_data_size;	/* (Kpix/frame) */
	unsigned int num_camera;

	/* parameter for each camera */
	unsigned int sensor_image_w[4];
	unsigned int sensor_image_h[4];
	unsigned int crop_16_9_on_off[4];
	unsigned int input_resizer[4];		/* (%) */
	unsigned int d_yuv_on_off[4];
	unsigned int d_yuv_size[4];		/* (Kpix) */
	unsigned int d_yuv_simple_on_off[4];
	unsigned int hdr_on_off[4];
	unsigned int num_raw[4];
	unsigned int v_blanking[4];		/* in % of frame period */
};
struct ParameterDip {
	unsigned int operating_voltage;
	unsigned int operating_frequency;
	unsigned int operating_temperature;
	unsigned int mcl;
	unsigned int num_flow;

	/* parameters for each flow */
	unsigned int image_w[5];
	unsigned int image_h[5];
	unsigned int fps[5];
	unsigned int num_dip[5];
	unsigned int sw_overhead[5];		/* (ms) */
	unsigned int basic_rgb_path_on_off[5];
	unsigned int basic_yuv_path_on_off[5];
	unsigned int tile_raw_on_off[5];
	unsigned int cnr_on_off[5];
	unsigned int nr3d_on_off[5];
	unsigned int smart_tile_on_off[5];
	unsigned int fbc_on_off[5];
	unsigned int fe_fm_on_off[5];
	unsigned int wpe_on_off[5];
	/* 0 -> off, 1 -> mss_msf, 2 -> full */
	unsigned int mfb_mode[5];
};
struct ParameterIpe {
	unsigned int operating_voltage;
	unsigned int dpe_operating_frequency;
	unsigned int fd_rsc_operating_frequency;
	unsigned int operating_temperature;
	unsigned int mcl;
	unsigned int rsc_on_off;
	unsigned int rsc_frame_rate;
	unsigned int fd_on_off;
	unsigned int fd_frame_rate;
	unsigned int dpe_on_off;
	unsigned int dpe_frame_rate;
};

/* isp share memory data structure */
struct isp_swpm_rec_data {
	/* 4(int) * 256 = 1024 bytes */

	struct ParameterCam cam_idx;

	struct ParameterDip dip_idx;

	struct ParameterIpe ipe_idx;

	unsigned int isp_data[ISP_SWPM_RESERVED_SIZE - 49 - 80 - 11];
};

/* isp power index structure */
struct isp_swpm_index {
	struct ParameterCam cam_idx;

	struct ParameterDip dip_idx;

	struct ParameterIpe ipe_idx;
};

#endif

