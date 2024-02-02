/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_CCI_CORE_H_
#define _CAM_CCI_CORE_H_

#include <linux/irqreturn.h>
#include <media/cam_sensor.h>
#include <media/ais_sensor.h>
#include "cam_cci_dev.h"
#include "cam_cci_soc.h"

/**
 * @cci_dev: CCI device structure
 * @c_ctrl: CCI control structure
 *
 * This API gets CCI clk rates
 */
void cam_cci_get_clk_rates(struct cci_device *cci_dev,
	struct cam_cci_ctrl *c_ctrl);

/**
 * @sd: V4L2 sub device
 * @c_ctrl: CCI control structure
 *
 * This API handles I2C operations for CCI
 */
int32_t cam_cci_core_cfg(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *cci_ctrl);

/**
 * @sd: V4L2 sub device
 * @arg: user argument
 *
 * This API handles user I2C operations for CCI
 */
int32_t cam_cci_core_cam_ctrl(struct v4l2_subdev *sd,
	void *arg);

/**
 * @irq_num: IRQ number
 * @data: CCI private structure
 *
 * This API handles CCI IRQs
 */
irqreturn_t cam_cci_irq(int irq_num, void *data);

#endif /* _CAM_CCI_CORE_H_ */
