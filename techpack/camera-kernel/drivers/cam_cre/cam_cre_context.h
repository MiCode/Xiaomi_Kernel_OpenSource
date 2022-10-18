/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CRE_CONTEXT_H_
#define _CAM_CRE_CONTEXT_H_

#include <media/cam_cre.h>

#include "cam_context.h"
#include "cam_cre_hw_mgr_intf.h"

#define CAM_CRE_HW_EVENT_MAX 20

/**
 * struct cam_cre_context - CRE context
 * @base: Base cre cam context object
 * @req_base: Common request structure
 */
struct cam_cre_context {
	struct cam_context *base;
	struct cam_ctx_request req_base[CAM_CTX_REQ_MAX];
};

/* cam cre context irq handling function type */
typedef int (*cam_cre_hw_event_cb_func)(
	struct cam_cre_context *ctx_cre,
	void *evt_data);

/**
 * struct cam_cre_ctx_irq_ops - Function table for handling IRQ callbacks
 *
 * @irq_ops: Array of handle function pointers.
 *
 */
struct cam_cre_ctx_irq_ops {
	cam_cre_hw_event_cb_func irq_ops[CAM_CRE_HW_EVENT_MAX];
};

/**
 * cam_cre_context_init()
 *
 * @brief: Initialization function for the CRE context
 *
 * @ctx: CRE context obj to be initialized
 * @hw_intf: CRE hw manager interface
 * @ctx_id: ID for this context
 * @img_iommu_hdl: IOMMU HDL for image buffers
 *
 */
int cam_cre_context_init(struct cam_cre_context *ctx,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id,
	int img_iommu_hdl);

/**
 * cam_cre_context_deinit()
 *
 * @brief: Deinitialize function for the CRE context
 *
 * @ctx: CRE context obj to be deinitialized
 *
 */
int cam_cre_context_deinit(struct cam_cre_context *ctx);

#endif  /* __CAM_CRE_CONTEXT_H__ */
