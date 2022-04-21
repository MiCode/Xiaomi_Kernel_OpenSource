/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HXP_CTRL_H
#define MTK_HXP_CTRL_H

#include <linux/atomic.h>

// Forward declaration
struct mtk_hxp;

struct hxp_ctrl {
	atomic_t have_slb;
};

int hxp_ctrl_init(struct mtk_hxp *hxp_dev);

int hxp_ctrl_set(struct mtk_hxp *device,
	int32_t code, int32_t value, size_t size);

int hxp_ctrl_reset(struct mtk_hxp *device);

int hxp_ctrl_uninit(struct mtk_hxp *hxp_dev);

#endif  // MTK_HXP_CTRL_H
