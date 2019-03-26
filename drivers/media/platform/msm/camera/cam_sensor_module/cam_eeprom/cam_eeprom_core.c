/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_eeprom_core.h"
#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

//sdg add arcsoft Calibration 2018.12.07
#ifdef LC_ARCSOFT_CALIBRATION
static struct kobject *msm_eepromrw_device=NULL;
#define IMX586_ARCSOFT_START 				0x1363
#define IMX586_ARCSOFT_DUAL_START 		0x1364
#define IMX586_ARCSOFT_DUAL_CHECKSUM 	0x1B64
#define IMX586_ARCSOFT_DUAL_DATA_NUM 	2048
#define IMX586_EEPROM_PROTECTION_ADDR 	0x8000
bool imx586_Rewrite_enable = false;
static bool imx586_power_save = false;
#endif
///end sdg

/**
 * cam_eeprom_read_memory() - read map data into buffer
 * @e_ctrl:     eeprom control struct
 * @block:      block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */
static int cam_eeprom_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block)
{
	int                                rc = 0;
	int                                j;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	struct cam_eeprom_memory_map_t    *emap = block->map;
	struct cam_eeprom_soc_private     *eb_info = NULL;
	uint8_t                           *memptr = block->mapdata;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	eb_info = (struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;

	for (j = 0; j < block->num_map; j++) {
		CAM_DBG(CAM_EEPROM, "slave-addr = 0x%X", emap[j].saddr);
		if (emap[j].saddr) {
			eb_info->i2c_info.slave_addr = emap[j].saddr;
			rc = cam_eeprom_update_i2c_info(e_ctrl,
				&eb_info->i2c_info);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"failed: to update i2c info rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].page.valid_size) {
			i2c_reg_settings.addr_type = emap[j].page.addr_type;
			i2c_reg_settings.data_type = emap[j].page.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].page.addr;
			i2c_reg_array.reg_data = emap[j].page.data;
			i2c_reg_array.delay = emap[j].page.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "page write failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[j].pageen.addr_type;
			i2c_reg_settings.data_type = emap[j].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].pageen.addr;
			i2c_reg_array.reg_data = emap[j].pageen.data;
			i2c_reg_array.delay = emap[j].pageen.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "page enable failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].poll.valid_size) {
			rc = camera_io_dev_poll(&e_ctrl->io_master_info,
				emap[j].poll.addr, emap[j].poll.data,
				0, emap[j].poll.addr_type,
				emap[j].poll.data_type,
				emap[j].poll.delay);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "poll failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].mem.valid_size) {
			rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				emap[j].mem.addr, memptr,
				emap[j].mem.addr_type,
				emap[j].mem.data_type,
				emap[j].mem.valid_size);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "read failed rc %d",
					rc);
				return rc;
			}
			memptr += emap[j].mem.valid_size;
		}

		if (emap[j].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[j].pageen.addr_type;
			i2c_reg_settings.data_type = emap[j].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].pageen.addr;
			i2c_reg_array.reg_data = 0;
			i2c_reg_array.delay = emap[j].pageen.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"page disable failed rc %d",
					rc);
				return rc;
			}
		}
	}
	return rc;
}

/**
 * cam_eeprom_power_up - Power up eeprom hardware
 * @e_ctrl:     ctrl structure
 * @power_info: power up/down info for eeprom
 *
 * Returns success or failure
 */
static int cam_eeprom_power_up(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int32_t                 rc = 0;
	struct cam_hw_soc_info *soc_info =
		&e_ctrl->soc_info;

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"failed to fill power up vreg params rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"failed to fill power down vreg params  rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed in eeprom power up rc %d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = camera_io_init(&(e_ctrl->io_master_info));
		if (rc) {
			CAM_ERR(CAM_EEPROM, "cci_init failed");
			return -EINVAL;
		}
	}
	return rc;
}

/**
 * cam_eeprom_power_down - Power down eeprom hardware
 * @e_ctrl:    ctrl structure
 *
 * Returns success or failure
 */
static int cam_eeprom_power_down(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info         *soc_info;
	struct cam_eeprom_soc_private  *soc_private;
	int                             rc = 0;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "failed: e_ctrl %pK", e_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &e_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_EEPROM, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "power down the core is failed:%d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER)
		camera_io_release(&(e_ctrl->io_master_info));

	return rc;
}

/**
 * cam_eeprom_match_id - match eeprom id
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int cam_eeprom_match_id(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int                      rc;
	struct camera_io_master *client = &e_ctrl->io_master_info;
	uint8_t                  id[2];

	rc = cam_spi_query_id(client, 0, CAMERA_SENSOR_I2C_TYPE_WORD,
		&id[0], 2);
	if (rc)
		return rc;
	CAM_DBG(CAM_EEPROM, "read 0x%x 0x%x, check 0x%x 0x%x",
		id[0], id[1], client->spi_client->mfr_id0,
		client->spi_client->device_id0);
	if (id[0] != client->spi_client->mfr_id0
		|| id[1] != client->spi_client->device_id0)
		return -ENODEV;
	return 0;
}

/**
 * cam_eeprom_parse_read_memory_map - Parse memory map
 * @of_node:    device node
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
int32_t cam_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct cam_eeprom_ctrl_t *e_ctrl)
{
	int32_t                         rc = 0;
	struct cam_eeprom_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	rc = cam_eeprom_parse_dt_memory_map(of_node, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed: eeprom dt parse rc %d", rc);
		return rc;
	}
	rc = cam_eeprom_power_up(e_ctrl, power_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed: eeprom power up rc %d", rc);
		goto data_mem_free;
	}

	e_ctrl->cam_eeprom_state = CAM_EEPROM_CONFIG;
	if (e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE) {
		rc = cam_eeprom_match_id(e_ctrl);
		if (rc) {
			CAM_DBG(CAM_EEPROM, "eeprom not matching %d", rc);
			goto power_down;
		}
	}
	rc = cam_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
		goto power_down;
	}

	rc = cam_eeprom_power_down(e_ctrl);
	if (rc)
		CAM_ERR(CAM_EEPROM, "failed: eeprom power down rc %d", rc);

	e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	return rc;
power_down:
	cam_eeprom_power_down(e_ctrl);
data_mem_free:
	vfree(e_ctrl->cal_data.mapdata);
	vfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
	e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	return rc;
}

/**
 * cam_eeprom_get_dev_handle - get device handle
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_get_dev_handle(struct cam_eeprom_ctrl_t *e_ctrl,
	void *arg)
{
	struct cam_sensor_acquire_dev    eeprom_acq_dev;
	struct cam_create_dev_hdl        bridge_params;
	struct cam_control              *cmd = (struct cam_control *)arg;

	if (e_ctrl->bridge_intf.device_hdl != -1) {
		CAM_ERR(CAM_EEPROM, "Device is already acquired");
		return -EFAULT;
	}
	if (copy_from_user(&eeprom_acq_dev,
		u64_to_user_ptr(cmd->handle),
		sizeof(eeprom_acq_dev))) {
		CAM_ERR(CAM_EEPROM,
			"EEPROM:ACQUIRE_DEV: copy from user failed");
		return -EFAULT;
	}

	bridge_params.session_hdl = eeprom_acq_dev.session_handle;
	bridge_params.ops = &e_ctrl->bridge_intf.ops;
	bridge_params.v4l2_sub_dev_flag = 0;
	bridge_params.media_entity_flag = 0;
	bridge_params.priv = e_ctrl;

	eeprom_acq_dev.device_handle =
		cam_create_device_hdl(&bridge_params);
	e_ctrl->bridge_intf.device_hdl = eeprom_acq_dev.device_handle;
	e_ctrl->bridge_intf.session_hdl = eeprom_acq_dev.session_handle;

	CAM_DBG(CAM_EEPROM, "Device Handle: %d", eeprom_acq_dev.device_handle);
	if (copy_to_user(u64_to_user_ptr(cmd->handle),
		&eeprom_acq_dev, sizeof(struct cam_sensor_acquire_dev))) {
		CAM_ERR(CAM_EEPROM, "EEPROM:ACQUIRE_DEV: copy to user failed");
		return -EFAULT;
	}
	return 0;
}

/**
 * cam_eeprom_update_slaveInfo - Update slave info
 * @e_ctrl:     ctrl structure
 * @cmd_buf:    command buffer
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_update_slaveInfo(struct cam_eeprom_ctrl_t *e_ctrl,
	void *cmd_buf)
{
	int32_t                         rc = 0;
	struct cam_eeprom_soc_private  *soc_private;
	struct cam_cmd_i2c_info        *cmd_i2c_info = NULL;

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	cmd_i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	soc_private->i2c_info.slave_addr = cmd_i2c_info->slave_addr;
	soc_private->i2c_info.i2c_freq_mode = cmd_i2c_info->i2c_freq_mode;

	rc = cam_eeprom_update_i2c_info(e_ctrl,
		&soc_private->i2c_info);
	CAM_DBG(CAM_EEPROM, "Slave addr: 0x%x Freq Mode: %d",
		soc_private->i2c_info.slave_addr,
		soc_private->i2c_info.i2c_freq_mode);

	return rc;
}

/**
 * cam_eeprom_parse_memory_map - Parse memory map info
 * @data:             memory block data
 * @cmd_buf:          command buffer
 * @cmd_length:       command buffer length
 * @num_map:          memory map size
 * @cmd_length_bytes: command length processed in this function
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_parse_memory_map(
	struct cam_eeprom_memory_block_t *data,
	void *cmd_buf, int cmd_length, uint16_t *cmd_length_bytes,
	int *num_map)
{
	int32_t                            rc = 0;
	int32_t                            cnt = 0;
	int32_t                            processed_size = 0;
	uint8_t                            generic_op_code;
	struct cam_eeprom_memory_map_t    *map = data->map;
	struct common_header              *cmm_hdr =
		(struct common_header *)cmd_buf;
	uint16_t                           cmd_length_in_bytes = 0;
	struct cam_cmd_i2c_random_wr      *i2c_random_wr = NULL;
	struct cam_cmd_i2c_continuous_rd  *i2c_cont_rd = NULL;
	struct cam_cmd_conditional_wait   *i2c_poll = NULL;
	struct cam_cmd_unconditional_wait *i2c_uncond_wait = NULL;

	generic_op_code = cmm_hdr->third_byte;
	switch (cmm_hdr->cmd_type) {
	case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
		i2c_random_wr = (struct cam_cmd_i2c_random_wr *)cmd_buf;
		cmd_length_in_bytes   = sizeof(struct cam_cmd_i2c_random_wr) +
			((i2c_random_wr->header.count - 1) *
			sizeof(struct i2c_random_wr_payload));

		for (cnt = 0; cnt < (i2c_random_wr->header.count);
			cnt++) {
			map[*num_map + cnt].page.addr =
				i2c_random_wr->random_wr_payload[cnt].reg_addr;
			map[*num_map + cnt].page.addr_type =
				i2c_random_wr->header.addr_type;
			map[*num_map + cnt].page.data =
				i2c_random_wr->random_wr_payload[cnt].reg_data;
			map[*num_map + cnt].page.data_type =
				i2c_random_wr->header.data_type;
			map[*num_map + cnt].page.valid_size = 1;
		}

		*num_map += (i2c_random_wr->header.count - 1);
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		break;
	case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD:
		i2c_cont_rd = (struct cam_cmd_i2c_continuous_rd *)cmd_buf;
		cmd_length_in_bytes = sizeof(struct cam_cmd_i2c_continuous_rd);

		map[*num_map].mem.addr = i2c_cont_rd->reg_addr;
		map[*num_map].mem.addr_type = i2c_cont_rd->header.addr_type;
		map[*num_map].mem.data_type = i2c_cont_rd->header.data_type;
		map[*num_map].mem.valid_size =
			i2c_cont_rd->header.count;
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		data->num_data += map[*num_map].mem.valid_size;
		break;
	case CAMERA_SENSOR_CMD_TYPE_WAIT:
		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND ||
			generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_SW_UCND) {
			i2c_uncond_wait =
				(struct cam_cmd_unconditional_wait *)cmd_buf;
			cmd_length_in_bytes =
				sizeof(struct cam_cmd_unconditional_wait);

			if (*num_map < 1) {
				CAM_ERR(CAM_EEPROM,
					"invalid map number, num_map=%d",
					*num_map);
				return -EINVAL;
			}

			/*
			 * Though delay is added all of them, but delay will
			 * be applicable to only one of them as only one of
			 * them will have valid_size set to >= 1.
			 */
			map[*num_map - 1].mem.delay = i2c_uncond_wait->delay;
			map[*num_map - 1].page.delay = i2c_uncond_wait->delay;
			map[*num_map - 1].pageen.delay = i2c_uncond_wait->delay;
		} else if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_COND) {
			i2c_poll = (struct cam_cmd_conditional_wait *)cmd_buf;
			cmd_length_in_bytes =
				sizeof(struct cam_cmd_conditional_wait);

			map[*num_map].poll.addr = i2c_poll->reg_addr;
			map[*num_map].poll.addr_type = i2c_poll->addr_type;
			map[*num_map].poll.data = i2c_poll->reg_data;
			map[*num_map].poll.data_type = i2c_poll->data_type;
			map[*num_map].poll.delay = i2c_poll->timeout;
			map[*num_map].poll.valid_size = 1;
		}
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		break;
	default:
		break;
	}

	*cmd_length_bytes = processed_size;
	return rc;
}

/**
 * cam_eeprom_init_pkt_parser - Parse eeprom packet
 * @e_ctrl:       ctrl structure
 * @csl_packet:	  csl packet received
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_init_pkt_parser(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	int32_t                         rc = 0;
	int                             i = 0;
	struct cam_cmd_buf_desc        *cmd_desc = NULL;
	uint32_t                       *offset = NULL;
	uint32_t                       *cmd_buf = NULL;
	uintptr_t                        generic_pkt_addr;
	size_t                          pkt_len = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	uint32_t                        processed_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uint16_t                        cmd_length_in_bytes = 0;
	struct cam_cmd_i2c_info        *i2c_info = NULL;
	int                             num_map = -1;
	struct cam_eeprom_memory_map_t *map = NULL;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	e_ctrl->cal_data.map = vzalloc((MSM_EEPROM_MEMORY_MAP_MAX_SIZE *
		MSM_EEPROM_MAX_MEM_MAP_CNT) *
		(sizeof(struct cam_eeprom_memory_map_t)));
	if (!e_ctrl->cal_data.map) {
		rc = -ENOMEM;
		CAM_ERR(CAM_EEPROM, "failed");
		return rc;
	}
	map = e_ctrl->cal_data.map;

	offset = (uint32_t *)&csl_packet->payload;
	offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	/* Loop through multiple command buffers */
	for (i = 0; i < csl_packet->num_cmd_buf; i++) {
		total_cmd_buf_in_bytes = cmd_desc[i].length;
		processed_cmd_buf_in_bytes = 0;
		if (!total_cmd_buf_in_bytes)
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&generic_pkt_addr, &pkt_len);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed to get cpu buf");
			return rc;
		}
		cmd_buf = (uint32_t *)generic_pkt_addr;
		if (!cmd_buf) {
			CAM_ERR(CAM_EEPROM, "invalid cmd buf");
			rc = -EINVAL;
			goto rel_cmd_buf;
		}
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
		/* Loop through multiple cmd formats in one cmd buffer */
		while (processed_cmd_buf_in_bytes < total_cmd_buf_in_bytes) {
			cmm_hdr = (struct common_header *)cmd_buf;
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
				/* Configure the following map slave address */
				map[num_map + 1].saddr = i2c_info->slave_addr;
				rc = cam_eeprom_update_slaveInfo(e_ctrl,
					cmd_buf);
				cmd_length_in_bytes =
					sizeof(struct cam_cmd_i2c_info);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/
					sizeof(uint32_t);
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				cmd_length_in_bytes = total_cmd_buf_in_bytes;
				rc = cam_sensor_update_power_settings(cmd_buf,
					cmd_length_in_bytes, power_info);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/
					sizeof(uint32_t);
				if (rc) {
					CAM_ERR(CAM_EEPROM, "Failed");
					goto rel_cmd_buf;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD:
			case CAMERA_SENSOR_CMD_TYPE_WAIT:
				num_map++;
				rc = cam_eeprom_parse_memory_map(
					&e_ctrl->cal_data, cmd_buf,
					total_cmd_buf_in_bytes,
					&cmd_length_in_bytes, &num_map);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/sizeof(uint32_t);
				break;
			default:
				break;
			}
		}
		e_ctrl->cal_data.num_map = num_map + 1;
		if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
			CAM_WARN(CAM_EEPROM, "Failed to put cpu buf: 0x%x",
				cmd_desc[i].mem_handle);
	}

	return rc;

rel_cmd_buf:
	if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
		CAM_WARN(CAM_EEPROM, "Failed to put cpu buf: 0x%x",
			cmd_desc[i].mem_handle);

	return rc;
}

/**
 * cam_eeprom_get_cal_data - parse the userspace IO config and
 *                                        copy read data to share with userspace
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_get_cal_data(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0;
	int                   rc = 0;
	uintptr_t              buf_addr;
	size_t                buf_size;
	uint8_t               *read_buffer;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&csl_packet->payload +
		csl_packet->io_configs_offset);

	CAM_DBG(CAM_EEPROM, "number of IO configs: %d:",
		csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_DBG(CAM_EEPROM, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
				&buf_addr, &buf_size);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "Fail in get buffer: %d",
					rc);
				return rc;
			}

			CAM_DBG(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu\n",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				rc = -EINVAL;
				goto rel_cmd_buf;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				rc = -EINVAL;
				goto rel_cmd_buf;
			}

			CAM_DBG(CAM_EEPROM, "copy the data, len:%d",
				e_ctrl->cal_data.num_data);
			memcpy(read_buffer, e_ctrl->cal_data.mapdata,
					e_ctrl->cal_data.num_data);
			if (cam_mem_put_cpu_buf(io_cfg->mem_handle[0]))
				CAM_WARN(CAM_EEPROM, "Fail in put buffer: 0x%x",
					io_cfg->mem_handle[0]);
		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}
	return rc;

rel_cmd_buf:
	if (cam_mem_put_cpu_buf(io_cfg->mem_handle[0]))
		CAM_WARN(CAM_EEPROM, "Fail in put buffer : 0x%x",
			io_cfg->mem_handle[0]);

	return rc;
}

//sdg add arcsoft Calibration 2018.12.07
#ifdef LC_ARCSOFT_CALIBRATION
static int cam_eeprom_write_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block)
{
       int rc = 0;
	int j = 0;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	struct cam_eeprom_memory_map_t    *emap = block->map;
	struct cam_eeprom_soc_private     *eb_info = NULL;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}
	eb_info = (struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;

	for (j = 0; j < block->num_map; j++) {
		if (emap[j].saddr) {
			eb_info->i2c_info.slave_addr = emap[j].saddr;
			rc = cam_eeprom_update_i2c_info(e_ctrl,
				&eb_info->i2c_info);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"sdg++ failed: to update i2c info rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].page.valid_size) {
			i2c_reg_settings.addr_type = emap[j].page.addr_type;
			i2c_reg_settings.data_type = emap[j].page.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].page.addr;
			i2c_reg_array.reg_data = emap[j].page.data;
			i2c_reg_array.delay = emap[j].page.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;

			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			msleep(2);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "page write failed rc %d",
					rc);
				return rc;
			}
		}
	}
	return rc;
}

static int eeprom_write_dualcam_cal_data(struct cam_eeprom_ctrl_t *e_ctrl, uint8_t *sumbuf)
{
	int rc = 0;
	int j = 0;
	uint32_t addr = IMX586_ARCSOFT_DUAL_START;

	unsigned char *buf = NULL;
	struct cam_eeprom_memory_block_t block;
	struct cam_eeprom_memory_map_t *map = NULL;

	mm_segment_t fs;
	loff_t pos;
	struct file *fp = NULL;

	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open("/data/vendor/camera/rewrite_arcsoft_calibration_data.bin", O_RDWR, 0777);

	if (IS_ERR(fp))
	{
		CAM_ERR(CAM_EEPROM, "sdg++ Brave open file fail,Not need rewrite!");
		set_fs(fs);
		return -EINVAL;
	}
	CAM_ERR(CAM_EEPROM, "sdg++Brave open file succeed,need rewrite!");

	buf = vzalloc(sizeof(unsigned char) * IMX586_ARCSOFT_DUAL_DATA_NUM);
	if(!buf)
	{
		CAM_ERR(CAM_EEPROM, "sdg++vzalloc buf fail");
	}
	pos = 0;
	vfs_read(fp, buf, sizeof(*buf) * IMX586_ARCSOFT_DUAL_DATA_NUM, &pos);
	set_fs(fs);
	filp_close(fp, NULL);

	map = vzalloc(sizeof(struct cam_eeprom_memory_map_t) * IMX586_ARCSOFT_DUAL_DATA_NUM);
	if(!map)
	{
		CAM_ERR(CAM_EEPROM, "sdg++vzalloc map fail");
	}

	block.map = map;
	block.num_map = IMX586_ARCSOFT_DUAL_DATA_NUM;

	for (j= 0 ;j < IMX586_ARCSOFT_DUAL_DATA_NUM;j++)
	{
		map[j].page.data = buf[j];
		map[j].page.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		map[j].page.addr = addr;
		map[j].page.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		map[j].page.valid_size = 1;
		map[j].page.delay = 1;
		addr += 1;
		sumbuf[0] += map[j].page.data;
	}

	rc = cam_eeprom_write_memory(e_ctrl,&block);
	if(rc)
	{
		CAM_ERR(CAM_EEPROM, "sdg++ cam_eeprom_write_memory fail");
	}

	vfree(map);
	vfree(buf);
	map = NULL;
	buf = NULL;

	return rc;
}

static ssize_t cam_eeprom_rw_power_save(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_soc_private  *soc_private)
{
	struct file *fp = NULL;
	uint16_t i = 0;
	mm_segment_t fs;
	loff_t pos;

	fp = filp_open("/data/vendor/camera/imx586_arcsoft_poweron_data.bin", O_RDWR | O_CREAT , 0777);
	if (IS_ERR(fp))
	{
		CAM_ERR(CAM_EEPROM, "sdg++ Power-on save file creation failed!");
		return -EINVAL;
	} else {
		CAM_ERR(CAM_EEPROM, "sdg++ The power-on save file is created successfully!");
	}
	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = fp->f_pos;
       vfs_write(fp, (unsigned char *)e_ctrl, sizeof(struct cam_eeprom_ctrl_t), &pos);
       fp->f_pos = pos;

       pos = fp->f_pos;
       vfs_write(fp, (unsigned char *)(&soc_private->power_info), sizeof(struct cam_sensor_power_ctrl_t), &pos);
       fp->f_pos = pos;

	for (i = 0;i < (soc_private->power_info.power_setting_size); i++) {
	pos = fp->f_pos;
	vfs_write(fp, (unsigned char *)(&(soc_private->power_info.power_setting[i])), sizeof(struct cam_sensor_power_setting) , &pos);
	fp->f_pos = pos;
	}

	for (i = 0;i < (soc_private->power_info.power_down_setting_size); i++) {
	pos = fp->f_pos;
	vfs_write(fp, (unsigned char *)(&(soc_private->power_info.power_down_setting[i])), sizeof(struct cam_sensor_power_setting), &pos);
	fp->f_pos = pos;
	}

	pos = fp->f_pos;
	vfs_write(fp, (unsigned char *)(&soc_private->power_info.gpio_num_info), sizeof(struct msm_camera_gpio_num_info), &pos);
	fp->f_pos = pos;

	CAM_ERR(CAM_EEPROM, "sdg++ The power-on write file is successfully!");

	set_fs(fs);
	filp_close(fp, NULL);

	return 0;
}

static int update_power_config(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_soc_private  *soc_private)
{
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;
	uint16_t i = 0;
	static struct cam_sensor_power_setting *power_setting = NULL;
	static struct cam_sensor_power_setting *power_down_setting = NULL;
	static struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	fp = filp_open("/data/vendor/camera/imx586_arcsoft_poweron_data.bin", O_RDWR, 0777);
	if (IS_ERR(fp))
	{
		CAM_ERR(CAM_EEPROM, "sdg++ open Power-on file creation failed!");
		return -EINVAL;
	} else {
		CAM_ERR(CAM_EEPROM, "sdg++ open The power-on file is created successfully!");
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = fp->f_pos;
       vfs_read(fp, (unsigned char *)e_ctrl, sizeof(struct cam_eeprom_ctrl_t), &pos);
       fp->f_pos = pos;

       pos = fp->f_pos;
       vfs_read(fp, (unsigned char *)(&soc_private->power_info), sizeof(struct cam_sensor_power_ctrl_t), &pos);
       fp->f_pos = pos;
	CAM_ERR(CAM_EEPROM, "sdg++ pos = %d power_setting_size = %d %d",pos,soc_private->power_info.power_setting_size,soc_private->power_info.power_down_setting_size);

	gpio_num_info = vzalloc(sizeof(struct msm_camera_gpio_num_info));
	if (!gpio_num_info) {
		CAM_ERR(CAM_EEPROM, "sdg++ failed");
		goto error;
	}
	power_setting = vzalloc(sizeof(struct cam_sensor_power_setting) * (soc_private->power_info.power_setting_size));
	if (!power_setting) {
		CAM_ERR(CAM_EEPROM, "sdg++ failed");
		vfree(gpio_num_info);
		gpio_num_info = NULL;
		goto error;
	}

	power_down_setting = vzalloc(sizeof(struct cam_sensor_power_setting)* (soc_private->power_info.power_down_setting_size));
	if (!power_down_setting) {
		CAM_ERR(CAM_EEPROM, "sdg++ failed");
		vfree(gpio_num_info);
		vfree(power_setting);
		gpio_num_info = NULL;
		power_setting = NULL;
		goto error;
	}

	for (i = 0;i < (soc_private->power_info.power_setting_size); i++) {
	pos = fp->f_pos;
	vfs_read(fp, (unsigned char *)(&power_setting[i]), sizeof(struct cam_sensor_power_setting) , &pos);
	fp->f_pos = pos;
	}

	for (i = 0;i < (soc_private->power_info.power_down_setting_size); i++) {
	pos = fp->f_pos;
	vfs_read(fp, (unsigned char *)(&power_down_setting[i]), sizeof(struct cam_sensor_power_setting), &pos);
	fp->f_pos = pos;
	}

	pos = fp->f_pos;
	vfs_read(fp, (unsigned char *)gpio_num_info, sizeof(struct msm_camera_gpio_num_info), &pos);
	fp->f_pos = pos;

	set_fs(fs);
	filp_close(fp, NULL);

	soc_private->power_info.gpio_num_info = gpio_num_info;
	soc_private->power_info.power_setting = power_setting;
	soc_private->power_info.power_down_setting = power_down_setting;
	e_ctrl->soc_info.soc_private = (void *)soc_private;
	CAM_ERR(CAM_EEPROM, "sdg++ The power-on update is successfully!");

	return 0;
error:
	set_fs(fs);
	filp_close(fp, NULL);
	return -EINVAL;
}

static ssize_t cam_eeprom_rw_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	if (strtobool(buf, &imx586_Rewrite_enable) < 0)
		return -EINVAL;

	 CAM_ERR(CAM_EEPROM, "sdg++  imx586 Rewrite enable = %d",imx586_Rewrite_enable);
	 return count;
}

static ssize_t cam_eeprom_rw_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	int rc = 0;
	//int i = 0;
	uint8_t *eeprombuf = NULL;
	uint32_t arcsoft_rw_start_addr =IMX586_ARCSOFT_START;
	static struct cam_eeprom_ctrl_t *g_ectrl = NULL;
	static struct cam_eeprom_soc_private  *s_soc_private = NULL;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	struct cam_eeprom_soc_private  *soc_private = NULL;
	bool rw_success = FALSE;

	if (!imx586_Rewrite_enable)
		goto disable_free;

	g_ectrl = vzalloc(sizeof(struct cam_eeprom_ctrl_t));
	if (!g_ectrl) {
		CAM_ERR(CAM_EEPROM, "sdg++ 11 failed xxx");
		goto disable_free;
	}
	s_soc_private = vzalloc(sizeof(struct cam_eeprom_soc_private));
	if (!s_soc_private) {
		CAM_ERR(CAM_EEPROM, "sdg++ 22 failed xxxxxx");
		vfree(g_ectrl);
		g_ectrl = NULL;
		goto disable_free;
	}

	rc = update_power_config(g_ectrl,s_soc_private);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "sdg++ The power-on update is failed!");
		vfree(g_ectrl);
		vfree(s_soc_private);
		g_ectrl = NULL;
		s_soc_private = NULL;
		goto disable_free;
	}

	soc_private = (struct cam_eeprom_soc_private *)g_ectrl->soc_info.soc_private;
	CAM_ERR(CAM_EEPROM, "sdg++ power start ");
	rc = cam_eeprom_power_up(g_ectrl,
			&soc_private->power_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "sdg++ power up failed rc %d", rc);
		goto memdata_free;
	}
	g_ectrl->cam_eeprom_state = CAM_EEPROM_CONFIG;

	eeprombuf = vzalloc(sizeof(uint8_t) * 10);
	if(!eeprombuf){
		rc = -ENOMEM;
		CAM_ERR(CAM_EEPROM, "sdg++ vzalloc failed ");
		goto memdata_free;
		}

	rc = camera_io_dev_read_seq(
		&g_ectrl->io_master_info,arcsoft_rw_start_addr,
		&eeprombuf[0], CAMERA_SENSOR_I2C_TYPE_WORD,CAMERA_SENSOR_I2C_TYPE_BYTE,1);
		if (rc)
		{
			CAM_ERR(CAM_EEPROM, "sdg++ read failed rc %d",rc);
			goto memdata_free;
		}
		msleep(5);
		rc = camera_io_dev_read_seq(
		&g_ectrl->io_master_info,IMX586_EEPROM_PROTECTION_ADDR,
		&eeprombuf[1], CAMERA_SENSOR_I2C_TYPE_WORD,CAMERA_SENSOR_I2C_TYPE_BYTE,1);
		if (rc)
		{
			CAM_ERR(CAM_EEPROM, "sdg++ read failed rc %d",rc);
			goto memdata_free;
		}
	CAM_ERR(CAM_EEPROM, "sdg++ arcsoft_rw_start_addr = 0x%x eeprombuf[0] = 0x%x",arcsoft_rw_start_addr,eeprombuf[0]);
	CAM_ERR(CAM_EEPROM, "sdg++ arcsoft_rw_start_addr = 0x%x eeprombuf[1] = 0x%x",arcsoft_rw_start_addr,eeprombuf[1]);
	CAM_ERR(CAM_EEPROM, "sdg++  imx586_Rewrite = %d",imx586_Rewrite_enable);
	msleep(5);
	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = IMX586_EEPROM_PROTECTION_ADDR;
	i2c_reg_array.reg_data = 0x10;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&g_ectrl->io_master_info, &i2c_reg_settings);
	msleep(5);

	rc = eeprom_write_dualcam_cal_data(g_ectrl,eeprombuf);
	msleep(5);

	if (!rc) {
	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = IMX586_ARCSOFT_DUAL_CHECKSUM;
	i2c_reg_array.reg_data = (eeprombuf[0])%0xFF +1;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&g_ectrl->io_master_info, &i2c_reg_settings);
	CAM_ERR(CAM_EEPROM, "sdg++ checksum = 0x%x ",((eeprombuf[0])%0xFF +1));
	msleep(5);
	rw_success = TRUE;
	}

	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = IMX586_EEPROM_PROTECTION_ADDR;
	i2c_reg_array.reg_data = 0x1e;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&g_ectrl->io_master_info, &i2c_reg_settings);
	msleep(5);
	//arcsoft_rw_start_addr += 0x7f0;
       /*
		for (i=0; i<10; i++)
		{
		rc = camera_io_dev_read_seq(
			&g_ectrl->io_master_info,arcsoft_rw_start_addr,
			&eeprombuf[i], CAMERA_SENSOR_I2C_TYPE_WORD,CAMERA_SENSOR_I2C_TYPE_BYTE,1);

		if (rc)
		{
			CAM_ERR(CAM_EEPROM, "sdg++ read failed rc %d",rc);
			return rc;
		}
			CAM_ERR(CAM_EEPROM,"sdg++ read from eeprom reg_addr 0x%x eeprombuf[%d]=0x%x",arcsoft_rw_start_addr,i,eeprombuf[i]);
			arcsoft_rw_start_addr += 1;
		}
	 */
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "sdg++ camera_io_dev_write failed rc %d", rc);
		goto memdata_free;
	}

	rc = cam_eeprom_power_down(g_ectrl);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "sdg++ power down failed rc %d", rc);
		goto memdata_free;
	}
	g_ectrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;

	vfree(eeprombuf);
	eeprombuf = NULL;
	vfree(s_soc_private->power_info.gpio_num_info);
	s_soc_private->power_info.gpio_num_info = NULL;
	vfree(s_soc_private->power_info.power_setting);
	s_soc_private->power_info.power_setting = NULL;
	vfree(s_soc_private->power_info.power_down_setting);
	s_soc_private->power_info.power_down_setting = NULL;
	vfree(s_soc_private);
	s_soc_private = NULL;
	vfree(g_ectrl);
	g_ectrl = NULL;
	imx586_Rewrite_enable = FALSE;

	if(!rc && rw_success)
		{
		sprintf(buf, "%s", "EEPROM RW OK,PLS REBOOT!!!\n");
		rc = strlen(buf) + 1;
		}
	else
		{
		sprintf(buf, "%s", "EEPROM RW FAILED,PLS REBOOT AND RETRY!!!\n");
		rc = strlen(buf) + 1;
		}
	return rc;

memdata_free:

	vfree(eeprombuf);
	eeprombuf = NULL;
	vfree(s_soc_private->power_info.gpio_num_info);
	s_soc_private->power_info.gpio_num_info = NULL;
	vfree(s_soc_private->power_info.power_setting);
	s_soc_private->power_info.power_setting = NULL;
	vfree(s_soc_private->power_info.power_down_setting);
	s_soc_private->power_info.power_down_setting = NULL;
	vfree(s_soc_private);
	s_soc_private = NULL;
	vfree(g_ectrl);
	g_ectrl = NULL;
	imx586_Rewrite_enable = FALSE;
	sprintf(buf, "%s", "EEPROM RW FAILED,PLS REBOOT AND RETRY!!!\n");
	rc = strlen(buf) + 1;

	return rc;
disable_free:
	sprintf(buf, "%s", "Calibration is not enabled Or other questions!!!\n");
	rc = strlen(buf) + 1;
	return rc;
}

static DEVICE_ATTR(eepromrw, 0644, cam_eeprom_rw_show, cam_eeprom_rw_state);
int32_t msm_eepromrw_init_device_name(void)
{
	int32_t rc = 0;
	CAM_ERR(CAM_EEPROM,"%s %d\n");
	if(msm_eepromrw_device != NULL){
		CAM_ERR(CAM_EEPROM,"sdg++ Macle android_camera already created");
		return 0;
	}
	msm_eepromrw_device = kobject_create_and_add("camera_eepromrw", NULL);
	if (msm_eepromrw_device == NULL) {
		CAM_ERR(CAM_EEPROM,"sdg++ subsystem_register failed");
		rc = -ENOMEM;
		return rc ;
	}
	rc = sysfs_create_file(msm_eepromrw_device, &dev_attr_eepromrw.attr);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"sdg++ sysfs_create_file failed");
		kobject_del(msm_eepromrw_device);
	}
	return 0 ;
}
#endif
///end sdg

/**
 * cam_eeprom_pkt_parse - Parse csl packet
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_pkt_parse(struct cam_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t                         rc = 0;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	uintptr_t                        generic_pkt_addr;
	size_t                          pkt_len;
	struct cam_packet              *csl_packet = NULL;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user(&dev_config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(dev_config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(dev_config.packet_handle,
		&generic_pkt_addr, &pkt_len);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"error in converting command Handle Error: %d", rc);
		return rc;
	}

	if (dev_config.offset > pkt_len) {
		CAM_ERR(CAM_EEPROM,
			"Offset is out of bound: off: %lld, %zu",
			dev_config.offset, pkt_len);
		rc = -EINVAL;
		goto release_buf;
	}

	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_EEPROM_PACKET_OPCODE_INIT:
		if (e_ctrl->userspace_probe == false) {
			rc = cam_eeprom_parse_read_memory_map(
					e_ctrl->soc_info.dev->of_node, e_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "Failed: rc : %d", rc);
				goto release_buf;
			}
			rc = cam_eeprom_get_cal_data(e_ctrl, csl_packet);
			vfree(e_ctrl->cal_data.mapdata);
			vfree(e_ctrl->cal_data.map);
			e_ctrl->cal_data.num_data = 0;
			e_ctrl->cal_data.num_map = 0;
			CAM_DBG(CAM_EEPROM,
				"Returning the data using kernel probe");
			break;
		}
		rc = cam_eeprom_init_pkt_parser(e_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_EEPROM,
				"Failed in parsing the pkt");
			goto release_buf;
		}

		e_ctrl->cal_data.mapdata =
			vzalloc(e_ctrl->cal_data.num_data);
		if (!e_ctrl->cal_data.mapdata) {
			rc = -ENOMEM;
			CAM_ERR(CAM_EEPROM, "failed");
			goto error;
		}

		//sdg add arcsoft Calibration 2018.12.07
		#ifdef LC_ARCSOFT_CALIBRATION
		if((soc_private->i2c_info.slave_addr==0xA2) && (!imx586_power_save)){
			cam_eeprom_rw_power_save(e_ctrl,soc_private);
			imx586_power_save = true;
		}
		#endif
		///end sdg

		rc = cam_eeprom_power_up(e_ctrl,
			&soc_private->power_info);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "failed rc %d", rc);
			goto memdata_free;
		}

		e_ctrl->cam_eeprom_state = CAM_EEPROM_CONFIG;
		rc = cam_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc) {
			CAM_ERR(CAM_EEPROM,
				"read_eeprom_memory failed");
			goto power_down;
		}

		rc = cam_eeprom_get_cal_data(e_ctrl, csl_packet);
		rc = cam_eeprom_power_down(e_ctrl);
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
		vfree(e_ctrl->cal_data.mapdata);
		vfree(e_ctrl->cal_data.map);
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_setting_size = 0;
		power_info->power_down_setting_size = 0;
		e_ctrl->cal_data.num_data = 0;
		e_ctrl->cal_data.num_map = 0;
		break;
	default:
		break;
	}

	if (cam_mem_put_cpu_buf(dev_config.packet_handle))
		CAM_WARN(CAM_EEPROM, "Put cpu buffer failed : 0x%x",
			dev_config.packet_handle);

	return rc;

power_down:
	cam_eeprom_power_down(e_ctrl);
memdata_free:
	vfree(e_ctrl->cal_data.mapdata);
error:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	vfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
	e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
release_buf:
	if (cam_mem_put_cpu_buf(dev_config.packet_handle))
		CAM_WARN(CAM_EEPROM, "Put cpu buffer failed : 0x%x",
			dev_config.packet_handle);

	return rc;
}

void cam_eeprom_shutdown(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int rc;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_INIT)
		return;

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_CONFIG) {
		rc = cam_eeprom_power_down(e_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM, "EEPROM Power down failed");
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	}

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_ACQUIRE) {
		rc = cam_destroy_device_hdl(e_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM, "destroying the device hdl");

		e_ctrl->bridge_intf.device_hdl = -1;
		e_ctrl->bridge_intf.link_hdl = -1;
		e_ctrl->bridge_intf.session_hdl = -1;

		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_setting_size = 0;
		power_info->power_down_setting_size = 0;
	}

	e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
}

/**
 * cam_eeprom_driver_cmd - Handle eeprom cmds
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int32_t cam_eeprom_driver_cmd(struct cam_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int                            rc = 0;
	struct cam_eeprom_query_cap_t  eeprom_cap = {0};
	struct cam_control            *cmd = (struct cam_control *)arg;

	if (!e_ctrl || !cmd) {
		CAM_ERR(CAM_EEPROM, "Invalid Arguments");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_EEPROM, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	mutex_lock(&(e_ctrl->eeprom_mutex));
	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		eeprom_cap.slot_info = e_ctrl->soc_info.index;
		if (e_ctrl->userspace_probe == false)
			eeprom_cap.eeprom_kernel_probe = true;
		else
			eeprom_cap.eeprom_kernel_probe = false;

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&eeprom_cap,
			sizeof(struct cam_eeprom_query_cap_t))) {
			CAM_ERR(CAM_EEPROM, "Failed Copy to User");
			return -EFAULT;
			goto release_mutex;
		}
		CAM_DBG(CAM_EEPROM, "eeprom_cap: ID: %d", eeprom_cap.slot_info);
		break;
	case CAM_ACQUIRE_DEV:
		rc = cam_eeprom_get_dev_handle(e_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed to acquire dev");
			goto release_mutex;
		}
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
		break;
	case CAM_RELEASE_DEV:
		if (e_ctrl->cam_eeprom_state != CAM_EEPROM_ACQUIRE) {
			rc = -EINVAL;
			CAM_WARN(CAM_EEPROM,
			"Not in right state to release : %d",
			e_ctrl->cam_eeprom_state);
			goto release_mutex;
		}

		if (e_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_EEPROM,
				"Invalid Handles: link hdl: %d device hdl: %d",
				e_ctrl->bridge_intf.device_hdl,
				e_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(e_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM,
				"failed in destroying the device hdl");
		e_ctrl->bridge_intf.device_hdl = -1;
		e_ctrl->bridge_intf.link_hdl = -1;
		e_ctrl->bridge_intf.session_hdl = -1;
		e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
		break;
	case CAM_CONFIG_DEV:
		//sdg add arcsoft Calibration 2018.12.07
		#ifdef LC_ARCSOFT_CALIBRATION
		msm_eepromrw_init_device_name();
		#endif
		///end sdg
		rc = cam_eeprom_pkt_parse(e_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed in eeprom pkt Parsing");
			goto release_mutex;
		}
		break;
	default:
		CAM_DBG(CAM_EEPROM, "invalid opcode");
		break;
	}

release_mutex:
	mutex_unlock(&(e_ctrl->eeprom_mutex));

	return rc;
}

