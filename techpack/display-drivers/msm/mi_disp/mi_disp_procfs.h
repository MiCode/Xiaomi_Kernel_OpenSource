/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DISP_PROCFS_H_
#define _MI_DISP_PROCFS_H_

#include "mi_disp_config.h"

#if MI_DISP_PROCFS_ENABLE
int mi_disp_procfs_init(void *d_display, int disp_id);
int mi_disp_procfs_deinit(void *d_display, int disp_id);
#else
static inline int mi_disp_procfs_init(void *d_display, int disp_id) { return 0; }
static inline int mi_disp_procfs_deinit(void *d_display, int disp_id) { return 0; }
#endif

#endif /* _MI_DISP_PROCFS_H_ */
