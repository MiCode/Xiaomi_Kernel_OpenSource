/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
 * @bh_cmd:              Tasklet cmd used to enqueue task
 * @handler_priv:        Private Handler data that will be passed to the
 *                       handler function
 * @evt_payload_priv:    Event payload that will be passed to the handler
 *                       function
 * @bottom_half_handler: Callback function that will be called by tasklet
 *                       for handling event
 *
 * @return:              Void
 */
void cam_tasklet_enqueue_cmd(
	void                              *bottom_half,
	void                              *bh_cmd,
	void                              *handler_priv,
	void                              *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler);

/**
 * cam_tasklet_get_cmd()
 *
 * @brief:              Get free cmd from tasklet
 *
 * @bottom_half:        Tasklet Info structure to get cmd from
 * @bh_cmd:             Return tasklet_cmd pointer if successful
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_tasklet_get_cmd(void *bottom_half, void **bh_cmd);

/**
 * cam_tasklet_put_cmd()
 *
 * @brief:              Put back cmd to free list
 *
 * @bottom_half:        Tasklet Info structure to put cmd into
 * @bh_cmd:             tasklet_cmd pointer that needs to be put back
 *
 * @return:             Void
 */
void cam_tasklet_put_cmd(void *bottom_half, void **bh_cmd);

extern struct cam_irq_bh_api tasklet_bh_api;

#endif /* _CAM_TASKLET_UTIL_H_ */
