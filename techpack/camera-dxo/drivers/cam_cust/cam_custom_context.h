/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CUSTOM_CONTEXT_H_
#define _CAM_CUSTOM_CONTEXT_H_

#include <linux/spinlock.h>
#include <uapi/media/cam_custom.h>
#include <uapi/media/cam_defs.h>

#include "cam_context.h"
#include "cam_custom_hw_mgr_intf.h"

/*
 * Maximum hw resource - This number is based on the maximum
 * output port resource. The current maximum resource number
 * is 2.
 */
#define CAM_CUSTOM_DEV_CTX_RES_MAX                     2

#define CAM_CUSTOM_CTX_CFG_MAX                         8

/* forward declaration */
struct cam_custom_context;

/**
 * struct cam_custom_dev_ctx_req - Custom context request object
 *
 * @base:                  Common request object pointer
 * @cfg:                   Custom hardware configuration array
 * @num_cfg:               Number of custom hardware configuration entries
 * @fence_map_out:         Output fence mapping array
 * @num_fence_map_out:     Number of the output fence map
 * @fence_map_in:          Input fence mapping array
 * @num_fence_map_in:      Number of input fence map
 * @num_acked:             Count to track acked entried for output.
 *                         If count equals the number of fence out, it means
 *                         the request has been completed.
 * @hw_update_data:        HW update data for this request
 *
 */
struct cam_custom_dev_ctx_req {
	struct cam_ctx_request                  *base;
	struct cam_hw_update_entry               cfg
						[CAM_CUSTOM_CTX_CFG_MAX];
	uint32_t                                 num_cfg;
	struct cam_hw_fence_map_entry            fence_map_out
						[CAM_CUSTOM_DEV_CTX_RES_MAX];
	uint32_t                                 num_fence_map_out;
	struct cam_hw_fence_map_entry            fence_map_in
						[CAM_CUSTOM_DEV_CTX_RES_MAX];
	uint32_t                                 num_fence_map_in;
	uint32_t                                 num_acked;
	struct cam_custom_prepare_hw_update_data hw_update_data;
};

/**
 * struct cam_custom_context - Custom device context
 * @base: custom device context object
 * @state_machine: state machine for Custom device context
 * @state: Common context state
 * @hw_ctx: HW object returned by the acquire device command
 * @init_received: Indicate whether init config packet is received
 * @subscribe_event: The irq event mask that CRM subscribes to,
 *                   custom HW will invoke CRM cb at those event.
 * @active_req_cnt: Counter for the active request
 * @frame_id: Frame id tracking for the custom context
 * @req_base: common request structure
 * @req_custom: custom request structure
 *
 */
struct cam_custom_context {
	struct cam_context           *base;
	struct cam_ctx_ops           *state_machine;
	uint32_t                      state;
	void                         *hw_ctx;
	bool                          init_received;
	uint32_t                      subscribe_event;
	uint32_t                      active_req_cnt;
	int64_t                       frame_id;
	struct cam_ctx_request        req_base[CAM_CTX_REQ_MAX];
	struct cam_custom_dev_ctx_req req_custom[CAM_CTX_REQ_MAX];
};


/**
 * cam_custom_dev_context_init()
 *
 * @brief:              Initialization function for the custom context
 *
 * @ctx:                Custom context obj to be initialized
 * @bridge_ops:         Bridge call back funciton
 * @hw_intf:            Cust hw manager interface
 * @ctx_id:             ID for this context
 *
 */
int cam_custom_dev_context_init(struct cam_custom_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *bridge_ops,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id);

/**
 * cam_custom_dev_context_deinit()
 *
 * @brief:               Deinitialize function for the Custom context
 *
 * @ctx:                 Custom context obj to be deinitialized
 *
 */
int cam_custom_dev_context_deinit(struct cam_custom_context *ctx);

#endif  /* _CAM_CUSTOM_CONTEXT_H_ */
