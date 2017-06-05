/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include <cam_sensor_util.h>

static int32_t cam_sensor_i2c_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uint64_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	size_t len_of_buff = 0;
	uint32_t *offset = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings *i2c_data = NULL;
	struct cam_req_mgr_add_request add_req;

	ioctl_ctrl = (struct cam_control *)arg;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		pr_err("%s:%d :Error: Invalid Handle Type\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (copy_from_user(&config, (void __user *) ioctl_ctrl->handle,
		sizeof(config)))
		return -EFAULT;

	rc = cam_mem_get_cpu_buf(
		config.packet_handle,
		(uint64_t *)&generic_ptr,
		&len_of_buff);
	if (rc < 0) {
		pr_err("%s:%d :Error: Failed in getting the buffer: %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	csl_packet = (struct cam_packet *)(generic_ptr +
		config.offset);
	if (config.offset > len_of_buff) {
		pr_err("%s: %d offset is out of bounds: off: %lld len: %zu\n",
			__func__, __LINE__, config.offset, len_of_buff);
		return -EINVAL;
	}

	i2c_data = &(s_ctrl->i2c_data);
	CDBG("%s:%d Header OpCode: %d\n",
		__func__, __LINE__, csl_packet->header.op_code);
	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG) {
		i2c_reg_settings = &i2c_data->init_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
	} else if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings =
			&i2c_data->
			per_frame[csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY];
		CDBG("%s:%d Received Packet: %lld\n", __func__, __LINE__,
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY);
		if (i2c_reg_settings->is_settings_valid == 1) {
			pr_err("%s:%d :Error: Already some pkt in offset req : %lld\n",
				__func__, __LINE__,
				csl_packet->header.request_id);
			rc = delete_request(i2c_reg_settings);
			if (rc < 0) {
				pr_err("%s: %d :Error: Failed in Deleting the err: %d\n",
					__func__, __LINE__, rc);
				return rc;
			}
		}

		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
	} else if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_PKT_NOP_OPCODE) {
		goto update_req_mgr;
	} else {
		pr_err("%s:%d Invalid Packet Header\n", __func__, __LINE__);
		return -EINVAL;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_pkt_parser(i2c_reg_settings, cmd_desc, 1);
	if (rc < 0) {
		pr_err("%s:%d :Error: Fail parsing I2C Pkt: %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

update_req_mgr:
	if (((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_PKT_NOP_OPCODE) || (csl_packet->header.op_code ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE)) {
		add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
		add_req.req_id = csl_packet->header.request_id;
		CDBG("%s:%d Rxed Req Id: %lld\n",
			__func__, __LINE__, csl_packet->header.request_id);
		add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
		if (s_ctrl->bridge_intf.crm_cb &&
			s_ctrl->bridge_intf.crm_cb->add_req)
			s_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		CDBG("%s:%d add req to req mgr: %lld\n",
			__func__, __LINE__, add_req.req_id);
	}
	return rc;
}

int32_t cam_sensor_update_i2c_info(struct cam_cmd_i2c_info *i2c_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_cci_client   *cci_client = NULL;

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = s_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			pr_err("failed: cci_client %pK", cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
		cci_client->sid = i2c_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CDBG("%s:%d Master: %d sid: %d freq_mode: %d\n",
			__func__, __LINE__,
			cci_client->cci_i2c_master, i2c_info->slave_addr,
			i2c_info->i2c_freq_mode);
	}

	return rc;
}

int32_t cam_sensor_update_slave_info(struct cam_cmd_probe *probe_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
		probe_info->reg_addr;
	s_ctrl->sensordata->slave_info.sensor_id =
		probe_info->expected_data;
	s_ctrl->sensordata->slave_info.sensor_id_mask =
		probe_info->data_mask;

	s_ctrl->sensor_probe_addr_type =  probe_info->addr_type;
	s_ctrl->sensor_probe_data_type =  probe_info->data_type;
	CDBG("%s:%d Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x\n",
		__func__, __LINE__,
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask);
	return rc;
}

int32_t cam_sensor_update_power_settings(void *cmd_buf,
	int cmd_length, struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, tot_size = 0, last_cmd_type = 0;
	int32_t i = 0, pwr_up = 0, pwr_down = 0;
	void *ptr = cmd_buf, *scr;
	struct cam_cmd_power *pwr_cmd = (struct cam_cmd_power *)cmd_buf;
	struct common_header *cmm_hdr = (struct common_header *)cmd_buf;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;

	if (!pwr_cmd || !cmd_length) {
		pr_err("%s:%d Invalid Args: pwr_cmd %pK, cmd_length: %d\n",
			__func__, __LINE__, pwr_cmd, cmd_length);
		return -EINVAL;
	}

	power_info->power_setting_size = 0;
	power_info->power_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_down_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	while (tot_size < cmd_length) {
		if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_UP) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			power_info->
				power_setting_size +=
				pwr_cmd->count;
			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);

			if (pwr_cmd->count == 0)
				CDBG("%s:%d Un expected Command\n",
					__func__, __LINE__);

			for (i = 0; i < pwr_cmd->count; i++, pwr_up++) {
				power_info->
					power_setting[pwr_up].seq_type =
					pwr_cmd->power_settings[i].
						power_seq_type;
				power_info->
					power_setting[pwr_up].config_val =
					pwr_cmd->power_settings[i].
						config_val_low;
				power_info->power_setting[pwr_up].delay = 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size = tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					pr_err("%s:%d :Error: Command Buffer is wrong\n",
						__func__, __LINE__);
					rc = -EINVAL;
					goto free_power_down_settings;
				}
				CDBG("Seq Type[%d]: %d Config_val: %ldn",
					pwr_up,
					power_info->
						power_setting[pwr_up].seq_type,
					power_info->
						power_setting[pwr_up].
						config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_UP;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_WAIT) {
			struct cam_cmd_unconditional_wait *wait_cmd =
				(struct cam_cmd_unconditional_wait *)ptr;
			if (wait_cmd->op_code ==
				CAMERA_SENSOR_WAIT_OP_SW_UCND) {
				if (last_cmd_type ==
					CAMERA_SENSOR_CMD_TYPE_PWR_UP) {
					if (pwr_up > 0)
						power_info->
							power_setting
							[pwr_up - 1].delay +=
							wait_cmd->delay;
					else
						pr_err("%s:%d Delay is expected only after valid power up setting\n",
							__func__, __LINE__);
				} else if (last_cmd_type ==
					CAMERA_SENSOR_CMD_TYPE_PWR_DOWN) {
					if (pwr_down > 0)
						power_info->
							power_down_setting
							[pwr_down - 1].delay +=
							wait_cmd->delay;
					else
						pr_err("%s:%d Delay is expected only after valid power down setting\n",
							__func__, __LINE__);
				}
			} else
				CDBG("%s:%d Invalid op code: %d\n",
					__func__, __LINE__, wait_cmd->op_code);
			tot_size = tot_size +
				sizeof(struct cam_cmd_unconditional_wait);
			if (tot_size > cmd_length) {
				pr_err("Command Buffer is wrong\n");
				return -EINVAL;
			}
			scr = (void *) (wait_cmd);
			ptr = (void *)
				(scr +
				sizeof(struct cam_cmd_unconditional_wait));
			CDBG("%s:%d ptr: %pK sizeof: %d Next: %pK\n",
				__func__, __LINE__, scr,
				(int32_t)sizeof(
				struct cam_cmd_unconditional_wait), ptr);

			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_DOWN) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);
			power_info->power_down_setting_size += pwr_cmd->count;

			if (pwr_cmd->count == 0)
				pr_err("%s:%d Invalid Command\n",
					__func__, __LINE__);

			for (i = 0; i < pwr_cmd->count; i++, pwr_down++) {
				power_info->
					power_down_setting[pwr_down].
					seq_type =
					pwr_cmd->power_settings[i].
					power_seq_type;
				power_info->
					power_down_setting[pwr_down].
					config_val =
					pwr_cmd->power_settings[i].
					config_val_low;
				power_info->
					power_down_setting[pwr_down].delay = 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size =
						tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					pr_err("Command Buffer is wrong\n");
					rc = -EINVAL;
					goto free_power_down_settings;
				}
				CDBG("%s:%d Seq Type[%d]: %d Config_val: %ldn",
					__func__, __LINE__,
					pwr_down,
					power_info->
						power_down_setting[pwr_down].
						seq_type,
					power_info->
						power_down_setting[pwr_down].
						config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_DOWN;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else {
			pr_err("%s:%d: :Error: Un expected Header Type: %d\n",
				__func__, __LINE__, cmm_hdr->cmd_type);
		}
	}

	return rc;
free_power_down_settings:
	kfree(power_info->power_down_setting);
free_power_settings:
	kfree(power_info->power_setting);
	return rc;
}

int32_t cam_handle_cmd_buffers_for_probe(void *cmd_buf,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t cmd_buf_num, int cmd_buf_length)
{
	int32_t rc = 0;

	switch (cmd_buf_num) {
	case 0: {
		struct cam_cmd_i2c_info *i2c_info = NULL;
		struct cam_cmd_probe *probe_info;

		i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
		rc = cam_sensor_update_i2c_info(i2c_info, s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d Failed in Updating the i2c Info\n",
				__func__, __LINE__);
			return rc;
		}
		probe_info = (struct cam_cmd_probe *)
			(cmd_buf + sizeof(struct cam_cmd_i2c_info));
		rc = cam_sensor_update_slave_info(probe_info, s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d :Error: Updating the slave Info\n",
				__func__, __LINE__);
			return rc;
		}
		cmd_buf = probe_info;
	}
		break;
	case 1: {
		rc = cam_sensor_update_power_settings(cmd_buf,
			cmd_buf_length, s_ctrl);
		if (rc < 0) {
			pr_err("Failed in updating power settings\n");
			return rc;
		}
	}
		break;
	default:
		pr_err("%s:%d Invalid command buffer\n",
			__func__, __LINE__);
		break;
	}
	return rc;
}

int32_t cam_handle_mem_ptr(uint64_t handle, struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	void *packet = NULL, *cmd_buf1 = NULL;
	uint32_t *cmd_buf;
	void *ptr;
	size_t len;
	struct cam_packet *pkt;
	struct cam_cmd_buf_desc *cmd_desc;

	rc = cam_mem_get_cpu_buf(handle,
		(uint64_t *)&packet, &len);
	if (rc < 0) {
		pr_err("%s: %d Failed to get the command Buffer\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	pkt = (struct cam_packet *)packet;
	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		pr_err("%s: %d command descriptor pos is invalid\n",
		__func__, __LINE__);
		return -EINVAL;
	}
	if (pkt->num_cmd_buf != 2) {
		pr_err("%s: %d Expected More Command Buffers : %d\n",
			__func__, __LINE__, pkt->num_cmd_buf);
		return -EINVAL;
	}
	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			(uint64_t *)&cmd_buf1, &len);
		if (rc < 0) {
			pr_err("%s: %d Failed to parse the command Buffer Header\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd_desc[i].length);
		if (rc < 0) {
			pr_err("%s: %d Failed to parse the command Buffer Header\n",
			__func__, __LINE__);
			return -EINVAL;
		}
	}
	return rc;
}

void cam_sensor_query_cap(struct cam_sensor_ctrl_t *s_ctrl,
	struct  cam_sensor_query_cap *query_cap)
{
	query_cap->pos_roll = s_ctrl->sensordata->pos_roll;
	query_cap->pos_pitch = s_ctrl->sensordata->pos_pitch;
	query_cap->pos_yaw = s_ctrl->sensordata->pos_yaw;
	query_cap->secure_camera = 0;
	query_cap->actuator_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_ACTUATOR];
	query_cap->csiphy_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY];
	query_cap->eeprom_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_EEPROM];
	query_cap->flash_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_LED_FLASH];
	query_cap->ois_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_OIS];
	query_cap->slot_info =
		s_ctrl->id;
}

static uint16_t cam_sensor_id_by_mask(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t chipid)
{
	uint16_t sensor_id = (uint16_t)(chipid & 0xFFFF);
	int16_t sensor_id_mask = s_ctrl->sensordata->slave_info.sensor_id_mask;

	if (!sensor_id_mask)
		sensor_id_mask = ~sensor_id_mask;

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;
	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}
	return sensor_id;
}

int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		pr_err("%s:%d failed: %pK\n",
			__func__, __LINE__, slave_info);
		return -EINVAL;
	}

	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD);

	CDBG("%s:%d read id: 0x%x expected id 0x%x:\n",
			__func__, __LINE__, chipid, slave_info->sensor_id);
	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		pr_err("%s: chip id %x does not match %x\n",
				__func__, chipid, slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_setting *pu = NULL;
	struct cam_sensor_power_setting *pd = NULL;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;

	if (!s_ctrl || !arg) {
		pr_err("%s: %d s_ctrl is NULL\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		if (s_ctrl->is_probe_succeed == 1) {
			pr_err("Already Sensor Probed in the slot\n");
			break;
		}
		/* Allocate memory for power up setting */
		pu = kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
		if (!pu) {
			rc = -ENOMEM;
			goto release_mutex;
		}

		pd = kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
		if (!pd) {
			kfree(pu);
			rc = -ENOMEM;
			goto release_mutex;
		}

		power_info->power_setting = pu;
		power_info->power_down_setting = pd;

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, s_ctrl);
			if (rc < 0) {
				pr_err("%s: %d Get Buffer Handle Failed\n",
					__func__, __LINE__);
				kfree(pu);
				kfree(pd);
				goto release_mutex;
			}
		} else {
			pr_err("%s:%d :Error: Invalid Command Type: %d",
				__func__, __LINE__, cmd->handle_type);
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			s_ctrl->sensordata->power_info.cam_vreg,
			s_ctrl->sensordata->power_info.num_vreg,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			pr_err("%s:%d :Error: Fail in filling vreg params for PUP rc %d",
				__func__, __LINE__, rc);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			s_ctrl->sensordata->power_info.cam_vreg,
			s_ctrl->sensordata->power_info.num_vreg,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			pr_err("%s:%d :Error: Fail in filling vreg params for PDOWN rc %d",
				__func__, __LINE__, rc);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			pr_err("power up failed");
			cam_sensor_power_down(s_ctrl);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		/* Match sensor ID */
		rc = cam_sensor_match_id(s_ctrl);
		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		CDBG("%s:%d Probe Succeeded on the slot: %d\n",
			__func__, __LINE__, s_ctrl->id);
		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d :Error: fail in Sensor Power Down\n",
				__func__, __LINE__);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}
		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			pr_err("%s:%d Device is already acquired\n",
				__func__, __LINE__);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&sensor_acq_dev,
			(void __user *) cmd->handle, sizeof(sensor_acq_dev));
		if (rc < 0) {
			pr_err("Failed Copying from user\n");
			goto release_mutex;
		}

		bridge_params.session_hdl = sensor_acq_dev.session_handle;
		bridge_params.ops = &s_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CDBG("%s:%d Device Handle: %d\n", __func__, __LINE__,
			sensor_acq_dev.device_handle);
		if (copy_to_user((void __user *) cmd->handle, &sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			pr_err("Failed Copy to User\n");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_RELEASE_DEV: {
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			pr_err("%s:%d Invalid Handles: link hdl: %d device hdl: %d\n",
				__func__, __LINE__,
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			pr_err("%s:%d Failed in destroying the device hdl\n",
			__func__, __LINE__);
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		cam_sensor_query_cap(s_ctrl, &sensor_cap);
		if (copy_to_user((void __user *) cmd->handle, &sensor_cap,
			sizeof(struct  cam_sensor_query_cap))) {
			pr_err("Failed Copy to User\n");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d :Error: Sensor Power up failed\n",
				__func__, __LINE__);
			goto release_mutex;
		}
		rc = cam_sensor_apply_settings(s_ctrl, 0);
		if (rc < 0) {
			pr_err("cannot apply settings\n");
			goto release_mutex;
		}
		rc = delete_request(&s_ctrl->i2c_data.init_settings);
		if (rc < 0) {
			pr_err("%s:%d Fail in deleting the Init settings\n",
				__func__, __LINE__);
			rc = -EINVAL;
			goto release_mutex;
		}
	}
		break;
	case CAM_STOP_DEV: {
		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d Sensor Power Down failed\n",
				__func__, __LINE__);
			goto release_mutex;
		}
	}
		break;
	case CAM_CONFIG_DEV: {
		rc = cam_sensor_i2c_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			pr_err("%s:%d :Error: Failed CCI Config: %d\n",
				__func__, __LINE__, rc);
			goto release_mutex;
		}
	}
		break;
	default:
		pr_err("%s:%d :Error: Invalid Opcode: %d\n",
			__func__, __LINE__, cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	int rc = 0;

	if (!info)
		return -EINVAL;

	info->dev_id = CAM_REQ_MGR_DEVICE_SENSOR;
	strlcpy(info->name, CAM_SENSOR_NAME, sizeof(info->name));
	info->p_delay = 2;

	return rc;
}

int cam_sensor_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!link)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!s_ctrl) {
		pr_err("%s: Device data is NULL\n", __func__);
		return -EINVAL;
	}
	if (link->link_enable) {
		s_ctrl->bridge_intf.link_hdl = link->link_hdl;
		s_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.crm_cb = NULL;
	}

	return 0;
}

int cam_sensor_power(struct v4l2_subdev *sd, int on)
{
	struct cam_sensor_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if (!on && s_ctrl->sensor_state == CAM_SENSOR_POWER_UP) {
		cam_sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_POWER_DOWN;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_camera_slave_info *slave_info;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!power_info || !slave_info) {
		pr_err("%s:%d failed: %pK %pK\n",
			__func__, __LINE__, power_info,
			slave_info);
		return -EINVAL;
	}

	rc = cam_sensor_core_power_up(power_info);
	if (rc < 0) {
		pr_err("%s:%d power up the core is failed:%d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = camera_io_init(&(s_ctrl->io_master_info));
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			return -EINVAL;
		}
	}

	s_ctrl->sensor_state = CAM_SENSOR_POWER_UP;

	return rc;
}

int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	int rc = 0;

	if (!s_ctrl) {
		pr_err("%s:%d failed: s_ctrl %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;

	if (!power_info) {
		pr_err("%s:%d failed: power_info %pK\n",
			__func__, __LINE__, power_info);
		return -EINVAL;
	}
	rc = msm_camera_power_down(power_info);
	if (rc < 0) {
		pr_err("%s:%d power down the core is failed:%d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	if (s_ctrl->io_master_info.master_type == CCI_MASTER)
		camera_io_release(&(s_ctrl->io_master_info));

	s_ctrl->sensor_state = CAM_SENSOR_POWER_DOWN;

	return rc;
}

int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	int64_t req_id)
{
	int rc = 0, offset, del_req_id;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;

	if (req_id == 0) {
		i2c_set = &s_ctrl->i2c_data.init_settings;
		if (i2c_set->is_settings_valid == 1) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = camera_io_dev_write(
					&(s_ctrl->io_master_info),
					&(i2c_list->i2c_settings));
				if (rc < 0) {
					pr_err("Failed to write the I2C settings\n");
					return rc;
				}
			}
			rc = delete_request(&(s_ctrl->i2c_data.init_settings));
			i2c_set->is_settings_valid = 0;
			if (rc < 0) {
				pr_err("%s:%d :Error: Failed in deleting the Init request: %d\n",
					__func__, __LINE__, rc);
			}
		}
	} else {
		offset = req_id % MAX_PER_FRAME_ARRAY;
		i2c_set = &(s_ctrl->i2c_data.per_frame[offset]);
		if (i2c_set->is_settings_valid == 1 &&
			i2c_set->request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = camera_io_dev_write(
					&(s_ctrl->io_master_info),
					&(i2c_list->i2c_settings));
				if (rc < 0) {
					pr_err("%s:%d :Error: Fail to write the I2C settings: %d\n",
						__func__, __LINE__, rc);
					return rc;
				}
			}
			del_req_id = (req_id +
				MAX_PER_FRAME_ARRAY -
				MAX_SYSTEM_PIPELINE_DELAY) %
				MAX_PER_FRAME_ARRAY;
			CDBG("%s:%d Deleting the Request: %d\n",
				__func__, __LINE__,	del_req_id);
			if (req_id >
				s_ctrl->i2c_data.per_frame[del_req_id].
				request_id) {
				s_ctrl->i2c_data.per_frame[del_req_id].
					request_id = 0;
				rc = delete_request(
					&(s_ctrl->i2c_data.
					per_frame[del_req_id]));
				if (rc < 0)
					pr_err("%s:%d :Error: Failed in deleting the request: %d rc: %d\n",
						__func__, __LINE__,
						del_req_id, rc);
			}
		} else {
			CDBG("%s:%d Invalid/NOP request to apply: %lld\n",
				__func__, __LINE__, req_id);
		}
	}
	return rc;
}

int32_t cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		pr_err("%s: Device data is NULL\n", __func__);
		return -EINVAL;
	}
	CDBG("%s:%d Req Id: %lld\n", __func__, __LINE__,
		apply->request_id);
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id);
	return rc;
}
