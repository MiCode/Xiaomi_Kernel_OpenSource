/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include "cam_ir_led_core.h"

static int cam_ir_cut_on(struct cam_ir_led_ctrl *ictrl)
{
	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, "Ir_led control Null");
		return -EINVAL;
	}

	gpio_direction_output(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[4].gpio, 0);
	gpio_direction_input(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[3].gpio);

	return 0;
}

static int cam_ir_cut_off(struct cam_ir_led_ctrl *ictrl)
{
	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, "Ir_led control Null");
		return -EINVAL;
	}

	gpio_direction_output(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio, 0);
	gpio_direction_output(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[3].gpio, 0);
	gpio_direction_input(
		ictrl->soc_info.gpio_data->cam_gpio_common_tbl[4].gpio);

	return 0;
}

static int cam_ir_led_set_intensity(struct cam_ir_led_ctrl *ictrl,
			uint32_t ir_led_intensity)
{
	CAM_DBG(CAM_IR_LED, "ir_led_intensity=%d", ir_led_intensity);
	switch (ir_led_intensity) {
	case IRLED_INTENSITY_OFF:
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			0);
		break;

	case IRLED_INTENSITY_LEVEL1:
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[1].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[2].gpio,
			1);
		break;

	case IRLED_INTENSITY_LEVEL2:
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[1].gpio,
			0);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[2].gpio,
			1);
		break;
	case IRLED_INTENSITY_LEVEL3:
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[1].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[2].gpio,
			0);
		break;
	case IRLED_INTENSITY_LEVEL4:
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[0].gpio,
			1);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[1].gpio,
			0);
		gpio_direction_output(
			ictrl->soc_info.gpio_data->cam_gpio_common_tbl[2].gpio,
			0);
		break;
	}
	return 0;
}

int cam_ir_led_parser(struct cam_ir_led_ctrl *ictrl, void *arg)
{
	int rc = 0;
	uint32_t  *cmd_buf =  NULL;
	uintptr_t generic_ptr;
	uint32_t  *offset = NULL;
	size_t len_of_buffer;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_ir_led_set_on_off *cam_ir_led_info = NULL;

	if (!ictrl || !arg) {
		CAM_ERR(CAM_IR_LED, "ictrl/arg is NULL");
		return -EINVAL;
	}
	/* getting CSL Packet */
	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user((&config), u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config))) {
		CAM_ERR(CAM_IR_LED, "Copy cmd handle from user failed");
		rc = -EFAULT;
		return rc;
	}

	rc = cam_mem_get_cpu_buf(config.packet_handle,
		(uintptr_t *)&generic_ptr, &len_of_buffer);
	if (rc) {
		CAM_ERR(CAM_IR_LED, "Failed in getting the buffer : %d", rc);
		return rc;
	}

	if (config.offset > len_of_buffer) {
		CAM_ERR(CAM_IR_LED,
			"offset is out of bounds: offset: %lld len: %zu",
			config.offset, len_of_buffer);
		return -EINVAL;
	}

	/* Add offset to the ir_led csl header */
	csl_packet = (struct cam_packet *)(uintptr_t)(generic_ptr +
			config.offset);

	offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
		csl_packet->cmd_buf_offset);
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);
	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		(uintptr_t *)&generic_ptr, &len_of_buffer);
	if (rc < 0) {
		CAM_ERR(CAM_IR_LED, "Failed to get the command Buffer");
		return -EINVAL;
	}

	cmd_buf = (uint32_t *)((uint8_t *)generic_ptr +
		cmd_desc->offset);
	cam_ir_led_info = (struct cam_ir_led_set_on_off *)cmd_buf;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_IR_LED_PACKET_OPCODE_ON:
		CAM_DBG(CAM_IR_LED, ":CAM_IR_LED_PACKET_OPCODE_ON");
		cam_ir_cut_on(ictrl);
		cam_ir_led_set_intensity(ictrl,
				cam_ir_led_info->ir_led_intensity);
		break;
	case CAM_IR_LED_PACKET_OPCODE_OFF:
		CAM_DBG(CAM_IR_LED, "CAM_IR_LED_PACKET_OPCODE_OFF");
		cam_ir_cut_off(ictrl);
		break;
	case CAM_PKT_NOP_OPCODE:
		CAM_DBG(CAM_IR_LED, "CAM_IR_LED: CAM_PKT_NOP_OPCODE");
		break;
	default:
		CAM_ERR(CAM_IR_LED, "Wrong Opcode : %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		return -EINVAL;
	}

	return 0;
}

int cam_ir_led_stop_dev(struct cam_ir_led_ctrl *ictrl)
{
	int rc = 0;

	rc = cam_ir_cut_off(ictrl);

	return rc;
}

int cam_ir_led_release_dev(struct cam_ir_led_ctrl *ictrl)
{
	int rc = 0;

	if (ictrl->device_hdl != -1) {
		rc = cam_destroy_device_hdl(ictrl->device_hdl);
		if (rc)
			CAM_ERR(CAM_IR_LED,
				"Failed in destroying device handle rc = %d",
				rc);
		ictrl->device_hdl = -1;
	}

	return rc;
}

void cam_ir_led_shutdown(struct cam_ir_led_ctrl *ictrl)
{
	int rc;

	if (ictrl->ir_led_state == CAM_IR_LED_STATE_INIT)
		return;

	if ((ictrl->ir_led_state == CAM_IR_LED_STATE_CONFIG) ||
		(ictrl->ir_led_state == CAM_IR_LED_STATE_START)) {
		rc = cam_ir_led_stop_dev(ictrl);
		if (rc)
			CAM_ERR(CAM_IR_LED, "Stop Failed rc: %d", rc);
	}

	rc = cam_ir_led_release_dev(ictrl);
	if (rc)
		CAM_ERR(CAM_IR_LED, "Release failed rc: %d", rc);

	ictrl->ir_led_state = CAM_IR_LED_STATE_INIT;
}
