/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>

#include "cam_sensor_cmn_header.h"
#include "cam_flash_core.h"
#include "cam_res_mgr_api.h"

int cam_flash_prepare(struct cam_flash_ctrl *flash_ctrl,
	bool regulator_enable)
{
	int rc = 0;

	if (!(flash_ctrl->switch_trigger)) {
		CAM_ERR(CAM_FLASH, "Invalid argument");
		return -EINVAL;
	}

	if (regulator_enable &&
		(flash_ctrl->is_regulator_enabled == false)) {
		rc = qpnp_flash_led_prepare(flash_ctrl->switch_trigger,
			ENABLE_REGULATOR, NULL);
		if (rc) {
			CAM_ERR(CAM_FLASH, "regulator enable failed rc = %d",
				rc);
			return rc;
		}
		flash_ctrl->is_regulator_enabled = true;
	} else if ((!regulator_enable) &&
		(flash_ctrl->is_regulator_enabled == true)) {
		rc = qpnp_flash_led_prepare(flash_ctrl->switch_trigger,
			DISABLE_REGULATOR, NULL);
		if (rc) {
			CAM_ERR(CAM_FLASH, "regulator disable failed rc = %d",
				rc);
			return rc;
		}
		flash_ctrl->is_regulator_enabled = false;
	} else {
		CAM_ERR(CAM_FLASH, "Wrong Flash State : %d",
			flash_ctrl->flash_state);
		rc = -EINVAL;
	}

	return rc;
}

static int cam_flash_flush_nrt(struct cam_flash_ctrl *fctrl)
{
	int j = 0;
	struct cam_flash_frame_setting *nrt_settings;

	if (!fctrl)
		return -EINVAL;

	nrt_settings = &fctrl->nrt_info;

	if (nrt_settings->cmn_attr.cmd_type ==
		CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_INFO) {
		fctrl->flash_init_setting.cmn_attr.is_settings_valid = false;
	} else if ((nrt_settings->cmn_attr.cmd_type ==
		CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET) ||
		(nrt_settings->cmn_attr.cmd_type ==
		CAMERA_SENSOR_FLASH_CMD_TYPE_RER) ||
		(nrt_settings->cmn_attr.cmd_type ==
		CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE)) {
		fctrl->nrt_info.cmn_attr.is_settings_valid = false;
		fctrl->nrt_info.cmn_attr.count = 0;
		fctrl->nrt_info.num_iterations = 0;
		fctrl->nrt_info.led_on_delay_ms = 0;
		fctrl->nrt_info.led_off_delay_ms = 0;
		for (j = 0; j < CAM_FLASH_MAX_LED_TRIGGERS; j++)
			fctrl->nrt_info.led_current_ma[j] = 0;
	}

	return 0;
}

int cam_flash_flush_request(struct cam_req_mgr_flush_request *flush)
{
	int rc = 0;
	int i = 0, j = 0;
	struct cam_flash_ctrl *fctrl = NULL;
	int frame_offset = 0;

	fctrl = (struct cam_flash_ctrl *) cam_get_device_priv(flush->dev_hdl);
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Device data is NULL");
		return -EINVAL;
	}

	if (flush->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
	/* flush all requests*/
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			fctrl->per_frame[i].cmn_attr.request_id = 0;
			fctrl->per_frame[i].cmn_attr.is_settings_valid = false;
			fctrl->per_frame[i].cmn_attr.count = 0;
			for (j = 0; j < CAM_FLASH_MAX_LED_TRIGGERS; j++)
				fctrl->per_frame[i].led_current_ma[j] = 0;
		}

		rc = cam_flash_flush_nrt(fctrl);
		if (rc)
			CAM_ERR(CAM_FLASH, "NonRealTime flush error");
	} else if (flush->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
	/* flush request with req_id*/
		frame_offset = flush->req_id % MAX_PER_FRAME_ARRAY;
		fctrl->per_frame[frame_offset].cmn_attr.request_id = 0;
		fctrl->per_frame[frame_offset].cmn_attr.is_settings_valid =
			false;
		fctrl->per_frame[frame_offset].cmn_attr.count = 0;
		for (i = 0; i < CAM_FLASH_MAX_LED_TRIGGERS; i++)
			fctrl->per_frame[frame_offset].led_current_ma[i] = 0;
	}
	return rc;
}

static int cam_flash_ops(struct cam_flash_ctrl *flash_ctrl,
	struct cam_flash_frame_setting *flash_data, enum camera_flash_opcode op)
{
	uint32_t curr = 0, max_current = 0;
	struct cam_flash_private_soc *soc_private = NULL;
	int i = 0;

	if (!flash_ctrl || !flash_data) {
		CAM_ERR(CAM_FLASH, "Fctrl or Data NULL");
		return -EINVAL;
	}

	soc_private = (struct cam_flash_private_soc *)
		flash_ctrl->soc_info.soc_private;

	if (op == CAMERA_SENSOR_FLASH_OP_FIRELOW) {
		for (i = 0; i < flash_ctrl->torch_num_sources; i++) {
			if (flash_ctrl->torch_trigger[i]) {
				max_current = soc_private->torch_max_current[i];

				if (flash_data->led_current_ma[i] <=
					max_current)
					curr = flash_data->led_current_ma[i];
				else
					curr = soc_private->torch_op_current[i];

				CAM_DBG(CAM_PERF,
					"Led_Current[%d] = %d", i, curr);
				cam_res_mgr_led_trigger_event(
					flash_ctrl->torch_trigger[i],
					curr);
			}
		}
	} else if (op == CAMERA_SENSOR_FLASH_OP_FIREHIGH) {
		for (i = 0; i < flash_ctrl->flash_num_sources; i++) {
			if (flash_ctrl->flash_trigger[i]) {
				max_current = soc_private->flash_max_current[i];

				if (flash_data->led_current_ma[i] <=
					max_current)
					curr = flash_data->led_current_ma[i];
				else
					curr = soc_private->flash_op_current[i];

				CAM_DBG(CAM_PERF, "LED flash_current[%d]: %d",
					i, curr);
				cam_res_mgr_led_trigger_event(
					flash_ctrl->flash_trigger[i],
					curr);
			}
		}
	} else {
		CAM_ERR(CAM_FLASH, "Wrong Operation: %d", op);
		return -EINVAL;
	}

	if (flash_ctrl->switch_trigger)
		cam_res_mgr_led_trigger_event(
			flash_ctrl->switch_trigger,
			LED_SWITCH_ON);

	return 0;
}

int cam_flash_off(struct cam_flash_ctrl *flash_ctrl)
{
	if (!flash_ctrl) {
		CAM_ERR(CAM_FLASH, "Flash control Null");
		return -EINVAL;
	}

	if (flash_ctrl->switch_trigger)
		cam_res_mgr_led_trigger_event(flash_ctrl->switch_trigger,
			LED_SWITCH_OFF);

	flash_ctrl->flash_state = CAM_FLASH_STATE_START;
	return 0;
}

static int cam_flash_low(
	struct cam_flash_ctrl *flash_ctrl,
	struct cam_flash_frame_setting *flash_data)
{
	int i = 0, rc = 0;

	if (!flash_data) {
		CAM_ERR(CAM_FLASH, "Flash Data Null");
		return -EINVAL;
	}

	for (i = 0; i < flash_ctrl->flash_num_sources; i++)
		if (flash_ctrl->flash_trigger[i])
			cam_res_mgr_led_trigger_event(
				flash_ctrl->flash_trigger[i],
				LED_OFF);

	rc = cam_flash_ops(flash_ctrl, flash_data,
		CAMERA_SENSOR_FLASH_OP_FIRELOW);
	if (rc)
		CAM_ERR(CAM_FLASH, "Fire Torch failed: %d", rc);

	return rc;
}

static int cam_flash_high(
	struct cam_flash_ctrl *flash_ctrl,
	struct cam_flash_frame_setting *flash_data)
{
	int i = 0, rc = 0;

	if (!flash_data) {
		CAM_ERR(CAM_FLASH, "Flash Data Null");
		return -EINVAL;
	}

	for (i = 0; i < flash_ctrl->torch_num_sources; i++)
		if (flash_ctrl->torch_trigger[i])
			cam_res_mgr_led_trigger_event(
				flash_ctrl->torch_trigger[i],
				LED_OFF);

	rc = cam_flash_ops(flash_ctrl, flash_data,
		CAMERA_SENSOR_FLASH_OP_FIREHIGH);
	if (rc)
		CAM_ERR(CAM_FLASH, "Fire Flash Failed: %d", rc);

	return rc;
}

static int delete_req(struct cam_flash_ctrl *fctrl, uint64_t req_id)
{
	int i = 0;
	struct cam_flash_frame_setting *flash_data = NULL;
	uint64_t top = 0, del_req_id = 0;

	if (req_id == 0) {
		flash_data = &fctrl->nrt_info;
		if ((fctrl->nrt_info.cmn_attr.cmd_type ==
			CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET) ||
			(fctrl->nrt_info.cmn_attr.cmd_type ==
			CAMERA_SENSOR_FLASH_CMD_TYPE_RER)) {
			flash_data->cmn_attr.is_settings_valid = false;
			for (i = 0; i < flash_data->cmn_attr.count; i++)
				flash_data->led_current_ma[i] = 0;
		} else {
			fctrl->flash_init_setting.cmn_attr.is_settings_valid
				= false;
		}
	} else {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			flash_data = &fctrl->per_frame[i];
			if (req_id >= flash_data->cmn_attr.request_id &&
				flash_data->cmn_attr.is_settings_valid
				== 1) {
				if (top < flash_data->cmn_attr.request_id) {
					del_req_id = top;
					top = flash_data->cmn_attr.request_id;
				} else if (top >
					flash_data->cmn_attr.request_id &&
					del_req_id <
					flash_data->cmn_attr.request_id) {
					del_req_id =
						flash_data->cmn_attr.request_id;
				}
			}
		}

		if (top < req_id) {
			if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
				(((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
				del_req_id = req_id;
		}

		if (!del_req_id)
			return 0;

		CAM_DBG(CAM_FLASH, "top: %llu, del_req_id:%llu",
			top, del_req_id);

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			flash_data = &fctrl->per_frame[i];
			if ((del_req_id ==
				flash_data->cmn_attr.request_id) &&
				(flash_data->cmn_attr.
					is_settings_valid == 1)) {
				CAM_DBG(CAM_FLASH, "Deleting request[%d] %llu",
					i, flash_data->cmn_attr.request_id);
				flash_data->cmn_attr.request_id = 0;
				flash_data->cmn_attr.is_settings_valid = false;
				flash_data->opcode = 0;
				for (i = 0; i < flash_data->cmn_attr.count; i++)
					flash_data->led_current_ma[i] = 0;
			}
		}
	}

	return 0;
}

int cam_flash_apply_setting(struct cam_flash_ctrl *fctrl,
	uint64_t req_id)
{
	int rc = 0, i = 0;
	int frame_offset = 0;
	uint16_t num_iterations;
	struct cam_flash_frame_setting *flash_data = NULL;

	if (req_id == 0) {
		if (fctrl->nrt_info.cmn_attr.cmd_type ==
			CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE) {
			flash_data = &fctrl->nrt_info;
			CAM_DBG(CAM_REQ,
				"FLASH_INIT_FIRE req_id: %u flash_opcode: %d",
				req_id, flash_data->opcode);

			if (flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_FIREHIGH) {
				if (fctrl->flash_state !=
					CAM_FLASH_STATE_CONFIG) {
					CAM_WARN(CAM_FLASH,
					"Cannot apply Start Dev:Prev state: %d",
					fctrl->flash_state);
					return rc;
				}
				rc = cam_flash_prepare(fctrl, true);
				if (rc) {
					CAM_ERR(CAM_FLASH,
					"Enable Regulator Failed rc = %d", rc);
					return rc;
				}
				rc = cam_flash_high(fctrl, flash_data);
				if (rc)
					CAM_ERR(CAM_FLASH,
						"FLASH ON failed : %d",
						rc);
			}
			if (flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_OFF) {
				rc = cam_flash_off(fctrl);
				if (rc) {
					CAM_ERR(CAM_FLASH,
					"LED OFF FAILED: %d",
					rc);
					return rc;
				}
				if ((fctrl->flash_state ==
					CAM_FLASH_STATE_START) &&
					(fctrl->is_regulator_enabled == true)) {
					rc = cam_flash_prepare(fctrl, false);
					if (rc)
						CAM_ERR(CAM_FLASH,
						"Disable Regulator failed: %d",
						rc);
				}
			}
		} else if (fctrl->nrt_info.cmn_attr.cmd_type ==
			CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET) {
			flash_data = &fctrl->nrt_info;
			CAM_DBG(CAM_REQ,
				"FLASH_WIDGET req_id: %u flash_opcode: %d",
				req_id, flash_data->opcode);

			if (flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_FIRELOW) {
				rc = cam_flash_low(fctrl, flash_data);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Torch ON failed : %d",
						rc);
					goto nrt_del_req;
				}
			} else if (flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_OFF) {
				rc = cam_flash_off(fctrl);
				if (rc)
					CAM_ERR(CAM_FLASH,
					"LED off failed: %d",
					rc);
			}
		} else if (fctrl->nrt_info.cmn_attr.cmd_type ==
			CAMERA_SENSOR_FLASH_CMD_TYPE_RER) {
			flash_data = &fctrl->nrt_info;

			if (fctrl->flash_state != CAM_FLASH_STATE_START) {
				rc = cam_flash_off(fctrl);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Flash off failed: %d",
						rc);
					goto nrt_del_req;
				}
			}
			CAM_DBG(CAM_REQ, "FLASH_RER req_id: %u", req_id);

			num_iterations = flash_data->num_iterations;
			for (i = 0; i < num_iterations; i++) {
				/* Turn On Torch */
				if (fctrl->flash_state ==
					CAM_FLASH_STATE_START) {
					rc = cam_flash_low(fctrl, flash_data);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Fire Torch Failed");
						goto nrt_del_req;
					}

					usleep_range(
					flash_data->led_on_delay_ms * 1000,
					flash_data->led_on_delay_ms * 1000 +
						100);
				}
				/* Turn Off Torch */
				rc = cam_flash_off(fctrl);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Flash off failed: %d",
						rc);
					continue;
				}
				fctrl->flash_state = CAM_FLASH_STATE_START;
				usleep_range(
				flash_data->led_off_delay_ms * 1000,
				flash_data->led_off_delay_ms * 1000 + 100);
			}
		}
	} else {
		frame_offset = req_id % MAX_PER_FRAME_ARRAY;
		flash_data = &fctrl->per_frame[frame_offset];
		CAM_DBG(CAM_REQ, "FLASH_RT req_id: %u flash_opcode: %d",
			req_id, flash_data->opcode);

		if ((flash_data->opcode == CAMERA_SENSOR_FLASH_OP_FIREHIGH) &&
			(flash_data->cmn_attr.is_settings_valid) &&
			(flash_data->cmn_attr.request_id == req_id)) {
			/* Turn On Flash */
			if (fctrl->flash_state == CAM_FLASH_STATE_START) {
				rc = cam_flash_high(fctrl, flash_data);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Flash ON failed: rc= %d",
						rc);
					goto apply_setting_err;
				}
			}
		} else if ((flash_data->opcode ==
			CAMERA_SENSOR_FLASH_OP_FIRELOW) &&
			(flash_data->cmn_attr.is_settings_valid) &&
			(flash_data->cmn_attr.request_id == req_id)) {
			/* Turn On Torch */
			if (fctrl->flash_state == CAM_FLASH_STATE_START) {
				rc = cam_flash_low(fctrl, flash_data);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Torch ON failed: rc= %d",
						rc);
					goto apply_setting_err;
				}
			}
		} else if ((flash_data->opcode == CAMERA_SENSOR_FLASH_OP_OFF) &&
			(flash_data->cmn_attr.is_settings_valid) &&
			(flash_data->cmn_attr.request_id == req_id)) {
			rc = cam_flash_off(fctrl);
			if (rc) {
				CAM_ERR(CAM_FLASH,
					"Flash off failed %d", rc);
				goto apply_setting_err;
			}
		} else if (flash_data->opcode == CAM_PKT_NOP_OPCODE) {
			flash_data->opcode = 0;
			CAM_DBG(CAM_FLASH, "NOP Packet");
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_FLASH, "Invalid opcode: %d req_id: %llu",
				flash_data->opcode, req_id);
			goto apply_setting_err;
		}
	}

nrt_del_req:
	delete_req(fctrl, req_id);
apply_setting_err:
	return rc;
}

int cam_flash_parser(struct cam_flash_ctrl *fctrl, void *arg)
{
	int rc = 0, i = 0;
	uint64_t generic_ptr;
	uint32_t *cmd_buf =  NULL;
	uint32_t *offset = NULL;
	uint32_t frm_offset = 0;
	size_t len_of_buffer;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct common_header *cmn_hdr;
	struct cam_config_dev_cmd config;
	struct cam_req_mgr_add_request add_req;
	struct cam_flash_init *cam_flash_info = NULL;
	struct cam_flash_set_rer *flash_rer_info = NULL;
	struct cam_flash_set_on_off *flash_operation_info = NULL;
	struct cam_flash_query_curr *flash_query_info = NULL;
	struct cam_flash_frame_setting *flash_data = NULL;

	if (!fctrl || !arg) {
		CAM_ERR(CAM_FLASH, "fctrl/arg is NULL");
		return -EINVAL;
	}
	/* getting CSL Packet */
	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user((&config), (void __user *) ioctl_ctrl->handle,
		sizeof(config))) {
		CAM_ERR(CAM_FLASH, "Copy cmd handle from user failed");
		rc = -EFAULT;
		return rc;
	}

	rc = cam_mem_get_cpu_buf(config.packet_handle,
		(uint64_t *)&generic_ptr, &len_of_buffer);
	if (rc) {
		CAM_ERR(CAM_FLASH, "Failed in getting the buffer : %d", rc);
		return rc;
	}

	if (config.offset > len_of_buffer) {
		CAM_ERR(CAM_FLASH,
			"offset is out of bounds: offset: %lld len: %zu",
			config.offset, len_of_buffer);
		return -EINVAL;
	}

	/* Add offset to the flash csl header */
	csl_packet = (struct cam_packet *)(generic_ptr + config.offset);

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_FLASH_PACKET_OPCODE_INIT: {
		/* INIT packet*/
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		fctrl->flash_init_setting.cmn_attr.request_id = 0;
		fctrl->flash_init_setting.cmn_attr.is_settings_valid = true;
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buffer);
		cmd_buf = (uint32_t *)((uint8_t *)generic_ptr +
			cmd_desc->offset);
		cam_flash_info = (struct cam_flash_init *)cmd_buf;

		switch (cam_flash_info->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_INFO:
			fctrl->flash_type = cam_flash_info->flash_type;
			fctrl->is_regulator_enabled = false;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_INFO;
			fctrl->flash_state =
				CAM_FLASH_STATE_CONFIG;
			break;
		case CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE:
			CAM_DBG(CAM_FLASH, "Widget Flash Operation");
				flash_operation_info =
					(struct cam_flash_set_on_off *) cmd_buf;
				fctrl->nrt_info.cmn_attr.count =
					flash_operation_info->count;
				fctrl->nrt_info.cmn_attr.request_id = 0;
				fctrl->nrt_info.opcode =
					flash_operation_info->opcode;
				fctrl->nrt_info.cmn_attr.cmd_type =
					CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE;
				for (i = 0;
					i < flash_operation_info->count; i++)
					fctrl->nrt_info.led_current_ma[i] =
					flash_operation_info->led_current_ma[i];

				mutex_lock(&fctrl->flash_wq_mutex);
				rc = cam_flash_apply_setting(fctrl, 0);
				if (rc)
					CAM_ERR(CAM_FLASH,
						"Apply setting failed: %d",
						rc);
				mutex_unlock(&fctrl->flash_wq_mutex);
				fctrl->flash_state =
					CAM_FLASH_STATE_CONFIG;
			break;
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type = %d",
				cam_flash_info->cmd_type);
			return -EINVAL;
		}
		break;
	}
	case CAM_FLASH_PACKET_OPCODE_SET_OPS: {
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		frm_offset = csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY;
		flash_data = &fctrl->per_frame[frm_offset];

		if (flash_data->cmn_attr.is_settings_valid == true) {
			flash_data->cmn_attr.request_id = 0;
			flash_data->cmn_attr.is_settings_valid = false;
			for (i = 0; i < flash_data->cmn_attr.count; i++)
				flash_data->led_current_ma[i] = 0;
		}

		flash_data->cmn_attr.request_id = csl_packet->header.request_id;
		flash_data->cmn_attr.is_settings_valid = true;
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buffer);
		cmd_buf = (uint32_t *)((uint8_t *)generic_ptr +
			cmd_desc->offset);

		if (!cmd_buf)
			return -EINVAL;

		cmn_hdr = (struct common_header *)cmd_buf;

		switch (cmn_hdr->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_FIRE: {
			CAM_DBG(CAM_FLASH,
				"CAMERA_FLASH_CMD_TYPE_OPS case called");
			if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
				(fctrl->flash_state ==
					CAM_FLASH_STATE_ACQUIRE)) {
				CAM_WARN(CAM_FLASH,
					"Rxed Flash fire ops without linking");
				flash_data->cmn_attr.is_settings_valid = false;
				return 0;
			}

			flash_operation_info =
				(struct cam_flash_set_on_off *) cmd_buf;
			if (!flash_operation_info) {
				CAM_ERR(CAM_FLASH,
					"flash_operation_info Null");
				return -EINVAL;
			}

			flash_data->opcode = flash_operation_info->opcode;
			flash_data->cmn_attr.count =
				flash_operation_info->count;
			for (i = 0; i < flash_operation_info->count; i++)
				flash_data->led_current_ma[i]
				= flash_operation_info->led_current_ma[i];
			}
			break;
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type = %d",
				cmn_hdr->cmd_type);
			return -EINVAL;
		}
		break;
	}
	case CAM_FLASH_PACKET_OPCODE_NON_REALTIME_SET_OPS: {
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		fctrl->nrt_info.cmn_attr.is_settings_valid = true;
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buffer);
		cmd_buf = (uint32_t *)((uint8_t *)generic_ptr +
			cmd_desc->offset);
		cmn_hdr = (struct common_header *)cmd_buf;

		switch (cmn_hdr->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET: {
			CAM_DBG(CAM_FLASH, "Widget Flash Operation");
			flash_operation_info =
				(struct cam_flash_set_on_off *) cmd_buf;
			fctrl->nrt_info.cmn_attr.count =
				flash_operation_info->count;
			fctrl->nrt_info.cmn_attr.request_id = 0;
			fctrl->nrt_info.opcode =
				flash_operation_info->opcode;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET;

			for (i = 0; i < flash_operation_info->count; i++)
				fctrl->nrt_info.led_current_ma[i] =
					flash_operation_info->led_current_ma[i];

			mutex_lock(&fctrl->flash_wq_mutex);
			rc = cam_flash_apply_setting(fctrl, 0);
			if (rc)
				CAM_ERR(CAM_FLASH, "Apply setting failed: %d",
					rc);
			mutex_unlock(&fctrl->flash_wq_mutex);
			return rc;
		}
		case CAMERA_SENSOR_FLASH_CMD_TYPE_QUERYCURR: {
			int query_curr_ma = 0;

			flash_query_info =
				(struct cam_flash_query_curr *)cmd_buf;

			rc = qpnp_flash_led_prepare(fctrl->switch_trigger,
				QUERY_MAX_CURRENT, &query_curr_ma);
			CAM_DBG(CAM_FLASH, "query_curr_ma = %d",
				query_curr_ma);
			if (rc) {
				CAM_ERR(CAM_FLASH,
				"Query current failed with rc=%d", rc);
				return rc;
			}
			flash_query_info->query_current_ma = query_curr_ma;
			break;
		}
		case CAMERA_SENSOR_FLASH_CMD_TYPE_RER: {
			rc = 0;
			flash_rer_info = (struct cam_flash_set_rer *)cmd_buf;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_RER;
			fctrl->nrt_info.opcode = flash_rer_info->opcode;
			fctrl->nrt_info.cmn_attr.count = flash_rer_info->count;
			fctrl->nrt_info.cmn_attr.request_id = 0;
			fctrl->nrt_info.num_iterations =
				flash_rer_info->num_iteration;
			fctrl->nrt_info.led_on_delay_ms =
				flash_rer_info->led_on_delay_ms;
			fctrl->nrt_info.led_off_delay_ms =
				flash_rer_info->led_off_delay_ms;

			for (i = 0; i < flash_rer_info->count; i++)
				fctrl->nrt_info.led_current_ma[i] =
					flash_rer_info->led_current_ma[i];


			mutex_lock(&fctrl->flash_wq_mutex);
			rc = cam_flash_apply_setting(fctrl, 0);
			if (rc)
				CAM_ERR(CAM_FLASH, "apply_setting failed: %d",
					rc);
			mutex_unlock(&fctrl->flash_wq_mutex);
			return rc;
		}
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type : %d",
				cmn_hdr->cmd_type);
			return -EINVAL;
		}

		break;
	}
	case CAM_PKT_NOP_OPCODE: {
		frm_offset = csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY;
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_ACQUIRE)) {
			CAM_WARN(CAM_FLASH,
				"Rxed NOP packets without linking");
			fctrl->per_frame[frm_offset].cmn_attr.is_settings_valid
				= false;
			return 0;
		}

		fctrl->per_frame[frm_offset].cmn_attr.is_settings_valid = false;
		fctrl->per_frame[frm_offset].cmn_attr.request_id = 0;
		fctrl->per_frame[frm_offset].opcode = CAM_PKT_NOP_OPCODE;
		CAM_DBG(CAM_FLASH, "NOP Packet is Received: req_id: %u",
			csl_packet->header.request_id);
		goto update_req_mgr;
	}
	default:
		CAM_ERR(CAM_FLASH, "Wrong Opcode : %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		return -EINVAL;
	}
update_req_mgr:
	if (((csl_packet->header.op_code  & 0xFFFFF) ==
		CAM_PKT_NOP_OPCODE) ||
		((csl_packet->header.op_code & 0xFFFFF) ==
		CAM_FLASH_PACKET_OPCODE_SET_OPS)) {
		add_req.link_hdl = fctrl->bridge_intf.link_hdl;
		add_req.req_id = csl_packet->header.request_id;
		add_req.dev_hdl = fctrl->bridge_intf.device_hdl;

		if ((csl_packet->header.op_code & 0xFFFFF) ==
			CAM_FLASH_PACKET_OPCODE_SET_OPS)
			add_req.skip_before_applying = 1;
		else
			add_req.skip_before_applying = 0;

		if (fctrl->bridge_intf.crm_cb &&
			fctrl->bridge_intf.crm_cb->add_req)
			fctrl->bridge_intf.crm_cb->add_req(&add_req);
		CAM_DBG(CAM_FLASH, "add req to req_mgr= %lld", add_req.req_id);
	}

	return rc;
}

int cam_flash_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	info->dev_id = CAM_REQ_MGR_DEVICE_FLASH;
	strlcpy(info->name, CAM_FLASH_NAME, sizeof(info->name));
	info->p_delay = CAM_FLASH_PIPELINE_DELAY;
	info->trigger = CAM_TRIGGER_POINT_SOF;
	return 0;
}

int cam_flash_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_flash_ctrl *fctrl = NULL;

	if (!link)
		return -EINVAL;

	fctrl = (struct cam_flash_ctrl *)cam_get_device_priv(link->dev_hdl);
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, " Device data is NULL");
		return -EINVAL;
	}

	if (link->link_enable) {
		fctrl->bridge_intf.link_hdl = link->link_hdl;
		fctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		fctrl->bridge_intf.link_hdl = -1;
		fctrl->bridge_intf.crm_cb = NULL;
	}

	return 0;
}


int cam_flash_stop_dev(struct cam_flash_ctrl *fctrl)
{
	int rc = 0, i, j;

	cam_flash_off(fctrl);

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		fctrl->per_frame[i].cmn_attr.request_id = 0;
		fctrl->per_frame[i].cmn_attr.is_settings_valid = false;
		fctrl->per_frame[i].cmn_attr.count = 0;
		for (j = 0; j < CAM_FLASH_MAX_LED_TRIGGERS; j++)
			fctrl->per_frame[i].led_current_ma[j] = 0;
	}

	rc = cam_flash_flush_nrt(fctrl);
	if (rc) {
		CAM_ERR(CAM_FLASH,
			"NonRealTime Dev flush failed rc: %d", rc);
		return rc;
	}

	if ((fctrl->flash_state == CAM_FLASH_STATE_START) &&
		(fctrl->is_regulator_enabled == true)) {
		rc = cam_flash_prepare(fctrl, false);
		if (rc)
			CAM_ERR(CAM_FLASH, "Disable Regulator Failed rc: %d",
				rc);
	}

	return rc;
}

int cam_flash_release_dev(struct cam_flash_ctrl *fctrl)
{
	int rc = 0;

	if (fctrl->bridge_intf.device_hdl != 1) {
		rc = cam_destroy_device_hdl(fctrl->bridge_intf.device_hdl);
		if (rc)
			CAM_ERR(CAM_FLASH,
				"Failed in destroying device handle rc = %d",
				rc);
		fctrl->bridge_intf.device_hdl = -1;
		fctrl->bridge_intf.link_hdl = -1;
		fctrl->bridge_intf.session_hdl = -1;
	}

	return rc;
}

void cam_flash_shutdown(struct cam_flash_ctrl *fctrl)
{
	int rc;

	if (fctrl->flash_state == CAM_FLASH_STATE_INIT)
		return;

	if ((fctrl->flash_state == CAM_FLASH_STATE_CONFIG) ||
		(fctrl->flash_state == CAM_FLASH_STATE_START)) {
		rc = cam_flash_stop_dev(fctrl);
		if (rc)
			CAM_ERR(CAM_FLASH, "Stop Failed rc: %d", rc);
	}

	rc = cam_flash_release_dev(fctrl);
	if (rc)
		CAM_ERR(CAM_FLASH, "Release failed rc: %d", rc);

	fctrl->flash_state = CAM_FLASH_STATE_INIT;
}

int cam_flash_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_flash_ctrl *fctrl = NULL;

	if (!apply)
		return -EINVAL;

	fctrl = (struct cam_flash_ctrl *) cam_get_device_priv(apply->dev_hdl);
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&fctrl->flash_wq_mutex);
	rc = cam_flash_apply_setting(fctrl, apply->request_id);
	if (rc)
		CAM_ERR(CAM_FLASH, "apply_setting failed with rc=%d",
			rc);
	mutex_unlock(&fctrl->flash_wq_mutex);

	return rc;
}
