// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_DEBUGFS_H_
#define _MI_DISP_DEBUGFS_H_

#include "mi_disp_config.h"

#ifdef CONFIG_MI_DISP_DEBUGFS
int mi_disp_debugfs_init(void *d_display, int disp_id);
int mi_disp_debugfs_deinit(void *d_display, int disp_id);
bool is_enable_debug_log(void);
#else
static inline int mi_disp_debugfs_init(void *d_display, int disp_id) { return 0; }
static inline int mi_disp_debugfs_deinit(void *d_display, int disp_id) { return 0; }
static inline bool is_enable_debug_log(void) { return 0; }
#endif

#endif /* _MI_DISP_DEBUGFS_H_ */
