/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"

//#undef CAM_DBG
//#define CAM_DBG CAM_INFO

#if MV_TEMP_SET
#define IR_CAMERA_ID	3
static uint32_t sensor_write_regs_addr;
static uint32_t sensor_write_regs_data;
static uint32_t sensor_read_regs_addr;
static uint32_t sensor_read_regs_data;
extern struct cam_sensor_ctrl_t *g_sctrl[MAX_CAMERAS];

ssize_t mv_operate_sensor_write_regs_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
 {
	int rc = 0;
	struct cam_sensor_ctrl_t  *s_ctrl = dev->driver_data;
	struct cam_sensor_i2c_reg_setting ir_write_setting;
	struct cam_sensor_i2c_reg_array ir_reg_array;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR,"%s: can not get the IR camera sensor!\n", __func__);
		return 0;
	}
	rc = sscanf(buf, "0x%x 0x%x", &sensor_write_regs_addr, &sensor_write_regs_data);
	if (rc == 0) {
		CAM_ERR(CAM_SENSOR,"%s: can not get right IR camera regs!\n", __func__);
	} else {
		ir_write_setting.size=  1;
		ir_write_setting.addr_type =CAMERA_SENSOR_I2C_TYPE_WORD;
		ir_write_setting.data_type= CAMERA_SENSOR_I2C_TYPE_BYTE;
		ir_write_setting.delay=0;
		ir_write_setting.reg_setting= &ir_reg_array;
		ir_reg_array.reg_addr = sensor_write_regs_addr;
		ir_reg_array.reg_data= sensor_write_regs_data;

		rc = camera_io_dev_write(&(s_ctrl->io_master_info),
		&ir_write_setting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,"%s: can not write IR camera sensor reg!\n", __func__);
		}
		CAM_DBG(CAM_SENSOR,"%s: sensor_write_regs_addr = 0x%x, sensor_write_regs_data = 0x%x ",
			__func__,sensor_write_regs_addr,sensor_write_regs_data);

		return count;
}

ssize_t mv_operate_sensor_write_regs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x 0x%x\n", sensor_write_regs_addr, sensor_write_regs_data);
}

ssize_t mv_operate_sensor_read_regs_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
 {
	int rc = 0;
	struct cam_sensor_ctrl_t  *s_ctrl = dev->driver_data;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR,"%s: can not get the IR camera sensor!\n", __func__);
		return 0;
	}
	rc = sscanf(buf, "0x%x", &sensor_read_regs_addr);
	if (rc == 0) {
		CAM_ERR(CAM_SENSOR,"%s: can not get right IR camera regs!\n", __func__);
	}
	else {
		rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		sensor_read_regs_addr,
		&sensor_read_regs_data, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,"%s: can not read IR camera sensor reg!\n", __func__);
		}
		CAM_DBG(CAM_SENSOR,"%s: sensor_read_regs_addr = 0x%x, sensor_read_regs_data = 0x%x ",
			__func__,sensor_read_regs_addr,sensor_read_regs_data);

		return count;
}

ssize_t mv_operate_sensor_read_regs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x 0x%x\n", sensor_read_regs_addr, sensor_read_regs_data);
}

#endif
static void cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_req_mgr_add_request add_req;

	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %lld",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	add_req.skip_before_applying = 0;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req)
		s_ctrl->bridge_intf.crm_cb->add_req(&add_req);

	CAM_DBG(CAM_SENSOR, " add req to req mgr: %lld",
			add_req.req_id);
}

static void cam_sensor_release_stream_rsc(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int rc;

	i2c_set = &(s_ctrl->i2c_data.streamoff_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamoff settings");
	}

	i2c_set = &(s_ctrl->i2c_data.streamon_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamon settings");
	}
}

static void cam_sensor_release_resource(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int i, rc;

	i2c_set = &(s_ctrl->i2c_data.init_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Init settings");
	}

	i2c_set = &(s_ctrl->i2c_data.config_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Res settings");
	}

	if (s_ctrl->i2c_data.per_frame != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.per_frame[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}
}

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

	ioctl_ctrl = (struct cam_control *)arg;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_SENSOR, "Invalid Handle Type");
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
		CAM_ERR(CAM_SENSOR, "Failed in getting the buffer: %d", rc);
		return rc;
	}

	csl_packet = (struct cam_packet *)(generic_ptr +
		config.offset);
	if (config.offset > len_of_buff) {
		CAM_ERR(CAM_SENSOR,
			"offset is out of bounds: off: %lld len: %zu",
			 config.offset, len_of_buff);
		return -EINVAL;
	}

	i2c_data = &(s_ctrl->i2c_data);
	CAM_DBG(CAM_SENSOR, "Header OpCode: %d", csl_packet->header.op_code);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
		i2c_reg_settings = &i2c_data->init_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
		i2c_reg_settings = &i2c_data->config_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
		if (s_ctrl->streamon_count > 0)
			return 0;
		s_ctrl->streamon_count = s_ctrl->streamon_count + 1;
		i2c_reg_settings = &i2c_data->streamon_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
		if (s_ctrl->streamoff_count > 0)
			return 0;
		s_ctrl->streamoff_count = s_ctrl->streamoff_count + 1;
		i2c_reg_settings = &i2c_data->streamoff_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}

	case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			return 0;
		}

		i2c_reg_settings =
			&i2c_data->
			per_frame[csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received Packet: %lld",
		csl_packet->header.request_id % MAX_PER_FRAME_ARRAY);
		if (i2c_reg_settings->is_settings_valid == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already some pkt in offset req : %lld",
				csl_packet->header.request_id);
			/*
			 * Update req mgr even in case of failure.
			 * This will help not to wait indefinitely
			 * and freeze. If this log is triggered then
			 * fix it.
			 */
			cam_sensor_update_req_mgr(s_ctrl, csl_packet);
			return 0;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_NOP: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed NOP packets without linking");
			return 0;
		}

		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		return 0;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header");
		return -EINVAL;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_command_parser(i2c_reg_settings, cmd_desc, 1);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
		return rc;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
	}

	return rc;
}

static int32_t cam_sensor_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;
	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			0);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			1);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
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
				CAM_ERR(CAM_SENSOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
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
			CAM_ERR(CAM_SENSOR, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
		cci_client->sid = i2c_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CAM_DBG(CAM_SENSOR, " Master: %d sid: %d freq_mode: %d",
			cci_client->cci_i2c_master, i2c_info->slave_addr,
			i2c_info->i2c_freq_mode);
	}

	s_ctrl->sensordata->slave_info.sensor_slave_addr =
		i2c_info->slave_addr;
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
	CAM_DBG(CAM_SENSOR,
		"Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x",
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask);
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
			CAM_ERR(CAM_SENSOR, "Failed in Updating the i2c Info");
			return rc;
		}
		probe_info = (struct cam_cmd_probe *)
			(cmd_buf + sizeof(struct cam_cmd_i2c_info));
		rc = cam_sensor_update_slave_info(probe_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Updating the slave Info");
			return rc;
		}
		cmd_buf = probe_info;
	}
		break;
	case 1: {
		rc = cam_sensor_update_power_settings(cmd_buf,
			cmd_buf_length, &s_ctrl->sensordata->power_info);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating power settings");
			return rc;
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid command buffer");
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
		CAM_ERR(CAM_SENSOR, "Failed to get the command Buffer");
		return -EINVAL;
	}
	pkt = (struct cam_packet *)packet;
	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR, "command descriptor pos is invalid");
		return -EINVAL;
	}
	if (pkt->num_cmd_buf != 2) {
		CAM_ERR(CAM_SENSOR, "Expected More Command Buffers : %d",
			 pkt->num_cmd_buf);
		return -EINVAL;
	}
	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			(uint64_t *)&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			return -EINVAL;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd_desc[i].length);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
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
		s_ctrl->soc_info.index;
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

void cam_sensor_shutdown(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	int rc = 0;

	s_ctrl->is_probe_succeed = 0;
	if (s_ctrl->sensor_state == CAM_SENSOR_INIT)
		return;

	cam_sensor_release_resource(s_ctrl);
	cam_sensor_release_stream_rsc(s_ctrl);
	if (s_ctrl->sensor_state >= CAM_SENSOR_ACQUIRE)
		cam_sensor_power_down(s_ctrl);

	rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, " failed destroying dhdl");
	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.session_hdl = -1;

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	s_ctrl->streamon_count = 0;
	s_ctrl->streamoff_count = 0;
	s_ctrl->sensor_state = CAM_SENSOR_INIT;
}

int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, " failed: %pK",
			 slave_info);
		return -EINVAL;
	}

	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD);

	CAM_INFO(CAM_SENSOR, "read id: 0x%x expected id 0x%x:",
			 chipid, slave_info->sensor_id);
	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_ERR(CAM_SENSOR, "chip id %x does not match %x",
				chipid, slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

uint32_t g_operation_mode;

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
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		if (s_ctrl->is_probe_succeed == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already Sensor Probed in the slot");
			break;
		}
		/* Allocate memory for power up setting */
		pu = kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
		if (!pu) {
			rc = -ENOMEM;
			CAM_ERR(CAM_SENSOR,
				"failed");
			goto release_mutex;
		}

		pd = kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
		if (!pd) {
			kfree(pu);
			rc = -ENOMEM;
			CAM_ERR(CAM_SENSOR,
				"failed");
			goto release_mutex;
		}

		power_info->power_setting = pu;
		power_info->power_down_setting = pd;

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Get Buffer Handle Failed");
				kfree(pu);
				kfree(pd);
				goto release_mutex;
			}
		} else {
			CAM_ERR(CAM_SENSOR, "Invalid Command Type: %d",
				 cmd->handle_type);
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PUP rc %d",
				 rc);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PDOWN rc %d",
				 rc);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "power up failed");
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

		CAM_INFO(CAM_SENSOR,
			"Probe Succees,slot:%d,slave_addr:0x%x,sensor_id:0x%x",
			s_ctrl->soc_info.index,
			s_ctrl->sensordata->slave_info.sensor_slave_addr,
			s_ctrl->sensordata->slave_info.sensor_id);

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "fail in Sensor Power Down");
			kfree(pu);
			kfree(pd);
			goto release_mutex;
		}
		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;
	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_SENSOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&sensor_acq_dev,
			(void __user *) cmd->handle, sizeof(sensor_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed Copying from user");
			goto release_mutex;
		}

		CAM_DBG(CAM_SENSOR, "[xdgu] operation mode :%d", sensor_acq_dev.operation_mode);
		g_operation_mode = sensor_acq_dev.operation_mode;

		bridge_params.session_hdl = sensor_acq_dev.session_handle;
		bridge_params.ops = &s_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CAM_DBG(CAM_SENSOR, "Device Handle: %d",
			sensor_acq_dev.device_handle);
		if (copy_to_user((void __user *) cmd->handle, &sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power up failed");
			goto release_mutex;
		}

		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		CAM_INFO(CAM_SENSOR,
			"CAM_ACQUIRE_DEV Success, sensor_id:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id);
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to release : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power Down failed");
			goto release_mutex;
		}

		cam_sensor_release_resource(s_ctrl);
		cam_sensor_release_stream_rsc(s_ctrl);
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_SENSOR,
				"Invalid Handles: link hdl: %d device hdl: %d",
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed in destroying the device hdl");
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;
		s_ctrl->streamon_count = 0;
		s_ctrl->streamoff_count = 0;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;
		CAM_INFO(CAM_SENSOR,
			"CAM_RELEASE_DEV Success, sensor_id:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id);
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		cam_sensor_query_cap(s_ctrl, &sensor_cap);
		if (copy_to_user((void __user *) cmd->handle, &sensor_cap,
			sizeof(struct  cam_sensor_query_cap))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to start : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamon_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamon_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply streamon settings");
				goto release_mutex;
			}
		}
		s_ctrl->sensor_state = CAM_SENSOR_START;
		CAM_INFO(CAM_SENSOR,
			"CAM_START_DEV Success, sensor_id:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id);
	}
		break;
	case CAM_STOP_DEV: {
		if (s_ctrl->sensor_state != CAM_SENSOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to stop : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings");
			}
		}

		cam_sensor_release_resource(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		CAM_INFO(CAM_SENSOR,
			"CAM_STOP_DEV Success, sensor_id:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id);
	}
		break;
	case CAM_CONFIG_DEV: {
		rc = cam_sensor_i2c_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed CCI Config: %d", rc);
			goto release_mutex;
		}
		if (s_ctrl->i2c_data.init_settings.is_settings_valid &&
			(s_ctrl->i2c_data.init_settings.request_id == 0)) {

			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply init settings");
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the Init settings");
				goto release_mutex;
			}
			s_ctrl->i2c_data.init_settings.request_id = -1;
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {

			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply config settings");
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the config settings");
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
			s_ctrl->i2c_data.config_settings.request_id = -1;
		}
	}
		break;
	case CAM_IR_UPDATE: {
		struct cam_sensor_i2c_reg_setting user_reg_setting;
		struct cam_sensor_i2c_reg_array i2c_reg_setting[cmd->size];

		rc = copy_from_user(&user_reg_setting, (void __user *)cmd->handle, sizeof(user_reg_setting));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Copy data from user space failed\n");
			goto release_mutex;
		}

		rc = copy_from_user(i2c_reg_setting, (void __user *)user_reg_setting.reg_setting, sizeof(i2c_reg_setting));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Copy i2c setting from user space failed\n");
			goto release_mutex;
		}

		user_reg_setting.reg_setting = i2c_reg_setting;

		rc = camera_io_dev_write(&s_ctrl->io_master_info, &user_reg_setting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Write setting failed, rc = %d\n", rc);
	}
		break;
	case CAM_IR_GET_POWER_STATE: {
		if (copy_to_user((void __user *)cmd->handle, &s_ctrl->sensor_state, sizeof(s_ctrl->sensor_state))) {
			CAM_ERR(CAM_SENSOR, "Copy state to user space failed\n");
			rc = -EFAULT;
		}
	}
		break;
	case CAM_IR_LUMA_READ: {
		int ret = 0;
		unsigned int reg_3500, reg_3501, reg_3502, reg_3509;
		struct cam_luma_data luma_data;

		ret += camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x3500,
				&reg_3500, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE);
		ret += camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x3501,
				&reg_3501, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE);
		ret += camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x3502,
				&reg_3502, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE);
		ret += camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x3509,
				&reg_3509, CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE);

		luma_data.expo = ((reg_3500 & 0xf) << 8) + ((reg_3501 & 0xff) << 4) + ((reg_3502 & 0xff) >> 4);
		luma_data.gain = reg_3509;
		CAM_ERR(CAM_SENSOR, "CAM_IR_LUMA_READ=0x%x, expo=0x%x, gain=0x%x. \n",
			CAM_IR_LUMA_READ, luma_data.expo, luma_data.gain);

		if (copy_to_user((void __user *)cmd->handle, &luma_data, sizeof(luma_data)) || ret != 0) {
			CAM_ERR(CAM_SENSOR, "Copy state to user space failed\n");
			rc = -EFAULT;
		}
	}
		break;

	default:
		CAM_ERR(CAM_SENSOR, "Invalid Opcode: %d", cmd->op_code);
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

	//Note: Only for face unlock usecase
	if (g_operation_mode == 0x8006)
		info->p_delay = 0;

	info->trigger = CAM_TRIGGER_POINT_SOF;

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
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
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
	if (!on && s_ctrl->sensor_state == CAM_SENSOR_START) {
		cam_sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_camera_slave_info *slave_info;
	struct cam_hw_soc_info *soc_info =
		&s_ctrl->soc_info;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: %pK", s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!power_info || !slave_info) {
		CAM_ERR(CAM_SENSOR, "failed: %pK %pK", power_info, slave_info);
		return -EINVAL;
	}

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power up the core is failed:%d", rc);
		return rc;
	}

	rc = camera_io_init(&(s_ctrl->io_master_info));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);

	return rc;
}

int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info;
	int rc = 0;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: s_ctrl %pK", s_ctrl);
		return -EINVAL;
	}

	power_info = &s_ctrl->sensordata->power_info;
	soc_info = &s_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SENSOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = msm_camera_power_down(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&(s_ctrl->io_master_info));

	return rc;
}

int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	int64_t req_id, enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0, offset, del_req_id;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;
		uint16_t i;
	if (req_id == 0) {
		switch (opcode) {
		CAM_DBG(CAM_SENSOR,"[CDBG] opcode: %d", opcode);
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
			i2c_set = &s_ctrl->i2c_data.streamon_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.init_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.config_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
			i2c_set = &s_ctrl->i2c_data.streamoff_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
		}
		if (i2c_set->is_settings_valid == 1) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				CAM_DBG(CAM_SENSOR,"[CDBG] master type = %d",s_ctrl->io_master_info.master_type);
				for (i=0;i<i2c_list->i2c_settings.size;i++) {
					CAM_DBG(CAM_SENSOR,"[%04d] [CDBG] 0x%04X 0x%04X 0x%02X", i,
						i2c_list->i2c_settings.reg_setting[i].reg_addr,
						i2c_list->i2c_settings.reg_setting[i].reg_data,
						i2c_list->i2c_settings.reg_setting[i].delay);
				}
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		}
	} else {
		offset = req_id % MAX_PER_FRAME_ARRAY;
		i2c_set = &(s_ctrl->i2c_data.per_frame[offset]);
		if (i2c_set->is_settings_valid == 1 &&
			i2c_set->request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		del_req_id = (req_id + MAX_PER_FRAME_ARRAY -
			MAX_SYSTEM_PIPELINE_DELAY) % MAX_PER_FRAME_ARRAY;
		CAM_DBG(CAM_SENSOR, "Deleting the Request: %d", del_req_id);

		if ((req_id >
			 s_ctrl->i2c_data.per_frame[del_req_id].request_id) &&
			(s_ctrl->i2c_data.per_frame[del_req_id].
				is_settings_valid == 1)) {
			s_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
			rc = delete_request(
				&(s_ctrl->i2c_data.per_frame[del_req_id]));
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"Delete request Fail:%d rc:%d",
					del_req_id, rc);
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
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}
	CAM_DBG(CAM_SENSOR, " Req Id: %lld", apply->request_id);
	trace_cam_apply_req("Sensor", apply->request_id);
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE);
	return rc;
}

int32_t cam_sensor_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
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
		CAM_DBG(CAM_SENSOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	return rc;
}
