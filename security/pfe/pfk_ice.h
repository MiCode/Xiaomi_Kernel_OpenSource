/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <crypto/ice.h>

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			struct ice_device *ice_dev, unsigned int data_unit,
			size_t size);
int qti_pfk_ice_invalidate_key(uint32_t index, struct ice_device *ice_dev);

#endif /* PFK_ICE_H_ */
