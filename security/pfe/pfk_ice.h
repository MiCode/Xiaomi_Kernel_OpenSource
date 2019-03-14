/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

int pfk_ice_init(void);
int pfk_ice_deinit(void);

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			char *storage_type, unsigned int data_unit);
int qti_pfk_ice_invalidate_key(uint32_t index, char *storage_type);

#endif /* PFK_ICE_H_ */
