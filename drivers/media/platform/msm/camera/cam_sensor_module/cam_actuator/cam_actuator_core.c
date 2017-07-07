/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_sensor_util.h"
#include "cam_trace.h"

int32_t cam_actuator_slaveInfo_pkt_parser(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t *cmd_buf)
{
	int32_t rc = 0;
	struct cam_cmd_i2c_info *i2c_info;

	if (!a_ctrl || !cmd_buf) {
		pr_err("%s:%d Invalid Args\n", __func__, __LINE__);
		return -EINVAL;
	}

	i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	a_ctrl->io_master_info.cci_client->i2c_freq_mode =
		i2c_info->i2c_freq_mode;
	a_ctrl->io_master_info.cci_client->sid =
		i2c_info->slave_addr >> 1;
	CDBG("%s:%d Slave addr: 0x%x Freq Mode: %d\n", __func__,
		__LINE__, i2c_info->slave_addr, i2c_info->i2c_freq_mode);

	return rc;
}

int32_t cam_actuator_apply_settings(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;
	uint32_t i, size;

	if (a_ctrl == NULL || i2c_set == NULL) {
		pr_err("%s:%d Invalid Args\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		pr_err("%s: %d :Error: Invalid settings\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		if (i2c_list->op_code ==  CAM_SENSOR_I2C_WRITE_RANDOM) {
			rc = camera_io_dev_write(&(a_ctrl->io_master_info),
				&(i2c_list->i2c_settings));
			if (rc < 0) {
				pr_err("%s: %d :Error: Failed in Applying i2c write settings\n",
					__func__, __LINE__);
				return rc;
			}
		} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
			size = i2c_list->i2c_settings.size;
			for (i = 0; i < size; i++) {
				rc = camera_io_dev_poll(
					&(a_ctrl->io_master_info),
					i2c_list->i2c_settings.
						reg_setting[i].reg_addr,
					i2c_list->i2c_settings.
						reg_setting[i].reg_data,
					i2c_list->i2c_settings.
						reg_setting[i].data_mask,
					i2c_list->i2c_settings.addr_type,
					i2c_list->i2c_settings.data_type,
					i2c_list->i2c_settings.
						reg_setting[i].delay);
				if (rc < 0) {
					pr_err("%s: %d :Error: Failed in Applying i2c poll settings\n",
						__func__, __LINE__);
					return rc;
				}
			}
		}
	}

	return rc;
}

int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0, request_id, del_req_id;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		pr_err("%s:%d :Error: Invalid Input Args\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!a_ctrl) {
		pr_err("%s: %d :Error: Device data is NULL\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	request_id = apply->request_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", apply);

	CDBG("%s:%d Request Id: %lld\n",
		__func__, __LINE__, apply->request_id);

	if ((apply->request_id ==
		a_ctrl->i2c_data.per_frame[request_id].request_id) &&
		(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
		== 1) {
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.per_frame[request_id]);
		if (rc < 0) {
			pr_err("%s:%d Failed in applying the request: %lld\n",
				__func__, __LINE__, apply->request_id);
			return rc;
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
			pr_err("%s: %d :Error: Fail deleting the req: %d err: %d\n",
				__func__, __LINE__, del_req_id, rc);
			return rc;
		}
	} else {
		CDBG("%s:%d No Valid Req to clean Up\n", __func__, __LINE__);
	}

	return rc;
}

int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!link) {
		pr_err("%s:%d Invalid Args\n", __func__, __LINE__);
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!a_ctrl) {
		pr_err("%s:%d :Error: Device data is NULL\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	if (link->link_enable) {
		a_ctrl->bridge_intf.link_hdl = link->link_hdl;
		a_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.crm_cb = NULL;
	}

	return 0;
}

int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	if (!info) {
		pr_err("%s:%d Invalid Args\n", __func__, __LINE__);
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_ACTUATOR;
	strlcpy(info->name, CAM_ACTUATOR_NAME, sizeof(info->name));
	info->p_delay = 0;

	return 0;
}

int32_t cam_actuator_i2c_pkt_parse(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uint64_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings *i2c_data = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	size_t len_of_buff = 0;
	uint32_t *offset = NULL, *cmd_buf;
	struct cam_req_mgr_add_request add_req;

	if (!a_ctrl || !arg) {
		pr_err("%s:%d :Error: Invalid Args\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&config, (void __user *) ioctl_ctrl->handle,
		sizeof(config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(config.packet_handle,
		(uint64_t *)&generic_ptr, &len_of_buff);
	if (rc < 0) {
		pr_err("%s:%d :Error: error in converting command Handle %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	if (config.offset > len_of_buff) {
		pr_err("%s: %d offset is out of bounds: offset: %lld len: %zu\n",
			__func__, __LINE__, config.offset, len_of_buff);
		return -EINVAL;
	}

	csl_packet = (struct cam_packet *)(generic_ptr +
		config.offset);
	CDBG("%s:%d Pkt opcode: %d\n",
		__func__, __LINE__, csl_packet->header.op_code);

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
			CAM_ACTUATOR_PACKET_OPCODE_INIT) {
		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->init_settings;

		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		if (csl_packet->num_cmd_buf != 2) {
			pr_err("%s:: %d :Error: cmd Buffers in Init : %d\n",
				__func__, __LINE__, csl_packet->num_cmd_buf);
			return -EINVAL;
		}

		rc = cam_mem_get_cpu_buf(cmd_desc[0].mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buff);
		if (rc < 0) {
			pr_err("%s:%d Failed to get cpu buf\n",
				__func__, __LINE__);
			return rc;
		}
		cmd_buf = (uint32_t *)generic_ptr;
		cmd_buf += cmd_desc->offset / sizeof(uint32_t);
		rc = cam_actuator_slaveInfo_pkt_parser(a_ctrl, cmd_buf);
		if (rc < 0) {
			pr_err("%s:%d Failed in parsing the pkt\n",
				__func__, __LINE__);
			return rc;
		}
		cmd_buf += (sizeof(struct cam_cmd_i2c_info)/sizeof(uint32_t));
		i2c_data->init_settings.request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		rc = cam_sensor_i2c_pkt_parser(i2c_reg_settings,
			&cmd_desc[1], 1);
		if (rc < 0) {
			pr_err("%s:%d :Error: actuator pkt parsing failed: %d\n",
				__func__, __LINE__, rc);
			return rc;
		}
	} else if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_ACTUATOR_PACKET_AUTO_MOVE_LENS) {
		a_ctrl->act_apply_state =
			ACT_APPLY_SETTINGS_NOW;

		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->init_settings;

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_pkt_parser(i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			pr_err("%s:%d :Error: actuator pkt parsing failed: %d\n",
				__func__, __LINE__, rc);
			return rc;
		}
	} else if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_ACTUATOR_PACKET_MANUAL_MOVE_LENS) {
		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings =
			&i2c_data->per_frame
			[csl_packet->header.request_id % MAX_PER_FRAME_ARRAY];

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_pkt_parser(i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			pr_err("%s:%d :Error: actuator pkt parsing failed: %d\n",
				__func__, __LINE__, rc);
			return rc;
		}
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_ACTUATOR_PACKET_OPCODE_INIT) {
		add_req.link_hdl = a_ctrl->bridge_intf.link_hdl;
		add_req.req_id = csl_packet->header.request_id;
		add_req.dev_hdl = a_ctrl->bridge_intf.device_hdl;
		if (a_ctrl->bridge_intf.crm_cb &&
			a_ctrl->bridge_intf.crm_cb->add_req)
			a_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		CDBG("%s: %d Req Id: %lld added to Bridge\n",
			__func__, __LINE__, add_req.req_id);
	}

	return rc;
}

static int32_t cam_actuator_vreg_control(
	struct cam_actuator_ctrl_t *a_ctrl,
	int config)
{
	int rc = 0, cnt;
	struct cam_hw_soc_info  *soc_info;

	soc_info = &a_ctrl->soc_info;
	cnt = soc_info->num_rgltr;

	if (!cnt)
		return 0;

	if (cnt >= CAM_SOC_MAX_REGULATOR) {
		pr_err("%s:%d Regulators more than supported %d\n",
			__func__, __LINE__, cnt);
		return -EINVAL;
	}

	if (config)
		rc = cam_soc_util_enable_platform_resource(soc_info, false, 0,
			false);
	else
		rc = cam_soc_util_disable_platform_resource(soc_info, false,
			false);

	return rc;
}

static int32_t cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_hw_soc_info  *soc_info =
		&a_ctrl->soc_info;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	rc = cam_actuator_vreg_control(a_ctrl, 1);
	if (rc < 0) {
		pr_err("%s:%d Actuator Reg Failed %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	gpio_num_info = a_ctrl->gpio_num_info;

	if (soc_info->gpio_data &&
		gpio_num_info &&
		gpio_num_info->valid[SENSOR_VAF] == 1) {
		rc = cam_soc_util_request_platform_resource(&a_ctrl->soc_info,
			NULL, NULL);
		rc = cam_soc_util_enable_platform_resource(&a_ctrl->soc_info,
			false, 0, false);
		if (rc < 0) {
			pr_err("%s:%d :Error: Failed in req gpio: %d\n",
				__func__, __LINE__, rc);
			return rc;
		}

		gpio_set_value_cansleep(
			gpio_num_info->gpio_num[SENSOR_VAF],
			1);
	}

	/* VREG needs some delay to power up */
	usleep_range(2000, 2050);

	return rc;
}

static int32_t cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_hw_soc_info *soc_info =
		&a_ctrl->soc_info;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	rc = cam_actuator_vreg_control(a_ctrl, 0);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	gpio_num_info = a_ctrl->gpio_num_info;

	if (soc_info->gpio_data &&
		gpio_num_info &&
		gpio_num_info->valid[SENSOR_VAF] == 1) {

		gpio_set_value_cansleep(
			gpio_num_info->gpio_num[SENSOR_VAF],
			GPIOF_OUT_INIT_LOW);

		rc = cam_soc_util_release_platform_resource(&a_ctrl->soc_info);
		rc |= cam_soc_util_disable_platform_resource(&a_ctrl->soc_info,
					0, 0);
		if (rc < 0)
			pr_err("%s:%d Failed to disable platform resources: %d\n",
				__func__, __LINE__, rc);
	}

	return rc;
}

int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;

	if (!a_ctrl || !cmd) {
		pr_err("%s: %d :Error: Invalid Args\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	pr_debug("%s:%d Opcode to Actuator: %d\n",
		__func__, __LINE__, cmd->op_code);

	mutex_lock(&(a_ctrl->actuator_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev actuator_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (a_ctrl->bridge_intf.device_hdl != -1) {
			pr_err("%s:%d Device is already acquired\n",
				__func__, __LINE__);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&actuator_acq_dev,
			(void __user *) cmd->handle,
			sizeof(actuator_acq_dev));
		if (rc < 0) {
			pr_err("%s:%d :Error: Failed Copying from user\n",
				__func__, __LINE__);
			goto release_mutex;
		}

		bridge_params.session_hdl = actuator_acq_dev.session_handle;
		bridge_params.ops = &a_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = a_ctrl;

		actuator_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		a_ctrl->bridge_intf.device_hdl = actuator_acq_dev.device_handle;
		a_ctrl->bridge_intf.session_hdl =
			actuator_acq_dev.session_handle;

		CDBG("%s:%d Device Handle: %d\n",
			__func__, __LINE__, actuator_acq_dev.device_handle);
		if (copy_to_user((void __user *) cmd->handle, &actuator_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			pr_err("%s:%d :Error: Failed Copy to User\n",
				__func__, __LINE__);
			rc = -EFAULT;
			goto release_mutex;
		}

	}
		break;
	case CAM_RELEASE_DEV: {
		if (a_ctrl->bridge_intf.device_hdl == -1) {
			pr_err("%s:%d :Error: link hdl: %d device hdl: %d\n",
				__func__, __LINE__,
				a_ctrl->bridge_intf.device_hdl,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			pr_err("%s:%d :Error: destroying the device hdl\n",
				__func__, __LINE__);
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_actuator_query_cap actuator_cap;

		actuator_cap.slot_info = a_ctrl->id;
		if (copy_to_user((void __user *) cmd->handle, &actuator_cap,
			sizeof(struct cam_actuator_query_cap))) {
			pr_err("%s:%d :Error: Failed Copy to User\n",
				__func__, __LINE__);
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_START_DEV: {
		rc = cam_actuator_power_up(a_ctrl);
		if (rc < 0) {
			pr_err("%s: %d :Error: Actuator Power up failed\n",
				__func__, __LINE__);
			goto release_mutex;
		}
		rc = camera_io_init(&a_ctrl->io_master_info);
		if (rc < 0) {
			pr_err("%s:%d :Error: cci_init failed\n",
				__func__, __LINE__);
			cam_actuator_power_down(a_ctrl);
		}

		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.init_settings);
		if (rc < 0)
			pr_err("%s: %d :Error: Cannot apply Init settings\n",
				__func__, __LINE__);

		/* Delete the request even if the apply is failed */
		rc = delete_request(&a_ctrl->i2c_data.init_settings);
		if (rc < 0) {
			pr_err("%s:%d Fail in deleting the Init settings\n",
				__func__, __LINE__);
			rc = -EINVAL;
			goto release_mutex;
		}
	}
		break;
	case CAM_STOP_DEV: {
		rc = camera_io_release(&a_ctrl->io_master_info);
		if (rc < 0)
			pr_err("%s:%d :Error: Failed in releasing CCI\n",
				__func__, __LINE__);
		rc = cam_actuator_power_down(a_ctrl);
		if (rc < 0) {
			pr_err("%s:%d :Error: Actuator Power down failed\n",
				__func__, __LINE__);
			goto release_mutex;
		}
	}
		break;
	case CAM_CONFIG_DEV: {
		a_ctrl->act_apply_state =
			ACT_APPLY_SETTINGS_LATER;
		rc = cam_actuator_i2c_pkt_parse(a_ctrl, arg);
		if (rc < 0) {
			pr_err("%s:%d :Error: Failed in actuator Parsing\n",
				__func__, __LINE__);
		}

		if (a_ctrl->act_apply_state ==
			ACT_APPLY_SETTINGS_NOW) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if (rc < 0)
				pr_err("%s:%d :Error: Cannot apply Update settings\n",
					__func__, __LINE__);

			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				pr_err("%s: %d :Error: Failed in Deleting the Init Pkt: %d\n",
					__func__, __LINE__, rc);
				goto release_mutex;
			}
		}
	}
		break;
	case CAM_SD_SHUTDOWN:
		break;
	default:
		pr_err("%s:%d Invalid Opcode %d\n",
			__func__, __LINE__, cmd->op_code);
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}
