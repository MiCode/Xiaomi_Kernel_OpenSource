/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CRE_DEV_H_
#define _CAM_CRE_DEV_H_

#include "cam_subdev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_context.h"
#include "cam_cre_context.h"

/**
 * struct cam_cre_dev - Camera CRE V4l2 device node
 *
 * @sd: Commone camera subdevice node
 * @node: Pointer to cre subdevice
 * @ctx: CRE base context storage
 * @ctx_cre: CRE private context storage
 * @cre_mutex: CRE dev mutex
 * @open_cnt: Open device count
 */
struct cam_cre_dev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CAM_CRE_CTX_MAX];
	struct cam_cre_context ctx_cre[CAM_CRE_CTX_MAX];
	struct mutex cre_mutex;
	int32_t open_cnt;
};

/**
 * @brief : API to register CRE dev to platform framework.
 * @return struct platform_device pointer on success, or ERR_PTR() on error.
 */
int cam_cre_dev_init_module(void);

/**
 * @brief : API to remove CRE dev from platform framework.
 */
void cam_cre_dev_exit_module(void);
#endif /* __CAM_CRE_DEV_H__ */
