/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_HW_H_
#define _CAM_HW_H_

#include "cam_soc_util.h"

/*
 * This file declares Enums, Structures and APIs to be used as template
 * when writing any HW driver in the camera subsystem.
 */

/* Hardware state enum */
enum cam_hw_state {
	CAM_HW_STATE_POWER_DOWN,
	CAM_HW_STATE_POWER_UP,
};

/**
 * struct cam_hw_info - Common hardware information
 *
 * @hw_mutex:              Hardware mutex
 * @hw_lock:               Hardware spinlock
 * @hw_complete:           Hardware Completion
 * @open_count:            Count to track the HW enable from the client
 * @hw_state:              Hardware state
 * @soc_info:              Platform SOC properties for hardware
 * @node_info:             Private HW data related to nodes
 * @core_info:             Private HW data related to core logic
 *
 */
struct cam_hw_info {
	struct mutex                    hw_mutex;
	spinlock_t                      hw_lock;
	struct completion               hw_complete;
	uint32_t                        open_count;
	enum cam_hw_state               hw_state;
	struct cam_hw_soc_info          soc_info;
	void                           *node_info;
	void                           *core_info;
};

#endif /* _CAM_HW_H_ */
