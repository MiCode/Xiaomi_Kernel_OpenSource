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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>

#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"

#define cam_eeprom_spi_parse_cmd(spi_dev, name, out)          \
	{                                                     \
		spi_dev->cmd_tbl.name.opcode = out[0];        \
		spi_dev->cmd_tbl.name.addr_len = out[1];      \
		spi_dev->cmd_tbl.name.dummy_len = out[2];     \
		spi_dev->cmd_tbl.name.delay_intv = out[3];    \
		spi_dev->cmd_tbl.name.delay_count = out[4];   \
	}

int cam_eeprom_spi_parse_of(struct cam_sensor_spi_client *spi_dev)
{
	int rc = -EFAULT;
	uint32_t tmp[5];

	rc  = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-read", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, read, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get read data");
		return -EFAULT;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-readseq", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, read_seq, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get readseq data");
		return -EFAULT;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-queryid", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, query_id, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get queryid data");
		return -EFAULT;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-pprog", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, page_program, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get page program data");
		return -EFAULT;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-wenable", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, write_enable, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get write enable data");
		return rc;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-readst", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, read_status, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get readdst data");
		return rc;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"spiop-erase", tmp, 5);
	if (!rc) {
		cam_eeprom_spi_parse_cmd(spi_dev, erase, tmp);
	} else {
		CAM_ERR(CAM_EEPROM, "Failed to get erase data");
		return rc;
	}

	rc = of_property_read_u32_array(spi_dev->spi_master->dev.of_node,
		"eeprom-id", tmp, 2);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "Failed to get eeprom id");
		return rc;
	}

	spi_dev->mfr_id0 = tmp[0];
	spi_dev->device_id0 = tmp[1];

	return 0;
}

/*
 * cam_eeprom_parse_memory_map() - parse memory map in device node
 * @of:         device node
 * @data:       memory block for output
 *
 * This functions parses @of to fill @data.  It allocates map itself, parses
 * the @of node, calculate total data length, and allocates required buffer.
 * It only fills the map, but does not perform actual reading.
 */
int cam_eeprom_parse_dt_memory_map(struct device_node *node,
	struct cam_eeprom_memory_block_t *data)
{
	int       i, rc = 0;
	char      property[PROPERTY_MAXSIZE];
	uint32_t  count = MSM_EEPROM_MEM_MAP_PROPERTIES_CNT;
	struct    cam_eeprom_memory_map_t *map;

	snprintf(property, PROPERTY_MAXSIZE, "num-blocks");
	rc = of_property_read_u32(node, property, &data->num_map);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "failed: num-blocks not available rc %d",
			rc);
		return rc;
	}

	map = vzalloc((sizeof(*map) * data->num_map));
	if (!map) {
		rc = -ENOMEM;
		return rc;
	}
	data->map = map;

	for (i = 0; i < data->num_map; i++) {
		snprintf(property, PROPERTY_MAXSIZE, "page%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].page, count);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "failed: page not available rc %d",
				rc);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "pageen%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].pageen, count);
		if (rc < 0)
			CAM_DBG(CAM_EEPROM, "pageen not needed");

		snprintf(property, PROPERTY_MAXSIZE, "saddr%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].saddr, 1);
		if (rc < 0)
			CAM_DBG(CAM_EEPROM, "saddr not needed - block %d", i);

		snprintf(property, PROPERTY_MAXSIZE, "poll%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].poll, count);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "failed: poll not available rc %d",
				rc);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "mem%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].mem, count);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "failed: mem not available rc %d",
				rc);
			goto ERROR;
		}
		data->num_data += map[i].mem.valid_size;
	}

	data->mapdata = vzalloc(data->num_data);
	if (!data->mapdata) {
		rc = -ENOMEM;
		goto ERROR;
	}
	return rc;

ERROR:
	vfree(data->map);
	memset(data, 0, sizeof(*data));
	return rc;
}

/**
 * @e_ctrl: ctrl structure
 *
 * Parses eeprom dt
 */
static int cam_eeprom_get_dt_data(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int                             rc = 0;
	struct cam_hw_soc_info         *soc_info = &e_ctrl->soc_info;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;
	struct device_node             *of_node = NULL;

	of_node = soc_info->dev->of_node;

	if (e_ctrl->userspace_probe == false) {
		rc = cam_get_dt_power_setting_data(of_node,
			soc_info, power_info);
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM, "failed in getting power settings");
			return rc;
		}
	}

	if (!soc_info->gpio_data) {
		CAM_INFO(CAM_EEPROM, "No GPIO found");
		return 0;
	}

	if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
		CAM_INFO(CAM_EEPROM, "No GPIO found");
		return -EINVAL;
	}

	rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
		&power_info->gpio_num_info);
	if ((rc < 0) || (!power_info->gpio_num_info)) {
		CAM_ERR(CAM_EEPROM, "No/Error EEPROM GPIOs");
		return -EINVAL;
	}

	return rc;
}

/**
 * @eb_info: eeprom private data structure
 * @of_node: eeprom device node
 *
 * This function parses the eeprom dt to get the MM data
 */
static int cam_eeprom_cmm_dts(struct cam_eeprom_soc_private *eb_info,
	struct device_node *of_node)
{
	int                      rc = 0;
	struct cam_eeprom_cmm_t *cmm_data = &eb_info->cmm_data;

	cmm_data->cmm_support =
		of_property_read_bool(of_node, "cmm-data-support");
	if (!cmm_data->cmm_support) {
		CAM_DBG(CAM_EEPROM, "No cmm support");
		return 0;
	}

	cmm_data->cmm_compression =
		of_property_read_bool(of_node, "cmm-data-compressed");

	rc = of_property_read_u32(of_node, "cmm-data-offset",
		&cmm_data->cmm_offset);
	if (rc < 0)
		CAM_DBG(CAM_EEPROM, "No MM offset data rc %d", rc);

	rc = of_property_read_u32(of_node, "cmm-data-size",
		&cmm_data->cmm_size);
	if (rc < 0)
		CAM_DBG(CAM_EEPROM, "No MM size data rc %d", rc);

	CAM_DBG(CAM_EEPROM, "cmm_compr %d, cmm_offset %d, cmm_size %d",
		cmm_data->cmm_compression, cmm_data->cmm_offset,
		cmm_data->cmm_size);
	return 0;
}

/**
 * @e_ctrl: ctrl structure
 *
 * This function is called from cam_eeprom_platform/i2c/spi_driver_probe
 * it parses the eeprom dt node and decides for userspace or kernel probe.
 */
int cam_eeprom_parse_dt(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int                             i, rc = 0;
	struct cam_hw_soc_info         *soc_info = &e_ctrl->soc_info;
	struct device_node             *of_node = NULL;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	uint32_t                        temp;

	if (!soc_info->dev) {
		CAM_ERR(CAM_EEPROM, "Dev is NULL");
		return -EINVAL;
	}

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "Failed to read DT properties rc : %d", rc);
		return rc;
	}

	of_node = soc_info->dev->of_node;

	rc = of_property_read_string(of_node, "eeprom-name",
		&soc_private->eeprom_name);
	if (rc < 0) {
		CAM_DBG(CAM_EEPROM, "kernel probe is not enabled");
		e_ctrl->userspace_probe = true;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = of_property_read_u32(of_node, "cci-master",
			&e_ctrl->cci_i2c_master);
		if (rc < 0 || (e_ctrl->cci_i2c_master >= MASTER_MAX)) {
			CAM_DBG(CAM_EEPROM, "failed rc %d", rc);
			rc = -EFAULT;
			return rc;
		}
	}

	if (e_ctrl->io_master_info.master_type == SPI_MASTER) {
		rc = cam_eeprom_cmm_dts(soc_private, soc_info->dev->of_node);
		if (rc < 0)
			CAM_DBG(CAM_EEPROM, "MM data not available rc %d", rc);
	}

	rc = cam_eeprom_get_dt_data(e_ctrl);
	if (rc < 0)
		CAM_DBG(CAM_EEPROM, "failed: eeprom get dt data rc %d", rc);

	if ((e_ctrl->userspace_probe == false) &&
			(e_ctrl->io_master_info.master_type != SPI_MASTER)) {
		rc = of_property_read_u32(of_node, "slave-addr", &temp);
		if (rc < 0)
			CAM_DBG(CAM_EEPROM, "failed: no slave-addr rc %d", rc);

		soc_private->i2c_info.slave_addr = temp;

		rc = of_property_read_u32(of_node, "i2c-freq-mode", &temp);
		soc_private->i2c_info.i2c_freq_mode = temp;
		if (rc < 0) {
			CAM_ERR(CAM_EEPROM,
				"i2c-freq-mode read fail %d", rc);
			soc_private->i2c_info.i2c_freq_mode = 0;
		}
		if (soc_private->i2c_info.i2c_freq_mode >= I2C_MAX_MODES) {
			CAM_ERR(CAM_EEPROM, "invalid i2c_freq_mode = %d",
				soc_private->i2c_info.i2c_freq_mode);
			soc_private->i2c_info.i2c_freq_mode = 0;
		}
		CAM_DBG(CAM_EEPROM, "slave-addr = 0x%X",
			soc_private->i2c_info.slave_addr);
	}

	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = devm_clk_get(soc_info->dev,
			soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			CAM_ERR(CAM_EEPROM, "get failed for %s",
				soc_info->clk_name[i]);
			rc = -ENOENT;
			return rc;
		}
	}

	return rc;
}
