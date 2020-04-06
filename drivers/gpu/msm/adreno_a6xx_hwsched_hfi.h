/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_A6XX_HWSCHED_HFI_H_
#define _ADRENO_A6XX_HWSCHED_HFI_H_

/**
 * a6xx_hwsched_hfi_init - Initialize hfi resources
 * @adreno_dev - Pointer to adreno device structure
 *
 * This function is used to initialize hfi resources
 * once before the very first gmu boot
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hwsched_hfi_init(struct adreno_device *adreno_dev);
#endif
