/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __DEBUG_PLAT_INTERNAL_H__
#define __DEBUG_PLAT_INTERNAL_H__

#include <linux/platform_device.h>

struct debug_plat_drv {
	int platform_idx;

	u32 apusys_base;
	u32 apusys_reg_size;
	u32 total_dbg_mux_count;
};
#endif /* __DEBUG_PLAT_INTERNAL_H__ */

