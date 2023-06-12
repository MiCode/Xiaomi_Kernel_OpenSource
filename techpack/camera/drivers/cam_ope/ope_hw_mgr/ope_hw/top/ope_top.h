/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef OPE_TOP_H
#define OPE_TOP_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_ope.h>
#include "ope_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_soc_util.h"
#include "cam_context.h"
#include "cam_ope_context.h"
#include "cam_ope_hw_mgr.h"

/**
 * struct ope_top_ctx
 *
 * @ope_acquire: OPE acquire info
 */
struct ope_top_ctx {
	struct ope_acquire_dev_info *ope_acquire;
};

/**
 * struct ope_top
 *
 * @ope_hw_info:    OPE hardware info
 * @top_ctx:        OPE top context
 * @reset_complete: Reset complete flag
 * @ope_mutex:      OPE hardware mutex
 * @hw_lock:        OPE hardware spinlock
 */
struct ope_top {
	struct ope_hw *ope_hw_info;
	struct ope_top_ctx top_ctx[OPE_CTX_MAX];
	struct completion reset_complete;
	struct mutex      ope_hw_mutex;
	spinlock_t        hw_lock;
};
#endif /* OPE_TOP_H */
