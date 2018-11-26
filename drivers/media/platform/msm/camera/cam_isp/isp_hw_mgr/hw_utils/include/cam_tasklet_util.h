/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef _CAM_TASKLET_UTIL_H_
#define _CAM_TASKLET_UTIL_H_

#include "cam_irq_controller.h"

/*
 * cam_tasklet_init()
 *
 * @brief:              Initialize the tasklet info structure
 *
 * @tasklet:            Tasklet to initialize
 * @hw_mgr_ctx:         Private Ctx data that will be passed to the handler
 *                      function
 * @idx:                Index of tasklet used as identity
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_tasklet_init(
	void                   **tasklet,
	void                    *hw_mgr_ctx,
	uint32_t                 idx);

/*
 * cam_tasklet_deinit()
 *
 * @brief:              Deinitialize the tasklet info structure
 *
 * @tasklet:            Tasklet to deinitialize
 *
 * @return:             Void
 */
void cam_tasklet_deinit(void   **tasklet);

/*
 * cam_tasklet_start()
 *
 * @brief:              Enable the tasklet to be scheduled and run.
 *                      Caller should make sure this function is called
 *                      before trying to enqueue.
 *
 * @tasklet:            Tasklet to start
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_tasklet_start(void    *tasklet);

/*
 * cam_tasklet_stop()
 *
 * @brief:              Disable the tasklet so it can no longer be scheduled.
 *                      Need to enable again to run.
 *
 * @tasklet:            Tasklet to stop
 *
 * @return:             Void
 */
void cam_tasklet_stop(void    *tasklet);

/*
 * cam_tasklet_enqueue_cmd()
 *
 * @brief:               Enqueue the tasklet_cmd to used list
 *
 * @bottom_half:         Tasklet info to enqueue onto
 * @handler_priv:        Private Handler data that will be passed to the
 *                       handler function
 * @evt_payload_priv:    Event payload that will be passed to the handler
 *                       function
 * @bottom_half_handler: Callback function that will be called by tasklet
 *                       for handling event
 *
 * @return:              0: Success
 *                       Negative: Failure
 */
int cam_tasklet_enqueue_cmd(
	void                              *bottom_half,
	void                              *handler_priv,
	void                              *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler);

#endif /* _CAM_TASKLET_UTIL_H_ */
