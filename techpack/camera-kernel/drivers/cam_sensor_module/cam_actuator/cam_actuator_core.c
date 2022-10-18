// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_sensor_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "cam_actuator_parklens_thread.h" //xiaomi add

#define MAX_RETRY_TIMES 3  //xiaomi add

int32_t cam_actuator_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 1;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 2;

	power_info->power_down_setting_size = 1;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

static int32_t cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_hw_soc_info  *soc_info =
		&a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	struct timespec64 ts1, ts2; // xiaomi add
	long microsec = 0; // xiaomi add

	/* xiaomi add begin */
	CAM_GET_TIMESTAMP(ts1);
	CAM_DBG(MI_DEBUG, "%s start power_up", a_ctrl->device_name);
	/* xiaomi add end */
	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_ACTUATOR,
			"Using default power settings");
		rc = cam_actuator_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Construct default actuator power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed in actuator power up rc %d", rc);
		return rc;
	}

	rc = camera_io_init(&a_ctrl->io_master_info);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "cci init failed: rc: %d", rc);
		goto cci_failure;
	}
	/* xiaomi add begin */
	CAM_GET_TIMESTAMP(ts2);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts1, ts2, microsec);
	CAM_DBG(MI_DEBUG, "%s end power_up, occupy time is: %ld ms",
		a_ctrl->device_name, microsec/1000);
	/* xiaomi add end */

	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_ERR(CAM_ACTUATOR, "Power down failure");

	return rc;
}

static int32_t cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;
	struct timespec64 ts1, ts2; // xiaomi add
	long microsec = 0; // xiaomi add

	/* xiaomi add begin */
	CAM_GET_TIMESTAMP(ts1);
	CAM_DBG(MI_DEBUG, "%s start power_down", a_ctrl->device_name);
	/* xiaomi add end */
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &a_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_ACTUATOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&a_ctrl->io_master_info);
	/* xiaomi add begin */
	CAM_GET_TIMESTAMP(ts2);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts1, ts2, microsec);
	CAM_DBG(MI_DEBUG, "%s end power_down, occupy time is: %ld ms",
		a_ctrl->device_name, microsec/1000);
	/* xiaomi add end */

	return rc;
}

static int32_t cam_actuator_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_SEQ);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
			}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_BURST);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_actuator_slaveInfo_pkt_parser(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t *cmd_buf, size_t len)
{
	int32_t rc = 0;
	struct cam_cmd_i2c_info *i2c_info;

	if (!a_ctrl || !cmd_buf || (len < sizeof(struct cam_cmd_i2c_info))) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	if (a_ctrl->io_master_info.master_type == CCI_MASTER) {
		a_ctrl->io_master_info.cci_client->cci_i2c_master =
			a_ctrl->cci_i2c_master;
		a_ctrl->io_master_info.cci_client->i2c_freq_mode =
			i2c_info->i2c_freq_mode;
		a_ctrl->io_master_info.cci_client->sid =
			i2c_info->slave_addr >> 1;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x Freq Mode: %d",
			i2c_info->slave_addr, i2c_info->i2c_freq_mode);
	} else if (a_ctrl->io_master_info.master_type == I2C_MASTER) {
		a_ctrl->io_master_info.client->addr = i2c_info->slave_addr;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x", i2c_info->slave_addr);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Invalid Master type: %d",
			a_ctrl->io_master_info.master_type);
		 rc = -EINVAL;
	}

	return rc;
}

int32_t cam_actuator_apply_settings(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;
	int32_t j = 0, i = 0; // xiaomi add

	if (a_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_ACTUATOR, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		/* xiaomi add I2C trace begin */
		switch (i2c_list->op_code) {
		case CAM_SENSOR_I2C_WRITE_RANDOM:
		case CAM_SENSOR_I2C_WRITE_BURST:
		case CAM_SENSOR_I2C_WRITE_SEQ: {
			for (j = 0;j < i2c_list->i2c_settings.size;j++) {
				trace_cam_i2c_write_log_event("[ACTUATORSETTINGS]", a_ctrl->device_name,
					i2c_set->request_id, j, "WRITE", i2c_list->i2c_settings.reg_setting[j].reg_addr,
					i2c_list->i2c_settings.reg_setting[j].reg_data);
			}
			break;
		}
		case CAM_SENSOR_I2C_READ_RANDOM:
		case CAM_SENSOR_I2C_READ_SEQ: {
			for (j = 0;j < i2c_list->i2c_settings.size;j++) {
				trace_cam_i2c_write_log_event("[ACTUATORSETTINGS]", a_ctrl->device_name,
					i2c_set->request_id, j, "READ", i2c_list->i2c_settings.reg_setting[j].reg_addr,
					i2c_list->i2c_settings.reg_setting[j].reg_data);
			}
			break;
		}
		default:
			break;
		} /* xiaomi add I2C trace end */
		rc = cam_actuator_i2c_modes_util(
			&(a_ctrl->io_master_info),
			i2c_list);
		if (rc < 0) {
			CAM_WARN(CAM_ACTUATOR,
				"Failed to apply settings: %d",
				rc);
			/* xiaomi add to ignore the apply setting fail - begin */
			for (i = 0; i < MAX_RETRY_TIMES; i++) {
				usleep_range(1000, 1010);
				rc = cam_actuator_i2c_modes_util(
					&(a_ctrl->io_master_info),
					i2c_list);
				if(rc < 0){
					CAM_WARN(CAM_ACTUATOR,
					"Failed to apply settings: %d times:%d",rc,i);
				}else{
					break;
				}
			}

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Failed to re-apply settings: %d, skip",
					rc);
				rc = 0;
				break;
			}
			/* xiaomi add to ignore the apply setting fail - end */
		} else {
			CAM_DBG(CAM_ACTUATOR,
				"Success:request ID: %d",
				i2c_set->request_id);
		}
	}

	return rc;
}

int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0, request_id, del_req_id;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Input Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	request_id = apply->request_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", a_ctrl->soc_info.index, apply->request_id, apply->link_hdl);

	CAM_DBG(CAM_ACTUATOR, "Request Id: %lld", apply->request_id);
	mutex_lock(&(a_ctrl->actuator_mutex));
	if ((apply->request_id ==
		a_ctrl->i2c_data.per_frame[request_id].request_id) &&
		(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
		== 1) {
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.per_frame[request_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in applying the request: %lld\n",
				apply->request_id);
			goto release_mutex;
		}
	}
	del_req_id = (request_id +
		MAX_PER_FRAME_ARRAY - MAX_SYSTEM_PIPELINE_DELAY) %
		MAX_PER_FRAME_ARRAY;

	if (apply->request_id >
		a_ctrl->i2c_data.per_frame[del_req_id].request_id) {
		a_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
		rc = delete_request(&a_ctrl->i2c_data.per_frame[del_req_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Fail deleting the req: %d err: %d\n",
				del_req_id, rc);
			goto release_mutex;
		}
	} else {
		CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!link) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (link->link_enable) {
		a_ctrl->bridge_intf.link_hdl = link->link_hdl;
		a_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return 0;
}

static int cam_actuator_update_req_mgr(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_packet *csl_packet)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req;

	memset(&add_req, 0, sizeof(add_req));
	add_req.link_hdl = a_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	add_req.dev_hdl = a_ctrl->bridge_intf.device_hdl;

	if (a_ctrl->bridge_intf.crm_cb &&
		a_ctrl->bridge_intf.crm_cb->add_req) {
		rc = a_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Adding request: %llu failed: rc: %d",
				csl_packet->header.request_id, rc);
			return rc;
		}
		CAM_DBG(CAM_ACTUATOR, "Request Id: %lld added to CRM",
			add_req.req_id);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Can't add Request ID: %lld to CRM",
			csl_packet->header.request_id);
		rc = -EINVAL;
	}

	return rc;
}

int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info)
{
#if IS_ENABLED(CONFIG_ISPV3)
	struct cam_actuator_ctrl_t *a_ctrl;
#endif

	if (!info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_ISPV3)
	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(info->dev_hdl);
#endif

	info->dev_id = CAM_REQ_MGR_DEVICE_ACTUATOR;
	strlcpy(info->name, CAM_ACTUATOR_NAME, sizeof(info->name));
	info->p_delay = 1;
	info->trigger = CAM_TRIGGER_POINT_SOF;
#if IS_ENABLED(CONFIG_ISPV3)
	info->trigger_source = a_ctrl->trigger_source;
	info->latest_frame_id = -1;
#endif

	return 0;
}

int32_t cam_actuator_i2c_pkt_parse(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int32_t  rc = 0;
	int32_t  i = 0;
	uint32_t total_cmd_buf_in_bytes = 0;
	size_t   len_of_buff = 0;
	size_t   remain_len = 0;
	uint32_t *offset = NULL;
	uint32_t *cmd_buf = NULL;
	uintptr_t generic_ptr;
	uintptr_t generic_pkt_ptr;
	struct common_header      *cmm_hdr = NULL;
	struct cam_control        *ioctl_ctrl = NULL;
	struct cam_packet         *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings  *i2c_data = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc   *cmd_desc = NULL;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;
	bool parklens_power_down = true; //xiaomi add
	int32_t parklens_state = 0;      //xiaomi add

	if (!a_ctrl || !arg) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(config.packet_handle,
		&generic_pkt_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Error in converting command Handle %d",
			rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_ACTUATOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)
			(generic_pkt_ptr + (uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_ACTUATOR, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ACTUATOR, "Pkt opcode: %d",	csl_packet->header.op_code);

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_ACTUATOR_PACKET_OPCODE_INIT &&
		csl_packet->header.request_id <= a_ctrl->last_flush_req
		&& a_ctrl->last_flush_req != 0) {
		CAM_DBG(CAM_ACTUATOR,
			"reject request %lld, last request to flush %lld",
			csl_packet->header.request_id, a_ctrl->last_flush_req);
		rc = -EINVAL;
		goto end;
	}

	if (csl_packet->header.request_id > a_ctrl->last_flush_req)
		a_ctrl->last_flush_req = 0;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_ACTUATOR_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		/* Loop through multiple command buffers */
		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;
			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
					&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Failed to get cpu buf");
				goto end;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_ACTUATOR, "invalid cmd buf");
				rc = -EINVAL;
				goto end;
			}
			if ((len_of_buff < sizeof(struct common_header)) ||
				(cmd_desc[i].offset > (len_of_buff -
				sizeof(struct common_header)))) {
				CAM_ERR(CAM_ACTUATOR,
					"Invalid length for sensor cmd");
				rc = -EINVAL;
				goto end;
			}
			remain_len = len_of_buff - cmd_desc[i].offset;
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				CAM_DBG(CAM_ACTUATOR,
					"Received slave info buffer");
				rc = cam_actuator_slaveInfo_pkt_parser(
					a_ctrl, cmd_buf, remain_len);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed to parse slave info: %d", rc);
					goto end;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_ACTUATOR,
					"Received power settings buffer");
                                /* xiaomi add begin */                                
				if (PARKLENS_INVALID !=
					parklens_atomic_read(
                                                &(a_ctrl->parklens_ctrl.parklens_state))) {
						parklens_thread_stop(a_ctrl,
                                                        EXIT_PARKLENS_WITHOUT_POWERDOWN);
						parklens_power_down =
							is_parklens_power_down(a_ctrl);
						deinit_parklens_info(a_ctrl);
						CAM_DBG(MI_PARKLENS,
							"stop parklens thread, powerdown:%d",
							parklens_power_down);
				} else {
					CAM_DBG(MI_PARKLENS,
						"parklens thread is invalid, powerdown:%d",
						parklens_power_down);
                                }

				if(parklens_power_down == true) {
				 	CAM_DBG(MI_PARKLENS,
				 		"need power up again");
					rc = cam_sensor_update_power_settings(
					        cmd_buf,
					        total_cmd_buf_in_bytes,
					        power_info, remain_len);
					if (rc) {
						CAM_ERR(CAM_ACTUATOR,
						        "Failed:parse power settings: %d",
					        	rc);
						goto end;
					}
				} else {
					CAM_DBG(MI_PARKLENS,
						"no need repower up again");
				}
                                /* xiaomi add end */
				break;
			default:
				CAM_DBG(CAM_ACTUATOR,
					"Received initSettings buffer");
				i2c_data = &(a_ctrl->i2c_data);
				i2c_reg_settings =
					&i2c_data->init_settings;

				i2c_reg_settings->request_id = 0;
				i2c_reg_settings->is_settings_valid = 1;
				rc = cam_sensor_i2c_command_parser(
					&a_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1, NULL);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse init settings: %d",
					rc);
					goto end;
				}
				break;
			}
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_ACQUIRE) {
			if (parklens_power_down == true) { //xiaomi add
				rc = cam_actuator_power_up(a_ctrl);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						" Actuator Power up failed");
					goto end;
				}
			}//xiaomi add
			a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
		}

		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.init_settings);

		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Cannot apply Init settings");
			goto end;
		}

		/* Delete the request even if the apply is failed */
		rc = delete_request(&a_ctrl->i2c_data.init_settings);
		if (rc < 0) {
			CAM_WARN(CAM_ACTUATOR,
				"Fail in deleting the Init settings");
			rc = 0;
		}
		break;
	case CAM_ACTUATOR_PACKET_AUTO_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			goto end;
		}
		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_NOW;

		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->init_settings;

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(
			&a_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Auto move lens parsing failed: %d", rc);
			goto end;
		}
		rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in adding request to request manager");
			goto end;
		}
		break;
	case CAM_ACTUATOR_PACKET_MANUAL_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			goto end;
		}

		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_LATER;
		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->per_frame[
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY];

		 i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(
			&a_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Manual move lens parsing failed: %d", rc);
			goto end;
		}

		rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in adding request to request manager");
			goto end;
		}
		break;
	case CAM_PKT_NOP_OPCODE:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			CAM_WARN(CAM_ACTUATOR,
				"Received NOP packets in invalid state: %d",
				a_ctrl->cam_act_state);
			rc = -EINVAL;
			goto end;
		}
		rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in adding request to request manager");
			goto end;
		}
		break;
	case CAM_ACTUATOR_PACKET_OPCODE_READ: {
		struct cam_buf_io_cfg *io_cfg;
		struct i2c_settings_array i2c_read_settings;

		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to read actuator: %d",
				a_ctrl->cam_act_state);
			goto end;
		}
		CAM_DBG(CAM_ACTUATOR, "number of I/O configs: %d:",
			csl_packet->num_io_configs);
		if (csl_packet->num_io_configs == 0) {
			CAM_ERR(CAM_ACTUATOR, "No I/O configs to process");
			rc = -EINVAL;
			goto end;
		}

		INIT_LIST_HEAD(&(i2c_read_settings.list_head));

		io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload +
			csl_packet->io_configs_offset);

		if (io_cfg == NULL) {
			CAM_ERR(CAM_ACTUATOR, "I/O config is invalid(NULL)");
			rc = -EINVAL;
			goto end;
		}

		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		i2c_read_settings.is_settings_valid = 1;
		i2c_read_settings.request_id = 0;
		rc = cam_sensor_i2c_command_parser(&a_ctrl->io_master_info,
			&i2c_read_settings,
			cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"actuator read pkt parsing failed: %d", rc);
			goto end;
		}

		rc = cam_sensor_i2c_read_data(
			&i2c_read_settings,
			&a_ctrl->io_master_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "cannot read data, rc:%d", rc);
			delete_request(&i2c_read_settings);
			goto end;
		}

		rc = delete_request(&i2c_read_settings);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in deleting the read settings");
			goto end;
		}
		break;
		}
        /* xiaomi add begin */
	case CAM_ACTUATOR_PACKET_OPCODE_PARKLENS: {
		CAM_INFO(MI_PARKLENS,
			"Received parklens buffer");

		parklens_state = parklens_atomic_read(
                                        &(a_ctrl->parklens_ctrl.parklens_state));
		if ((parklens_state != PARKLENS_INVALID) ||
		    (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG)) {
			rc = -EINVAL;
			CAM_WARN(MI_PARKLENS,
				"Not in right state to do parklens: %d/%d",
				a_ctrl->cam_act_state,
				parklens_state);
			goto end;
		}

		i2c_reg_settings = &(a_ctrl->i2c_data.parklens_settings);
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		rc = cam_sensor_i2c_command_parser(
			&a_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Not in right state to do parklens: %d/%d",
				rc);
			delete_request(i2c_reg_settings);
			goto end;
		}

		parklens_thread_trigger(a_ctrl);

		break;
	}
        /* xiaomi add end */
	default:
		CAM_ERR(CAM_ACTUATOR, "Wrong Opcode: %d",
			csl_packet->header.op_code & 0xFFFFFF);
		rc = -EINVAL;
		goto end;
	}

end:
	return rc;
}

void cam_actuator_shutdown(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_actuator_soc_private  *soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = 
                &soc_private->power_info;

        /* xiaomi add begin */
	if (PARKLENS_INVALID !=
		parklens_atomic_read(&(a_ctrl->parklens_ctrl.parklens_state))) {
		parklens_thread_stop(a_ctrl, EXIT_PARKLENS_WITH_POWERDOWN);
		CAM_DBG(MI_PARKLENS,
			"exit parklens with powerdown in shutdown");
	} else {
		CAM_DBG(MI_PARKLENS,
			"parklens is invalid in shutdown");
        }
        /* xiaomi add end */

	if (a_ctrl->cam_act_state == CAM_ACTUATOR_INIT)
		return;

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG) {
        /* xiaomi add begin */
		if(false == is_parklens_power_down(a_ctrl)) {
			rc = cam_actuator_power_down(a_ctrl);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR, "Actuator Power down failed");
			CAM_DBG(MI_PARKLENS,
				"parklens isn't powerdown");
		} else {
			CAM_DBG(MI_PARKLENS,
				"parklens has been powerdown");
                }
		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	} else {
		CAM_DBG(MI_PARKLENS,
			"shut down but not in CONFIG, parklens powerdown: %d",
			is_parklens_power_down(a_ctrl));
	}
        /* xiaomi add end */
		
	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_ACQUIRE) {
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying  dhdl failed");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
	}

        /* xiaomi add begin */
	if (PARKLENS_INVALID !=
		parklens_atomic_read(
			&(a_ctrl->parklens_ctrl.parklens_state))) {
		deinit_parklens_info(a_ctrl);
		CAM_DBG(MI_PARKLENS,
                        "parklens is not valid, deinit parklens info");
	}
        /* xiaomi add end */

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;
	a_ctrl->last_flush_req = 0;

	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
}

int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int rc = 0;
	int32_t parklens_power_down = 0; //xiaomi add
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

	if (!a_ctrl || !cmd) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_ACTUATOR, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ACTUATOR, "Opcode to Actuator: %d", cmd->op_code);

	mutex_lock(&(a_ctrl->actuator_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev actuator_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (a_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&actuator_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(actuator_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copying from user\n");
			goto release_mutex;
		}

		bridge_params.session_hdl = actuator_acq_dev.session_handle;
		bridge_params.ops = &a_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = a_ctrl;
		bridge_params.dev_id = CAM_ACTUATOR;

#if IS_ENABLED(CONFIG_ISPV3)
		if (actuator_acq_dev.reserved)
			a_ctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_EXTERNAL;
		else
			a_ctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_INTERNAL;
#endif


		actuator_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		if (actuator_acq_dev.device_handle <= 0) {
			rc = -EFAULT;
			CAM_ERR(CAM_ACTUATOR, "Can not create device handle");
			goto release_mutex;
		}
		a_ctrl->bridge_intf.device_hdl = actuator_acq_dev.device_handle;
		a_ctrl->bridge_intf.session_hdl =
			actuator_acq_dev.session_handle;

#if IS_ENABLED(CONFIG_ISPV3)
		CAM_DBG(CAM_ACTUATOR, "Device Handle: %d trigger_source: %s",
			actuator_acq_dev.device_handle,
			(a_ctrl->trigger_source == CAM_REQ_MGR_TRIG_SRC_INTERNAL) ?
			"internal" : "external");
#else
		CAM_DBG(CAM_ACTUATOR, "Device Handle: %d",
			actuator_acq_dev.device_handle);
#endif

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}
		break;
	case CAM_RELEASE_DEV: {
		if (a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Cant release actuator: in start state");
			goto release_mutex;
		}

		if (a_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_ACTUATOR, "link hdl: %d device hdl: %d",
				a_ctrl->bridge_intf.device_hdl,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}

		parklens_power_down = is_parklens_power_down(a_ctrl); //xiaomi add
		if (a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
			if (parklens_power_down == false) { //xiaomi add
				rc = cam_actuator_power_down(a_ctrl);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Actuator Power Down Failed");
					goto release_mutex;
				}
			} //xiaomi add
		}

		if (a_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR,
				"Device [%d] still active on link 0x%x",
				a_ctrl->cam_act_state,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying the device hdl");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
		a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
		a_ctrl->last_flush_req = 0;

		if (parklens_power_down == false) { //xiaomi add
			kfree(power_info->power_setting);
			kfree(power_info->power_down_setting);
			power_info->power_setting = NULL;
			power_info->power_down_setting = NULL;
			power_info->power_down_setting_size = 0;
			power_info->power_setting_size = 0;
		} //xiaomi add

	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_actuator_query_cap actuator_cap = {0};

		actuator_cap.slot_info = a_ctrl->soc_info.index;
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_cap,
			sizeof(struct cam_actuator_query_cap))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_START_DEV: {
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to start : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}
		a_ctrl->cam_act_state = CAM_ACTUATOR_START;
		a_ctrl->last_flush_req = 0;
	}
		break;
	case CAM_STOP_DEV: {
		struct i2c_settings_array *i2c_set = NULL;
		int i;

		if (a_ctrl->cam_act_state != CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to stop : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

			if (i2c_set->is_settings_valid == 1) {
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
		a_ctrl->last_flush_req = 0;
		a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
	}
		break;
	case CAM_CONFIG_DEV: {
		a_ctrl->setting_apply_state =
			ACT_APPLY_SETTINGS_LATER;
		rc = cam_actuator_i2c_pkt_parse(a_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed in actuator Parsing");
			goto release_mutex;
		}

		if (a_ctrl->setting_apply_state ==
			ACT_APPLY_SETTINGS_NOW) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if ((rc == -EAGAIN) &&
			(a_ctrl->io_master_info.master_type == CCI_MASTER)) {
				CAM_WARN(CAM_ACTUATOR,
					"CCI HW is in resetting mode:: Reapplying Init settings");
				usleep_range(1000, 1010);
				rc = cam_actuator_apply_settings(a_ctrl,
					&a_ctrl->i2c_data.init_settings);
			}

			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"Failed to apply Init settings: rc = %d",
					rc);
			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in Deleting the Init Pkt: %d",
					rc);
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid Opcode %d", cmd->op_code);
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}

int32_t cam_actuator_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	if (a_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_ACTUATOR, "i2c frame data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		a_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_ACTUATOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_ACTUATOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

/* xiaomi add begin */
static int32_t parklens_thread_func(void *arg)
{
        struct cam_actuator_parklens_ctrl_t *parklens_ctrl = NULL;
	struct cam_actuator_soc_private  *soc_private = NULL;
	struct cam_sensor_power_ctrl_t *power_info = NULL;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct i2c_settings_array *i2c_set  = NULL;
	struct i2c_settings_list *i2c_list = NULL;
	struct i2c_settings_list *i2c_list_last = NULL;
	struct parklens_wake_lock parklens_wakelock;

	unsigned short sleeptime = PARKLENS_SLEEPTIME;
	int parklens_step = 10;
	int wait_result = 0;
	int32_t rc = 0;
	int32_t parklens_opcode = 0;
	uint32_t size = 0;
	int i = 0;

	if (!arg) {
		rc = -EINVAL;
		parklens_atomic_set(&(parklens_ctrl->exit_result),
			PARKLENS_EXIT_CREATE_WAKELOCK_FAILED);
		CAM_ERR(CAM_ACTUATOR, "parklens_thread_func arg is NULL");
	} else {
		a_ctrl = (struct cam_actuator_ctrl_t *)arg;
		parklens_ctrl = &(a_ctrl->parklens_ctrl);
		soc_private =
                        (struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
		i2c_set =
                        (struct i2c_settings_array *)(&(a_ctrl->i2c_data.parklens_settings));
		power_info = &soc_private->power_info;

 		rc = parklens_wake_lock_create(&parklens_wakelock,"parklens_wakeup");
		if (rc < 0) {
			parklens_atomic_set(&(parklens_ctrl->exit_result),
				PARKLENS_EXIT_CREATE_WAKELOCK_FAILED);
			CAM_ERR(MI_PARKLENS, "acquire wakelock at parklens thread failed");
		} else {
			parklens_wake_lock_acquire(&parklens_wakelock);
			CAM_DBG(MI_PARKLENS, "acquire wakelock at parklens thread");
		}
	}

	parklens_event_set(&(parklens_ctrl->start_event));
	if (rc < 0)
		goto exit_without_powerdown;;

	CAM_DBG(CAM_ACTUATOR,"parklens thread start up");

	while (parklens_step > 0) {
		wait_result = parklens_wait_queue_timeout(
                                parklens_ctrl->parklens_wait_queue,\
				(parklens_atomic_read(&(parklens_ctrl->parklens_opcode)) >= \
				EXIT_PARKLENS_WITH_POWERDOWN),\
				msecs_to_jiffies(sleeptime));

		if (wait_result > 0) {
			CAM_DBG(MI_PARKLENS,
				"Parklens time to be delayed %d",
				wait_result);

			parklens_opcode =
				parklens_atomic_read(&(parklens_ctrl->parklens_opcode));
			if (parklens_opcode == EXIT_PARKLENS_WITHOUT_POWERDOWN) {
				CAM_DBG(MI_PARKLENS,
					"exit parklens thread no need power off");
				parklens_atomic_set(
					&(parklens_ctrl->exit_result),
					PARKLENS_EXIT_WITHOUT_POWERDOWN);
				goto exit_without_powerdown;
			} else if (parklens_opcode == EXIT_PARKLENS_WITH_POWERDOWN) {
				CAM_DBG(MI_PARKLENS,
					"exit parklens thread no need power off");
				parklens_atomic_set(
					&(parklens_ctrl->exit_result),
					PARKLENS_EXIT_WITH_POWEDOWN);
				goto exit_with_powerdown;
			} else {
				CAM_ERR(MI_PARKLENS, "Invalid opcode for parklens");
				continue;
			}
		} else {
			CAM_DBG(MI_PARKLENS, "Parklens time out %d", wait_result);
                }
                parklens_step--;

		if (i2c_set == NULL) {
			CAM_ERR(MI_PARKLENS,
				"Invalid parklens settings");
			parklens_atomic_set(
				&(parklens_ctrl->exit_result),
				PARKLENS_EXIT_WITHOUT_PARKLENS);
			goto exit_with_powerdown;
		} else if (i2c_set->is_settings_valid != 1) {
			CAM_ERR(MI_PARKLENS,
				"parklens settings is not valid");
			parklens_atomic_set(
				&(parklens_ctrl->exit_result),
				PARKLENS_EXIT_WITHOUT_PARKLENS);
			goto exit_with_powerdown;
		} else {
			i2c_list_last = i2c_list;

			if (i2c_list == NULL)
				i2c_list = list_first_entry(&(i2c_set->list_head), typeof(*i2c_list), list);
			else
				i2c_list = list_next_entry(i2c_list, list);

			if ((i2c_list_last != NULL) && list_entry_is_head(i2c_list, &(i2c_set->list_head), list)) {
				CAM_DBG(MI_PARKLENS,
					"parklens settings execute done");
				parklens_atomic_set(
					&(parklens_ctrl->exit_result),
					PARKLENS_EXIT_WITH_POWEDOWN);
				goto exit_with_powerdown;
			} else {
				if (i2c_list->i2c_settings.delay > 10 && i2c_list->i2c_settings.delay < 100)
					sleeptime = i2c_list->i2c_settings.delay;
				else
					sleeptime = PARKLENS_SLEEPTIME;
				i2c_list->i2c_settings.delay = 0;

				rc = cam_actuator_i2c_modes_util(
					&(a_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(MI_PARKLENS,
						"parklens step failed, %d", rc);
					parklens_atomic_set(
						&(parklens_ctrl->exit_result),
						PARKLENS_EXIT_CCI_ERROR);
					goto exit_with_powerdown;
				} else
					CAM_DBG(MI_PARKLENS,
						"parklens step %d success",
						parklens_step);

				size = i2c_list->i2c_settings.size;
				for (i = 0; i < size; i++) {
					CAM_DBG(MI_PARKLENS,
						"parklens addr: %lld, data:%lld, delay:%u",
						i2c_list->i2c_settings.reg_setting[i].reg_addr,
						i2c_list->i2c_settings.reg_setting[i].reg_data,
						i2c_list->i2c_settings.delay);
				}
			}
		}

		CAM_DBG(MI_PARKLENS,
			"parklens_thread_func arg is %d/%d",\
			parklens_step,
			parklens_ctrl->parklens_state);
	}

exit_with_powerdown:
	CAM_DBG(MI_PARKLENS,
		"parklens thread exit with power down");
	CAM_DBG(MI_PARKLENS,
		"parklens thread exit step/result %d/%d",
		parklens_step,
		parklens_atomic_read(&(parklens_ctrl->exit_result)));
	rc = cam_actuator_power_down(a_ctrl);
	if (rc < 0) {
		CAM_DBG(MI_PARKLENS,
			"parklens power down failed rc: %d", rc);
		parklens_atomic_set(
			&(parklens_ctrl->exit_result),
			PARKLENS_EXIT_WITH_POWEDOWN_FAILED);
	}

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;

exit_without_powerdown:
	if (parklens_atomic_read(&(parklens_ctrl->exit_result)) <=
		PARKLENS_EXIT_WITHOUT_POWERDOWN) {
		CAM_DBG(MI_PARKLENS,
			"parklens thread exit without power down");
		CAM_DBG(MI_PARKLENS,
			"parklens thread exit step/result %d/%d",
			parklens_step,
			parklens_atomic_read(&(parklens_ctrl->exit_result)));
	}

	rc = delete_request(&(a_ctrl->i2c_data.parklens_settings));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR,
			"delete parklens request failed rc: %d", rc);

	parklens_event_set(&(parklens_ctrl->shutdown_event));
	parklens_wake_lock_release(&parklens_wakelock);
	parklens_wake_lock_destroy(&parklens_wakelock);

	parklens_exit_thread(true);

	return 0;
}

int32_t init_parklens_info(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_actuator_parklens_ctrl_t *parklens_ctrl;

	if(!a_ctrl) {
		CAM_ERR(MI_PARKLENS, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	parklens_ctrl = &(a_ctrl->parklens_ctrl);

	CAM_DBG(MI_PARKLENS, "init parklens: %s", a_ctrl->device_name);

	parklens_atomic_set(&(parklens_ctrl->parklens_opcode),
		ENTER_PARKLENS_WITH_POWERDOWN);
	parklens_atomic_set(&(parklens_ctrl->exit_result),
		PARKLENS_ENTER);
	parklens_atomic_set(&(parklens_ctrl->parklens_state),
		PARKLENS_INVALID);

	rc = parklens_event_create(&(parklens_ctrl->start_event));
	if (rc < 0) {
		CAM_ERR(MI_PARKLENS,
                        "failed to create start event for parklens");
		return rc;
	}

	rc = parklens_event_create(&(parklens_ctrl->shutdown_event));
	if (rc < 0) {
		CAM_ERR(MI_PARKLENS,
                        "failed to create shutdown event for parklens");
		return rc;
	}

	parklens_init_waitqueue_head(&(parklens_ctrl->parklens_wait_queue));
	parklens_ctrl->parklens_thread = NULL;
	parklens_atomic_set(&(parklens_ctrl->parklens_state), PARKLENS_STOP);

	return 0;
}

int32_t deinit_parklens_info(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_actuator_parklens_ctrl_t *parklens_ctrl;

	if(!a_ctrl) {
		CAM_ERR(MI_PARKLENS, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	CAM_DBG(MI_PARKLENS, "deinit parklens: %s", a_ctrl->device_name);

	parklens_ctrl = &(a_ctrl->parklens_ctrl);

	if (PARKLENS_STOP !=
		parklens_atomic_read(&(parklens_ctrl->parklens_state))) {
		CAM_ERR(MI_PARKLENS, "deinit parklens in wrong state");
		return -EINVAL;
	}

	rc = parklens_event_destroy(&(parklens_ctrl->start_event));
	if (rc < 0)
		CAM_ERR(MI_PARKLENS,
		    "failed to destroy start event for parklens");

	rc = parklens_event_destroy(&(parklens_ctrl->shutdown_event));
	if (rc < 0)
		CAM_ERR(MI_PARKLENS,
		    "failed to destroy shutdown event for parklens");

	parklens_atomic_set(&(parklens_ctrl->parklens_opcode),
		ENTER_PARKLENS_WITH_POWERDOWN);
	parklens_atomic_set(&(parklens_ctrl->exit_result),
		PARKLENS_ENTER);
	parklens_atomic_set(&(parklens_ctrl->parklens_state),
		PARKLENS_INVALID);

	return 0;
}

bool is_parklens_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	bool is_power_down = false;
	int32_t parklens_state = 0;
	int32_t parklens_exit_status = 0;
	struct cam_actuator_parklens_ctrl_t *parklens_ctrl;

	if(!a_ctrl) {
		CAM_ERR(MI_PARKLENS, "failed: a_ctrl %pK", a_ctrl);
		return false;
	}

	parklens_ctrl = &(a_ctrl->parklens_ctrl);
	parklens_state = parklens_atomic_read(&(parklens_ctrl->parklens_state));

	switch (parklens_state) {
	case PARKLENS_RUNNING: {
		CAM_DBG(MI_PARKLENS, "parklens is running, no need powerdown");
		is_power_down = true;
	}
		break;

	case PARKLENS_STOP: {
		parklens_exit_status =
                        parklens_atomic_read(&(parklens_ctrl->exit_result));

		if (parklens_exit_status <= PARKLENS_EXIT_WITHOUT_POWERDOWN) {
			CAM_DBG(MI_PARKLENS,
				"parklens stop without power down");
			is_power_down = false;
		} else {
			is_power_down = true;
		}
	}
		break;

	case PARKLENS_INVALID: {
		CAM_DBG(MI_PARKLENS,
			"parklens is not created, power down");
		is_power_down = false;
	}
		break;

	default:
		CAM_ERR(MI_PARKLENS, "Invalid parklens_state %d", parklens_state);
	}

	return is_power_down;
}

int32_t parklens_thread_trigger(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_actuator_parklens_ctrl_t *parklens_ctrl;

	if(!a_ctrl) {
		CAM_ERR(MI_PARKLENS, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;;
	}

	rc = init_parklens_info(a_ctrl);
	if(rc < 0){
		CAM_ERR(MI_PARKLENS, "failed to init parklens info %d", rc);
		return rc;
	}

	parklens_ctrl = &(a_ctrl->parklens_ctrl);

	/* intialize parameters of parklens*/
	parklens_atomic_set(&(parklens_ctrl->parklens_opcode),
		ENTER_PARKLENS_WITH_POWERDOWN);
	parklens_atomic_set(&(parklens_ctrl->exit_result),
		PARKLENS_ENTER);
	parklens_atomic_set(&(parklens_ctrl->parklens_state),
		PARKLENS_STOP);

	parklens_ctrl->parklens_thread =
                parklens_thread_run(parklens_thread_func,
                                    a_ctrl,
                                    "parklens-thread");

	if (!(parklens_ctrl->parklens_thread)) {
		CAM_ERR(MI_PARKLENS, "parklens_thread create failed");
		rc = -ENOMEM;
		goto deinit_parklens;
	} else {
		CAM_DBG(MI_PARKLENS, "trigger parklens thread");
		parklens_wait_single_event(&(parklens_ctrl->start_event),0);

		rc = parklens_atomic_read(&(parklens_ctrl->exit_result));
		if (rc > PARKLENS_ENTER) {
			CAM_DBG(MI_PARKLENS, "parklens thread execute failed %d", rc);
			goto clear_thread;
		} else
		    parklens_atomic_set(&(parklens_ctrl->parklens_state),
			    PARKLENS_RUNNING);
	}

	return rc;

clear_thread:
	parklens_wait_single_event(&(parklens_ctrl->shutdown_event),0);
	rc = parklens_atomic_read(&(parklens_ctrl->exit_result));
	CAM_DBG(MI_PARKLENS, "parklens thread exit: %d", rc);

	parklens_atomic_set(&(parklens_ctrl->parklens_state),PARKLENS_STOP);
	parklens_ctrl->parklens_thread = NULL;
	return rc;

deinit_parklens:
	deinit_parklens_info(a_ctrl);
	return rc;
}

int32_t parklens_thread_stop(
	struct cam_actuator_ctrl_t *a_ctrl,
	enum parklens_opcodes opcode)
{
	int32_t exit_result = 0;
	int32_t parklens_state = 0;
	struct cam_actuator_parklens_ctrl_t *parklens_ctrl;

	if(!a_ctrl) {
		CAM_ERR(MI_PARKLENS, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	parklens_ctrl = &(a_ctrl->parklens_ctrl);

	parklens_state = parklens_atomic_read(&(parklens_ctrl->parklens_state)) ;
	exit_result = parklens_atomic_read(&(parklens_ctrl->exit_result));

	if (parklens_state != PARKLENS_RUNNING) {
		CAM_DBG(MI_PARKLENS,
                        "parklens thread is in other state: %d", parklens_state);
		return -EINVAL;
	} else {
		if(exit_result != PARKLENS_ENTER) {
			CAM_DBG(MI_PARKLENS,
				"parklens thread is already end: %d", exit_result);
		} else {
			if ((opcode == EXIT_PARKLENS_WITH_POWERDOWN) ||
			    (opcode == EXIT_PARKLENS_WITHOUT_POWERDOWN)) {
				parklens_atomic_set(&(parklens_ctrl->parklens_opcode), opcode);
			} else {
				CAM_DBG(MI_PARKLENS,
					"Invalid stop opcode, stop parklens with power down");
				parklens_atomic_set(&(parklens_ctrl->parklens_opcode),
					EXIT_PARKLENS_WITH_POWERDOWN);
			}

			CAM_DBG(MI_PARKLENS, "wait for parklens thread start");
			parklens_wake_up_interruptible(&(parklens_ctrl->parklens_wait_queue));
			CAM_DBG(MI_PARKLENS, "wake up parklens thread to stop it");
		}

		parklens_wait_single_event(&(parklens_ctrl->shutdown_event), 0);

		parklens_atomic_set(&(parklens_ctrl->parklens_state), PARKLENS_STOP);
		parklens_ctrl->parklens_thread = NULL;

		exit_result = parklens_atomic_read(&(parklens_ctrl->exit_result));
		if(exit_result) {
			CAM_INFO(MI_PARKLENS, "parklens thread exit status: %d", exit_result);
		} else
			CAM_ERR(MI_PARKLENS, "parklens thread exit failed ! ! !");
	}

	return exit_result;
}
/* xiaomi add end */
