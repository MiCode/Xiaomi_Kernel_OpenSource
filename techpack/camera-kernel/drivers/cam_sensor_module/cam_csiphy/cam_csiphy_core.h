/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_CSIPHY_CORE_H_
#define _CAM_CSIPHY_CORE_H_

#include <linux/irqreturn.h>
#include "cam_csiphy_dev.h"
#include <cam_mem_mgr.h>
#include <cam_req_mgr_util.h>
#include <cam_io_util.h>

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API programs CSIPhy IRQ  registers
 */
void cam_csiphy_cphy_irq_config(struct csiphy_device *csiphy_dev);

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API resets CSIPhy hardware
 */
void cam_csiphy_reset(struct csiphy_device *csiphy_dev);

/**
 * @csiphy_dev: CSIPhy device structure
 * @arg:    Camera control command argument
 *
 * This API handles the camera control argument reached to CSIPhy
 */
int cam_csiphy_core_cfg(void *csiphy_dev, void *arg);

/**
 * @irq_num: IRQ number
 * @data: CSIPhy device structure
 *
 * This API handles CSIPhy IRQs
 */
irqreturn_t cam_csiphy_irq(int irq_num, void *data);

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API handles the CSIPhy close
 */
void cam_csiphy_shutdown(struct csiphy_device *csiphy_dev);

/**
 * @soc_idx : CSIPHY cell index
 *
 * This API registers base address per soc_idx
 */
int cam_csiphy_register_baseaddress(struct csiphy_device *csiphy_dev);

/**
 * @get_access : Get Access for the Main Camera over AON Camera
 * @phy_idx    : To acquire the correct PHY hw to do the operation with
 *
 * This API provides Utility/helper function to program the MUX for
 * correct PHY hw.
 *
 */
int cam_csiphy_util_update_aon_ops(bool get_access, uint32_t phy_idx);


/**
 * @csiphy_dev : CSIPhy device structure
 *
 * This API updates the auxiliary settings mask for the current data rate
 *
 */
void cam_csiphy_update_auxiliary_mask(struct csiphy_device *csiphy_dev);

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API allows to print all the cphy/dphy specific status registers
 */
int cam_csiphy_dump_status_reg(struct csiphy_device *csiphy_dev);

/**
 * @phy_idx    : To acquire the correct PHY hw to do the operation with
 *
 * This API provides Utility/helper function to register AON Hw supprot for
 * correct PHY hw.
 *
 */
int cam_csiphy_util_update_aon_registration(uint32_t phy_idx, bool is_aon_user);
#endif /* _CAM_CSIPHY_CORE_H_ */
