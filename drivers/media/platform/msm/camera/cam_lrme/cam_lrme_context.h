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

#ifndef _CAM_LRME_CONTEXT_H_
#define _CAM_LRME_CONTEXT_H_

#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_interface.h"
#include "cam_sync_api.h"

#define CAM_LRME_CTX_INDEX_SHIFT 32

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
