/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef PFK_ICE_H_
#define PFK_ICE_H_

/*
 * PFK ICE
 *
 * ICE keys configuration through scm calls.
 *
 */

#include <linux/types.h>

struct pfk_ice_key_req {
	uint32_t cmd_id;
	uint32_t index;
	uint32_t ice_key_offset;
	uint32_t ice_key_size;
	uint32_t ice_salt_offset;
	uint32_t ice_salt_size;
} __packed;

struct pfk_ice_key_rsp {
	uint32_t ret;
	uint32_t cmd_id;
} __packed;

struct pfk_km_get_version_req {
	uint32_t cmd_id;
} __packed;

struct pfk_km_get_version_rsp {
	int status;
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t ta_major_version;
	uint32_t ta_minor_version;
} __packed;

int pfk_ice_init(void);
int pfk_ice_deinit(void);

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			char *storage_type, unsigned int data_unit);
int qti_pfk_ice_invalidate_key(uint32_t index, char *storage_type);

#endif /* PFK_ICE_H_ */
