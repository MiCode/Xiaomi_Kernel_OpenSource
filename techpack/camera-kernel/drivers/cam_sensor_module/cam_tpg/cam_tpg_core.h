/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __CAM_TPG_CORE_H__
#define __CAM_TPG_CORE_H__

#include "cam_tpg_dev.h"

/**
 * @brief : do clean up of driver on user space process restart
 *
 * @param tpg_dev: tpg device
 *
 * @return : 0 on success
 */
int cam_tpg_shutdown(struct cam_tpg_device *tpg_dev);

/**
 * @brief initialize crm interface
 *
 * @param tpg_dev: tpg device instance
 *
 * @return : 0 on success
 */
int tpg_crm_intf_init(struct cam_tpg_device *tpg_dev);

/**
 * @brief : handle tpg device configuration
 *
 * @param tpg_dev : tpg device instance
 * @param arg     : configuration argument
 *
 * @return        : 0 on success
 */
int32_t cam_tpg_core_cfg(struct cam_tpg_device *tpg_dev, void *arg);

#endif
