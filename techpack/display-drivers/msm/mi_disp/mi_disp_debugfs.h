/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DISP_DEBUGFS_H_
#define _MI_DISP_DEBUGFS_H_

#include <linux/types.h>

#include "mi_disp_config.h"

enum backlight_log_mask {
	BACKLIGHT_LOG_NONE = 0,
	BACKLIGHT_LOG_ENABLE = BIT(0),
	BACKLIGHT_LOG_DUMP_STACK = BIT(1),
	BACKLIGHT_LOG_MIX
};

#if MI_DISP_DEBUGFS_ENABLE
int mi_disp_debugfs_init(void *d_display, int disp_id);
int mi_disp_debugfs_deinit(void *d_display, int disp_id);
bool is_enable_debug_log(void);
u32 mi_get_backlight_log_mask(void);
#else
static inline int mi_disp_debugfs_init(void *d_display, int disp_id) { return 0; }
static inline int mi_disp_debugfs_deinit(void *d_display, int disp_id) { return 0; }
static inline bool is_enable_debug_log(void) { return 0; }
static inline u32 mi_get_backlight_log_mask(void) { return 0; }
#endif

#endif /* _MI_DISP_DEBUGFS_H_ */
