/*
 *  linux/drivers/mmc/host/msm_sdcc_dml.h - Qualcomm SDCC DML driver
 *					    header file
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MSM_SDCC_DML_H
#define _MSM_SDCC_DML_H

#include <linux/types.h>
#include <linux/mmc/host.h>

#include "msm_sdcc.h"

#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
/**
 * Initialize DML HW connected with SDCC core
 *
 * This function initialize DML HW.
 *
 * This function should only be called once
 * typically during driver probe.
 *
 * @host - Pointer to sdcc host structure
 *
 * @return - 0 if successful else negative value.
 *
 */
int msmsdcc_dml_init(struct msmsdcc_host *host);

/**
 * Start data transfer.
 *
 * This function configure DML HW registers with
 * data transfer direction and data transfer size.
 *
 * This function should be called after submitting
 * data transfer request to SPS HW and before kick
 * starting data transfer in SDCC core.
 *
 * @host - Pointer to sdcc host structure
 * @data - Pointer to mmc_data structure
 *
 */
void msmsdcc_dml_start_xfer(struct msmsdcc_host *host, struct mmc_data *data);

/**
 * Checks if DML HW is busy or not?
 *
 * @host - Pointer to sdcc host structure
 *
 * @return - 1 if DML HW is busy with data transfer
 *           0 if DML HW is IDLE.
 *
 */
bool msmsdcc_is_dml_busy(struct msmsdcc_host *host);

/**
 * Soft reset DML HW
 *
 * This function give soft reset to DML HW.
 *
 * This function should be called to reset DML HW
 * if data transfer error is detected.
 *
 * @host - Pointer to sdcc host structure
 *
 */
void msmsdcc_dml_reset(struct msmsdcc_host *host);

/**
 * Deinitialize DML HW connected with SDCC core
 *
 * This function resets DML HW and unmap DML
 * register region.
 *
 * This function should only be called once
 * typically during driver remove.
 *
 * @host - Pointer to sdcc host structure
 *
 */
void msmsdcc_dml_exit(struct msmsdcc_host *host);
#else
static inline int msmsdcc_dml_init(struct msmsdcc_host *host) { return 0; }
static inline int msmsdcc_dml_start_xfer(struct msmsdcc_host *host,
				struct mmc_data *data) { return 0; }
static inline bool msmsdcc_is_dml_busy(
				struct msmsdcc_host *host) { return 0; }
static inline void msmsdcc_dml_reset(struct msmsdcc_host *host) { }
static inline void msmsdcc_dml_exit(struct msmsdcc_host *host) { }
#endif /* CONFIG_MMC_MSM_SPS_SUPPORT */

#endif /* _MSM_SDCC_DML_H */
