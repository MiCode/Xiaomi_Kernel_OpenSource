/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021, The Linux Foundation. All rights reserved.
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
 * @presil_hw_lock:        Mutex lock used for presil in place of hw_lock,
 *                         for drivers like CDM
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

#ifdef CONFIG_CAM_PRESIL
	struct mutex                    presil_hw_lock;
#endif
};

#endif /* _CAM_HW_H_ */
