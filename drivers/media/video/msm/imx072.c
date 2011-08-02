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
 *
 */

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "imx072.h"

/* SENSOR REGISTER DEFINES */
#define REG_GROUPED_PARAMETER_HOLD		0x0104
#define GROUPED_PARAMETER_HOLD_OFF		0x00
#define GROUPED_PARAMETER_HOLD			0x01
/* Integration Time */
#define REG_COARSE_INTEGRATION_TIME		0x0202
/* Gain */
#define REG_GLOBAL_GAIN					0x0204

/* PLL registers */
#define REG_FRAME_LENGTH_LINES			0x0340
#define REG_LINE_LENGTH_PCK				0x0342

/* 16bit address - 8 bit context register structure */
#define Q8  0x00000100
#define Q10 0x00000400
#define IMX072_MASTER_CLK_RATE 24000000
#define IMX072_OFFSET		3

/* AF Total steps parameters */
#define IMX072_AF_I2C_ADDR	0x18
#define IMX072_TOTAL_STEPS_NEAR_TO_FAR    30

static uint16_t imx072_step_position_table[IMX072_TOTAL_STEPS_NEAR_TO_FAR+1];
static uint16_t imx072_nl_region_boundary1;
static uint16_t imx072_nl_region_code_per_step1;
static uint16_t imx072_l_region_code_per_step = 12;
static uint16_t imx072_sw_damping_time_wait = 8;
static uint16_t imx072_af_initial_code = 350;
static uint16_t imx072_damping_threshold = 10;

struct imx072_work_t {
	struct work_struct work;
};

static struct imx072_work_t *imx072_sensorw;
static struct i2c_client *imx072_client;

struct imx072_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;

	uint16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;

	enum imx072_resolution_t prev_res;
	enum imx072_resolution_t pict_res;
	enum imx072_resolution_t curr_res;
	enum imx072_test_mode_t  set_test;
	enum imx072_cam_mode_t cam_mode;
};

static uint16_t prev_line_length_pck;
static uint16_t prev_frame_length_lines;
static uint16_t snap_line_length_pck;
static uint16_t snap_frame_length_lines;

static bool CSI_CONFIG;
static struct imx072_ctrl_t *imx072_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(imx072_wait_queue);
DEFINE_MUTEX(imx072_mut);

#ifdef CONFIG_DEBUG_FS
static int cam_debug_init(void);
static struct dentry *debugfs_base;
#endif

static int imx072_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = length,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(imx072_client->adapter, msgs, 2) < 0) {
		pr_err("imx072_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}

static int32_t imx072_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	if (i2c_transfer(imx072_client->adapter, msg, 1) < 0) {
		pr_err("imx072_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t imx072_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);
	rc = imx072_i2c_rxdata(imx072_client->addr>>1, buf, rlen);
	if (rc < 0) {
		pr_err("imx072_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
	CDBG("imx072_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
	return rc;
}

static int32_t imx072_i2c_write_w_sensor(unsigned short waddr,
	uint16_t wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00) >> 8;
	buf[3] = (wdata & 0x00FF);
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, wdata);
	rc = imx072_i2c_txdata(imx072_client->addr>>1, buf, 4);
	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);
	}
	return rc;
}

static int32_t imx072_i2c_write_b_sensor(unsigned short waddr,
	uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = imx072_i2c_txdata(imx072_client->addr>>1, buf, 3);
	if (rc < 0)
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);
	return rc;
}

static int32_t imx072_i2c_write_b_af(uint8_t msb, uint8_t lsb)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	buf[0] = msb;
	buf[1] = lsb;
	rc = imx072_i2c_txdata(IMX072_AF_I2C_ADDR>>1, buf, 2);
	if (rc < 0)
		pr_err("af_i2c_write faield msb = 0x%x lsb = 0x%x",
			msb, lsb);
	return rc;
}

static int32_t imx072_i2c_write_w_table(struct imx072_i2c_reg_conf const
					 *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = imx072_i2c_write_b_sensor(reg_conf_tbl->waddr,
			reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static void imx072_group_hold_on(void)
{
	imx072_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD);
}

static void imx072_group_hold_off(void)
{
	imx072_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD_OFF);
}

static void imx072_start_stream(void)
{
	imx072_i2c_write_b_sensor(0x0100, 0x01);
}

static void imx072_stop_stream(void)
{
	imx072_i2c_write_b_sensor(0x0100, 0x00);
}

static void imx072_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider, d1, d2;

	d1 = prev_frame_length_lines * 0x00000400 / snap_frame_length_lines;
	d2 = prev_line_length_pck * 0x00000400 / snap_line_length_pck;
	divider = d1 * d2 / 0x400;

	/*Verify PCLK settings and frame sizes.*/
	*pfps = (uint16_t) (fps * divider / 0x400);
}

static uint16_t imx072_get_prev_lines_pf(void)
{
	return prev_frame_length_lines;
}

static uint16_t imx072_get_prev_pixels_pl(void)
{
	return prev_line_length_pck;
}

static uint16_t imx072_get_pict_lines_pf(void)
{
	return snap_frame_length_lines;
}

static uint16_t imx072_get_pict_pixels_pl(void)
{
	return snap_line_length_pck;
}

static uint32_t imx072_get_pict_max_exp_lc(void)
{
	return snap_frame_length_lines  * 24;
}

static int32_t imx072_set_fps(struct fps_cfg   *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	total_lines_per_frame = (uint16_t)
		((prev_frame_length_lines *
		imx072_ctrl->fps_divider)/0x400);
	imx072_ctrl->fps_divider = fps->fps_div;
	imx072_ctrl->pict_fps_divider = fps->pict_fps_div;

	imx072_group_hold_on();
	rc = imx072_i2c_write_w_sensor(REG_FRAME_LENGTH_LINES,
							total_lines_per_frame);
	imx072_group_hold_off();
	return rc;
}

static int32_t imx072_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint32_t fl_lines = 0;
	uint8_t offset;
	int32_t rc = 0;
	if (imx072_ctrl->curr_res == imx072_ctrl->prev_res)
		fl_lines = prev_frame_length_lines;
	else if (imx072_ctrl->curr_res == imx072_ctrl->pict_res)
		fl_lines = snap_frame_length_lines;
	line = (line * imx072_ctrl->fps_divider) / Q10;
	offset = IMX072_OFFSET;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	imx072_group_hold_on();
	rc = imx072_i2c_write_w_sensor(REG_FRAME_LENGTH_LINES, fl_lines);
	rc = imx072_i2c_write_w_sensor(REG_COARSE_INTEGRATION_TIME, line);
	rc = imx072_i2c_write_w_sensor(REG_GLOBAL_GAIN, gain);
	imx072_group_hold_off();
	return rc;
}

static int32_t imx072_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = imx072_write_exp_gain(gain, line);
	return rc;
}

static int32_t imx072_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;
	struct msm_camera_csi_params imx072_csi_params;

	imx072_stop_stream();
	msleep(30);
	if (update_type == REG_INIT) {
		msleep(20);
		CSI_CONFIG = 0;
		imx072_i2c_write_w_table(imx072_regs.rec_settings,
			imx072_regs.rec_size);
	} else if (update_type == UPDATE_PERIODIC) {
#ifdef CONFIG_DEBUG_FS
		cam_debug_init();
#endif
		msleep(20);
		if (!CSI_CONFIG) {
			imx072_csi_params.lane_cnt = 2;
			imx072_csi_params.data_format = CSI_10BIT;
			imx072_csi_params.lane_assign = 0xe4;
			imx072_csi_params.dpcm_scheme = 0;
			imx072_csi_params.settle_cnt = 0x18;
			msm_camio_vfe_clk_rate_set(192000000);
			rc = msm_camio_csi_config(&imx072_csi_params);
			msleep(100);
			CSI_CONFIG = 1;
		}
		imx072_i2c_write_w_table(
			imx072_regs.conf_array[rt].conf,
			imx072_regs.conf_array[rt].size);
		imx072_start_stream();
		msleep(30);
	}
	return rc;
}

static int32_t imx072_video_config(int mode)
{

	int32_t rc = 0;
	/* change sensor resolution if needed */
	if (imx072_sensor_setting(UPDATE_PERIODIC,
		imx072_ctrl->prev_res) < 0)
		return rc;

	imx072_ctrl->curr_res = imx072_ctrl->prev_res;
	imx072_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx072_snapshot_config(int mode)
{
	int32_t rc = 0;
	/*change sensor resolution if needed */
	if (imx072_ctrl->curr_res != imx072_ctrl->pict_res) {
		if (imx072_sensor_setting(UPDATE_PERIODIC,
					imx072_ctrl->pict_res) < 0)
			return rc;
	}

	imx072_ctrl->curr_res = imx072_ctrl->pict_res;
	imx072_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx072_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	/* change sensor resolution if needed */
	if (imx072_ctrl->curr_res != imx072_ctrl->pict_res) {
		if (imx072_sensor_setting(UPDATE_PERIODIC,
					imx072_ctrl->pict_res) < 0)
			return rc;
	}

	imx072_ctrl->curr_res = imx072_ctrl->pict_res;
	imx072_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx072_mode_init(int mode, struct sensor_init_cfg init_info)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != imx072_ctrl->cam_mode) {
		imx072_ctrl->prev_res = init_info.prev_res;
		imx072_ctrl->pict_res = init_info.pict_res;
		imx072_ctrl->cam_mode = mode;

		prev_frame_length_lines =
			imx072_regs.conf_array[imx072_ctrl->prev_res].
			conf[IMX072_FRAME_LENGTH_LINES_HI].wdata << 8 |
			imx072_regs.conf_array[imx072_ctrl->prev_res].
			conf[IMX072_FRAME_LENGTH_LINES_LO].wdata;
		prev_line_length_pck =
			imx072_regs.conf_array[imx072_ctrl->prev_res].
			conf[IMX072_LINE_LENGTH_PCK_HI].wdata << 8 |
			imx072_regs.conf_array[imx072_ctrl->prev_res].
			conf[IMX072_LINE_LENGTH_PCK_LO].wdata;
		snap_frame_length_lines =
			imx072_regs.conf_array[imx072_ctrl->pict_res].
			conf[IMX072_FRAME_LENGTH_LINES_HI].wdata << 8 |
			imx072_regs.conf_array[imx072_ctrl->pict_res].
			conf[IMX072_FRAME_LENGTH_LINES_LO].wdata;
		snap_line_length_pck =
			imx072_regs.conf_array[imx072_ctrl->pict_res].
			conf[IMX072_LINE_LENGTH_PCK_HI].wdata << 8 |
			imx072_regs.conf_array[imx072_ctrl->pict_res].
			conf[IMX072_LINE_LENGTH_PCK_LO].wdata;

		rc = imx072_sensor_setting(REG_INIT,
			imx072_ctrl->prev_res);
	}
	return rc;
}

static int32_t imx072_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		imx072_ctrl->prev_res = res;
		rc = imx072_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		imx072_ctrl->pict_res = res;
		rc = imx072_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		imx072_ctrl->pict_res = res;
		rc = imx072_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

#define DIV_CEIL(x, y) ((x/y + ((x%y) ? 1 : 0)))
static int32_t imx072_move_focus(int direction,
	int32_t num_steps)
{
	int32_t rc = 0;
	int16_t step_direction, dest_lens_position, dest_step_position;
	uint8_t code_val_msb, code_val_lsb;
	int16_t next_lens_position, target_dist, small_step;

	if (direction == MOVE_NEAR)
		step_direction = 1;
	else if (direction == MOVE_FAR)
		step_direction = -1;
	else {
		pr_err("Illegal focus direction\n");
		return -EINVAL;
	}
	dest_step_position = imx072_ctrl->curr_step_pos +
			(step_direction * num_steps);

	if (dest_step_position < 0)
		dest_step_position = 0;
	else if (dest_step_position > IMX072_TOTAL_STEPS_NEAR_TO_FAR)
		dest_step_position = IMX072_TOTAL_STEPS_NEAR_TO_FAR;

	if (dest_step_position == imx072_ctrl->curr_step_pos) {
		CDBG("imx072 same position No-Move exit\n");
		return rc;
	}
	CDBG("%s Index = [%d]\n", __func__, dest_step_position);

	dest_lens_position = imx072_step_position_table[dest_step_position];
	CDBG("%s lens_position value = %d\n", __func__, dest_lens_position);
	target_dist = step_direction * (dest_lens_position -
		imx072_ctrl->curr_lens_pos);
	if (step_direction < 0 && (target_dist >=
		(imx072_step_position_table[imx072_damping_threshold]
			- imx072_af_initial_code))) {
		small_step = DIV_CEIL(target_dist, 10);
		imx072_sw_damping_time_wait = 30;
	} else {
		small_step = DIV_CEIL(target_dist, 4);
		imx072_sw_damping_time_wait = 20;
	}

	CDBG("%s: small_step:%d, wait_time:%d\n", __func__, small_step,
		imx072_sw_damping_time_wait);
	for (next_lens_position = imx072_ctrl->curr_lens_pos +
		(step_direction * small_step);
		(step_direction * next_lens_position) <=
		(step_direction * dest_lens_position);
		next_lens_position += (step_direction * small_step)) {

		code_val_msb = ((next_lens_position & 0x03F0) >> 4);
		code_val_lsb = ((next_lens_position & 0x000F) << 4);
		CDBG("position value = %d\n", next_lens_position);
		CDBG("movefocus vcm_msb = %d\n", code_val_msb);
		CDBG("movefocus vcm_lsb = %d\n", code_val_lsb);
		rc = imx072_i2c_write_b_af(code_val_msb, code_val_lsb);
		if (rc < 0) {
			pr_err("imx072_move_focus failed writing i2c\n");
			return rc;
			}
		imx072_ctrl->curr_lens_pos = next_lens_position;
		usleep(imx072_sw_damping_time_wait*100);
	}
	if (imx072_ctrl->curr_lens_pos != dest_lens_position) {
		code_val_msb = ((dest_lens_position & 0x03F0) >> 4);
		code_val_lsb = ((dest_lens_position & 0x000F) << 4);
		CDBG("position value = %d\n", dest_lens_position);
		CDBG("movefocus vcm_msb = %d\n", code_val_msb);
		CDBG("movefocus vcm_lsb = %d\n", code_val_lsb);
		rc = imx072_i2c_write_b_af(code_val_msb, code_val_lsb);
		if (rc < 0) {
			pr_err("imx072_move_focus failed writing i2c\n");
			return rc;
			}
		usleep(imx072_sw_damping_time_wait * 100);
	}
	imx072_ctrl->curr_lens_pos = dest_lens_position;
	imx072_ctrl->curr_step_pos = dest_step_position;
	return rc;

}

static int32_t imx072_init_focus(void)
{
	uint8_t i;
	int32_t rc = 0;

	imx072_step_position_table[0] = imx072_af_initial_code;
	for (i = 1; i <= IMX072_TOTAL_STEPS_NEAR_TO_FAR; i++) {
		if (i <= imx072_nl_region_boundary1)
			imx072_step_position_table[i] =
				imx072_step_position_table[i-1]
				+ imx072_nl_region_code_per_step1;
		else
			imx072_step_position_table[i] =
				imx072_step_position_table[i-1]
				+ imx072_l_region_code_per_step;

		if (imx072_step_position_table[i] > 1023)
			imx072_step_position_table[i] = 1023;
	}
	imx072_ctrl->curr_lens_pos = 0;

	return rc;
}

static int32_t imx072_set_default_focus(void)
{
	int32_t rc = 0;
	uint8_t code_val_msb, code_val_lsb;
	int16_t dest_lens_position = 0;

	CDBG("%s Index = [%d]\n", __func__, 0);
	if (imx072_ctrl->curr_step_pos != 0)
		rc = imx072_move_focus(MOVE_FAR,
		imx072_ctrl->curr_step_pos);
	else {
		dest_lens_position = imx072_af_initial_code;
		code_val_msb = ((dest_lens_position & 0x03F0) >> 4);
		code_val_lsb = ((dest_lens_position & 0x000F) << 4);

		CDBG("position value = %d\n", dest_lens_position);
		CDBG("movefocus vcm_msb = %d\n", code_val_msb);
		CDBG("movefocus vcm_lsb = %d\n", code_val_lsb);
		rc = imx072_i2c_write_b_af(code_val_msb, code_val_lsb);
		if (rc < 0) {
			pr_err("imx072_set_default_focus failed writing i2c\n");
			return rc;
		}

		imx072_ctrl->curr_lens_pos = dest_lens_position;
		imx072_ctrl->curr_step_pos = 0;

	}
	usleep(5000);
	return rc;
}

static int32_t imx072_af_power_down(void)
{
	int32_t rc = 0;
	int32_t i = 0;
	int16_t dest_lens_position = imx072_af_initial_code;

	if (imx072_ctrl->curr_lens_pos != 0) {
		rc = imx072_set_default_focus();
		CDBG("%s after imx072_set_default_focus\n", __func__);
		msleep(40);
		/*to avoid the sound during the power off.
		brings the actuator to mechanical infinity gradually.*/
		for (i = 0; i < IMX072_TOTAL_STEPS_NEAR_TO_FAR; i++) {
			dest_lens_position = dest_lens_position -
				(imx072_af_initial_code /
					IMX072_TOTAL_STEPS_NEAR_TO_FAR);
			CDBG("position value = %d\n", dest_lens_position);
			rc = imx072_i2c_write_b_af(
				((dest_lens_position & 0x03F0) >> 4),
				((dest_lens_position & 0x000F) << 4));
			CDBG("count = %d\n", i);
			msleep(20);
			if (rc < 0) {
				pr_err("imx072_set_default_focus failed writing i2c\n");
				return rc;
			}
		}
		rc = imx072_i2c_write_b_af(0x00, 00);
		msleep(40);
	}
	rc = imx072_i2c_write_b_af(0x80, 00);
	return rc;
}

static int32_t imx072_power_down(void)
{
	int32_t rc = 0;

	rc = imx072_af_power_down();
	return rc;
}

static int imx072_probe_init_done(const struct msm_camera_sensor_info *data)
{
	pr_err("probe done\n");
	gpio_free(data->sensor_reset);
	return 0;
}

static int imx072_probe_init_sensor(
	const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	CDBG("%s: %d\n", __func__, __LINE__);
	rc = gpio_request(data->sensor_reset, "imx072");
	CDBG(" imx072_probe_init_sensor\n");
	if (!rc) {
		pr_err("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		msleep(50);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(20);
	} else
		goto gpio_req_fail;

	CDBG(" imx072_probe_init_sensor is called\n");
	rc = imx072_i2c_read(0x0, &chipid, 2);
	CDBG("ID: %d\n", chipid);
	/* 4. Compare sensor ID to IMX072 ID: */
	if (chipid != 0x0045) {
		rc = -ENODEV;
		pr_err("imx072_probe_init_sensor chip id doesnot match\n");
		goto init_probe_fail;
	}

	return rc;
init_probe_fail:
	pr_err(" imx072_probe_init_sensor fails\n");
	gpio_set_value_cansleep(data->sensor_reset, 0);
	imx072_probe_init_done(data);
	if (data->vcm_enable) {
		int ret = gpio_request(data->vcm_pwd, "imx072_af");
		if (!ret) {
			gpio_direction_output(data->vcm_pwd, 0);
			msleep(20);
			gpio_free(data->vcm_pwd);
		}
	}
gpio_req_fail:
	return rc;
}

int imx072_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	CDBG("%s: %d\n", __func__, __LINE__);
	imx072_ctrl = kzalloc(sizeof(struct imx072_ctrl_t), GFP_KERNEL);
	if (!imx072_ctrl) {
		pr_err("imx072_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	imx072_ctrl->fps_divider = 1 * 0x00000400;
	imx072_ctrl->pict_fps_divider = 1 * 0x00000400;
	imx072_ctrl->set_test = TEST_OFF;
	imx072_ctrl->cam_mode = MODE_INVALID;

	if (data)
		imx072_ctrl->sensordata = data;
	if (rc < 0) {
		pr_err("Calling imx072_sensor_open_init fail1\n");
		return rc;
	}
	CDBG("%s: %d\n", __func__, __LINE__);
	/* enable mclk first */
	msm_camio_clk_rate_set(IMX072_MASTER_CLK_RATE);
	rc = imx072_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	imx072_init_focus();
	imx072_ctrl->fps = 30*Q8;
	if (rc < 0) {
		gpio_set_value_cansleep(data->sensor_reset, 0);
		goto init_fail;
	} else
		goto init_done;
init_fail:
	pr_err("init_fail\n");
	imx072_probe_init_done(data);
init_done:
	pr_err("init_done\n");
	return rc;
}

static int imx072_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&imx072_wait_queue);
	return 0;
}

static const struct i2c_device_id imx072_i2c_id[] = {
	{"imx072", 0},
	{ }
};

static int imx072_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("imx072_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	imx072_sensorw = kzalloc(sizeof(struct imx072_work_t),
			GFP_KERNEL);
	if (!imx072_sensorw) {
		pr_err("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, imx072_sensorw);
	imx072_init_client(client);
	imx072_client = client;

	msleep(50);

	CDBG("imx072_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	pr_err("imx072_probe failed! rc = %d\n", rc);
	return rc;
}

static int imx072_send_wb_info(struct wb_info_cfg *wb)
{
	return 0;

}

static int __exit imx072_remove(struct i2c_client *client)
{
	struct imx072_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	imx072_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver imx072_i2c_driver = {
	.id_table = imx072_i2c_id,
	.probe  = imx072_i2c_probe,
	.remove = __exit_p(imx072_i2c_remove),
	.driver = {
		.name = "imx072",
	},
};

int imx072_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&imx072_mut);
	CDBG("imx072_sensor_config: cfgtype = %d\n",
		 cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		imx072_get_pict_fps(
			cdata.cfg.gfps.prevfps,
			&(cdata.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf =
		imx072_get_prev_lines_pf();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl =
			imx072_get_prev_pixels_pl();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf =
			imx072_get_pict_lines_pf();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
			imx072_get_pict_pixels_pl();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc =
			imx072_get_pict_max_exp_lc();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = imx072_set_fps(&(cdata.cfg.fps));
		break;
	case CFG_SET_EXP_GAIN:
		rc = imx072_write_exp_gain(
			cdata.cfg.exp_gain.gain,
			cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_PICT_EXP_GAIN:
		rc = imx072_set_pict_exp_gain(
			cdata.cfg.exp_gain.gain,
			cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_MODE:
		rc = imx072_set_sensor_mode(cdata.mode, cdata.rs);
		break;
	case CFG_PWR_DOWN:
		rc = imx072_power_down();
		break;
	case CFG_MOVE_FOCUS:
		rc = imx072_move_focus(cdata.cfg.focus.dir,
				cdata.cfg.focus.steps);
		break;
	case CFG_SET_DEFAULT_FOCUS:
		imx072_set_default_focus();
		break;
	case CFG_GET_AF_MAX_STEPS:
		cdata.max_steps = IMX072_TOTAL_STEPS_NEAR_TO_FAR;
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SET_EFFECT:
		break;
	case CFG_SEND_WB_INFO:
		rc = imx072_send_wb_info(
			&(cdata.cfg.wb_info));
	break;
	case CFG_SENSOR_INIT:
		rc = imx072_mode_init(cdata.mode,
				cdata.cfg.init_info);
	break;
	case CFG_SET_LENS_SHADING:
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&imx072_mut);

	return rc;
}

static int imx072_sensor_release(void)
{
	int rc = -EBADF;
	mutex_lock(&imx072_mut);
	imx072_power_down();
	gpio_set_value_cansleep(imx072_ctrl->sensordata->sensor_reset, 0);
	msleep(20);
	gpio_free(imx072_ctrl->sensordata->sensor_reset);
	if (imx072_ctrl->sensordata->vcm_enable) {
		gpio_set_value_cansleep(imx072_ctrl->sensordata->vcm_pwd, 0);
		gpio_free(imx072_ctrl->sensordata->vcm_pwd);
	}
	kfree(imx072_ctrl);
	imx072_ctrl = NULL;
	pr_err("imx072_release completed\n");
	mutex_unlock(&imx072_mut);

	return rc;
}

static int imx072_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&imx072_i2c_driver);
	if (rc < 0 || imx072_client == NULL) {
		rc = -ENOTSUPP;
		pr_err("I2C add driver failed");
		goto probe_fail;
	}
	msm_camio_clk_rate_set(IMX072_MASTER_CLK_RATE);
	rc = imx072_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
	s->s_init = imx072_sensor_open_init;
	s->s_release = imx072_sensor_release;
	s->s_config  = imx072_sensor_config;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;

	gpio_set_value_cansleep(info->sensor_reset, 0);
	imx072_probe_init_done(info);
	if (info->vcm_enable) {
		rc = gpio_request(info->vcm_pwd, "imx072_af");
		if (!rc) {
			gpio_direction_output(info->vcm_pwd, 0);
			msleep(20);
			gpio_free(info->vcm_pwd);
		} else
			return rc;
	}
	pr_info("imx072_sensor_probe : SUCCESS\n");
	return rc;

probe_fail:
	pr_err("imx072_sensor_probe: SENSOR PROBE FAILS!\n");
	return rc;
}

static int __imx072_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, imx072_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __imx072_probe,
	.driver = {
		.name = "msm_camera_imx072",
		.owner = THIS_MODULE,
	},
};

static int __init imx072_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(imx072_init);
void imx072_exit(void)
{
	i2c_del_driver(&imx072_i2c_driver);
}
MODULE_DESCRIPTION("Aptina 8 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");

#ifdef CONFIG_DEBUG_FS
static bool streaming = 1;

static int cam_debug_stream_set(void *data, u64 val)
{
	int rc = 0;

	if (val) {
		imx072_start_stream();
		streaming = 1;
	} else {
		imx072_stop_stream();
		streaming = 0;
	}

	return rc;
}

static int cam_debug_stream_get(void *data, u64 *val)
{
	*val = streaming;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cam_stream, cam_debug_stream_get,
			cam_debug_stream_set, "%llu\n");



static int imx072_set_af_codestep(void *data, u64 val)
{
	imx072_l_region_code_per_step = val;
	imx072_init_focus();
	return 0;
}

static int imx072_get_af_codestep(void *data, u64 *val)
{
	*val = imx072_l_region_code_per_step;
	return 0;
}

static uint16_t imx072_linear_total_step = IMX072_TOTAL_STEPS_NEAR_TO_FAR;
static int imx072_set_linear_total_step(void *data, u64 val)
{
	imx072_linear_total_step = val;
	return 0;
}

static int imx072_af_linearity_test(void *data, u64 *val)
{
	int i = 0;

	imx072_set_default_focus();
	msleep(3000);
	for (i = 0; i < imx072_linear_total_step; i++) {
		imx072_move_focus(MOVE_NEAR, 1);
		CDBG("moved to index =[%d]\n", i);
		msleep(1000);
	}

	for (i = 0; i < imx072_linear_total_step; i++) {
		imx072_move_focus(MOVE_FAR, 1);
		CDBG("moved to index =[%d]\n", i);
		msleep(1000);
	}
	return 0;
}

static uint16_t imx072_step_val = IMX072_TOTAL_STEPS_NEAR_TO_FAR;
static uint8_t imx072_step_dir = MOVE_NEAR;
static int imx072_af_step_config(void *data, u64 val)
{
	imx072_step_val = val & 0xFFFF;
	imx072_step_dir = (val >> 16) & 0x1;
	return 0;
}

static int imx072_af_step(void *data, u64 *val)
{
	int i = 0;
	int dir = MOVE_NEAR;
	imx072_set_default_focus();
	msleep(3000);
	if (imx072_step_dir == 1)
		dir = MOVE_FAR;

	for (i = 0; i < imx072_step_val; i += 4) {
		imx072_move_focus(dir, 4);
		msleep(1000);
	}
	imx072_set_default_focus();
	msleep(3000);
	return 0;
}

static int imx072_af_set_resolution(void *data, u64 val)
{
	imx072_init_focus();
	return 0;
}

static int imx072_af_get_resolution(void *data, u64 *val)
{
	*val = 0xFF;
	return 0;
}



DEFINE_SIMPLE_ATTRIBUTE(af_codeperstep, imx072_get_af_codestep,
			imx072_set_af_codestep, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_linear, imx072_af_linearity_test,
			imx072_set_linear_total_step, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_step, imx072_af_step,
			imx072_af_step_config, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_step_res, imx072_af_get_resolution,
			imx072_af_set_resolution, "%llu\n");

static int cam_debug_init(void)
{
	struct dentry *cam_dir;
	debugfs_base = debugfs_create_dir("sensor", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	cam_dir = debugfs_create_dir("imx072", debugfs_base);
	if (!cam_dir)
		return -ENOMEM;

	if (!debugfs_create_file("stream", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &cam_stream))
		return -ENOMEM;

	if (!debugfs_create_file("af_codeperstep", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &af_codeperstep))
		return -ENOMEM;
	if (!debugfs_create_file("af_linear", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &af_linear))
		return -ENOMEM;
	if (!debugfs_create_file("af_step", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &af_step))
		return -ENOMEM;

	if (!debugfs_create_file("af_step_res", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &af_step_res))
		return -ENOMEM;

	return 0;
}
#endif
