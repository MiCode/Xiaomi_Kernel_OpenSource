/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <mach/camera.h>
#include <media/msm_camera.h>
#include <media/v4l2-subdev.h>
#define Q8  0x00000100
#define Q10 0x00000400

enum msm_sensor_resolution_t {
	MSM_SENSOR_RES_0,
	MSM_SENSOR_RES_1,
	MSM_SENSOR_RES_2,
	MSM_SENSOR_RES_3,
	MSM_SENSOR_RES_4,
	MSM_SENSOR_RES_5,
	MSM_SENSOR_RES_6,
	MSM_SENSOR_RES_7,
	MSM_SENSOR_INVALID_RES,
};

#define MSM_SENSOR_MCLK_8HZ 8000000
#define MSM_SENSOR_MCLK_16HZ 16000000
#define MSM_SENSOR_MCLK_24HZ 24000000

enum msm_sensor_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	MSM_SENSOR_REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	MSM_SENSOR_UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	MSM_SENSOR_UPDATE_ALL,
	/* Not valid update */
	MSM_SENSOR_UPDATE_INVALID
};

enum msm_sensor_cam_mode_t {
	MSM_SENSOR_MODE_2D_RIGHT,
	MSM_SENSOR_MODE_2D_LEFT,
	MSM_SENSOR_MODE_3D,
	MSM_SENSOR_MODE_INVALID
};

struct msm_sensor_i2c_reg_conf {
	unsigned short reg_addr;
	unsigned short reg_data;
};

struct msm_sensor_i2c_conf_array {
	struct msm_sensor_i2c_reg_conf *conf;
	unsigned short size;
	unsigned short delay;
};

struct msm_sensor_reg_t {
	struct msm_sensor_i2c_reg_conf *start_stream_conf;
	uint8_t start_stream_conf_size;
	struct msm_sensor_i2c_reg_conf *stop_stream_conf;
	uint8_t stop_stream_conf_size;
	struct msm_sensor_i2c_reg_conf *group_hold_on_conf;
	uint8_t group_hold_on_conf_size;
	struct msm_sensor_i2c_reg_conf *group_hold_off_conf;
	uint8_t group_hold_off_conf_size;
	struct msm_sensor_i2c_conf_array *init_settings;
	uint8_t init_size;
	struct msm_sensor_i2c_conf_array *res_settings;
	uint8_t num_conf;
};

struct v4l2_subdev_info {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	uint16_t fmt;
	uint16_t order;
};

struct msm_sensor_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;
	struct i2c_client *msm_sensor_client;
	struct i2c_driver *msm_sensor_i2c_driver;
	struct msm_sensor_reg_t msm_sensor_reg;

	uint16_t sensor_id_addr;
	uint16_t sensor_id;
	uint16_t frame_length_lines_addr;
	uint16_t line_length_pck_addr;
	uint16_t global_gain_addr;
	uint16_t coarse_int_time_addr;

	uint8_t frame_length_lines_array_addr;
	uint8_t line_length_pck_array_addr;

	uint16_t curr_line_length_pck;
	uint16_t curr_frame_length_lines;
	uint16_t prev_line_length_pck;
	uint16_t prev_frame_length_lines;
	uint16_t snap_line_length_pck;
	uint16_t snap_frame_length_lines;
	uint16_t vert_offset;

	uint16_t fps;
	uint32_t fps_divider;
	enum msm_sensor_resolution_t prev_res;
	enum msm_sensor_resolution_t pict_res;
	enum msm_sensor_resolution_t curr_res;
	enum msm_sensor_cam_mode_t cam_mode;
	enum msm_camera_type camera_type;

	struct mutex *msm_sensor_mutex;
	bool config_csi_flag;
	struct msm_camera_csi_params *csi_params;

	/*To Do: Changing v4l2_subdev to a pointer according to yupeng*/
	struct v4l2_subdev *sensor_v4l2_subdev;
	struct v4l2_subdev_info *sensor_v4l2_subdev_info;
	uint8_t sensor_v4l2_subdev_info_size;
	struct v4l2_subdev_ops *sensor_v4l2_subdev_ops;

	struct msm_sensor_fn_t {
		void (*sensor_start_stream) (struct msm_sensor_ctrl_t *);
		void (*sensor_stop_stream) (struct msm_sensor_ctrl_t *);
		void (*sensor_group_hold_on) (struct msm_sensor_ctrl_t *);
		void (*sensor_group_hold_off) (struct msm_sensor_ctrl_t *);

		uint16_t (*sensor_get_prev_lines_pf)
			(struct msm_sensor_ctrl_t *);
		uint16_t (*sensor_get_prev_pixels_pl)
			(struct msm_sensor_ctrl_t *);
		uint16_t (*sensor_get_pict_lines_pf)
			(struct msm_sensor_ctrl_t *);
		uint16_t (*sensor_get_pict_pixels_pl)
			(struct msm_sensor_ctrl_t *);
		uint32_t (*sensor_get_pict_max_exp_lc)
			(struct msm_sensor_ctrl_t *);
		void (*sensor_get_pict_fps) (struct msm_sensor_ctrl_t *,
				uint16_t, uint16_t *);
		int32_t (*sensor_set_fps) (struct msm_sensor_ctrl_t *,
				struct fps_cfg *);
		int32_t (*sensor_write_exp_gain) (struct msm_sensor_ctrl_t *,
				uint16_t, uint32_t);
		int32_t (*sensor_setting) (struct msm_sensor_ctrl_t *,
				int update_type, int rt);
		int32_t (*sensor_set_sensor_mode)
				(struct msm_sensor_ctrl_t *, int, int);
		int32_t (*sensor_mode_init) (struct msm_sensor_ctrl_t *,
			int, struct sensor_init_cfg *);
		int (*sensor_config) (void __user *);
		int (*sensor_open_init) (const struct msm_camera_sensor_info *);
		int (*sensor_release) (void);
		int (*sensor_power_down)
			(const struct msm_camera_sensor_info *);
		int (*sensor_power_up) (const struct msm_camera_sensor_info *);
		int (*sensor_probe) (struct msm_sensor_ctrl_t *s_ctrl,
				const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s);
	} func_tbl;
};

int32_t msm_sensor_i2c_rxdata(struct msm_sensor_ctrl_t *s_ctrl,
	unsigned char *rxdata, int length);

int32_t msm_sensor_i2c_txdata(struct msm_sensor_ctrl_t *s_ctrl,
	unsigned char *txdata, int length);

int32_t msm_sensor_i2c_waddr_write_b(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint8_t bdata);

int32_t msm_sensor_i2c_waddr_write_w(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint16_t wdata);

int32_t msm_sensor_i2c_waddr_read_w(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint16_t *data);

int32_t msm_sensor_i2c_waddr_write_b_tbl(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_i2c_reg_conf const *reg_conf_tbl, uint8_t size);

int32_t msm_sensor_i2c_waddr_write_w_tbl(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_i2c_reg_conf const *reg_conf_tbl, uint8_t size);

void msm_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl);
void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl);
void msm_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl);
void msm_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl);

uint16_t msm_sensor_get_prev_lines_pf(struct msm_sensor_ctrl_t *s_ctrl);
uint16_t msm_sensor_get_prev_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl);
uint16_t msm_sensor_get_pict_lines_pf(struct msm_sensor_ctrl_t *s_ctrl);
uint16_t msm_sensor_get_pict_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl);
uint32_t msm_sensor_get_pict_max_exp_lc(struct msm_sensor_ctrl_t *s_ctrl);
void msm_sensor_get_pict_fps(struct msm_sensor_ctrl_t *s_ctrl,
			uint16_t fps, uint16_t *pfps);
int32_t msm_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
			struct fps_cfg   *fps);
int32_t msm_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line);
int32_t msm_sensor_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line);
int32_t msm_sensor_set_sensor_mode_b(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res);
int32_t msm_sensor_set_sensor_mode_w(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res);
int32_t msm_sensor_mode_init_bdata(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info);
int32_t msm_sensor_mode_init_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info);
int32_t msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
			void __user *argp);
int16_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl);
uint16_t msm_sensor_read_b_conf_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			enum msm_sensor_resolution_t res, int8_t array_addr);
uint16_t msm_sensor_read_w_conf_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			enum msm_sensor_resolution_t res, int8_t array_addr);

int msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

int msm_sensor_probe(struct msm_sensor_ctrl_t *s_ctrl,
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s);

int msm_sensor_v4l2_probe(struct msm_sensor_ctrl_t *s_ctrl,
	const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s);

int msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			enum v4l2_mbus_pixelcode *code);

int msm_sensor_write_b_init_settings(struct msm_sensor_ctrl_t *s_ctrl);
int msm_sensor_write_w_init_settings(struct msm_sensor_ctrl_t *s_ctrl);
int msm_sensor_write_b_res_settings
	(struct msm_sensor_ctrl_t *s_ctrl, uint16_t res);
int msm_sensor_write_w_res_settings
	(struct msm_sensor_ctrl_t *s_ctrl, uint16_t res);

int msm_sensor_enable_debugfs(struct msm_sensor_ctrl_t *s_ctrl);
