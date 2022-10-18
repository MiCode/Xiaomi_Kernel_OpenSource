/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021, The Linux Foundation. All rights reserved.
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
	uint32_t ctx_id, int img_iommu_hdl);
int cam_fd_context_deinit(struct cam_fd_context *ctx);

#endif /* _CAM_FD_CONTEXT_H_ */
