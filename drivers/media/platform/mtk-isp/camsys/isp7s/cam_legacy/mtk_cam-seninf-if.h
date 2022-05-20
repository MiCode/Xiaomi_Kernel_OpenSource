/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_IF_H__
#define __MTK_CAM_SENINF_IF_H__

int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int *pixelmode);

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int pixelmode);

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg);

int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *pixelrate);

int mtk_cam_seninf_calc_pixelrate(struct device *dev, s64 width, s64 height, s64 hblank,
				  s64 vblank, int fps_n, int fps_d, s64 sensor_pixel_rate);

int mtk_cam_seninf_dump(struct v4l2_subdev *sd, u32 seq_id, bool force_check);

int mtk_cam_seninf_check_timeout(struct v4l2_subdev *sd, u64 time_waited);
u64 mtk_cam_seninf_get_frame_time(struct v4l2_subdev *sd, u32 seq_id);



/**
 * struct mtk_cam_seninf_mux_setting - mux setting change setting
 * @seninf: sensor interface's V4L2 subdev
 * @source: source pad id of the seninf subdev, to indicate the image
 *          processing engine to be conncted
 * @camtg: physical image processing engine's id (e.g. raw's device id)
 */
struct mtk_cam_seninf_mux_setting {
	struct v4l2_subdev *seninf;
	int source;
	int camtg;
	int enable;
};

/**
 * typedef mtk_cam_seninf_mux_change_done_fnc - mux change fininshed callback
 *
 * @private: Private data passed from mtk_cam_seninf_streaming_mux_change.
 *           In general, it is the request object indicate the mux changes.
 *
 * Returns true if the mux changes are all applied.
 */
typedef bool (*mtk_cam_seninf_mux_change_done_fn)(void *private);

/**
 * struct mtk_cam_seninf_mux_param - mux setting change parameters
 * @settings: per mux settings
 * @num: number of params
 * @fnc: callback function when seninf driver finihsed all the mux changes.
 * @private: Private data of the caller. The private data will be return from
 *           seninf driver through mtk_cam_seninf_mux_change_done_fnc callback.
 */
struct mtk_cam_seninf_mux_param {
	struct mtk_cam_seninf_mux_setting *settings;
	int num;
	mtk_cam_seninf_mux_change_done_fn func;
	void *private;
};

/**
 * struct mtk_cam_seninf_streaming_mux_change - change connection during streaming
 * @param: a new connection from sensor interface to image processing engine
 *
 * To be called when camsys driver need to change the connection from sensor
 * interface to image processing engine during streaming. It is a asynchronized
 * call, the sensor interface driver will call back func to notify the caller.
 *
 * Returns true if the mux changes will be applied.
 */
bool
mtk_cam_seninf_streaming_mux_change(struct mtk_cam_seninf_mux_param *param);


struct mtk_seninf_sof_notify_param {
	struct v4l2_subdev *sd;
	unsigned int sof_cnt;
};

void
mtk_cam_seninf_sof_notify(struct mtk_seninf_sof_notify_param *param);

/**
 * struct mtk_seninf_pad_data_info - data information outputed by pad
 */
struct mtk_seninf_pad_data_info {
	u8 feature;
	u8 mux; // allocated per group
	u16 exp_hsize;
	u16 exp_vsize;
};

/**
 * Get data info by seninf pad
 *
 * @param sd v4l2_subdev
 * @param pad The pad id
 * @param result The result
 * @return 0 if success, and negative number if error occur
 */
int mtk_cam_seninf_get_pad_data_info(struct v4l2_subdev *sd,
				unsigned int pad,
				struct mtk_seninf_pad_data_info *result);

void
mtk_cam_seninf_set_secure(struct v4l2_subdev *sd, int enable, unsigned int SecInfo_addr);
#endif
