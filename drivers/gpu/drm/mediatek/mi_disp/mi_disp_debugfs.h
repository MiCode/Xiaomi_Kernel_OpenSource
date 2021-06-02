/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MI_DISP_DEBUGFS_H_
#define _MI_DISP_DEBUGFS_H_

#include "mi_disp_config.h"

#if MI_DISP_DEBUGFS_ENABLE
int mi_disp_debugfs_init(void *d_display, int disp_id);
int mi_disp_debugfs_deinit(void *d_display, int disp_id);
bool is_enable_debug_log(void);

#else
static inline int mi_disp_debugfs_init(void *d_display, int disp_id) { return 0; }
static inline int mi_disp_debugfs_deinit(void *d_display, int disp_id) { return 0; }
static inline bool is_enable_debug_log(void) { return 0; }
#endif


#endif /* _MI_DISP_DEBUGFS_H_ */
