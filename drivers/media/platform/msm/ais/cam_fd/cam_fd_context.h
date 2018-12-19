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

#ifndef _CAM_FD_CONTEXT_H_
#define _CAM_FD_CONTEXT_H_

#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_hw_mgr_intf.h"
#include "cam_req_mgr_interface.h"

/**
 * struct cam_fd_context - Face Detection context information
 *
 * @base     : Base context pointer for this FD context
 * @req_base : List of base requests for this FD context
 */
struct cam_fd_context {
	struct cam_context       *base;
	struct cam_ctx_request    req_base[CAM_CTX_REQ_MAX];
};

int cam_fd_context_init(struct cam_fd_context *fd_ctx,
	struct cam_context *base_ctx, struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id);
int cam_fd_context_deinit(struct cam_fd_context *ctx);

#endif /* _CAM_FD_CONTEXT_H_ */
