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
#ifndef _CAM_EEPROM_SOC_H_
#define _CAM_EEPROM_SOC_H_

#include "cam_eeprom_dev.h"

int cam_eeprom_spi_parse_of(struct cam_sensor_spi_client *client);

int cam_eeprom_parse_dt_memory_map(struct device_node *of,
	struct cam_eeprom_memory_block_t *data);

int cam_eeprom_parse_dt(struct cam_eeprom_ctrl_t *e_ctrl);
#endif/* _CAM_EEPROM_SOC_H_ */
