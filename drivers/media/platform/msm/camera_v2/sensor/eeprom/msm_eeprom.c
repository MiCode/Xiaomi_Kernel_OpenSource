/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include "msm_sd.h"
#include "msm_cci.h"
#include "msm_eeprom.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(msm_eeprom_mutex);
#ifdef CONFIG_COMPAT
static struct v4l2_file_operations msm_eeprom_v4l2_subdev_fops;
#endif

/*
 * msm_get_read_mem_size - Get the total size for allocation
 * @eeprom_map_array:	mem map
 *
 * Returns size after computation size, returns error in case of error
 */
static int msm_get_read_mem_size
	(struct msm_eeprom_memory_map_array *eeprom_map_array)
{
	int size = 0, i, j;
	struct msm_eeprom_mem_map_t *eeprom_map;

	if (eeprom_map_array->msm_size_of_max_mappings >
		MSM_EEPROM_MAX_MEM_MAP_CNT) {
		pr_err("%s:%d Memory map cnt greter then expected: %d",
			__func__, __LINE__,
			eeprom_map_array->msm_size_of_max_mappings);
		return -EINVAL;
	}
	for (j = 0; j < eeprom_map_array->msm_size_of_max_mappings; j++) {
		eeprom_map = &(eeprom_map_array->memory_map[j]);
		if (eeprom_map->memory_map_size >
			MSM_EEPROM_MEMORY_MAP_MAX_SIZE) {
			pr_err("%s:%d Memory map size greter then expected: %d",
				__func__, __LINE__,
				eeprom_map->memory_map_size);
			return -EINVAL;
		}
		for (i = 0; i < eeprom_map->memory_map_size; i++) {
			if (eeprom_map->mem_settings[i].i2c_operation ==
				MSM_CAM_READ) {
				size += eeprom_map->mem_settings[i].reg_data;
			}
		}
	}
	CDBG("Total Data Size: %d\n", size);
	return size;
}

/*
 * msm_eeprom_verify_sum - verify crc32 checksum
 * @mem:	data buffer
 * @size:	size of data buffer
 * @sum:	expected checksum
 *
 * Returns 0 if checksum match, -EINVAL otherwise.
 */
static int msm_eeprom_verify_sum(const char *mem, uint32_t size, uint32_t sum)
{
	uint32_t crc = ~0;

	/* check overflow */
	if (size > crc - sizeof(uint32_t))
		return -EINVAL;

	crc = crc32_le(crc, mem, size);
	if (~crc != sum) {
		CDBG("%s: expect 0x%x, result 0x%x\n", __func__, sum, ~crc);
		return -EINVAL;
	}
	CDBG("%s: checksum pass 0x%x\n", __func__, sum);
	return 0;
}

/*
 * msm_eeprom_match_crc - verify multiple regions using crc
 * @data:	data block to be verified
 *
 * Iterates through all regions stored in @data.  Regions with odd index
 * are treated as data, and its next region is treated as checksum.  Thus
 * regions of even index must have valid_size of 4 or 0 (skip verification).
 * Returns a bitmask of verified regions, starting from LSB.  1 indicates
 * a checksum match, while 0 indicates checksum mismatch or not verified.
 */
static uint32_t msm_eeprom_match_crc(struct msm_eeprom_memory_block_t *data)
{
	int j, rc;
	uint32_t *sum;
	uint32_t ret = 0;
	uint8_t *memptr;
	struct msm_eeprom_memory_map_t *map;

	if (!data) {
		pr_err("%s data is NULL", __func__);
		return -EINVAL;
	}
	map = data->map;
	memptr = data->mapdata;

	for (j = 0; j + 1 < data->num_map; j += 2) {
		/* empty table or no checksum */
		if (!map[j].mem.valid_size || !map[j+1].mem.valid_size) {
			memptr += map[j].mem.valid_size
				+ map[j+1].mem.valid_size;
			continue;
		}
		if (map[j+1].mem.valid_size != sizeof(uint32_t)) {
			CDBG("%s: malformatted data mapping\n", __func__);
			return -EINVAL;
		}
		sum = (uint32_t *) (memptr + map[j].mem.valid_size);
		rc = msm_eeprom_verify_sum(memptr, map[j].mem.valid_size,
					   *sum);
		if (!rc)
			ret |= 1 << (j/2);
		memptr += map[j].mem.valid_size + map[j+1].mem.valid_size;
	}
	return ret;
}

/*
 * read_eeprom_memory() - read map data into buffer
 * @e_ctrl:	eeprom control struct
 * @block:	block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */
static int read_eeprom_memory(struct msm_eeprom_ctrl_t *e_ctrl,
	struct msm_eeprom_memory_block_t *block)
{
	int rc = 0;
	int j;
	struct msm_eeprom_memory_map_t *emap = block->map;
	struct msm_eeprom_board_info *eb_info;
	uint8_t *memptr = block->mapdata;

	if (!e_ctrl) {
		pr_err("%s e_ctrl is NULL", __func__);
		return -EINVAL;
	}

	eb_info = e_ctrl->eboard_info;

	for (j = 0; j < block->num_map; j++) {
		if (emap[j].saddr.addr) {
			eb_info->i2c_slaveaddr = emap[j].saddr.addr;
			e_ctrl->i2c_client.cci_client->sid =
					eb_info->i2c_slaveaddr >> 1;
			pr_err("qcom,slave-addr = 0x%X\n",
				eb_info->i2c_slaveaddr);
		}

		if (emap[j].page.valid_size) {
			e_ctrl->i2c_client.addr_type = emap[j].page.addr_t;
			rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&(e_ctrl->i2c_client), emap[j].page.addr,
				emap[j].page.data, emap[j].page.data_t);
				msleep(emap[j].page.delay);
			if (rc < 0) {
				pr_err("%s: page write failed\n", __func__);
				return rc;
			}
		}
		if (emap[j].pageen.valid_size) {
			e_ctrl->i2c_client.addr_type = emap[j].pageen.addr_t;
			rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&(e_ctrl->i2c_client), emap[j].pageen.addr,
				emap[j].pageen.data, emap[j].pageen.data_t);
				msleep(emap[j].pageen.delay);
			if (rc < 0) {
				pr_err("%s: page enable failed\n", __func__);
				return rc;
			}
		}
		if (emap[j].poll.valid_size) {
			e_ctrl->i2c_client.addr_type = emap[j].poll.addr_t;
			rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
				&(e_ctrl->i2c_client), emap[j].poll.addr,
				emap[j].poll.data, emap[j].poll.data_t,
				emap[j].poll.delay);
			if (rc < 0) {
				pr_err("%s: poll failed\n", __func__);
				return rc;
			}
		}

		if (emap[j].mem.valid_size) {
			e_ctrl->i2c_client.addr_type = emap[j].mem.addr_t;
			rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
				&(e_ctrl->i2c_client), emap[j].mem.addr,
				memptr, emap[j].mem.valid_size);
			if (rc < 0) {
				pr_err("%s: read failed\n", __func__);
				return rc;
			}
			memptr += emap[j].mem.valid_size;
		}
		if (emap[j].pageen.valid_size) {
			e_ctrl->i2c_client.addr_type = emap[j].pageen.addr_t;
			rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&(e_ctrl->i2c_client), emap[j].pageen.addr,
				0, emap[j].pageen.data_t);
			if (rc < 0) {
				pr_err("%s: page disable failed\n", __func__);
				return rc;
			}
		}
	}
	return rc;
}
/*
 * msm_eeprom_parse_memory_map() - parse memory map in device node
 * @of:	device node
 * @data:	memory block for output
 *
 * This functions parses @of to fill @data.  It allocates map itself, parses
 * the @of node, calculate total data length, and allocates required buffer.
 * It only fills the map, but does not perform actual reading.
 */
static int msm_eeprom_parse_memory_map(struct device_node *of,
	struct msm_eeprom_memory_block_t *data)
{
	int i, rc = 0;
	char property[PROPERTY_MAXSIZE];
	uint32_t count = 6;
	struct msm_eeprom_memory_map_t *map;

	snprintf(property, PROPERTY_MAXSIZE, "qcom,num-blocks");
	rc = of_property_read_u32(of, property, &data->num_map);
	CDBG("%s: %s %d\n", __func__, property, data->num_map);
	if (rc < 0) {
		pr_err("%s failed rc %d\n", __func__, rc);
		return rc;
	}

	map = kzalloc((sizeof(*map) * data->num_map), GFP_KERNEL);
	if (!map) {
		rc = -ENOMEM;
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}
	data->map = map;

	for (i = 0; i < data->num_map; i++) {
		snprintf(property, PROPERTY_MAXSIZE, "qcom,page%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].page, count);
		if (rc < 0) {
			pr_err("%s: failed %d\n", __func__, __LINE__);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE,
			"qcom,pageen%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].pageen, count);
		if (rc < 0)
			CDBG("%s: pageen not needed\n", __func__);

		snprintf(property, PROPERTY_MAXSIZE, "qcom,saddr%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].saddr.addr, 1);
		if (rc < 0)
			CDBG("%s: saddr not needed - block %d\n", __func__, i);

		snprintf(property, PROPERTY_MAXSIZE, "qcom,poll%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].poll, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "qcom,mem%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].mem, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		data->num_data += map[i].mem.valid_size;
	}

	CDBG("%s num_bytes %d\n", __func__, data->num_data);

	data->mapdata = kzalloc(data->num_data, GFP_KERNEL);
	if (!data->mapdata) {
		rc = -ENOMEM;
		pr_err("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}
	return rc;

ERROR:
	kfree(data->map);
	memset(data, 0, sizeof(*data));
	return rc;
}

/*
 * eeprom_parse_memory_map - Parse mem map
 * @e_ctrl:	ctrl structure
 * @eeprom_map_array: eeprom map
 *
 * Returns success or failure
 */
static int eeprom_parse_memory_map(struct msm_eeprom_ctrl_t *e_ctrl,
	struct msm_eeprom_memory_map_array *eeprom_map_array)
{
	int rc =  0, i, j;
	uint8_t *memptr;
	struct msm_eeprom_mem_map_t *eeprom_map;

	e_ctrl->cal_data.mapdata = NULL;
	e_ctrl->cal_data.num_data = msm_get_read_mem_size(eeprom_map_array);
	if (e_ctrl->cal_data.num_data <= 0) {
		pr_err("%s:%d Error in reading mem size\n",
			__func__, __LINE__);
		e_ctrl->cal_data.num_data = 0;
		return -EINVAL;
	}
	e_ctrl->cal_data.mapdata =
		kzalloc(e_ctrl->cal_data.num_data, GFP_KERNEL);
	if (!e_ctrl->cal_data.mapdata)
		return -ENOMEM;

	memptr = e_ctrl->cal_data.mapdata;
	for (j = 0; j < eeprom_map_array->msm_size_of_max_mappings; j++) {
		eeprom_map = &(eeprom_map_array->memory_map[j]);
		if (e_ctrl->i2c_client.cci_client) {
			e_ctrl->i2c_client.cci_client->sid =
				eeprom_map->slave_addr >> 1;
		} else if (e_ctrl->i2c_client.client) {
			e_ctrl->i2c_client.client->addr =
				eeprom_map->slave_addr >> 1;
		}
		CDBG("Slave Addr: 0x%X\n", eeprom_map->slave_addr);
		CDBG("Memory map Size: %d",
			eeprom_map->memory_map_size);
		for (i = 0; i < eeprom_map->memory_map_size; i++) {
			switch (eeprom_map->mem_settings[i].i2c_operation) {
			case MSM_CAM_WRITE: {
				e_ctrl->i2c_client.addr_type =
					eeprom_map->mem_settings[i].addr_type;
				rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&(e_ctrl->i2c_client),
					eeprom_map->mem_settings[i].reg_addr,
					eeprom_map->mem_settings[i].reg_data,
					eeprom_map->mem_settings[i].data_type);
				msleep(eeprom_map->mem_settings[i].delay);
				if (rc < 0) {
					pr_err("%s: page write failed\n",
						__func__);
					goto clean_up;
				}
			}
			break;
			case MSM_CAM_POLL: {
				e_ctrl->i2c_client.addr_type =
					eeprom_map->mem_settings[i].addr_type;
				rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
					&(e_ctrl->i2c_client),
					eeprom_map->mem_settings[i].reg_addr,
					eeprom_map->mem_settings[i].reg_data,
					eeprom_map->mem_settings[i].data_type,
					eeprom_map->mem_settings[i].delay);
				if (rc < 0) {
					pr_err("%s: poll failed\n",
						__func__);
					goto clean_up;
				}
			}
			break;
			case MSM_CAM_READ: {
				e_ctrl->i2c_client.addr_type =
					eeprom_map->mem_settings[i].addr_type;
				rc = e_ctrl->i2c_client
					.i2c_func_tbl->i2c_read_seq(&(
					e_ctrl->i2c_client),
					eeprom_map->mem_settings[i].reg_addr,
					memptr,
					eeprom_map->mem_settings[i].reg_data);
				msleep(eeprom_map->mem_settings[i].delay);
				if (rc < 0) {
					pr_err("%s: read failed\n",
						__func__);
					goto clean_up;
				}
				memptr += eeprom_map->mem_settings[i].reg_data;
			}
			break;
			default:
				pr_err("%s: %d Invalid i2c operation LC:%d\n",
					__func__, __LINE__, i);
				return -EINVAL;
			}
		}
	}
	memptr = e_ctrl->cal_data.mapdata;
	for (i = 0; i < e_ctrl->cal_data.num_data; i++)
		CDBG("memory_data[%d] = 0x%X\n", i, memptr[i]);
	return rc;

clean_up:
	kfree(e_ctrl->cal_data.mapdata);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.mapdata = NULL;
	return rc;
}

/*
 * msm_eeprom_power_up - Do eeprom power up here
 * @e_ctrl:	ctrl structure
 * @power_info: power up info for eeprom
 *
 * Returns success or failure
 */
static int msm_eeprom_power_up(struct msm_eeprom_ctrl_t *e_ctrl,
	struct msm_camera_power_ctrl_t *power_info)
{
	int32_t rc = 0;

	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg, power_info->num_vreg,
		power_info->power_setting, power_info->power_setting_size);
	if (rc < 0) {
		pr_err("%s:%d failed in camera_fill_vreg_params  rc %d",
			__func__, __LINE__, rc);
		return rc;
	}

	/* Parse and fill vreg params for powerdown settings*/
	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg, power_info->num_vreg,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc < 0) {
		pr_err("%s:%d failed msm_camera_fill_vreg_params for PDOWN rc %d",
			__func__, __LINE__, rc);
		return rc;
	}

	rc = msm_camera_power_up(power_info, e_ctrl->eeprom_device_type,
			&e_ctrl->i2c_client);
	if (rc) {
		pr_err("%s:%d failed in eeprom Power up rc %d\n",
		__func__, __LINE__, rc);
		return rc;
	}
	return rc;
}

/*
 * msm_eeprom_power_up - Do power up, parse and power down
 * @e_ctrl: ctrl structure
 * Returns success or failure
 */
static int eeprom_init_config(struct msm_eeprom_ctrl_t *e_ctrl,
	void *argp)
{
	int rc =  0;
	struct msm_eeprom_cfg_data *cdata = argp;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_camera_power_ctrl_t *power_info;
	struct msm_eeprom_memory_map_array *memory_map_arr = NULL;

	power_setting_array =
		kzalloc(sizeof(struct msm_sensor_power_setting_array),
			GFP_KERNEL);
	if (!power_setting_array) {
		pr_err("%s:%d Mem Alloc Fail\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}
	memory_map_arr = kzalloc(sizeof(struct msm_eeprom_memory_map_array),
		GFP_KERNEL);
	if (!memory_map_arr) {
		rc = -ENOMEM;
		pr_err("%s:%d Mem Alloc Fail\n", __func__, __LINE__);
		goto free_mem;
	}

	if (copy_from_user(power_setting_array,
		(void __user *)cdata->cfg.eeprom_info.power_setting_array,
		sizeof(struct msm_sensor_power_setting_array))) {
		pr_err("%s copy_from_user failed %d\n",
			__func__, __LINE__);
		goto free_mem;
	}
	CDBG("%s:%d Size of power setting array: %d\n",
		__func__, __LINE__, power_setting_array->size);
	if (copy_from_user(memory_map_arr,
		(void __user *)cdata->cfg.eeprom_info.mem_map_array,
		sizeof(struct msm_eeprom_memory_map_array))) {
		rc = -EINVAL;
		pr_err("%s copy_from_user failed for memory map%d\n",
			__func__, __LINE__);
		goto free_mem;
	}

	power_info = &(e_ctrl->eboard_info->power_info);

	power_info->power_setting =
		power_setting_array->power_setting_a;
	power_info->power_down_setting =
		power_setting_array->power_down_setting_a;

	power_info->power_setting_size =
		power_setting_array->size;
	power_info->power_down_setting_size =
		power_setting_array->size_down;

	if ((power_info->power_setting_size >
		MAX_POWER_CONFIG) ||
		(power_info->power_down_setting_size >
		MAX_POWER_CONFIG) ||
		(!power_info->power_down_setting_size) ||
		(!power_info->power_setting_size)) {
		rc = -EINVAL;
		pr_err("%s:%d Invalid power setting size :%d, %d\n",
			__func__, __LINE__,
			power_info->power_setting_size,
			power_info->power_down_setting_size);
		goto free_mem;
	}

	if (e_ctrl->i2c_client.cci_client) {
		e_ctrl->i2c_client.cci_client->i2c_freq_mode =
			cdata->cfg.eeprom_info.i2c_freq_mode;
		if (e_ctrl->i2c_client.cci_client->i2c_freq_mode >
			I2C_MAX_MODES) {
			pr_err("%s::%d Improper I2C freq mode\n",
				__func__, __LINE__);
			e_ctrl->i2c_client.cci_client->i2c_freq_mode =
				I2C_STANDARD_MODE;
		}
	}

	/* Fill vreg power info and power up here */
	rc = msm_eeprom_power_up(e_ctrl, power_info);
	if (rc < 0) {
		pr_err("Power Up failed for eeprom\n");
		goto free_mem;
	}

	rc = eeprom_parse_memory_map(e_ctrl, memory_map_arr);
	if (rc < 0) {
		pr_err("%s::%d memory map parse failed\n", __func__, __LINE__);
		goto free_mem;
	}

	rc = msm_camera_power_down(power_info, e_ctrl->eeprom_device_type,
		&e_ctrl->i2c_client);
	if (rc < 0) {
		pr_err("%s:%d Power down failed rc %d\n",
			__func__, __LINE__, rc);
		goto free_mem;
	}

free_mem:
	kfree(power_setting_array);
	kfree(memory_map_arr);
	power_setting_array = NULL;
	memory_map_arr = NULL;
	return rc;
}

static int msm_eeprom_get_cmm_data(struct msm_eeprom_ctrl_t *e_ctrl,
				       struct msm_eeprom_cfg_data *cdata)
{
	int rc = 0;
	struct msm_eeprom_cmm_t *cmm_data = &e_ctrl->eboard_info->cmm_data;

	cdata->cfg.get_cmm_data.cmm_support = cmm_data->cmm_support;
	cdata->cfg.get_cmm_data.cmm_compression = cmm_data->cmm_compression;
	cdata->cfg.get_cmm_data.cmm_size = cmm_data->cmm_size;
	return rc;
}

static int eeprom_config_read_cal_data(struct msm_eeprom_ctrl_t *e_ctrl,
	struct msm_eeprom_cfg_data *cdata)
{
	int rc;

	/* check range */
	if (cdata->cfg.read_data.num_bytes >
		e_ctrl->cal_data.num_data) {
		CDBG("%s: Invalid size. exp %u, req %u\n", __func__,
			e_ctrl->cal_data.num_data,
			cdata->cfg.read_data.num_bytes);
		return -EINVAL;
	}

	rc = copy_to_user((void __user *)cdata->cfg.read_data.dbuffer,
		e_ctrl->cal_data.mapdata,
		cdata->cfg.read_data.num_bytes);

	return rc;
}

static int msm_eeprom_config(struct msm_eeprom_ctrl_t *e_ctrl,
	void *argp)
{
	struct msm_eeprom_cfg_data *cdata =
		(struct msm_eeprom_cfg_data *)argp;
	int rc = 0;
	size_t length = 0;

	CDBG("%s E\n", __func__);
	switch (cdata->cfgtype) {
	case CFG_EEPROM_GET_INFO:
		if (e_ctrl->userspace_probe == 1) {
			pr_err("%s:%d Eeprom name should be module driver",
				__func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		CDBG("%s E CFG_EEPROM_GET_INFO\n", __func__);
		cdata->is_supported = e_ctrl->is_supported;
		length = strlen(e_ctrl->eboard_info->eeprom_name) + 1;
		if (length > MAX_EEPROM_NAME) {
			pr_err("%s:%d invalid eeprom_name length %d\n",
				__func__, __LINE__, (int)length);
			rc = -EINVAL;
			break;
		}
		memcpy(cdata->cfg.eeprom_name,
			e_ctrl->eboard_info->eeprom_name, length);
		break;
	case CFG_EEPROM_GET_CAL_DATA:
		CDBG("%s E CFG_EEPROM_GET_CAL_DATA\n", __func__);
		cdata->cfg.get_data.num_bytes =
			e_ctrl->cal_data.num_data;
		break;
	case CFG_EEPROM_READ_CAL_DATA:
		CDBG("%s E CFG_EEPROM_READ_CAL_DATA\n", __func__);
		rc = eeprom_config_read_cal_data(e_ctrl, cdata);
		break;
	case CFG_EEPROM_GET_MM_INFO:
		CDBG("%s E CFG_EEPROM_GET_MM_INFO\n", __func__);
		rc = msm_eeprom_get_cmm_data(e_ctrl, cdata);
		break;
	case CFG_EEPROM_INIT:
		if (e_ctrl->userspace_probe == 0) {
			pr_err("%s:%d Eeprom already probed at kernel boot",
				__func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		if (e_ctrl->cal_data.num_data == 0) {
			rc = eeprom_init_config(e_ctrl, argp);
			if (rc < 0) {
				pr_err("%s:%d Eeprom init failed\n",
					__func__, __LINE__);
				return rc;
			}
		} else {
			CDBG("%s:%d Already read eeprom\n",
				__func__, __LINE__);
		}
		break;
	default:
		break;
	}

	CDBG("%s X rc: %d\n", __func__, rc);
	return rc;
}

static int msm_eeprom_get_subdev_id(struct msm_eeprom_ctrl_t *e_ctrl,
				    void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	CDBG("%s E\n", __func__);
	if (!subdev_id) {
		pr_err("%s failed\n", __func__);
		return -EINVAL;
	}
	*subdev_id = e_ctrl->subdev_id;
	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("%s X\n", __func__);
	return 0;
}

static long msm_eeprom_subdev_ioctl(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	struct msm_eeprom_ctrl_t *e_ctrl = v4l2_get_subdevdata(sd);
	void *argp = (void *)arg;

	CDBG("%s E\n", __func__);
	CDBG("%s:%d a_ctrl %pK argp %pK\n", __func__, __LINE__, e_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_eeprom_get_subdev_id(e_ctrl, argp);
	case VIDIOC_MSM_EEPROM_CFG:
		return msm_eeprom_config(e_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}

	CDBG("%s X\n", __func__);
}

static struct msm_camera_i2c_fn_t msm_eeprom_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll = msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_eeprom_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
	msm_camera_qup_i2c_write_table_w_microdelay,
};

static struct msm_camera_i2c_fn_t msm_eeprom_spi_func_tbl = {
	.i2c_read = msm_camera_spi_read,
	.i2c_read_seq = msm_camera_spi_read_seq,
};

static int msm_eeprom_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct msm_eeprom_ctrl_t *e_ctrl =  v4l2_get_subdevdata(sd);

	CDBG("%s E\n", __func__);
	if (!e_ctrl) {
		pr_err("%s failed e_ctrl is NULL\n", __func__);
		return -EINVAL;
	}
	CDBG("%s X\n", __func__);
	return rc;
}

static int msm_eeprom_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct msm_eeprom_ctrl_t *e_ctrl =  v4l2_get_subdevdata(sd);

	CDBG("%s E\n", __func__);
	if (!e_ctrl) {
		pr_err("%s failed e_ctrl is NULL\n", __func__);
		return -EINVAL;
	}
	CDBG("%s X\n", __func__);
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_eeprom_internal_ops = {
	.open = msm_eeprom_open,
	.close = msm_eeprom_close,
};

static struct v4l2_subdev_core_ops msm_eeprom_subdev_core_ops = {
	.ioctl = msm_eeprom_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_eeprom_subdev_ops = {
	.core = &msm_eeprom_subdev_core_ops,
};

static int msm_eeprom_get_dt_data(struct msm_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0, i = 0;
	struct msm_eeprom_board_info *eb_info;
	struct msm_camera_power_ctrl_t *power_info =
		&e_ctrl->eboard_info->power_info;
	struct device_node *of_node = NULL;
	struct msm_camera_gpio_conf *gconf = NULL;
	int8_t gpio_array_size = 0;
	uint16_t *gpio_array = NULL;

	eb_info = e_ctrl->eboard_info;
	if (e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE)
		of_node = e_ctrl->i2c_client
			.spi_client->spi_master->dev.of_node;
	else if (e_ctrl->eeprom_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		of_node = e_ctrl->pdev->dev.of_node;
	else if (e_ctrl->eeprom_device_type == MSM_CAMERA_I2C_DEVICE)
		of_node = e_ctrl->i2c_client.client->dev.of_node;

	if (!of_node) {
		pr_err("%s: %d of_node is NULL\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = msm_camera_get_dt_vreg_data(of_node, &power_info->cam_vreg,
					     &power_info->num_vreg);
	if (rc < 0)
		return rc;

	if (e_ctrl->userspace_probe == 0) {
		rc = msm_camera_get_dt_power_setting_data(of_node,
			power_info->cam_vreg, power_info->num_vreg,
			power_info);
		if (rc < 0)
			goto ERROR1;
	}

	power_info->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
					GFP_KERNEL);
	if (!power_info->gpio_conf) {
		rc = -ENOMEM;
		goto ERROR2;
	}
	gconf = power_info->gpio_conf;
	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size > 0) {
		gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t),
			GFP_KERNEL);
		if (!gpio_array)
			goto ERROR3;
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			CDBG("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}

		rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}
		kfree(gpio_array);
	}

	return rc;
ERROR4:
	kfree(gpio_array);
ERROR3:
	kfree(power_info->gpio_conf);
ERROR2:
	kfree(power_info->cam_vreg);
ERROR1:
	kfree(power_info->power_setting);
	return rc;
}

static int msm_eeprom_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int rc = 0;
	uint32_t temp = 0;
	struct msm_eeprom_ctrl_t *e_ctrl = NULL;
	struct msm_eeprom_board_info *eb_info = NULL;
	struct device_node *of_node = client->dev.of_node;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("%s E\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c_check_functionality failed\n", __func__);
		goto probe_failure;
	}

	e_ctrl = kzalloc(sizeof(*e_ctrl), GFP_KERNEL);
	if (!e_ctrl)
		return -ENOMEM;

	e_ctrl->eeprom_v4l2_subdev_ops = &msm_eeprom_subdev_ops;
	e_ctrl->eeprom_mutex = &msm_eeprom_mutex;

	e_ctrl->cal_data.mapdata = NULL;
	e_ctrl->cal_data.map = NULL;
	e_ctrl->userspace_probe = 0;
	e_ctrl->is_supported = 0;
	if (!of_node) {
		pr_err("%s dev.of_node NULL\n", __func__);
		rc = -EINVAL;
		goto ectrl_free;
	}
	/* Set device type as I2C */
	e_ctrl->eeprom_device_type = MSM_CAMERA_I2C_DEVICE;
	e_ctrl->i2c_client.i2c_func_tbl = &msm_eeprom_qup_func_tbl;

	e_ctrl->eboard_info = kzalloc(sizeof(
		struct msm_eeprom_board_info), GFP_KERNEL);
	if (!e_ctrl->eboard_info) {
		rc = -ENOMEM;
		goto ectrl_free;
	}
	eb_info = e_ctrl->eboard_info;
	power_info = &eb_info->power_info;
	e_ctrl->i2c_client.client = client;
	power_info->dev = &client->dev;

	/*Get clocks information*/
	rc = msm_camera_i2c_dev_get_clk_info(
		&e_ctrl->i2c_client.client->dev,
		&power_info->clk_info,
		&power_info->clk_ptr,
		&power_info->clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		goto board_free;
	}

	rc = of_property_read_u32(of_node, "cell-index",
		&e_ctrl->subdev_id);
	CDBG("cell-index %d, rc %d\n", e_ctrl->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto board_free;
	}

	rc = of_property_read_string(of_node, "qcom,eeprom-name",
		&eb_info->eeprom_name);
	CDBG("%s qcom,eeprom-name %s, rc %d\n", __func__,
		eb_info->eeprom_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		e_ctrl->userspace_probe = 1;
	}

	rc = msm_eeprom_get_dt_data(e_ctrl);
	if (rc < 0)
		goto board_free;

	if (e_ctrl->userspace_probe == 0) {
		rc = of_property_read_u32(of_node, "qcom,slave-addr",
			&temp);
		if (rc < 0) {
			pr_err("%s failed rc %d\n", __func__, rc);
			goto board_free;
		}

		rc = of_property_read_u32(of_node, "qcom,i2c-freq-mode",
			&e_ctrl->i2c_freq_mode);
		CDBG("qcom,i2c_freq_mode %d, rc %d\n",
			e_ctrl->i2c_freq_mode, rc);
		if (rc < 0) {
			pr_err("%s qcom,i2c-freq-mode read fail. Setting to 0 %d\n",
				__func__, rc);
			e_ctrl->i2c_freq_mode = 0;
		}
		if (e_ctrl->i2c_freq_mode >= I2C_MAX_MODES) {
			pr_err("%s:%d invalid i2c_freq_mode = %d\n",
				__func__, __LINE__, e_ctrl->i2c_freq_mode);
			e_ctrl->i2c_freq_mode = 0;
		}
		eb_info->i2c_slaveaddr = temp;
		CDBG("qcom,slave-addr = 0x%X\n", eb_info->i2c_slaveaddr);
		eb_info->i2c_freq_mode = e_ctrl->i2c_freq_mode;

		rc = msm_eeprom_parse_memory_map(of_node, &e_ctrl->cal_data);
		if (rc < 0)
			goto board_free;

		rc = msm_camera_power_up(power_info, e_ctrl->eeprom_device_type,
			&e_ctrl->i2c_client);
		if (rc) {
			pr_err("failed rc %d\n", rc);
			goto memdata_free;
		}

		rc = read_eeprom_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc < 0) {
			pr_err("%s read_eeprom_memory failed\n", __func__);
			goto power_down;
		}
		CDBG("%s cal_data: %*ph\n", __func__,
			e_ctrl->cal_data.num_data, e_ctrl->cal_data.mapdata);

		e_ctrl->is_supported |= msm_eeprom_match_crc(&e_ctrl->cal_data);

		rc = msm_camera_power_down(power_info,
			e_ctrl->eeprom_device_type, &e_ctrl->i2c_client);
		if (rc) {
			pr_err("failed rc %d\n", rc);
			goto memdata_free;
		}
	} else
		e_ctrl->is_supported = 1;

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&e_ctrl->msm_sd.sd,
		e_ctrl->i2c_client.client,
		e_ctrl->eeprom_v4l2_subdev_ops);
	v4l2_set_subdevdata(&e_ctrl->msm_sd.sd, e_ctrl);
	e_ctrl->msm_sd.sd.internal_ops = &msm_eeprom_internal_ops;
	e_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(e_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(e_ctrl->msm_sd.sd.name), "msm_eeprom");
	media_entity_pads_init(&e_ctrl->msm_sd.sd.entity, 0, NULL);
	e_ctrl->msm_sd.sd.entity.function = MSM_CAMERA_SUBDEV_EEPROM;
	msm_sd_register(&e_ctrl->msm_sd);
	e_ctrl->is_supported = (e_ctrl->is_supported << 1) | 1;
	pr_err("%s success result=%d X\n", __func__, rc);
	return rc;

power_down:
	msm_camera_power_down(power_info, e_ctrl->eeprom_device_type,
		&e_ctrl->i2c_client);
memdata_free:
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
board_free:
	kfree(e_ctrl->eboard_info);
ectrl_free:
	kfree(e_ctrl);
probe_failure:
	pr_err("%s failed! rc = %d\n", __func__, rc);
	return rc;
}

static int msm_eeprom_i2c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct msm_eeprom_ctrl_t  *e_ctrl;

	if (!sd) {
		pr_err("%s: Subdevice is NULL\n", __func__);
		return 0;
	}

	e_ctrl = (struct msm_eeprom_ctrl_t *)v4l2_get_subdevdata(sd);
	if (!e_ctrl) {
		pr_err("%s: eeprom device is NULL\n", __func__);
		return 0;
	}

	if (!e_ctrl->eboard_info) {
		pr_err("%s: eboard_info is NULL\n", __func__);
		return 0;
	}

	msm_camera_i2c_dev_put_clk_info(&e_ctrl->i2c_client.client->dev,
		&e_ctrl->eboard_info->power_info.clk_info,
		&e_ctrl->eboard_info->power_info.clk_ptr,
		e_ctrl->eboard_info->power_info.clk_info_size);

	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
	if (e_ctrl->eboard_info) {
		kfree(e_ctrl->eboard_info->power_info.gpio_conf);
		kfree(e_ctrl->eboard_info);
	}
	e_ctrl->cal_data.mapdata = NULL;
	kfree(e_ctrl);
	e_ctrl = NULL;

	return 0;
}

static int msm_eeprom_spi_parse_of(struct msm_camera_spi_client *spic)
{
	int rc = -EFAULT;
	uint32_t tmp[3];

	if (of_property_read_u32_array(
		spic->spi_master->dev.of_node,
		"qcom,spiop,read", tmp, 3)) {
		return -EFAULT;
	}
	spic->cmd_tbl.read.opcode = tmp[0];
	spic->cmd_tbl.read.addr_len = tmp[1];
	spic->cmd_tbl.read.dummy_len = tmp[2];

	if (of_property_read_u32_array(
		spic->spi_master->dev.of_node,
		"qcom,spiop,readseq", tmp, 3)) {
		return -EFAULT;
	}
	spic->cmd_tbl.read_seq.opcode = tmp[0];
	spic->cmd_tbl.read_seq.addr_len = tmp[1];
	spic->cmd_tbl.read_seq.dummy_len = tmp[2];

	if (of_property_read_u32_array(
		spic->spi_master->dev.of_node,
		"qcom,spiop,queryid", tmp, 3)) {
		return -EFAULT;
	}
	spic->cmd_tbl.query_id.opcode = tmp[0];
	spic->cmd_tbl.query_id.addr_len = tmp[1];
	spic->cmd_tbl.query_id.dummy_len = tmp[2];

	rc = of_property_read_u32_array(spic->spi_master->dev.of_node,
					"qcom,eeprom-id", tmp, 2);
	if (rc) {
		pr_err("%s: Failed to get eeprom id\n", __func__);
		return rc;
	}
	spic->mfr_id0 = tmp[0];
	spic->device_id0 = tmp[1];

	return 0;
}

static int msm_eeprom_match_id(struct msm_eeprom_ctrl_t *e_ctrl)
{
	int rc;
	struct msm_camera_i2c_client *client = &e_ctrl->i2c_client;
	uint8_t id[2];

	rc = msm_camera_spi_query_id(client, 0, &id[0], 2);
	if (rc < 0)
		return rc;
	CDBG("%s: read 0x%x 0x%x, check 0x%x 0x%x\n", __func__, id[0],
		id[1], client->spi_client->mfr_id0,
		client->spi_client->device_id0);
	if (id[0] != client->spi_client->mfr_id0
		|| id[1] != client->spi_client->device_id0)
		return -ENODEV;

	return 0;
}

static int msm_eeprom_cmm_dts(struct msm_eeprom_board_info *eb_info,
	struct device_node *of_node)
{
	int rc = 0;
	struct msm_eeprom_cmm_t *cmm_data = &eb_info->cmm_data;

	cmm_data->cmm_support =
		of_property_read_bool(of_node, "qcom,cmm-data-support");
	if (!cmm_data->cmm_support)
		return -EINVAL;
	cmm_data->cmm_compression =
		of_property_read_bool(of_node, "qcom,cmm-data-compressed");
	if (!cmm_data->cmm_compression)
		CDBG("No MM compression data\n");

	rc = of_property_read_u32(of_node, "qcom,cmm-data-offset",
		&cmm_data->cmm_offset);
	if (rc < 0)
		CDBG("No MM offset data\n");

	rc = of_property_read_u32(of_node, "qcom,cmm-data-size",
		&cmm_data->cmm_size);
	if (rc < 0)
		CDBG("No MM size data\n");

	CDBG("cmm_support: cmm_compr %d, cmm_offset %d, cmm_size %d\n",
		cmm_data->cmm_compression,
		cmm_data->cmm_offset,
		cmm_data->cmm_size);
	return 0;
}

static int msm_eeprom_spi_setup(struct spi_device *spi)
{
	struct msm_eeprom_ctrl_t *e_ctrl = NULL;
	struct msm_camera_i2c_client *client = NULL;
	struct msm_camera_spi_client *spi_client;
	struct msm_eeprom_board_info *eb_info;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	int rc = 0;

	e_ctrl = kzalloc(sizeof(*e_ctrl), GFP_KERNEL);
	if (!e_ctrl)
		return -ENOMEM;
	e_ctrl->eeprom_v4l2_subdev_ops = &msm_eeprom_subdev_ops;
	e_ctrl->eeprom_mutex = &msm_eeprom_mutex;
	client = &e_ctrl->i2c_client;
	e_ctrl->is_supported = 0;
	e_ctrl->userspace_probe = 0;
	e_ctrl->cal_data.mapdata = NULL;
	e_ctrl->cal_data.map = NULL;

	spi_client = kzalloc(sizeof(*spi_client), GFP_KERNEL);
	if (!spi_client) {
		kfree(e_ctrl);
		return -ENOMEM;
	}

	rc = of_property_read_u32(spi->dev.of_node, "cell-index",
				  &e_ctrl->subdev_id);
	CDBG("cell-index %d, rc %d\n", e_ctrl->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		kfree(e_ctrl);
		kfree(spi_client);
		return rc;
	}

	eb_info = kzalloc(sizeof(*eb_info), GFP_KERNEL);
	if (!eb_info)
		goto spi_free;
	e_ctrl->eboard_info = eb_info;

	rc = of_property_read_string(spi->dev.of_node, "qcom,eeprom-name",
		&eb_info->eeprom_name);
	CDBG("%s qcom,eeprom-name %s, rc %d\n", __func__,
		eb_info->eeprom_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		e_ctrl->userspace_probe = 1;
		goto board_free;
	}

	e_ctrl->eeprom_device_type = MSM_CAMERA_SPI_DEVICE;
	client->spi_client = spi_client;
	spi_client->spi_master = spi;
	client->i2c_func_tbl = &msm_eeprom_spi_func_tbl;
	client->addr_type = MSM_CAMERA_I2C_3B_ADDR;

	rc = msm_eeprom_cmm_dts(e_ctrl->eboard_info, spi->dev.of_node);
	if (rc < 0)
		CDBG("%s MM data miss:%d\n", __func__, __LINE__);

	power_info = &eb_info->power_info;
	power_info->dev = &spi->dev;

	/*Get clocks information*/
	rc = msm_camera_i2c_dev_get_clk_info(
		&spi->dev,
		&power_info->clk_info,
		&power_info->clk_ptr,
		&power_info->clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		goto board_free;
	}

	rc = msm_eeprom_get_dt_data(e_ctrl);
	if (rc < 0)
		goto board_free;

	/* set spi instruction info */
	spi_client->retry_delay = 1;
	spi_client->retries = 0;

	rc = msm_eeprom_spi_parse_of(spi_client);
	if (rc < 0) {
		dev_err(&spi->dev,
			"%s: Error parsing device properties\n", __func__);
		goto board_free;
	}

	if (e_ctrl->userspace_probe == 0) {
		/* prepare memory buffer */
		rc = msm_eeprom_parse_memory_map(spi->dev.of_node,
			&e_ctrl->cal_data);
		if (rc < 0)
			CDBG("%s: no cal memory map\n", __func__);

		/* power up eeprom for reading */
		rc = msm_camera_power_up(power_info, e_ctrl->eeprom_device_type,
			&e_ctrl->i2c_client);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto caldata_free;
		}

		/* check eeprom id */
		rc = msm_eeprom_match_id(e_ctrl);
		if (rc < 0) {
			CDBG("%s: eeprom not matching %d\n", __func__, rc);
			goto power_down;
		}
		/* read eeprom */
		if (e_ctrl->cal_data.map) {
			rc = read_eeprom_memory(e_ctrl, &e_ctrl->cal_data);
			if (rc < 0) {
				pr_err("%s: read cal data failed\n", __func__);
				goto power_down;
			}
			e_ctrl->is_supported |= msm_eeprom_match_crc(
				&e_ctrl->cal_data);
		}

		rc = msm_camera_power_down(power_info,
			e_ctrl->eeprom_device_type, &e_ctrl->i2c_client);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto caldata_free;
		}
	} else
		e_ctrl->is_supported = 1;

	/* initiazlie subdev */
	v4l2_spi_subdev_init(&e_ctrl->msm_sd.sd,
		e_ctrl->i2c_client.spi_client->spi_master,
		e_ctrl->eeprom_v4l2_subdev_ops);
	v4l2_set_subdevdata(&e_ctrl->msm_sd.sd, e_ctrl);
	e_ctrl->msm_sd.sd.internal_ops = &msm_eeprom_internal_ops;
	e_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_pads_init(&e_ctrl->msm_sd.sd.entity, 0, NULL);
	e_ctrl->msm_sd.sd.entity.function = MSM_CAMERA_SUBDEV_EEPROM;
	msm_sd_register(&e_ctrl->msm_sd);
	e_ctrl->is_supported = (e_ctrl->is_supported << 1) | 1;
	CDBG("%s success result=%d supported=%x X\n", __func__, rc,
	     e_ctrl->is_supported);

	return 0;

power_down:
	msm_camera_power_down(power_info, e_ctrl->eeprom_device_type,
		&e_ctrl->i2c_client);
caldata_free:
	msm_camera_i2c_dev_put_clk_info(
		&e_ctrl->i2c_client.spi_client->spi_master->dev,
		&e_ctrl->eboard_info->power_info.clk_info,
		&e_ctrl->eboard_info->power_info.clk_ptr,
		e_ctrl->eboard_info->power_info.clk_info_size);
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
board_free:
	kfree(e_ctrl->eboard_info);
spi_free:
	kfree(spi_client);
	kfree(e_ctrl);
	return rc;
}

static int msm_eeprom_spi_probe(struct spi_device *spi)
{
	int irq, cs, cpha, cpol, cs_high;

	CDBG("%s\n", __func__);
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);

	irq = spi->irq;
	cs = spi->chip_select;
	cpha = (spi->mode & SPI_CPHA) ? 1 : 0;
	cpol = (spi->mode & SPI_CPOL) ? 1 : 0;
	cs_high = (spi->mode & SPI_CS_HIGH) ? 1 : 0;
	CDBG("%s: irq[%d] cs[%x] CPHA[%x] CPOL[%x] CS_HIGH[%x]\n",
			__func__, irq, cs, cpha, cpol, cs_high);
	CDBG("%s: max_speed[%u]\n", __func__, spi->max_speed_hz);

	return msm_eeprom_spi_setup(spi);
}

static int msm_eeprom_spi_remove(struct spi_device *sdev)
{
	struct v4l2_subdev *sd = spi_get_drvdata(sdev);
	struct msm_eeprom_ctrl_t  *e_ctrl;

	if (!sd) {
		pr_err("%s: Subdevice is NULL\n", __func__);
		return 0;
	}

	e_ctrl = (struct msm_eeprom_ctrl_t *)v4l2_get_subdevdata(sd);
	if (!e_ctrl) {
		pr_err("%s: eeprom device is NULL\n", __func__);
		return 0;
	}

	if (!e_ctrl->eboard_info) {
		pr_err("%s: board info is NULL\n", __func__);
		return 0;
	}

	msm_camera_i2c_dev_put_clk_info(
		&e_ctrl->i2c_client.spi_client->spi_master->dev,
		&e_ctrl->eboard_info->power_info.clk_info,
		&e_ctrl->eboard_info->power_info.clk_ptr,
		e_ctrl->eboard_info->power_info.clk_info_size);

	kfree(e_ctrl->i2c_client.spi_client);
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
	if (e_ctrl->eboard_info) {
		kfree(e_ctrl->eboard_info->power_info.gpio_conf);
		kfree(e_ctrl->eboard_info);
	}
	e_ctrl->cal_data.mapdata = NULL;
	kfree(e_ctrl);
	e_ctrl = NULL;

	return 0;
}

#ifdef CONFIG_COMPAT
static void msm_eeprom_copy_power_settings_compat(
	struct msm_sensor_power_setting_array *ps,
	struct msm_sensor_power_setting_array32 *ps32)
{
	uint16_t i = 0;

	ps->size = ps32->size;
	for (i = 0; i < ps32->size; i++) {
		ps->power_setting_a[i].config_val =
			ps32->power_setting_a[i].config_val;
		ps->power_setting_a[i].delay =
			ps32->power_setting_a[i].delay;
		ps->power_setting_a[i].seq_type =
			ps32->power_setting_a[i].seq_type;
		ps->power_setting_a[i].seq_val =
			ps32->power_setting_a[i].seq_val;
	}

	ps->size_down = ps32->size_down;
	for (i = 0; i < ps32->size_down; i++) {
		ps->power_down_setting_a[i].config_val =
			ps32->power_down_setting_a[i].config_val;
		ps->power_down_setting_a[i].delay =
			ps32->power_down_setting_a[i].delay;
		ps->power_down_setting_a[i].seq_type =
			ps32->power_down_setting_a[i].seq_type;
		ps->power_down_setting_a[i].seq_val =
			ps32->power_down_setting_a[i].seq_val;
	}
}

static int eeprom_config_read_cal_data32(struct msm_eeprom_ctrl_t *e_ctrl,
	void *arg)
{
	int rc;
	uint8_t __user *ptr_dest = NULL;
	struct msm_eeprom_cfg_data32 *cdata32 =
		(struct msm_eeprom_cfg_data32 *) arg;
	struct msm_eeprom_cfg_data cdata;

	cdata.cfgtype = cdata32->cfgtype;
	cdata.is_supported = cdata32->is_supported;
	cdata.cfg.read_data.num_bytes = cdata32->cfg.read_data.num_bytes;
	/* check range */
	if (cdata.cfg.read_data.num_bytes >
	    e_ctrl->cal_data.num_data) {
		CDBG("%s: Invalid size. exp %u, req %u\n", __func__,
			e_ctrl->cal_data.num_data,
			cdata.cfg.read_data.num_bytes);
		return -EINVAL;
	}
	if (!e_ctrl->cal_data.mapdata)
		return -EFAULT;

	ptr_dest = (uint8_t __user *)compat_ptr(cdata32->cfg.read_data.dbuffer);

	rc = copy_to_user(ptr_dest, e_ctrl->cal_data.mapdata,
		cdata.cfg.read_data.num_bytes);

	return rc;
}

static int eeprom_init_config32(struct msm_eeprom_ctrl_t *e_ctrl,
	void *argp)
{
	int rc =  0;
	struct msm_eeprom_cfg_data32 *cdata32 = argp;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting_array32 *power_setting_array32 = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	struct msm_eeprom_memory_map_array *mem_map_array = NULL;

	power_setting_array32 =
		kzalloc(sizeof(struct msm_sensor_power_setting_array32),
			GFP_KERNEL);
	if (!power_setting_array32) {
		pr_err("%s:%d Mem Alloc Fail\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}
	power_setting_array =
		kzalloc(sizeof(struct msm_sensor_power_setting_array),
			GFP_KERNEL);
	if (power_setting_array ==  NULL) {
		pr_err("%s:%d Mem Alloc Fail\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto free_mem;
	}
	mem_map_array =
		kzalloc(sizeof(struct msm_eeprom_memory_map_array),
			GFP_KERNEL);
	if (mem_map_array == NULL) {
		pr_err("%s:%d Mem Alloc Fail\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto free_mem;
	}

	if (copy_from_user(power_setting_array32,
		(void __user *)compat_ptr(cdata32->cfg.eeprom_info
		.power_setting_array),
		sizeof(struct msm_sensor_power_setting_array32))) {
		pr_err("%s:%d copy_from_user failed\n",
			__func__, __LINE__);
		goto free_mem;
	}
	CDBG("%s:%d Size of power setting array: %d",
		__func__, __LINE__, power_setting_array32->size);
	if (copy_from_user(mem_map_array,
		(void __user *)
		compat_ptr(cdata32->cfg.eeprom_info.mem_map_array),
		sizeof(struct msm_eeprom_memory_map_array))) {
		pr_err("%s:%d copy_from_user failed for memory map\n",
			__func__, __LINE__);
		goto free_mem;
	}

	power_info = &(e_ctrl->eboard_info->power_info);

	if ((power_setting_array32->size > MAX_POWER_CONFIG) ||
		(power_setting_array32->size_down > MAX_POWER_CONFIG) ||
		(!power_setting_array32->size) ||
		(!power_setting_array32->size_down)) {
		pr_err("%s:%d invalid power setting size=%d size_down=%d\n",
			__func__, __LINE__, power_setting_array32->size,
			power_setting_array32->size_down);
		rc = -EINVAL;
		goto free_mem;
	}
	msm_eeprom_copy_power_settings_compat(
		power_setting_array,
		power_setting_array32);

	power_info->power_setting =
		power_setting_array->power_setting_a;
	power_info->power_down_setting =
		power_setting_array->power_down_setting_a;

	power_info->power_setting_size =
		power_setting_array->size;
	power_info->power_down_setting_size =
		power_setting_array->size_down;

	if (e_ctrl->i2c_client.cci_client) {
		e_ctrl->i2c_client.cci_client->i2c_freq_mode =
			cdata32->cfg.eeprom_info.i2c_freq_mode;
		if (e_ctrl->i2c_client.cci_client->i2c_freq_mode >
			I2C_MAX_MODES) {
			pr_err("%s::%d Improper I2C Freq Mode\n",
				__func__, __LINE__);
			e_ctrl->i2c_client.cci_client->i2c_freq_mode =
				I2C_STANDARD_MODE;
		}
		CDBG("%s:%d Not CCI probe", __func__, __LINE__);
	}
	/* Fill vreg power info and power up here */
	rc = msm_eeprom_power_up(e_ctrl, power_info);
	if (rc < 0) {
		pr_err("%s:%d Power Up failed for eeprom\n",
			__func__, __LINE__);
		goto free_mem;
	}

	rc = eeprom_parse_memory_map(e_ctrl, mem_map_array);
	if (rc < 0) {
		pr_err("%s:%d memory map parse failed\n",
			__func__, __LINE__);
		goto free_mem;
	}

	rc = msm_camera_power_down(power_info,
		e_ctrl->eeprom_device_type, &e_ctrl->i2c_client);
	if (rc < 0)
		pr_err("%s:%d Power down failed rc %d\n",
			__func__, __LINE__, rc);

free_mem:
	kfree(power_setting_array32);
	kfree(power_setting_array);
	kfree(mem_map_array);
	power_setting_array32 = NULL;
	power_setting_array = NULL;
	mem_map_array = NULL;
	return rc;
}

static int msm_eeprom_config32(struct msm_eeprom_ctrl_t *e_ctrl,
	void *argp)
{
	struct msm_eeprom_cfg_data32 *cdata =
		(struct msm_eeprom_cfg_data32 *)argp;
	int rc = 0;
	size_t length = 0;

	CDBG("%s E\n", __func__);
	switch (cdata->cfgtype) {
	case CFG_EEPROM_GET_INFO:
		if (e_ctrl->userspace_probe == 1) {
			pr_err("%s:%d Eeprom name should be module driver",
				__func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		CDBG("%s E CFG_EEPROM_GET_INFO\n", __func__);
		cdata->is_supported = e_ctrl->is_supported;
		length = strlen(e_ctrl->eboard_info->eeprom_name) + 1;
		if (length > MAX_EEPROM_NAME) {
			pr_err("%s:%d invalid eeprom_name length %d\n",
				__func__, __LINE__, (int)length);
			rc = -EINVAL;
			break;
		}
		memcpy(cdata->cfg.eeprom_name,
			e_ctrl->eboard_info->eeprom_name, length);
		break;
	case CFG_EEPROM_GET_CAL_DATA:
		CDBG("%s E CFG_EEPROM_GET_CAL_DATA\n", __func__);
		cdata->cfg.get_data.num_bytes =
			e_ctrl->cal_data.num_data;
		break;
	case CFG_EEPROM_READ_CAL_DATA:
		CDBG("%s E CFG_EEPROM_READ_CAL_DATA\n", __func__);
		rc = eeprom_config_read_cal_data32(e_ctrl, argp);
		break;
	case CFG_EEPROM_INIT:
		if (e_ctrl->userspace_probe == 0) {
			pr_err("%s:%d Eeprom already probed at kernel boot",
				__func__, __LINE__);
			rc = -EINVAL;
			break;
		}
		if (e_ctrl->cal_data.num_data == 0) {
			rc = eeprom_init_config32(e_ctrl, argp);
			if (rc < 0)
				pr_err("%s:%d Eeprom init failed\n",
					__func__, __LINE__);
		} else {
			CDBG("%s:%d Already read eeprom\n",
				__func__, __LINE__);
		}
		break;
	default:
		break;
	}

	CDBG("%s X rc: %d\n", __func__, rc);
	return rc;
}

static long msm_eeprom_subdev_ioctl32(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	struct msm_eeprom_ctrl_t *e_ctrl = v4l2_get_subdevdata(sd);
	void *argp = (void *)arg;

	CDBG("%s E\n", __func__);
	CDBG("%s:%d a_ctrl %pK argp %pK\n", __func__, __LINE__, e_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_eeprom_get_subdev_id(e_ctrl, argp);
	case VIDIOC_MSM_EEPROM_CFG32:
		return msm_eeprom_config32(e_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}

	CDBG("%s X\n", __func__);
}

static long msm_eeprom_subdev_do_ioctl32(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return msm_eeprom_subdev_ioctl32(sd, cmd, arg);
}

static long msm_eeprom_subdev_fops_ioctl32(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_eeprom_subdev_do_ioctl32);
}

#endif

static int msm_eeprom_platform_probe(struct platform_device *pdev)
{
	int rc = 0;
	int j = 0;
	uint32_t temp;

	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_eeprom_ctrl_t *e_ctrl = NULL;
	struct msm_eeprom_board_info *eb_info = NULL;
	struct device_node *of_node = pdev->dev.of_node;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("%s E\n", __func__);

	e_ctrl = kzalloc(sizeof(*e_ctrl), GFP_KERNEL);
	if (!e_ctrl)
		return -ENOMEM;
	e_ctrl->eeprom_v4l2_subdev_ops = &msm_eeprom_subdev_ops;
	e_ctrl->eeprom_mutex = &msm_eeprom_mutex;

	e_ctrl->cal_data.mapdata = NULL;
	e_ctrl->cal_data.map = NULL;
	e_ctrl->userspace_probe = 0;
	e_ctrl->is_supported = 0;
	if (!of_node) {
		pr_err("%s dev.of_node NULL\n", __func__);
		rc = -EINVAL;
		goto ectrl_free;
	}

	/* Set platform device handle */
	e_ctrl->pdev = pdev;
	/* Set device type as platform device */
	e_ctrl->eeprom_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	e_ctrl->i2c_client.i2c_func_tbl = &msm_eeprom_cci_func_tbl;
	e_ctrl->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!e_ctrl->i2c_client.cci_client) {
		rc = -ENOMEM;
		goto ectrl_free;
	}

	e_ctrl->eboard_info = kzalloc(sizeof(
		struct msm_eeprom_board_info), GFP_KERNEL);
	if (!e_ctrl->eboard_info) {
		rc = -ENOMEM;
		goto cciclient_free;
	}

	eb_info = e_ctrl->eboard_info;
	power_info = &eb_info->power_info;
	cci_client = e_ctrl->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->retries = 3;
	cci_client->id_map = 0;
	power_info->dev = &pdev->dev;

	/*Get clocks information*/
	rc = msm_camera_get_clk_info(e_ctrl->pdev,
		&power_info->clk_info,
		&power_info->clk_ptr,
		&power_info->clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		goto board_free;
	}

	rc = of_property_read_u32(of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto board_free;
	}
	e_ctrl->subdev_id = pdev->id;

	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&e_ctrl->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", e_ctrl->cci_master, rc);
	if (rc < 0) {
		pr_err("%s failed rc %d\n", __func__, rc);
		goto board_free;
	}
	cci_client->cci_i2c_master = e_ctrl->cci_master;

	rc = of_property_read_string(of_node, "qcom,eeprom-name",
		&eb_info->eeprom_name);
	CDBG("%s qcom,eeprom-name %s, rc %d\n", __func__,
		eb_info->eeprom_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		e_ctrl->userspace_probe = 1;
	}

	rc = msm_eeprom_get_dt_data(e_ctrl);
	if (rc < 0)
		goto board_free;

	if (e_ctrl->userspace_probe == 0) {
		rc = of_property_read_u32(of_node, "qcom,slave-addr",
			&temp);
		if (rc < 0) {
			pr_err("%s failed rc %d\n", __func__, rc);
			goto board_free;
		}

		rc = of_property_read_u32(of_node, "qcom,i2c-freq-mode",
			&e_ctrl->i2c_freq_mode);
		CDBG("qcom,i2c_freq_mode %d, rc %d\n",
			e_ctrl->i2c_freq_mode, rc);
		if (rc < 0) {
			pr_err("%s qcom,i2c-freq-mode read fail. Setting to 0 %d\n",
				__func__, rc);
			e_ctrl->i2c_freq_mode = 0;
		}
		if (e_ctrl->i2c_freq_mode >= I2C_MAX_MODES) {
			pr_err("%s:%d invalid i2c_freq_mode = %d\n",
				__func__, __LINE__, e_ctrl->i2c_freq_mode);
			e_ctrl->i2c_freq_mode = 0;
		}
		eb_info->i2c_slaveaddr = temp;
		CDBG("qcom,slave-addr = 0x%X\n", eb_info->i2c_slaveaddr);
		eb_info->i2c_freq_mode = e_ctrl->i2c_freq_mode;
		cci_client->i2c_freq_mode = e_ctrl->i2c_freq_mode;
		cci_client->sid = eb_info->i2c_slaveaddr >> 1;

		rc = msm_eeprom_parse_memory_map(of_node, &e_ctrl->cal_data);
		if (rc < 0)
			goto board_free;

		rc = msm_camera_power_up(power_info, e_ctrl->eeprom_device_type,
			&e_ctrl->i2c_client);
		if (rc) {
			pr_err("failed rc %d\n", rc);
			goto memdata_free;
		}
		rc = read_eeprom_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc < 0) {
			pr_err("%s read_eeprom_memory failed\n", __func__);
			goto power_down;
		}
		for (j = 0; j < e_ctrl->cal_data.num_data; j++)
			CDBG("memory_data[%d] = 0x%X\n", j,
				e_ctrl->cal_data.mapdata[j]);

		e_ctrl->is_supported |= msm_eeprom_match_crc(&e_ctrl->cal_data);

		rc = msm_camera_power_down(power_info,
			e_ctrl->eeprom_device_type, &e_ctrl->i2c_client);
		if (rc) {
			pr_err("failed rc %d\n", rc);
			goto memdata_free;
		}
	} else
		e_ctrl->is_supported = 1;

	v4l2_subdev_init(&e_ctrl->msm_sd.sd,
		e_ctrl->eeprom_v4l2_subdev_ops);
	v4l2_set_subdevdata(&e_ctrl->msm_sd.sd, e_ctrl);
	platform_set_drvdata(pdev, &e_ctrl->msm_sd.sd);
	e_ctrl->msm_sd.sd.internal_ops = &msm_eeprom_internal_ops;
	e_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(e_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(e_ctrl->msm_sd.sd.name), "msm_eeprom");
	media_entity_pads_init(&e_ctrl->msm_sd.sd.entity, 0, NULL);
	e_ctrl->msm_sd.sd.entity.function = MSM_CAMERA_SUBDEV_EEPROM;
	msm_sd_register(&e_ctrl->msm_sd);

#ifdef CONFIG_COMPAT
	msm_cam_copy_v4l2_subdev_fops(&msm_eeprom_v4l2_subdev_fops);
	msm_eeprom_v4l2_subdev_fops.compat_ioctl32 =
		msm_eeprom_subdev_fops_ioctl32;
	e_ctrl->msm_sd.sd.devnode->fops = &msm_eeprom_v4l2_subdev_fops;
#endif

	e_ctrl->is_supported = (e_ctrl->is_supported << 1) | 1;
	CDBG("%s X\n", __func__);
	return rc;

power_down:
	msm_camera_power_down(power_info, e_ctrl->eeprom_device_type,
		&e_ctrl->i2c_client);
memdata_free:
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
board_free:
	kfree(e_ctrl->eboard_info);
cciclient_free:
	kfree(e_ctrl->i2c_client.cci_client);
ectrl_free:
	kfree(e_ctrl);
	return rc;
}

static int msm_eeprom_platform_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct msm_eeprom_ctrl_t  *e_ctrl;

	if (!sd) {
		pr_err("%s: Subdevice is NULL\n", __func__);
		return 0;
	}

	e_ctrl = (struct msm_eeprom_ctrl_t *)v4l2_get_subdevdata(sd);
	if (!e_ctrl) {
		pr_err("%s: eeprom device is NULL\n", __func__);
		return 0;
	}

	if (!e_ctrl->eboard_info) {
		pr_err("%s: board info is NULL\n", __func__);
		return 0;
	}

	msm_camera_put_clk_info(e_ctrl->pdev,
		&e_ctrl->eboard_info->power_info.clk_info,
		&e_ctrl->eboard_info->power_info.clk_ptr,
		e_ctrl->eboard_info->power_info.clk_info_size);

	kfree(e_ctrl->i2c_client.cci_client);
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
	if (e_ctrl->eboard_info) {
		kfree(e_ctrl->eboard_info->power_info.gpio_conf);
		kfree(e_ctrl->eboard_info);
	}
	kfree(e_ctrl);
	return 0;
}

static const struct of_device_id msm_eeprom_dt_match[] = {
	{ .compatible = "qcom,eeprom" },
	{ }
};

MODULE_DEVICE_TABLE(of, msm_eeprom_dt_match);

static struct platform_driver msm_eeprom_platform_driver = {
	.driver = {
		.name = "qcom,eeprom",
		.owner = THIS_MODULE,
		.of_match_table = msm_eeprom_dt_match,
	},
	.probe = msm_eeprom_platform_probe,
	.remove = msm_eeprom_platform_remove,
};

static const struct i2c_device_id msm_eeprom_i2c_id[] = {
	{ "msm_eeprom", (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver msm_eeprom_i2c_driver = {
	.id_table = msm_eeprom_i2c_id,
	.probe  = msm_eeprom_i2c_probe,
	.remove = msm_eeprom_i2c_remove,
	.driver = {
		.name = "msm_eeprom",
	},
};

static struct spi_driver msm_eeprom_spi_driver = {
	.driver = {
		.name = "qcom_eeprom",
		.owner = THIS_MODULE,
		.of_match_table = msm_eeprom_dt_match,
	},
	.probe = msm_eeprom_spi_probe,
	.remove = msm_eeprom_spi_remove,
};

static int __init msm_eeprom_init_module(void)
{
	int rc = 0;

	CDBG("%s E\n", __func__);
	rc = platform_driver_register(&msm_eeprom_platform_driver);
	CDBG("%s:%d platform rc %d\n", __func__, __LINE__, rc);
	rc = spi_register_driver(&msm_eeprom_spi_driver);
	CDBG("%s:%d spi rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_eeprom_i2c_driver);
}

static void __exit msm_eeprom_exit_module(void)
{
	platform_driver_unregister(&msm_eeprom_platform_driver);
	spi_unregister_driver(&msm_eeprom_spi_driver);
	i2c_del_driver(&msm_eeprom_i2c_driver);
}

module_init(msm_eeprom_init_module);
module_exit(msm_eeprom_exit_module);
MODULE_DESCRIPTION("MSM EEPROM driver");
MODULE_LICENSE("GPL v2");
