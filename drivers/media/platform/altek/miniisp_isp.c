/*
 * File: miniisp_spi.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 * 2017/04/11; LouisWang; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/************************************************************
*			Include File									*
*************************************************************/
/* Linux headers*/
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/buffer_head.h>
#include <linux/of_gpio.h>


#include "include/miniisp.h"
#include "include/miniisp_ctrl.h"
#include "include/miniisp_customer_define.h"
#include "include/miniisp_chip_base_define.h"
#include "include/ispctrl_if_master.h"
#include "include/isp_camera_cmd.h"
#include "include/error/miniisp_err.h"

#if EN_605_IOCTRL_INTF
/* ALTEK_AL6100_ECHO >>> */
#include "include/miniisp_ctrl_intf.h"
/* ALTEK_AL6100_ECHO <<< */
#endif

#ifdef ALTEK_TEST
#include "include/altek_test.h"
#endif

/****************************************************************************
*						 Private Constant Definition						*
****************************************************************************/
#define DEBUG_NODE 0
/*#define DEBUG_ALERT*/
#define MINI_ISP_LOG_TAG "[miniisp_isp]"
/*drv debug defination*/
#define _SPI_DEBUG

/****************************************************************************
*						Private Global Variable								*
****************************************************************************/
static struct misp_global_variable *misp_drv_global_variable;
static struct class *mini_isp_class;
static struct device *mini_isp_dev;

#if EN_605_IOCTRL_INTF
/* ALTEK_AL6100_ECHO >>> */
struct file *l_internal_file[ECHO_OTHER_MAX];
/* ALTEK_AL6100_ECHO <<< */
#endif

extern u16 fw_version_before_point;
extern u16 fw_version_after_point;
extern char fw_build_by[];
extern char fw_project_name[];
extern u32 sc_build_date;
/************************************************************
 *		Public Global Variable
 *************************************************************/

/************************************************************
 *		Private Macro Definition
 *************************************************************/

/************************************************************
 *		Public Function Prototype
 *************************************************************/


/************************************************************
 *		Private Function
 *************************************************************/
#if EN_605_IOCTRL_INTF
void mini_isp_other_drv_open_l(char *file_name, u8 type)
{

	/* Error Code*/
	errcode err = ERR_SUCCESS;

	misp_info("%s filepath : %s", __func__, file_name);

#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	return;
#endif
	if (IS_ERR(l_internal_file[type])) {
		err = PTR_ERR(l_internal_file[type]);
		misp_err("%s open file failed. err: %x", __func__, err);
	} else {
		misp_info("%s open file success!", __func__);
	}

}
#endif
static ssize_t mini_isp_mode_config_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	/*misp_info("%s - mini_isp_spi_send return %d", __func__, ret);*/
	return snprintf(buf, 32, "load fw:0 e_to_a:1 a_to_e:2\n");
}

static ssize_t mini_isp_mode_config_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 buf_chip_id_use[4];

	if ('0' == buf[0]) {
		mini_isp_chip_init();
		mini_isp_e_to_a();
		mini_isp_drv_load_fw();
	} else if ('1' == buf[0]) {
		mini_isp_chip_init();
		mini_isp_e_to_a();
	} else if ('2' == buf[0]) {
		mini_isp_a_to_e();
	} else if ('4' == buf[0]) {
		mini_isp_get_chip_id();
	} else if ('7' == buf[0]) {
		buf_chip_id_use[0] = 0;
		mini_isp_debug_dump_img();
		mini_isp_a_to_e();
		mini_isp_chip_base_dump_irp_and_depth_based_register();
		mini_isp_memory_write(0x10, buf_chip_id_use, 1);
		mini_isp_e_to_a();
	}  else {
		mini_isp_poweron();
		mini_isp_drv_set_bypass_mode(1);
	}
	return size;
}

static DEVICE_ATTR(mini_isp_mode_config, 0660, mini_isp_mode_config_show,
		mini_isp_mode_config_store);

static ssize_t mini_isp_reset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = -EINVAL;

	ret = gpio_get_value(misp_drv_global_variable->reset_gpio);
	misp_info("%s - reset_gpio is %d", __func__, ret);

	return snprintf(buf, 32, "%d", ret);
}

static ssize_t mini_isp_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	if ('0' == buf[0])
		gpio_set_value(misp_drv_global_variable->reset_gpio, 0);
	else
		gpio_set_value(misp_drv_global_variable->reset_gpio, 1);

	misp_info("%s - ", __func__);

	return size;
}

static DEVICE_ATTR(mini_isp_reset, 0660,
					mini_isp_reset_show,
					mini_isp_reset_store);


static ssize_t mini_isp_rectab_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "Set rectab Param!!\n");
}

static ssize_t mini_isp_rectab_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 trans_mode;
	u32 block_size;
	struct depth_rectab_invrect_param rect_param[3];

	misp_info("Set rectab start!!");

	/* fill test pattern */
	memset((u8 *)&rect_param[0], 0xa,
		3*sizeof(struct depth_rectab_invrect_param));

	trans_mode = 0;
	block_size = 64;

	mini_isp_drv_write_depth_rectab_invrect(
		&rect_param[0], trans_mode, block_size);

	misp_info("Set rectab end!!");
	return size;
}

static DEVICE_ATTR(mini_isp_rectab, 0660,
					mini_isp_rectab_show,
					mini_isp_rectab_store);

ssize_t echo_mini_isp_drv_set_depth_3a_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	struct isp_cmd_depth_3a_info depth_3a_info;

	memset(&depth_3a_info, 0, sizeof(struct isp_cmd_depth_3a_info));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %hu %u %hu %hu %hu %hu %hu %hu %hd %hhu\
					%u %hu %hu %hu %hu %hu %hu %hd %hhu\
					%hu %hu %hd %hd %hu %hu %hhu",
	cmd_name,
	&depth_3a_info.hdr_ratio,
	&depth_3a_info.main_cam_exp_time,
	&depth_3a_info.main_cam_exp_gain,
	&depth_3a_info.main_cam_amb_r_gain,
	&depth_3a_info.main_cam_amb_g_gain,
	&depth_3a_info.main_cam_amb_b_gain,
	&depth_3a_info.main_cam_iso,
	&depth_3a_info.main_cam_bv,
	&depth_3a_info.main_cam_vcm_position,
	&depth_3a_info.main_cam_vcm_status,
	&depth_3a_info.sub_cam_exp_time,
	&depth_3a_info.sub_cam_exp_gain,
	&depth_3a_info.sub_cam_amb_r_gain,
	&depth_3a_info.sub_cam_amb_g_gain,
	&depth_3a_info.sub_cam_amb_b_gain,
	&depth_3a_info.sub_cam_iso,
	&depth_3a_info.sub_cam_bv,
	&depth_3a_info.sub_cam_vcm_position,
	&depth_3a_info.sub_cam_vcm_status,
	&depth_3a_info.main_cam_isp_d_gain,
	&depth_3a_info.sub_cam_isp_d_gain,
	&depth_3a_info.hdr_long_exp_ev_x1000,
	&depth_3a_info.hdr_short_exp_ev_x1000,
	&depth_3a_info.ghost_prevent_low,
	&depth_3a_info.ghost_prevent_high,
	&depth_3a_info.depth_proc_mode);

	if (ret != 27) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("%hu %u %hu %hu %hu %hu %hu %hu %hd %hhu\
				   %u %hu %hu %hu %hu %hu %hu %hd %hhu\
				   %hu %hu %hd %hd %hu %hu %hhu",
		depth_3a_info.hdr_ratio,
		depth_3a_info.main_cam_exp_time,
		depth_3a_info.main_cam_exp_gain,
		depth_3a_info.main_cam_amb_r_gain,
		depth_3a_info.main_cam_amb_g_gain,
		depth_3a_info.main_cam_amb_b_gain,
		depth_3a_info.main_cam_iso,
		depth_3a_info.main_cam_bv,
		depth_3a_info.main_cam_vcm_position,
		depth_3a_info.main_cam_vcm_status,
		depth_3a_info.sub_cam_exp_time,
		depth_3a_info.sub_cam_exp_gain,
		depth_3a_info.sub_cam_amb_r_gain,
		depth_3a_info.sub_cam_amb_g_gain,
		depth_3a_info.sub_cam_amb_b_gain,
		depth_3a_info.sub_cam_iso,
		depth_3a_info.sub_cam_bv,
		depth_3a_info.sub_cam_vcm_position,
		depth_3a_info.sub_cam_vcm_status,
		depth_3a_info.main_cam_isp_d_gain,
		depth_3a_info.sub_cam_isp_d_gain,
		depth_3a_info.hdr_long_exp_ev_x1000,
		depth_3a_info.hdr_short_exp_ev_x1000,
		depth_3a_info.ghost_prevent_low,
		depth_3a_info.ghost_prevent_high,
		depth_3a_info.depth_proc_mode);

	mini_isp_drv_set_depth_3a_info(&depth_3a_info);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_depth_auto_interleave_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[4];
	struct isp_cmd_depth_auto_interleave_param depth_auto_interleave_param;

	memset(&depth_auto_interleave_param, 0,
		sizeof(struct isp_cmd_depth_auto_interleave_param));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u", cmd_name,
		&param[0], &param[1], &param[2], &param[3]);
	depth_auto_interleave_param.depth_interleave_mode_on_off = (u8) param[0];
	depth_auto_interleave_param.skip_frame_num_after_illuminator_pulse = (u8) param[1];
	depth_auto_interleave_param.projector_power_level = (u8) param[2];
	depth_auto_interleave_param.illuminator_power_level = (u8) param[3];

	if (ret != 5) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("%d, %d, %d, %d",
		depth_auto_interleave_param.depth_interleave_mode_on_off,
		depth_auto_interleave_param.skip_frame_num_after_illuminator_pulse,
		depth_auto_interleave_param.projector_power_level,
		depth_auto_interleave_param.illuminator_power_level);

	mini_isp_drv_set_depth_auto_interleave_mode(&depth_auto_interleave_param);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_exposure_param(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[3];

	struct isp_cmd_exposure_param set_exposure_param;

	memset(&set_exposure_param, 0, sizeof(struct isp_cmd_exposure_param));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u", cmd_name, &param[0], &param[1], &param[2]);
	if (ret != 4) {
		errcode = -EINVAL;
		return errcode;
	}

	set_exposure_param.udExpTime = (u32)param[0];
	set_exposure_param.uwISO = (u16)param[1];
	set_exposure_param.ucActiveDevice = (u8)param[2];

	misp_info("menu exposure param: %d %d %d",
		set_exposure_param.udExpTime,
		set_exposure_param.uwISO,
		set_exposure_param.ucActiveDevice);

	mini_isp_drv_set_exposure_param(&set_exposure_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_depth_stream_size(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[1];

	struct isp_cmd_depth_stream_size depth_stream_size_param;

	memset(&depth_stream_size_param, 0, sizeof(struct isp_cmd_depth_stream_size));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param[0]);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	depth_stream_size_param.depth_stream_size = (u8)param[0];

	misp_info("depth stream size: %d",
		depth_stream_size_param.depth_stream_size);

	mini_isp_drv_set_depth_stream_size(&depth_stream_size_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_sensor_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u8 sensor_on_off = 0;
	u8 scenario_id = 0;
	u8 mipi_tx_skew_enable = 0;
	u8 ae_weighting_table_index = 0;
	u8 merge_mode_enable = 0;

	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %hhu %hhu %hhu %hhu %hhu",
		cmd_name, &sensor_on_off, &scenario_id, &mipi_tx_skew_enable,
		&ae_weighting_table_index, &merge_mode_enable);

	if (ret != 6) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("0x%x, 0x%x, 0x%x, 0x%x, 0x%x", sensor_on_off, scenario_id, mipi_tx_skew_enable,
						ae_weighting_table_index, merge_mode_enable);

	mini_isp_drv_set_sensor_mode(sensor_on_off, scenario_id,
		mipi_tx_skew_enable, ae_weighting_table_index, merge_mode_enable);
	misp_info("%s E!!", __func__);
	return errcode;
}


ssize_t echo_mini_isp_drv_set_output_format(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[3];
	struct isp_cmd_set_output_format output_format_param;

	memset(&output_format_param, 0, sizeof(struct isp_cmd_set_output_format));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u", cmd_name, &param[0], &param[1], &param[2]);
	if (ret != 4) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("0x%x, 0x%x, 0x%x", param[0], param[1], param[2]);
	output_format_param.depth_size = (u8)param[0];
	output_format_param.reserve[0] = (u8)param[1];
	output_format_param.reserve[1] = (u8)param[2];

	errcode = mini_isp_drv_set_output_format(&output_format_param);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_cp_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S!!", __func__);
	errcode = mini_isp_drv_set_cp_mode();
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_leave_cp_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u8 param[5];

	misp_info("%s S!!", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu %hhu %hhu %hhu %hhu",
			cmd_name, &param[0], &param[1],
			&param[2], &param[3], &param[4]);
	if (ret != 6) {
		errcode = -EINVAL;
		return errcode;
	}


	misp_info("0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
			param[0], param[1], param[2], param[3], param[4]);

	errcode = mini_isp_drv_leave_cp_mode(param[0], param[1], param[2], param[3], param[4]);
	misp_info("%s E!!", __func__);
	return errcode;
}
ssize_t echo_mini_isp_drv_preview_stream_on_off(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u8 tx0_stream_on_off;
	u8 tx1_stream_on_off;

	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %hhu %hhu", cmd_name,
		&tx0_stream_on_off, &tx1_stream_on_off);

	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("0x%x, 0x%x",
		tx0_stream_on_off, tx1_stream_on_off);
	errcode = mini_isp_drv_preview_stream_on_off(
		tx0_stream_on_off, tx1_stream_on_off);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_led_power_control(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[8];
	struct isp_cmd_led_power_control projector_control_param;

	memset(&projector_control_param, 0, sizeof(struct isp_cmd_led_power_control));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u %u %u %u %u",
		cmd_name, &param[0], &param[1], &param[2],
		&param[3], &param[4], &param[5],
		&param[6], &param[7]);

	if (ret != 9) {
		errcode = -EINVAL;
		return errcode;
	}

	projector_control_param.led_on_off =		  (u8)param[0];
	projector_control_param.led_power_level =	  (u8)param[1];
	projector_control_param.control_projector_id = (u8)param[2];
	projector_control_param.delay_after_sof =		  param[3];
	projector_control_param.pulse_time =			  param[4];
	projector_control_param.control_mode =		  (u8)param[5];
	projector_control_param.reserved = (u8)param[6];
	projector_control_param.rolling_shutter =	  (u8)param[7];

	misp_info("%d, %d, %d, %d, %d, %d, %d, %d",
		projector_control_param.led_on_off,
		projector_control_param.led_power_level,
		projector_control_param.control_projector_id,
		projector_control_param.delay_after_sof,
		projector_control_param.pulse_time,
		projector_control_param.control_mode,
		projector_control_param.reserved,
		projector_control_param.rolling_shutter);

	errcode = mini_isp_drv_led_power_control(&projector_control_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_active_ae(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[2];
	struct isp_cmd_active_ae active_ae_param;

	memset(&active_ae_param, 0,
		sizeof(struct isp_cmd_active_ae));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u",
		cmd_name, &param[0], &param[1]);

	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}


	active_ae_param.active_ae = (u8)param[0];
	active_ae_param.f_number_x1000 = param[1];

	misp_info("%d, %d",
		active_ae_param.active_ae,
		active_ae_param.f_number_x1000);

	errcode = mini_isp_drv_active_ae(&active_ae_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_isp_ae_control_mode_on_off(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param;

	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("%d", param);

	errcode = mini_isp_drv_isp_ae_control_mode_on_off(param);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_frame_rate_limits(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[4];
	struct isp_cmd_frame_rate_limits frame_rate_param;

	memset(&frame_rate_param, 0,
		sizeof(struct isp_cmd_frame_rate_limits));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u", cmd_name,
		&param[0], &param[1], &param[2], &param[3]);

	if (ret != 5) {
		errcode = -EINVAL;
		return errcode;
	}

	frame_rate_param.main_min_framerate_x100 = param[0];
	frame_rate_param.main_max_framerate_x100 = param[1];
	frame_rate_param.sub_min_framerate_x100 = param[2];
	frame_rate_param.sub_max_framerate_x100 = param[3];

	misp_info("%d %d %d %d",
		frame_rate_param.main_min_framerate_x100,
		frame_rate_param.main_max_framerate_x100,
		frame_rate_param.sub_min_framerate_x100,
		frame_rate_param.sub_max_framerate_x100);

	errcode = mini_isp_drv_set_frame_rate_limits(&frame_rate_param);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_max_exposure(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[3];

	struct isp_cmd_exposure_param set_max_exposure;

	memset(&set_max_exposure, 0, sizeof(struct isp_cmd_exposure_param));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u",
		cmd_name, &param[0], &param[1], &param[2]);

	if (ret != 4) {
		errcode = -EINVAL;
		return errcode;
	}

	set_max_exposure.udExpTime = (u32)param[0];
	set_max_exposure.uwISO = (u16)param[1];
	set_max_exposure.ucActiveDevice = (u8)param[2];

	misp_info("max exposure param: %d %d %d",
		set_max_exposure.udExpTime,
		set_max_exposure.uwISO,
		set_max_exposure.ucActiveDevice);

	mini_isp_drv_set_max_exposure(&set_max_exposure);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_frame_sync_control(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[5];
	struct isp_cmd_frame_sync_control frame_sync_control_param;

	memset(&frame_sync_control_param, 0, sizeof(struct isp_cmd_frame_sync_control));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u %u", cmd_name,
		&param[0], &param[1], &param[2], &param[3], &param[4]);

	if (ret != 6) {
		errcode = -EINVAL;
		return errcode;
	}

	frame_sync_control_param.control_deviceID = (u8) param[0];
	frame_sync_control_param.delay_framephase = (u8) param[1];
	frame_sync_control_param.active_framephase = (u8) param[2];
	frame_sync_control_param.deactive_framephase = (u8)param[3];
	frame_sync_control_param.active_timelevel = (u8) param[4];

	misp_info("%d %d %d %d %d",
		frame_sync_control_param.control_deviceID,
		frame_sync_control_param.delay_framephase,
		frame_sync_control_param.active_framephase,
		frame_sync_control_param.deactive_framephase,
		frame_sync_control_param.active_timelevel);

	errcode = mini_isp_drv_frame_sync_control(&frame_sync_control_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_shot_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[2];
	struct isp_cmd_set_shot_mode set_shot_mode_param;

	memset(&set_shot_mode_param, 0, sizeof(struct isp_cmd_set_shot_mode));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u", cmd_name, &param[0], &param[1]);

	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	set_shot_mode_param.shot_mode = (u8) param[0];
	set_shot_mode_param.frame_rate = (u16) param[1];

	misp_info("%d %d",
		set_shot_mode_param.shot_mode,
		set_shot_mode_param.frame_rate);

	errcode = mini_isp_drv_set_shot_mode(&set_shot_mode_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_lighting_ctrl(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u8 i = 0;
	u32 param[25];
	struct isp_cmd_lighting_ctrl lighting_ctrl;

	memset(&lighting_ctrl, 0, sizeof(struct isp_cmd_lighting_ctrl));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\
		%u %u %u %u %u",
		cmd_name, &param[0], &param[1], &param[2],
		&param[3], &param[4], &param[5], &param[6],
		&param[7], &param[8], &param[9], &param[10],
		&param[11], &param[12], &param[13], &param[14],
		&param[15], &param[16], &param[17], &param[18],
		&param[19], &param[20], &param[21], &param[22],
		&param[23], &param[24]);

	if (ret != 26) {
		errcode = -EINVAL;
		return errcode;
	}

	lighting_ctrl.cycle_len = param[0];
	for (i = 0; i < lighting_ctrl.cycle_len; i++) {
		lighting_ctrl.cycle[i].source = (u8)param[i*3+1];
		lighting_ctrl.cycle[i].TxDrop = (u8)param[i*3+2];
		lighting_ctrl.cycle[i].co_frame_rate = (u16)param[i*3+3];
	}

	misp_info("cycle_len: %d", lighting_ctrl.cycle_len);
	for (i = 0; i < lighting_ctrl.cycle_len; i++) {
		misp_info("cycle[%d]: %d, %d, %d", i,
			lighting_ctrl.cycle[i].source,
			lighting_ctrl.cycle[i].TxDrop,
			lighting_ctrl.cycle[i].co_frame_rate);
	}

	errcode = mini_isp_drv_lighting_ctrl(&lighting_ctrl);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_depth_compensation(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[4];
	u8 en_update = 0;
	struct isp_cmd_depth_compensation_param depth_compensation_param;

	memset(&depth_compensation_param, 0,
		sizeof(struct isp_cmd_depth_compensation_param));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u %u", cmd_name,
		&param[0], &param[1], &param[2], &param[3]);

	if (ret != 5) {
		errcode = -EINVAL;
		return errcode;
	}

	en_update = (param[0] << 4) | param[1];
	depth_compensation_param.en_updated = (u8)en_update;
	depth_compensation_param.short_distance_value = (u16)param[2];
	depth_compensation_param.compensation = (s8)param[3];

	misp_info("0x%x %d %d",
		depth_compensation_param.en_updated,
		depth_compensation_param.short_distance_value,
		depth_compensation_param.compensation);

	mini_isp_drv_depth_compensation(&depth_compensation_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_cycle_trigger_depth_process(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[3];

	struct isp_cmd_cycle_trigger_depth_process depth_cycle_param;

	memset(&depth_cycle_param, 0,
		sizeof(struct isp_cmd_cycle_trigger_depth_process));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u", cmd_name,
		&param[0], &param[1], &param[2]);

	if (ret != 4) {
		errcode = -EINVAL;
		return errcode;
	}

	depth_cycle_param.cycleLen = (u8)param[0];
	depth_cycle_param.depth_triggerBitField = (u16)param[1];
	depth_cycle_param.depthoutput_triggerBitField = (u16)param[2];

	misp_info("depth cycle len: 0%d %d %d",
		depth_cycle_param.cycleLen,
		depth_cycle_param.depth_triggerBitField,
		depth_cycle_param.depthoutput_triggerBitField);

	mini_isp_drv_cycle_trigger_depth_process(&depth_cycle_param);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_min_exposure(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[3];

	struct isp_cmd_exposure_param set_min_exposure;

	memset(&set_min_exposure, 0,
		sizeof(struct isp_cmd_exposure_param));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u %u", cmd_name,
		&param[0], &param[1], &param[2]);

	if (ret != 4) {
		errcode = -EINVAL;
		return errcode;
	}

	set_min_exposure.udExpTime = (u32)param[0];
	set_min_exposure.uwISO = (u16)param[1];
	set_min_exposure.ucActiveDevice = (u8)param[2];

	misp_info("min exposure param: %d %d %d",
		set_min_exposure.udExpTime,
		set_min_exposure.uwISO,
		set_min_exposure.ucActiveDevice);

	mini_isp_drv_set_min_exposure(&set_min_exposure);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_max_exposure_slope(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[2];

	struct isp_cmd_max_exposure_slope max_exposure_slope;

	memset(&max_exposure_slope, 0,
		sizeof(struct isp_cmd_max_exposure_slope));
	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u %u", cmd_name,
		&param[0], &param[1]);

	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	max_exposure_slope.max_exposure_slope = (u32)param[0];
	max_exposure_slope.ucActiveDevice = (u8)param[1];

	misp_info("max exposure slope: %d %d",
		max_exposure_slope.max_exposure_slope,
		max_exposure_slope.ucActiveDevice);

	mini_isp_drv_set_max_exposure_slope(&max_exposure_slope);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_led_active_delay(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 delay_ms = 0;

	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &delay_ms);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("[delay]: %d", delay_ms);

	mini_isp_drv_led_active_delay(delay_ms);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_isp_control_led_level(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 on = 0;

	misp_info("%s S!!", __func__);

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &on);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("[isp control led level]: %d", on);

	mini_isp_drv_isp_control_led_level((u8)on);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_get_chip_thermal(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	u16 thermal_val;

	misp_info("%s S!!", __func__);

	mini_isp_drv_get_chip_thermal(&thermal_val);
	misp_info("0x%x", thermal_val);
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_poweron(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S!!", __func__);
	mini_isp_poweron();
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_poweroff(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S!!", __func__);
	mini_isp_poweroff();
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_load_fw(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S!!", __func__);
	misp_info("%s no FSM", __func__);
	/*check bypass mode be set or not*/
	/*if yes, leave bypass mode*/
	mini_isp_check_and_leave_bypass_mode();
	/*normal case load FW*/
	errcode = mini_isp_drv_load_fw();

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_write_calibration_data(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param;

	misp_info("%s S!!", __func__);
	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("%d", param);
	if (param == 0 || param == 1) {
		misp_info("Currently echo mode not support"
			" IQ calibration and packdata!");
		goto ERR;
	}
	errcode = mini_isp_drv_write_calibration_data(param, NULL, 0);
ERR:
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_write_spinor_data(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param;

	misp_info("%s S!!", __func__);
	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("%d", param);
	if (param > 1)
		goto ERR;

	errcode = mini_isp_drv_write_spinor_data(param);
ERR:
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_mini_isp_get_chip_id(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S!!", __func__);
	errcode = mini_isp_get_chip_id();
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_get_comlog(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	struct common_log_hdr_info info;

	memset(&info, 0, sizeof(struct common_log_hdr_info));
	misp_info("%s S!!", __func__);

	info.block_size = SPI_TX_BULK_SIZE;
	info.total_size = LEVEL_LOG_BUFFER_SIZE;
	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_set_register(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param[2];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	misp_info("%s S", __func__);
	/* switch to E mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	ret = sscanf(cmd_buf, "%19s 0x%x 0x%x", cmd_name, &param[0], &param[1]);
	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	mini_isp_register_write(param[0], param[1]);
	misp_info("set register 0x%x, 0x%x", param[0], param[1]);

	/* switch to A mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_get_register(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	u32 param;
	u32 reg_val;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	misp_info("%s S", __func__);
	/* switch to E mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	ret = sscanf(cmd_buf, "%19s 0x%x", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	mini_isp_register_read(param, &reg_val);
	misp_info("get register 0x%x, 0x%x", param, reg_val);

	/* switch to A mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_memdump(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	char cmd_name[20];
	char filename[80];
	u32 param[2];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	misp_info("%s S", __func__);
	/* switch to E mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	ret = sscanf(cmd_buf, "%19s 0x%x %u", cmd_name, &param[0], &param[1]);

	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("Get mem 0x%x, %d\n", param[0], param[1]);

	snprintf(filename, 80, "memdump_0x%x_%d", param[0], param[1]);
	mini_isp_memory_read_then_write_file(param[0], param[1],
		MINIISP_INFO_DUMPLOCATION, filename);

	/* switch to A mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_show_version(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	misp_info("MINIISP_DRIVER_VERSION: %s", MINIISP_DRIVER_VERSION);
	misp_info("AL6100 project: %s, fw ver: %05d.%05d, build by %s",
		fw_project_name, fw_version_before_point, fw_version_after_point, fw_build_by);
	misp_info("SC table build data: %d", sc_build_date);
	misp_info("set fsm status: %d", dev_global_variable->now_state);
	return errcode;
}

ssize_t echo_set_fsm_status(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u32 param;
	char cmd_name[20];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	dev_global_variable->now_state = param;
	misp_info("set fsm status: %d", dev_global_variable->now_state);
	return errcode;
}

ssize_t echo_cfg_cmd_send(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u32 param;
	char cmd_name[20];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	ret = sscanf(cmd_buf, "%19s %u", cmd_name, &param);

	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	dev_global_variable->en_cmd_send = param;
	misp_info("set en_cmd_send: %d", dev_global_variable->en_cmd_send);
	return errcode;
}

ssize_t echo_mini_isp_a_to_e(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s, S", __func__);
	mini_isp_a_to_e();
	misp_info("%s, E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_e_to_a(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s, S", __func__);
	mini_isp_e_to_a();
	misp_info("%s, E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_chip_init(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s, S", __func__);
	mini_isp_chip_init();
	misp_info("%s, E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_dump_img(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s, S", __func__);
	errcode = mini_isp_debug_dump_img();
	misp_info("%s, E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_drv_set_bypass_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u16 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}
	if (param == 0)
		param = 1;
/*	errcode = mini_isp_drv_set_bypass_mode(param); */
	errcode = mini_isp_pure_bypass(param);

	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_utility_read_reg_e_mode_for_bypass_use(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_utility_read_reg_e_mode_for_bypass_use();
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_utility_read_reg_e_mode(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_utility_read_reg_e_mode();
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_packdata_dump(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_debug_packdata_dump();
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_IQCalib_dump(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_debug_IQCalib_dump();
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_metadata_dump(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_debug_metadata_dump();
	misp_info("%s E", __func__);
	return errcode;
}



ssize_t echo_mini_isp_debug_rect_combo_dump(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	misp_info("dump mode %hhu", param);
	errcode = mini_isp_debug_depth_rect_combo_dump(param);
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_depth_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_debug_depth_info();
	misp_info("%s E", __func__);
	return errcode;
}


ssize_t echo_mini_isp_debug_metadata_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);
	errcode = mini_isp_debug_metadata_info();
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_sensor_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	errcode = mini_isp_debug_sensor_info(param);
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_led_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	errcode = mini_isp_debug_led_info(param);
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_debug_rx_fps_info(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u8 param[2];
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu %hhu", cmd_name, &param[0], &param[1]);
	if (ret != 3) {
		errcode = -EINVAL;
		return errcode;
	}

	if (param[0] == 0) {
		misp_info("rx_fps_info initial!");
		errcode = mini_isp_debug_mipi_rx_fps_start(param[1]);
	} else if (param[0] == 1) {
		misp_info("rx_fps_info exit!");
		mini_isp_debug_mipi_rx_fps_stop();
	}

	misp_info("%s E", __func__);
	return errcode;
}
ssize_t echo_mini_isp_debug_GPIO_Status(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	errcode = mini_isp_debug_GPIO_Status(param);
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_eeprom_wp(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;
	int ret;
	struct misp_global_variable *dev_global_variable;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	if (WP_GPIO != NULL) {
		dev_global_variable = get_mini_isp_global_variable();
		if (param)
			gpio_set_value(dev_global_variable->wp_gpio, 1);
		else
			gpio_set_value(dev_global_variable->wp_gpio, 0);
	}
	misp_info("%s E", __func__);
	return errcode;
}

ssize_t echo_mini_isp_eeprom_op(const char *cmd_buf)
{
	errcode errcode = ERR_SUCCESS;
	int ret;
	struct misp_global_variable *dev_global_variable;
	u8 filename[80];
	struct file *filp = NULL;
	mm_segment_t oldfs;
	off_t currpos;
	u8 *data_buf_addr;
	u32 total_size;
	u8 param;
	char cmd_name[20];

	misp_info("%s S", __func__);
	ret = sscanf(cmd_buf, "%19s %hhu", cmd_name, &param);
	if (ret != 2) {
		errcode = -EINVAL;
		return errcode;
	}

	if (param == 1) {
		dev_global_variable = get_mini_isp_global_variable();

		snprintf(filename, 80, "%s/WRITE_OTP_DATA.bin",
			MINIISP_INFO_DUMPLOCATION);

		oldfs = get_fs();
		set_fs(KERNEL_DS);

		/*open the file*/
	#if ENABLE_FILP_OPEN_API
		/* use file open */
	#else
		misp_info("Error! Currently not support file open api");
		misp_info("See define ENABLE_FILP_OPEN_API");
		goto code_end;
	#endif
		/*get the file size*/
		currpos = vfs_llseek(filp, 0L, SEEK_END);
		if (currpos == -1) {
			misp_err("%s  llseek failed", __func__);
			errcode = ~ERR_SUCCESS;
			goto code_end;
		}
		total_size = (u32)currpos;
		misp_info("%s  filesize : %d", __func__, total_size);
		vfs_llseek(filp, 0L, SEEK_SET);

	if (total_size) {
		data_buf_addr = kzalloc(total_size, GFP_KERNEL);
		if (data_buf_addr == NULL) {
			misp_err("%s - Kzalloc data buf fail ", __func__);
			goto code_end;
		}
		errcode = vfs_read(filp, data_buf_addr, total_size,
				&filp->f_pos);

		if (errcode == -1) {
			misp_err("%s - Read file failed.", __func__);
		} else {
			if (WP_GPIO != NULL)
				gpio_set_value(dev_global_variable->wp_gpio, 0);

			errcode = mini_isp_drv_write_calibration_data(9,
						data_buf_addr, total_size);
			misp_info("%s write otp data = %x", __func__, errcode);

			if (WP_GPIO != NULL)
				gpio_set_value(dev_global_variable->wp_gpio, 1);
		}
		kfree(data_buf_addr);
	}
code_end:
	set_fs(oldfs);
	/*close the file*/
	filp_close(filp, NULL);
	} else if (param == 2)
		errcode = mini_isp_drv_read_calibration_data();
	else
		misp_err("%s - Not Support ", __func__);

	misp_info("%s E!!", __func__);
	return errcode;
}

ssize_t echo_test(const char *cmd_buf)
{
	size_t errcode = ERR_SUCCESS;

	misp_info("%s S", __func__);

	misp_info("%s, E", __func__);
	return errcode;
}

struct echo_cmd_format {
	u32 opcode;
	char cmd_name[30];
	ssize_t (*pfunc)(const char *cmd_buf);
};

struct echo_cmd_format echo_cmd_list[] = {
	{.opcode = 0x10B9,	.cmd_name = "depth_3a_info",
		.pfunc = echo_mini_isp_drv_set_depth_3a_info},
	{.opcode = 0x10BC,	.cmd_name = "interleave",
		.pfunc = echo_mini_isp_drv_set_depth_auto_interleave_mode},
	{.opcode = 0x10BF,	.cmd_name = "set_menu_exposure",
		.pfunc = echo_mini_isp_drv_set_exposure_param},
	{.opcode = 0x10C0,	.cmd_name = "set_depth_stream_size",
		.pfunc = echo_mini_isp_drv_set_depth_stream_size},
	{.opcode = 0x300A,	.cmd_name = "set_sensor",
		.pfunc = echo_mini_isp_drv_set_sensor_mode},
	{.opcode = 0x300D,	.cmd_name = "output_format",
		.pfunc = echo_mini_isp_drv_set_output_format},
	{.opcode = 0x300E,	.cmd_name = "enter_cp",
		.pfunc = echo_mini_isp_drv_set_cp_mode},
	{.opcode = 0x3010,	.cmd_name = "streamon",
		.pfunc = echo_mini_isp_drv_preview_stream_on_off},
	{.opcode = 0x3012,	.cmd_name = "led_power",
		.pfunc = echo_mini_isp_drv_led_power_control},
	{.opcode = 0x3013,	.cmd_name = "active_ae",
		.pfunc = echo_mini_isp_drv_active_ae},
	{.opcode = 0x3014,	.cmd_name = "ae_onoff",
		.pfunc = echo_mini_isp_drv_isp_ae_control_mode_on_off},
	{.opcode = 0x3015,	.cmd_name = "framerate",
		.pfunc = echo_mini_isp_drv_set_frame_rate_limits},
	{.opcode = 0x3017,	.cmd_name = "set_max_exposure",
		.pfunc = echo_mini_isp_drv_set_max_exposure},
	{.opcode = 0x3019,	.cmd_name = "frame_sync",
		.pfunc = echo_mini_isp_drv_frame_sync_control},
	{.opcode = 0x301A,	.cmd_name = "shot_mode",
		.pfunc = echo_mini_isp_drv_set_shot_mode},
	{.opcode = 0x301B,	.cmd_name = "lighting_ctrl",
		.pfunc = echo_mini_isp_drv_lighting_ctrl},
	{.opcode = 0x301C,	.cmd_name = "depth_compensation",
		.pfunc = echo_mini_isp_drv_depth_compensation},
	{.opcode = 0x301D,	.cmd_name = "cycle_trigger_depth",
		.pfunc = echo_mini_isp_drv_cycle_trigger_depth_process},
	{.opcode = 0x301E,	.cmd_name = "set_min_exposure",
		.pfunc = echo_mini_isp_drv_set_min_exposure},
	{.opcode = 0x301F,	.cmd_name = "set_max_exposure_slop",
		.pfunc = echo_mini_isp_drv_set_max_exposure_slope},
	{.opcode = 0x3020,	.cmd_name = "led_active_delay",
		.pfunc = echo_mini_isp_drv_led_active_delay},
	{.opcode = 0x3021,	.cmd_name = "isp_control_led_level",
		.pfunc = echo_mini_isp_drv_isp_control_led_level},
	{.opcode = 0x3022,	.cmd_name = "get_chip_thermal",
		.pfunc = echo_mini_isp_drv_get_chip_thermal},
	{.opcode = 0xFFFF,	.cmd_name = "power_on",
		.pfunc = echo_mini_isp_poweron},
	{.opcode = 0xFFFF,	.cmd_name = "power_off",
		.pfunc = echo_mini_isp_poweroff},
	{.opcode = 0xFFFF,	.cmd_name = "load_fw",
		.pfunc = echo_mini_isp_drv_load_fw},
	{.opcode = 0xFFFF,	.cmd_name = "load_cali",
		.pfunc = echo_mini_isp_drv_write_calibration_data},
	{.opcode = 0xFFFF,	.cmd_name = "load_spinor",
		.pfunc = echo_mini_isp_drv_write_spinor_data},
	{.opcode = 0xFFFF,	.cmd_name = "get_chip_id",
		.pfunc = echo_mini_isp_get_chip_id},
	{.opcode = 0xFFFF,	.cmd_name = "leave_cp",
		.pfunc = echo_mini_isp_drv_leave_cp_mode},
	{.opcode = 0xFFFF,	.cmd_name = "comlog",
		.pfunc = echo_get_comlog},
	{.opcode = 0xFFFF,	.cmd_name = "setreg",
		.pfunc = echo_set_register},
	{.opcode = 0xFFFF,	.cmd_name = "getreg",
		.pfunc = echo_get_register},
	{.opcode = 0xFFFF,	.cmd_name = "memdump",
		.pfunc = echo_memdump},
	{.opcode = 0xFFFF,	.cmd_name = "version",
		.pfunc = echo_show_version},
	{.opcode = 0xFFFF,	.cmd_name = "set_fsm_status",
		.pfunc = echo_set_fsm_status},
	{.opcode = 0xFFFF,	.cmd_name = "cfg_cmd_send",
		.pfunc = echo_cfg_cmd_send},
	{.opcode = 0xFFFF,	.cmd_name = "a2e",
		.pfunc = echo_mini_isp_a_to_e},
	{.opcode = 0xFFFF,	.cmd_name = "e2a",
		.pfunc = echo_mini_isp_e_to_a},
	{.opcode = 0xFFFF,	.cmd_name = "chip_init",
		.pfunc = echo_mini_isp_chip_init},
	{.opcode = 0xFFFF,	.cmd_name = "pure_bypass",
		.pfunc = echo_mini_isp_drv_set_bypass_mode},
	{.opcode = 0xFFFF,	.cmd_name = "dump_bypass_reg",
		.pfunc = echo_mini_isp_utility_read_reg_e_mode_for_bypass_use},
	{.opcode = 0xFFFF,	.cmd_name = "dump_normal_reg",
		.pfunc = echo_mini_isp_utility_read_reg_e_mode},
	{.opcode = 0xFFFF,	.cmd_name = "dump_packdata",
		.pfunc = echo_mini_isp_debug_packdata_dump},
	{.opcode = 0xFFFF,	.cmd_name = "dump_IQCalib",
		.pfunc = echo_mini_isp_debug_IQCalib_dump},
	{.opcode = 0xFFFF,	.cmd_name = "dump_Meta",
		.pfunc = echo_mini_isp_debug_metadata_dump},
	{.opcode = 0xFFFF,	.cmd_name = "dump_irp_img",
		.pfunc = echo_mini_isp_debug_dump_img},
	{.opcode = 0xFFFF,	.cmd_name = "dump_depth_reg",
		.pfunc = echo_mini_isp_debug_rect_combo_dump},
	{.opcode = 0xFFFF,	.cmd_name = "debug_depth_info",
		.pfunc = echo_mini_isp_debug_depth_info},
	{.opcode = 0xFFFF,	.cmd_name = "debug_metadata_info",
		.pfunc = echo_mini_isp_debug_metadata_info},
	{.opcode = 0xFFFF,	.cmd_name = "debug_sensor_info",
		.pfunc = echo_mini_isp_debug_sensor_info},
	{.opcode = 0xFFFF,	.cmd_name = "debug_led_info",
		.pfunc = echo_mini_isp_debug_led_info},
	{.opcode = 0xFFFF,	.cmd_name = "debug_rx_fps_info",
		.pfunc = echo_mini_isp_debug_rx_fps_info},
	{.opcode = 0xFFFF,	.cmd_name = "debug_GPIO_status",
		.pfunc = echo_mini_isp_debug_GPIO_Status},
	{.opcode = 0xFFFF,	.cmd_name = "eeprom_wp",
		.pfunc = echo_mini_isp_eeprom_wp},
	{.opcode = 0xFFFF,	.cmd_name = "eeprom_op",
		.pfunc = echo_mini_isp_eeprom_op},
	{.opcode = 0xFFFF,	.cmd_name = "test",
		.pfunc = echo_test},
};

#define echo_cmd_list_len \
	(sizeof(echo_cmd_list) / sizeof(struct echo_cmd_format))

static ssize_t mini_isp_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	/* print cmd list */
	u32 total_len = 0, count = 0;
	u32 i = 0;

	for (i = 0; i < echo_cmd_list_len; i++) {
		count = snprintf(buf, 70, "%s\n", echo_cmd_list[i].cmd_name);
		buf += count; /* move buffer pointer */
		total_len += count;
	}

	return total_len;
}

static ssize_t mini_isp_cmd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	char cmd_name[20];
	size_t errcode = ERR_SUCCESS;
	int ret;
	u32 opcode = 0x0;
	u32 err = ERR_SUCCESS;
	u32 loopidx = 0;

	ret = sscanf(buf, "%19s", cmd_name);
	if (ret != 1) {
		errcode = -EINVAL;
		return errcode;
	}
	/* check first input is opcode or cmd_name */
	if (cmd_name[0] == '0' && cmd_name[1] == 'x') {
		ret = sscanf(cmd_name, "0x%x", &opcode);

		if (ret != 1) {
			errcode = -EINVAL;
		return errcode;
	}
	}

	for (loopidx = 0; loopidx < echo_cmd_list_len; loopidx++) {
		if (echo_cmd_list[loopidx].opcode == opcode ||
			strcmp(echo_cmd_list[loopidx].cmd_name,
			cmd_name) == 0){

			err = echo_cmd_list[loopidx].pfunc(buf);
			if (err)
				misp_info("%s, err: 0x%x", __func__, err);

			return size;
		}
	}

	misp_info("command not find!");
	return size;
}

static
DEVICE_ATTR(mini_isp_cmd, 0660, mini_isp_cmd_show, mini_isp_cmd_store);

/************************************************************
*					Public Function						*
*************************************************************/

struct misp_data *get_mini_isp_intf(int i2c_type)
{
	if (misp_drv_global_variable->intf_status & INTF_SPI_READY) {
		return get_mini_isp_intf_spi();
	} else if (misp_drv_global_variable->intf_status & INTF_I2C_READY) {
		return get_mini_isp_intf_i2c(i2c_type);
	/*} else if (misp_drv_global_variable->intf_status & INTF_CCI_READY) {
		return get_mini_isp_intf_cci(i2c_type);*/
	} else {
		misp_err("%s - error i2c type %d", __func__, i2c_type);
		return NULL;
	}
}
void set_mini_isp_data(struct misp_data *data, int intf_type)
{
	if (!misp_drv_global_variable)
		misp_err("%s - set global_variable error", __func__);
	else
		misp_drv_global_variable->intf_status |= intf_type;
}

struct misp_global_variable *get_mini_isp_global_variable(void)
{
	if (!misp_drv_global_variable) {
		misp_err("%s - get global_variable error", __func__);
		return NULL;
	} else {
		return misp_drv_global_variable;
	}
}

int mini_isp_setup_resource(struct device *dev, struct misp_data *drv_data)
{
	int status = 0;

	misp_info("%s - start", __func__);
	if (misp_drv_global_variable != NULL) {
		misp_err("%s - resource already been setupped", __func__);
		goto setup_done;
	}

	/*step 1: alloc misp_drv_global_variable*/
	misp_drv_global_variable =
		kzalloc(sizeof(*misp_drv_global_variable), GFP_KERNEL);

	if (!misp_drv_global_variable) {
		misp_info("%s - Out of memory", __func__);
		status = -ENOMEM;
		goto alloc_fail;
	}
	misp_info("%s - step1 done.", __func__);

	/*step 2: init mutex and gpio resource*/
	mutex_init(&misp_drv_global_variable->busy_lock);
	status = mini_isp_gpio_init(dev, drv_data, misp_drv_global_variable);
	if (status < 0) {
		misp_info("%s - gpio init fail", __func__);
		goto setup_fail;
	}
	misp_info("%s - step2 done.", __func__);

	misp_drv_global_variable->before_booting = 1;
	misp_drv_global_variable->en_cmd_send = 1;

	/*step 3: register to VFS as character device*/
	mini_isp_class = class_create(THIS_MODULE, "mini_isp");
	if (IS_ERR(mini_isp_class))
		misp_err("Failed to create class(mini_isp_class)!");
	mini_isp_dev = miniisp_chdev_create(mini_isp_class);

	if (IS_ERR(mini_isp_dev))
		misp_err("Failed to create device(mini_isp_dev)!");

	status = device_create_file(mini_isp_dev,
				&dev_attr_mini_isp_mode_config);

	if (status < 0)
		misp_err("Failed to create device file(%s)!",
			dev_attr_mini_isp_mode_config.attr.name);

	if (RESET_GPIO != NULL) {
		status = device_create_file(mini_isp_dev,
						&dev_attr_mini_isp_reset);

		if (status < 0)
			misp_err("Failed to create device file(%s)!",
				dev_attr_mini_isp_reset.attr.name);
	}

	status = device_create_file(mini_isp_dev,
			&dev_attr_mini_isp_rectab);
	if (status < 0)
		misp_err("Failed to create device file(%s)!",
			dev_attr_mini_isp_rectab.attr.name);

	status = device_create_file(mini_isp_dev,
		&dev_attr_mini_isp_cmd);
	if (status < 0)
		misp_err("Failed to create device file(%s)!",
			dev_attr_mini_isp_cmd.attr.name);

	misp_info("%s - step3 done.", __func__);

	misp_info("%s - success.", __func__);
	goto setup_done;

setup_fail:
	mutex_destroy(&misp_drv_global_variable->busy_lock);
	kfree(misp_drv_global_variable);

alloc_fail:
	misp_drv_global_variable = NULL;

setup_done:
	return status;
}

struct device *mini_isp_getdev(void)
{
	return mini_isp_dev;
}

static int __init mini_isp_init(void)
{
	int ret = 0;
	extern struct spi_driver mini_isp_intf_spi;
	extern struct i2c_driver mini_isp_intf_i2c_slave;
	extern struct i2c_driver mini_isp_intf_i2c_top;

	misp_info("%s - start", __func__);
	isp_mast_camera_profile_para_init();

	/* register SPI driver */
	ret = spi_register_driver(&mini_isp_intf_spi);
	if (ret) {
		misp_err("%s - regsiter failed. Errorcode:%d",
			__func__, ret);
	}

	/* register I2C driver */
	ret = i2c_add_driver(&mini_isp_intf_i2c_slave);
	if (ret)
		misp_info("%s - failed. Error:%d", __func__, ret);

	ret = i2c_add_driver(&mini_isp_intf_i2c_top);
	if (ret)
		misp_info("%s - failed. Error:%d", __func__, ret);

	misp_info("MINIISP_DRIVER_VERSION: %s", MINIISP_DRIVER_VERSION);
	misp_info("%s - success", __func__);

	return ret;
}

static void __exit mini_isp_exit(void)
{
	extern struct spi_driver mini_isp_intf_spi;
	extern struct i2c_driver mini_isp_intf_i2c_slave;
	extern struct i2c_driver mini_isp_intf_i2c_top;

	misp_info("%s", __func__);

	if (misp_drv_global_variable->irq_gpio)
		gpio_free(misp_drv_global_variable->irq_gpio);

	/*if (misp_drv_global_variable)*/
		kfree(misp_drv_global_variable);

	/* unregister all driver */
	spi_unregister_driver(&mini_isp_intf_spi);
	i2c_del_driver(&mini_isp_intf_i2c_slave);
	i2c_del_driver(&mini_isp_intf_i2c_top);
}

module_init(mini_isp_init);
module_exit(mini_isp_exit);
MODULE_LICENSE("Dual BSD/GPL");
