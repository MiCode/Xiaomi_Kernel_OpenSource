/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef __CRYPTO_QTI_HWKM_H
#define __CRYPTO_QTI_HWKM_H

#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
extern int crypto_qti_program_key(uint32_t index, uint8_t *key,
				  struct ice_device *ice_dev,
				  unsigned int data_unit);

extern int crypto_qti_invalidate_key(struct ice_device *ice_dev,
				     uint32_t index);

#else
static inline int crypto_qti_hwkm_program_key(uint32_t index, uint8_t *key,
					      struct ice_device *ice_dev,
					      unsigned int data_unit)
{
	return -ENODEV;
}

static inline int crypto_qti_hwkm_invalidate_key(struct ice_device *ice_dev,
						 uint32_t index)
{
	return -ENODEV;
}

#endif //CONFIG_QTI_HW_KEY_MANAGER
#endif //__CRYPTO_QTI_HWKM_H
