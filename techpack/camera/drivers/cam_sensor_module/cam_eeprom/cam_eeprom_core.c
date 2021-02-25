// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_eeprom_core.h"
#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

#define MAX_READ_SIZE  0x7FFFF
#define MAX_RETRY_TIMES 3

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
			if (rc < 0) {
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
			goto cci_failure;
		}
	}
	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_ERR(CAM_EEPROM, "Power down failure");

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
	bridge_params.dev_id = CAM_EEPROM;

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
	void *cmd_buf, int cmd_length, uint32_t *cmd_length_bytes,
	int *num_map, size_t remain_buf_len)
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
	size_t                             validate_size = 0;

	generic_op_code = cmm_hdr->fifth_byte;

	if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR)
		validate_size = sizeof(struct cam_cmd_i2c_random_wr);
	else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD)
		validate_size = sizeof(struct cam_cmd_i2c_continuous_rd);
	else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_WAIT)
		validate_size = sizeof(struct cam_cmd_unconditional_wait);

	if (remain_buf_len < validate_size ||
	    *num_map >= (MSM_EEPROM_MAX_MEM_MAP_CNT *
		MSM_EEPROM_MEMORY_MAP_MAX_SIZE)) {
		CAM_ERR(CAM_EEPROM, "not enough buffer");
		return -EINVAL;
	}
	switch (cmm_hdr->cmd_type) {
	case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
		i2c_random_wr = (struct cam_cmd_i2c_random_wr *)cmd_buf;

		if (i2c_random_wr->header.count == 0 ||
		    i2c_random_wr->header.count >= MSM_EEPROM_MAX_MEM_MAP_CNT ||
		    (size_t)*num_map >= ((MSM_EEPROM_MAX_MEM_MAP_CNT *
				MSM_EEPROM_MEMORY_MAP_MAX_SIZE) -
				i2c_random_wr->header.count)) {
			CAM_ERR(CAM_EEPROM, "OOB Error");
			return -EINVAL;
		}
		cmd_length_in_bytes   = sizeof(struct cam_cmd_i2c_random_wr) +
			((i2c_random_wr->header.count - 1) *
			sizeof(struct i2c_random_wr_payload));

		if (cmd_length_in_bytes > remain_buf_len) {
			CAM_ERR(CAM_EEPROM, "Not enough buffer remaining");
			return -EINVAL;
		}
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

		if (i2c_cont_rd->header.count >= U32_MAX - data->num_data) {
			CAM_ERR(CAM_EEPROM,
				"int overflow on eeprom memory block");
			return -EINVAL;
		}
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

static struct i2c_settings_list*
	cam_eeprom_get_i2c_ptr(struct i2c_settings_array *i2c_reg_settings,
		uint32_t size)
{
	struct i2c_settings_list *tmp;

	tmp = kzalloc(sizeof(struct i2c_settings_list), GFP_KERNEL);

	if (tmp != NULL)
		list_add_tail(&(tmp->list),
			&(i2c_reg_settings->list_head));
	else
		return NULL;

	tmp->seq_settings.reg_data =
		kcalloc(size, sizeof(uint8_t), GFP_KERNEL);
	if (tmp->seq_settings.reg_data == NULL) {
		list_del(&(tmp->list));
		kfree(tmp);
		tmp = NULL;
		return NULL;
	}
	tmp->seq_settings.size = size;

	return tmp;
}

static int32_t cam_eeprom_handle_continuous_write(
	struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_cmd_i2c_continuous_wr *cam_cmd_i2c_continuous_wr,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0, cnt = 0;


	CAM_DBG(CAM_EEPROM, "Total Size: %d",
		cam_cmd_i2c_continuous_wr->header.count);

	i2c_list = cam_eeprom_get_i2c_ptr(i2c_reg_settings,
		cam_cmd_i2c_continuous_wr->header.count);
	if (i2c_list == NULL ||
		i2c_list->seq_settings.reg_data == NULL) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating i2c_list");
		return -ENOMEM;
	}

	*cmd_length_in_bytes = (sizeof(struct i2c_rdwr_header) +
		sizeof(cam_cmd_i2c_continuous_wr->reg_addr) +
		sizeof(struct cam_cmd_read) *
		(cam_cmd_i2c_continuous_wr->header.count));
	if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_BRST)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_BURST;
	else if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_SEQN)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_SEQ;
	else {
		rc = -EINVAL;
		goto deallocate_i2c_list;
	}

	i2c_list->seq_settings.addr_type =
		cam_cmd_i2c_continuous_wr->header.addr_type;

	CAM_ERR(CAM_EEPROM, "Write Address: 0x%x",
		cam_cmd_i2c_continuous_wr->reg_addr);
	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_RANDOM;
		e_ctrl->eebin_info.start_address =
			cam_cmd_i2c_continuous_wr->reg_addr;
		e_ctrl->eebin_info.size =
			cam_cmd_i2c_continuous_wr->header.count;
		CAM_DBG(CAM_EEPROM, "Header Count: %d",
			cam_cmd_i2c_continuous_wr->header.count);
		e_ctrl->eebin_info.is_valid = 1;

		i2c_list->seq_settings.reg_addr =
			cam_cmd_i2c_continuous_wr->reg_addr;
	} else
		CAM_ERR(CAM_EEPROM, "Burst Mode Not Supported\n");

	(*offset) += cnt;
	*list = &(i2c_list->list);
	return rc;
deallocate_i2c_list:
	kfree(i2c_list);
	return rc;
}

static int32_t cam_eeprom_handle_delay(
	uint32_t **cmd_buf,
	uint16_t generic_op_code,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t offset, uint32_t *byte_cnt,
	struct list_head *list_ptr,
	size_t remain_buf_len)
{
	int32_t rc = 0;
	struct i2c_settings_list *i2c_list = NULL;
	struct cam_cmd_unconditional_wait *cmd_uncond_wait =
		(struct cam_cmd_unconditional_wait *) *cmd_buf;

	if (remain_buf_len < (sizeof(struct cam_cmd_unconditional_wait))) {
		CAM_ERR(CAM_EEPROM, "Not Enough buffer");
		return -EINVAL;
	}

	if (list_ptr == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid list ptr");
		return -EINVAL;
	}

	if (offset > 0) {
		i2c_list =
			list_entry(list_ptr, struct i2c_settings_list, list);
		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND)
			i2c_list->i2c_settings.reg_setting[offset - 1].delay =
				cmd_uncond_wait->delay;
		else
			i2c_list->i2c_settings.delay =
				cmd_uncond_wait->delay;
		(*cmd_buf) +=
			sizeof(
			struct cam_cmd_unconditional_wait) / sizeof(uint32_t);
		(*byte_cnt) +=
			sizeof(
			struct cam_cmd_unconditional_wait);
	} else {
		CAM_ERR(CAM_SENSOR, "Delay Rxed before any buffer: %d", offset);
		return -EINVAL;
	}

	return rc;
}

/**
 * cam_eeprom_parse_write_memory_packet - Write eeprom packet
 * @csl_packet:   csl packet received
 * @e_ctrl:       ctrl structure
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_parse_write_memory_packet(
	struct cam_packet *csl_packet,
	struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_cmd_buf_desc        *cmd_desc = NULL;
	uint32_t                       *offset = NULL;
	int32_t                         i, rc = 0;
	uint32_t                       *cmd_buf = NULL;
	uintptr_t                       generic_pkt_addr;
	size_t                          pkt_len = 0;
	size_t                          remain_len = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	uint32_t                        processed_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uint32_t                        cmd_length_in_bytes = 0;
	struct cam_cmd_i2c_info        *i2c_info = NULL;


	offset = (uint32_t *)&csl_packet->payload;
	offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	CAM_DBG(CAM_EEPROM, "Number of Command Buffers: %d",
		csl_packet->num_cmd_buf);
	for (i = 0; i < csl_packet->num_cmd_buf; i++) {
		struct list_head               *list = NULL;
		uint16_t                       generic_op_code;
		uint32_t                       off = 0;
		int                            master;
		struct cam_sensor_cci_client   *cci;

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
			goto end;
		}

		if ((pkt_len < sizeof(struct common_header) ||
			(cmd_desc[i].offset) > (pkt_len -
			sizeof(struct common_header)))) {
			CAM_ERR(CAM_EEPROM, "Not enough buffer");
			rc = -EINVAL;
			goto end;
		}

		remain_len = pkt_len - cmd_desc[i].offset;
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);

		if (total_cmd_buf_in_bytes > remain_len) {
			CAM_ERR(CAM_EEPROM, "Not enough buffer for command");
			rc = -EINVAL;
			goto end;
		}

		master = e_ctrl->io_master_info.master_type;
		cci = e_ctrl->io_master_info.cci_client;
		while (processed_cmd_buf_in_bytes < total_cmd_buf_in_bytes) {
			if ((remain_len - processed_cmd_buf_in_bytes) <
				sizeof(struct common_header)) {
				CAM_ERR(CAM_EEPROM, "Not Enough buffer");
				rc = -EINVAL;
				goto end;
			}
			cmm_hdr = (struct common_header *)cmd_buf;
			generic_op_code = cmm_hdr->fifth_byte;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
				if ((remain_len - processed_cmd_buf_in_bytes) <
					sizeof(struct cam_cmd_i2c_info)) {
					CAM_ERR(CAM_EEPROM, "Not enough buf");
					rc = -EINVAL;
					goto end;
				}
				if (master == CCI_MASTER) {
					cci->cci_i2c_master =
						e_ctrl->cci_i2c_master;
					cci->i2c_freq_mode =
						i2c_info->i2c_freq_mode;
					cci->sid =
						i2c_info->slave_addr >> 1;
					CAM_DBG(CAM_EEPROM,
						"Slave addr: 0x%x Freq Mode: %d",
						i2c_info->slave_addr,
						i2c_info->i2c_freq_mode);
				} else if (master == I2C_MASTER) {
					e_ctrl->io_master_info.client->addr =
						i2c_info->slave_addr;
					CAM_DBG(CAM_EEPROM,
						"Slave addr: 0x%x",
						i2c_info->slave_addr);
				} else if (master == SPI_MASTER) {
					CAM_ERR(CAM_EEPROM,
						"No Need of Slave Info");
				} else {
					CAM_ERR(CAM_EEPROM,
						"Invalid Master type: %d",
						master);
					rc = -EINVAL;
					goto end;
				}
				cmd_length_in_bytes =
					sizeof(struct cam_cmd_i2c_info);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/4;
				break;
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_WR: {
				struct cam_cmd_i2c_continuous_wr
					*cam_cmd_i2c_continuous_wr =
					(struct cam_cmd_i2c_continuous_wr *)
					cmd_buf;
				if ((remain_len - processed_cmd_buf_in_bytes) <
				sizeof(struct cam_cmd_i2c_continuous_wr)) {
					CAM_ERR(CAM_EEPROM, "Not enough buf");
					rc = -EINVAL;
					goto end;
				}

				CAM_DBG(CAM_EEPROM,
					"CAMERA_SENSOR_CMD_TYPE_I2C_CONT_WR");
				rc = cam_eeprom_handle_continuous_write(
					e_ctrl,
					cam_cmd_i2c_continuous_wr,
					&(e_ctrl->wr_settings),
					&cmd_length_in_bytes, &off, &list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in continuous write %d", rc);
					goto end;
				}

				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_WAIT: {
				CAM_DBG(CAM_EEPROM,
					"CAMERA_SENSOR_CMD_TYPE_WAIT");
				if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_HW_UCND ||
					generic_op_code ==
						CAMERA_SENSOR_WAIT_OP_SW_UCND) {

					rc = cam_eeprom_handle_delay(
						&cmd_buf, generic_op_code,
						&(e_ctrl->wr_settings), off,
						&cmd_length_in_bytes,
						list, (remain_len -
						processed_cmd_buf_in_bytes));
					if (rc < 0) {
						CAM_ERR(CAM_EEPROM,
							"delay hdl failed: %d",
							rc);
						goto end;
					}
					processed_cmd_buf_in_bytes +=
						cmd_length_in_bytes;
					cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				} else {
					CAM_ERR(CAM_EEPROM,
						"Wrong Wait Command: %d",
						generic_op_code);
					rc = -EINVAL;
					goto end;
				}
				break;
			}
			default:
				CAM_ERR(CAM_EEPROM,
					"Invalid Cmd_type rxed: %d\n",
					cmm_hdr->cmd_type);
				rc = -EINVAL;
				break;
			}
		}
	}

end:
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
	size_t                          remain_len = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	uint32_t                        processed_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uint32_t                        cmd_length_in_bytes = 0;
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
			goto end;
		}

		if ((pkt_len < sizeof(struct common_header)) ||
			(cmd_desc[i].offset > (pkt_len -
			sizeof(struct common_header)))) {
			CAM_ERR(CAM_EEPROM, "Not enough buffer");
			rc = -EINVAL;
			goto end;
		}
		remain_len = pkt_len - cmd_desc[i].offset;
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);

		if (total_cmd_buf_in_bytes > remain_len) {
			CAM_ERR(CAM_EEPROM, "Not enough buffer for command");
			rc = -EINVAL;
			goto end;
		}
		/* Loop through multiple cmd formats in one cmd buffer */
		while (processed_cmd_buf_in_bytes < total_cmd_buf_in_bytes) {
			if ((remain_len - processed_cmd_buf_in_bytes) <
				sizeof(struct common_header)) {
				CAM_ERR(CAM_EEPROM, "Not enough buf");
				rc = -EINVAL;
				goto end;
			}
			cmm_hdr = (struct common_header *)cmd_buf;
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
				if ((remain_len - processed_cmd_buf_in_bytes) <
					sizeof(struct cam_cmd_i2c_info)) {
					CAM_ERR(CAM_EEPROM, "Not enough buf");
					rc = -EINVAL;
					goto end;
				}
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
					cmd_length_in_bytes, power_info,
					(remain_len -
					processed_cmd_buf_in_bytes));
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/
					sizeof(uint32_t);
				if (rc) {
					CAM_ERR(CAM_EEPROM, "Failed");
					goto end;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD:
			case CAMERA_SENSOR_CMD_TYPE_WAIT:
				num_map++;
				rc = cam_eeprom_parse_memory_map(
					&e_ctrl->cal_data, cmd_buf,
					total_cmd_buf_in_bytes,
					&cmd_length_in_bytes, &num_map,
					(remain_len -
					processed_cmd_buf_in_bytes));
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/sizeof(uint32_t);
				break;
			default:
				CAM_ERR(CAM_EEPROM, "Invalid cmd_type 0x%x",
					cmm_hdr->cmd_type);
				rc = -EINVAL;
				goto end;
			}
		}
		e_ctrl->cal_data.num_map = num_map + 1;
	}

end:
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
	size_t                remain_len = 0;

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
			if (buf_size <= io_cfg->offsets[0]) {
				CAM_ERR(CAM_EEPROM, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}

			remain_len = buf_size - io_cfg->offsets[0];
			CAM_DBG(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu\n",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				rc = -EINVAL;
				return rc;
			}
			read_buffer += io_cfg->offsets[0];

			if (remain_len < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				rc = -EINVAL;
				return rc;
			}

			CAM_DBG(CAM_EEPROM, "copy the data, len:%d",
				e_ctrl->cal_data.num_data);
			memcpy(read_buffer, e_ctrl->cal_data.mapdata,
					e_ctrl->cal_data.num_data);
		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}

	return rc;
}

static int32_t delete_eeprom_request(struct i2c_settings_array *i2c_array)
{
	struct i2c_settings_list *i2c_list = NULL, *i2c_next = NULL;
	int32_t rc = 0;

	if (i2c_array == NULL) {
		CAM_ERR(CAM_SENSOR, "FATAL:: Invalid argument");
		return -EINVAL;
	}

	list_for_each_entry_safe(i2c_list, i2c_next,
		&(i2c_array->list_head), list) {
		kfree(i2c_list->seq_settings.reg_data);
		list_del(&(i2c_list->list));
		kfree(i2c_list);
	}
	INIT_LIST_HEAD(&(i2c_array->list_head));
	i2c_array->is_settings_valid = 0;

	return rc;
}

/**
 * cam_eeprom_write - Write Packet
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_write(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int32_t rc = 0;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list = NULL;

	i2c_set = &e_ctrl->wr_settings;

	if (i2c_set->is_settings_valid == 1) {
		list_for_each_entry(i2c_list,
			&(i2c_set->list_head), list) {
			rc = camera_io_dev_write_continuous(
				&e_ctrl->io_master_info,
				&i2c_list->i2c_settings, 1);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM,
				"Error in EEPROM write");
			goto del_req;
		}
		}
	}

del_req:
	delete_eeprom_request(i2c_set);
	return rc;
}

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
	int32_t                         i = 0;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	uintptr_t                        generic_pkt_addr;
	size_t                          pkt_len;
	size_t                          remain_len = 0;
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

	remain_len = pkt_len;
	if ((sizeof(struct cam_packet) > pkt_len) ||
		((size_t)dev_config.offset >= pkt_len -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_EEPROM,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), pkt_len);
		rc = -EINVAL;
		return rc;
	}

	remain_len -= (size_t)dev_config.offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_EEPROM, "Invalid packet params");
		rc = -EINVAL;
		return rc;
	}

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_EEPROM_PACKET_OPCODE_INIT:
		if (e_ctrl->userspace_probe == false) {
			rc = cam_eeprom_parse_read_memory_map(
					e_ctrl->soc_info.dev->of_node, e_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "Failed: rc : %d", rc);
				return rc;
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
			return rc;
		}

		e_ctrl->cal_data.mapdata =
			vzalloc(e_ctrl->cal_data.num_data);
		if (!e_ctrl->cal_data.mapdata) {
			rc = -ENOMEM;
			CAM_ERR(CAM_EEPROM, "failed");
			goto error;
		}

		if (e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE) {
			rc = cam_eeprom_match_id(e_ctrl);
			if (rc) {
				CAM_DBG(CAM_EEPROM,
					"eeprom not matching %d", rc);
				goto memdata_free;
			}
		}

		/* xiaomi add eeprom read retry - begin */
		for (i = 0; i < MAX_RETRY_TIMES; i++) {
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
					"read_eeprom_memory failed at time %d", i);
				//goto power_down;
				rc = cam_eeprom_power_down(e_ctrl);
				usleep_range(10*1000, 11*1000);
			} else {
				rc = cam_eeprom_get_cal_data(e_ctrl, csl_packet);
				rc = cam_eeprom_power_down(e_ctrl);
				break;
			}
		}

		if (rc) {
			CAM_ERR(CAM_EEPROM,
				"read_eeprom_memory failed all the %d times",
				 MAX_RETRY_TIMES);
		}
		/* xiaomi add eeprom read retry - end */

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
	case CAM_EEPROM_WRITE: {
		struct i2c_settings_array *i2c_reg_settings =
			&e_ctrl->wr_settings;

		i2c_reg_settings->is_settings_valid = 1;
		rc = cam_eeprom_parse_write_memory_packet(
			csl_packet, e_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "Failed: rc : %d", rc);
			return rc;
		}

		rc = cam_eeprom_power_up(e_ctrl,
			&soc_private->power_info);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "failed power up rc %d", rc);
			goto memdata_free;
		}

		usleep_range(10*1000, 11*1000);
		CAM_DBG(CAM_EEPROM,
			"Calling Erase : %d start Address: 0x%x size: %d",
			rc, e_ctrl->eebin_info.start_address,
			e_ctrl->eebin_info.size);

		rc = camera_io_dev_erase(&e_ctrl->io_master_info,
			e_ctrl->eebin_info.start_address,
			e_ctrl->eebin_info.size);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "Failed in erase : %d", rc);
			return rc;
		}

		/* Buffer time margin */
		usleep_range(10*1000, 11*1000);

		rc = cam_eeprom_write(e_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "Failed: rc : %d", rc);
			return rc;
		}

		rc = cam_eeprom_power_down(e_ctrl);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "failed power down rc %d", rc);
			goto memdata_free;
		}

		break;
	}
	default:
		CAM_ERR(CAM_EEPROM, "Invalid op-code 0x%x",
			csl_packet->header.op_code & 0xFFFFFF);
		rc = -EINVAL;
		break;
	}

	return rc;
//power_down:
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
	e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
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

		eeprom_cap.is_multimodule_mode =
			e_ctrl->is_multimodule_mode;

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&eeprom_cap,
			sizeof(struct cam_eeprom_query_cap_t))) {
			CAM_ERR(CAM_EEPROM, "Failed Copy to User");
			rc = -EFAULT;
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
