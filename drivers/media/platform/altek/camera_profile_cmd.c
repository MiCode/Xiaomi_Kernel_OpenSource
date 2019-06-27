/*
 * File: camera_profile_cmd.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/10/14; Bruce Chung; Initial version
 *  2013/12/05; Bruce Chung; 2nd version
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



/******Include File******/

#include <linux/string.h>

#include "include/mtype.h"
#include "include/error.h"
#include "include/miniisp.h"
#include "include/isp_camera_cmd.h"
#include "include/ispctrl_if_master.h"
#include "include/ispctrl_if_master_local.h"

/******Private Constant Definition******/
#define MINI_ISP_LOG_TAG "[[miniisp]camera_profile_cmd]"

/******Private Type Declaration******/

/******Private Function Prototype******/

/******Private Global Variable******/

static struct isp_cmd_get_sensor_mode mast_sensors_info;

/******Public Function******/

/*
 *\brief Camera profile parameters init
 *\return None
 */
void isp_mast_camera_profile_para_init(void)
{
	/*Reset Camera profile parameters*/
	memset(&mast_sensors_info, 0x0,
		sizeof(struct isp_cmd_get_sensor_mode));
}

/*
 *\brief Set Sensor Mode
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_sensor_mode(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_set_sensor_mode *set_sensor_mode_param;

	set_sensor_mode_param = (struct isp_cmd_set_sensor_mode *)param;
	misp_info("%s - enter", __func__);
	misp_info("[on/off]: %d, [id]:%d, [txskew]: %d",
		set_sensor_mode_param->sensor_on_off,
		set_sensor_mode_param->scenario_id,
		set_sensor_mode_param->mipi_tx_skew);

	misp_info("[w_tbl_idx]: %d, [merge_enable]:%d",
		set_sensor_mode_param->ae_weighting_table_index,
		set_sensor_mode_param->merge_mode_enable);

	para_size = sizeof(struct isp_cmd_set_sensor_mode);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
				param, para_size);


	if ((((struct isp_cmd_set_sensor_mode *)param)->sensor_on_off)
			&& (err == ERR_SUCCESS))
		err = mini_isp_wait_for_event(MINI_ISP_RCV_SETSENSORMODE);

	return err;
}

/*
 *\brief Get Sensor Mode
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_get_sensor_mode(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);

	para_size = sizeof(struct isp_cmd_get_sensor_mode);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode, NULL, 0);
	if (err  != ERR_SUCCESS)
		goto mast_camera_profile_cmd_get_sensor_mode_end;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata,
			(u8 *)&mast_sensors_info, para_size, true);
	if (err  != ERR_SUCCESS)
		goto mast_camera_profile_cmd_get_sensor_mode_end;

	/* copy to usr defined target addr*/
	memcpy(param, &mast_sensors_info, para_size);

mast_camera_profile_cmd_get_sensor_mode_end:
	return err;
}

/*
 *\brief Set Output Format
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_output_format(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_set_output_format *set_output_format_param;

	set_output_format_param = (struct isp_cmd_set_output_format *)param;

	misp_info("%s - enter", __func__);
	misp_info("[DP_size]: 0x%x, [DP_Type_LV]: 0x%x, [InvRect_bypass]: 0x%x",
		set_output_format_param->depth_size,
		set_output_format_param->reserve[0],
		set_output_format_param->reserve[1]);

	para_size = sizeof(struct isp_cmd_set_output_format);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode, param,
						para_size);
	return err;
}

/*
 *\brief Set CP Mode
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_cp_mode(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);

	para_size = 0;

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
				param, para_size);
	if (err != ERR_SUCCESS)
		goto mast_camera_profile_cmd_set_cp_mode_end;

	err = mini_isp_wait_for_event(MINI_ISP_RCV_CPCHANGE);

mast_camera_profile_cmd_set_cp_mode_end:
	return err;
}

/*
 *\brief Set AE statistics
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_basic_setting_cmd_set_ae_statistics(
					void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_ae_statistics);

	misp_info("%s - enter", __func__);
	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Preview stream on off
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_preview_stream_on_off(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_preview_stream_on_off *preview_stream_on_off_param;

	preview_stream_on_off_param =
		(struct isp_cmd_preview_stream_on_off *)param;

	misp_info("%s - enter", __func__);
	misp_info("[tx0]: %d, [tx1]: %d",
		preview_stream_on_off_param->tx0_stream_on_off,
		preview_stream_on_off_param->tx1_stream_on_off);

	para_size = sizeof(struct isp_cmd_preview_stream_on_off);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief dual PD Y calculation weightings
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode
mast_camera_profile_cmd_dual_pd_y_cauculation_weightings(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_dual_pd_y_calculation_weightings);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief LED power control
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_led_power_control(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_led_power_control *led_power_control_param;

	led_power_control_param = (struct isp_cmd_led_power_control *)param;
	misp_info("%s - enter", __func__);
	misp_info("[led_onoff]: %d, [led_lv]: %d, [led_id]: %d",
		led_power_control_param->led_on_off,
		led_power_control_param->led_power_level,
		led_power_control_param->control_projector_id);

	misp_info("[delay_after_sof]: %d, [pulse_time]: %d",
		led_power_control_param->delay_after_sof,
		led_power_control_param->pulse_time);

	misp_info("[control_mode]: %d, [reserved]: %d, [rolling_shutter]: %d",
		led_power_control_param->control_mode,
		led_power_control_param->reserved,
		led_power_control_param->rolling_shutter);

	para_size = sizeof(struct isp_cmd_led_power_control);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Active AE
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_active_ae(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_active_ae *active_ae_param;

	active_ae_param = (struct isp_cmd_active_ae *) param;
	misp_info("%s - enter", __func__);
	misp_info("[active ae]: %d, [f_number]: %d",
		active_ae_param->active_ae, active_ae_param->f_number_x1000);

	para_size = sizeof(struct isp_cmd_active_ae);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Control AE on/off
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_isp_ae_control_on_off(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_isp_ae_control_on_off *ae_control_on_off_param;

	ae_control_on_off_param = (struct isp_cmd_isp_ae_control_on_off *)param;
	misp_info("%s - enter", __func__);
	misp_info("[ae control]: %d",
		ae_control_on_off_param->isp_ae_control_mode_on_off);

	para_size = sizeof(struct isp_cmd_isp_ae_control_on_off);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set Frame Rate Limits
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_frame_rate_limits(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_frame_rate_limits);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set period drop frame
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_period_drop_frame(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_period_drop_frame *period_drop_frame_param;

	period_drop_frame_param = (struct isp_cmd_period_drop_frame *)param;
	misp_info("%s - enter", __func__);
	misp_info("[period_drop_type]: %d",
		period_drop_frame_param->period_drop_type);

	para_size = sizeof(struct isp_cmd_period_drop_frame);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set Max exposure
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_max_exposure(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_exposure_param);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set target mean
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_target_mean(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_target_mean);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Frame sync control
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_frame_sync_control(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_frame_sync_control *frame_sync_control_param;

	frame_sync_control_param = (struct isp_cmd_frame_sync_control *)param;
	misp_info("%s - enter", __func__);
	misp_info("[deviceId]: %d, [delay_frame]: %d, [active_frame]: %d,",
		frame_sync_control_param->control_deviceID,
		frame_sync_control_param->delay_framephase,
		frame_sync_control_param->active_framephase);

	misp_info("[deactive_frame]: %d, [active_timeLv]: %d",
		frame_sync_control_param->deactive_framephase,
		frame_sync_control_param->active_timelevel);

	para_size = sizeof(struct isp_cmd_frame_sync_control);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set shot mode
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_shot_mode(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_set_shot_mode *set_shot_mode_param;

	set_shot_mode_param = (struct isp_cmd_set_shot_mode *)param;
	misp_info("%s - enter", __func__);
	misp_info("[shot_mode]: %d, [frame_rate]: %d",
		set_shot_mode_param->shot_mode,
		set_shot_mode_param->frame_rate);
	para_size = sizeof(struct isp_cmd_set_shot_mode);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Lighting control
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_lighting_ctrl(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size, i;
	struct isp_cmd_lighting_ctrl *lighting_ctrl_param;

	lighting_ctrl_param = (struct isp_cmd_lighting_ctrl *)param;
	misp_info("%s - enter", __func__);
	misp_info("[cycle_len]: %d", lighting_ctrl_param->cycle_len);
	for (i = 0; i < lighting_ctrl_param->cycle_len; i++) {
		misp_info("cycle[%d]: 0x%x, 0x%x, %d", i,
			lighting_ctrl_param->cycle[i].source,
			lighting_ctrl_param->cycle[i].TxDrop,
			lighting_ctrl_param->cycle[i].co_frame_rate);
	}
	para_size = sizeof(struct isp_cmd_lighting_ctrl);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set Min exposure
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_min_exposure(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_exposure_param);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set Max exposure slope
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_max_exposure_slope(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	para_size = sizeof(struct isp_cmd_max_exposure_slope);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set Depth Compensation
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_depth_compensation(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_depth_compensation_param *depth_compensation_param;

	depth_compensation_param =
		(struct isp_cmd_depth_compensation_param *)param;

	misp_info("%s - enter", __func__);
	misp_info("[en_updated]: 0x%x, [short_dist]: %d, [compensation]: %d",
		depth_compensation_param->en_updated,
		depth_compensation_param->short_distance_value,
		depth_compensation_param->compensation);

	para_size = sizeof(struct isp_cmd_depth_compensation_param);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}

/*
 *\brief Set cycle trigger depth process
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_cycle_trigger_depth(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;
	struct isp_cmd_cycle_trigger_depth_process
		*cycle_trigger_depth_process_param;

	cycle_trigger_depth_process_param =
		(struct isp_cmd_cycle_trigger_depth_process *)param;

	misp_info("%s - enter", __func__);
	misp_info("[Cycle_len]: %d, [DP_trigBitField]: 0x%x",
		cycle_trigger_depth_process_param->cycleLen,
		cycle_trigger_depth_process_param->depth_triggerBitField);

	misp_info("[DPOut_trigBitField]: 0x%x",
		cycle_trigger_depth_process_param->depthoutput_triggerBitField);

	para_size = sizeof(struct isp_cmd_cycle_trigger_depth_process);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}
/*
 *\brief Set led active delay time
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_set_led_active_delay(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	misp_info("[delay]: %d", *(u32 *)param);
	para_size = sizeof(u32);
	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}
/*
 *\brief Set isp control led level
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_camera_profile_cmd_isp_control_led_level(
					void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size;

	misp_info("%s - enter", __func__);
	misp_info("[isp control led level]: %d", *(u8 *)param);
	para_size = sizeof(u8);
	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
			param, para_size);
	return err;
}
/************************** End Of File *******************************/
