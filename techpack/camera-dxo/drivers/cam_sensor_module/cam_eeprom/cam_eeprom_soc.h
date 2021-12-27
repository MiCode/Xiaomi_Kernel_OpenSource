/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */
#ifndef _CAM_EEPROM_SOC_H_
#define _CAM_EEPROM_SOC_H_

#include "cam_eeprom_dev.h"

int cam_eeprom_spi_parse_of(struct cam_sensor_spi_client *client);

int cam_eeprom_parse_dt_memory_map(struct device_node *of,
	struct cam_eeprom_memory_block_t *data);

int cam_eeprom_parse_dt(struct cam_eeprom_ctrl_t *e_ctrl);
#endif/* _CAM_EEPROM_SOC_H_ */
