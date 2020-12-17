/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_OPE_CONTEXT_H_
#define _CAM_OPE_CONTEXT_H_

#include "cam_context.h"

#define OPE_CTX_MAX 32

/**
 * struct cam_ope_context - ope context
 * @base:           ope context object
 * @state_machine:  state machine for OPE context
 * @req_base:       common request structure
 * @state:          ope context state
 * @ctxt_to_hw_map: context to FW handle mapping
 */
struct cam_ope_context {
	struct cam_context *base;
	struct cam_ctx_ops *state_machine;
	struct cam_ctx_request req_base[CAM_CTX_REQ_MAX];
	uint32_t state;
	void *ctxt_to_hw_map;
};

/**
 * cam_ope_context_init() - OPE context init
 * @ctx:     Pointer to context
 * @hw_intf: Pointer to OPE hardware interface
 * @ctx_id:  ID for this context
 */
int cam_ope_context_init(struct cam_ope_context *ctx,
	struct cam_hw_mgr_intf *hw_intf, uint32_t ctx_id);

/**
 * cam_ope_context_deinit() - OPE context deinit
 * @ctx: Pointer to context
 */
int cam_ope_context_deinit(struct cam_ope_context *ctx);

#endif /* _CAM_OPE_CONTEXT_H_ */
