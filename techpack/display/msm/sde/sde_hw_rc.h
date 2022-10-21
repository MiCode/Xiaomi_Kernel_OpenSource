/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_RC_H
#define _SDE_HW_RC_H

#include "sde_hw_mdss.h"


/**
 * sde_hw_rc_init -  Initialize RC internal state object and ops
 * @hw_dspp: DSPP instance.
 * Return: 0 on success, non-zero otherwise.
 */
int sde_hw_rc_init(struct sde_hw_dspp *hw_dspp);

/**
 * sde_hw_rc_check_mask -  Validate RC mask configuration
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success, non-zero otherwise.
 */
int sde_hw_rc_check_mask(struct sde_hw_dspp *hw_dspp, void *cfg);

/**
 * sde_hw_rc_setup_mask -  Setup RC mask configuration
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success, non-zero otherwise.
 */
int sde_hw_rc_setup_mask(struct sde_hw_dspp *hw_dspp, void *cfg);

/**
 * sde_hw_rc_check_pu_roi -  Validate RC partial update region of interest
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success.
 *         > 0 on early return.
 *         < 0 on error.
 */
int sde_hw_rc_check_pu_roi(struct sde_hw_dspp *hw_dspp, void *cfg);

/**
 * sde_hw_rc_setup_pu_roi -  Setup RC partial update region of interest
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success.
 *         > 0 on early return.
 *         < 0 on error.
 */
int sde_hw_rc_setup_pu_roi(struct sde_hw_dspp *hw_dspp, void *cfg);

/**
 * sde_hw_rc_setup_data_ahb - Program mask data with AHB
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success, non-zero otherwise.
 */
int sde_hw_rc_setup_data_ahb(struct sde_hw_dspp *hw_dspp, void *cfg);

/**
 * sde_hw_rc_setup_data_dma - Program mask data with DMA
 * @hw_dspp: DSPP instance.
 * @cfg: Pointer to configuration blob.
 * Return: 0 on success, non-zero otherwise.
 */
int sde_hw_rc_setup_data_dma(struct sde_hw_dspp *hw_dspp, void *cfg);

#endif
