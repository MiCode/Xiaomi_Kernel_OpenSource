/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CRE_TOP_H
#define CRE_TOP_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_cre.h>
#include "cre_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_soc_util.h"
#include "cam_context.h"
#include "cam_cre_context.h"
#include "cam_cre_hw_mgr.h"

/**
 * struct cre_top_ctx
 *
 * @cre_acquire: CRE acquire info
 */
struct cre_top_ctx {
	struct cam_cre_acquire_dev_info *cre_acquire;
};

/**
 * struct cre_top
 *
 * @cre_hw_info:    CRE hardware info
 * @top_ctx:        CRE top context
 * @reset_complete: Reset complete flag
 * @cre_mutex:      CRE hardware mutex
 * @hw_lock:        CRE hardware spinlock
 */
struct cre_top {
	struct cam_cre_hw *cre_hw_info;
	struct cre_top_ctx top_ctx[CAM_CRE_CTX_MAX];
	struct completion reset_complete;
	struct completion idle_done;
	struct completion bufdone;
	struct mutex      cre_hw_mutex;
	spinlock_t        hw_lock;
};
#endif /* CRE_TOP_H */
