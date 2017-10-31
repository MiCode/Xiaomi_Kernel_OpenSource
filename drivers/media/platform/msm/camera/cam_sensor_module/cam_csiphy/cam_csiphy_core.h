/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#endif /* _CAM_CSIPHY_CORE_H_ */
