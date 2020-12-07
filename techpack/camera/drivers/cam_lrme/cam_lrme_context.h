/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_LRME_CONTEXT_H_
#define _CAM_LRME_CONTEXT_H_

#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_interface.h"

#define CAM_LRME_CTX_INDEX_SHIFT 16

/**
 * struct cam_lrme_context
 *
 * @base      : Base context pointer for this LRME context
 * @req_base  : List of base request for this LRME context
 */
struct cam_lrme_context {
	struct cam_context         *base;
	struct cam_ctx_request      req_base[CAM_CTX_REQ_MAX];
	uint64_t index;
};

int cam_lrme_context_init(struct cam_lrme_context *lrme_ctx,
	struct cam_context *base_ctx, struct cam_hw_mgr_intf *hw_intf,
	uint32_t index);
int cam_lrme_context_deinit(struct cam_lrme_context *lrme_ctx);

#endif /* _CAM_LRME_CONTEXT_H_ */
