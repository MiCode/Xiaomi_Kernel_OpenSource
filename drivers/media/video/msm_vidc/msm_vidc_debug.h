/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __MSM_VIDC_DEBUG__
#define __MSM_VIDC_DEBUG__
#include <linux/debugfs.h>
#include "msm_vidc_internal.h"

#define VIDC_DBG_TAG "msm_vidc: %d: "

enum vidc_msg_prio {
	VIDC_ERR,
	VIDC_WARN,
	VIDC_INFO,
	VIDC_DBG,
};

extern int msm_vidc_debug;
#define dprintk(level, fmt, arg...)	\
	do {							\
		if (msm_vidc_debug >= level) \
			printk(KERN_DEBUG VIDC_DBG_TAG fmt, \
				level, ## arg); \
	} while (0)

struct dentry *msm_vidc_debugfs_init_core(struct msm_vidc_core *core,
		struct dentry *parent);
struct dentry *msm_vidc_debugfs_init_inst(struct msm_vidc_inst *inst,
		struct dentry *parent);

#endif
