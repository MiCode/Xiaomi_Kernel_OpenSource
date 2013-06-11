/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include "msm_vidc_internal.h"

#define VIDC_DBG_TAG "msm_vidc: %d: "

/* To enable messages OR these values and
 * echo the result to debugfs file.
 *
 * To enable all messages set debug_level = 0x101F
 */

enum vidc_msg_prio {
	VIDC_ERR  = 0x0001,
	VIDC_WARN = 0x0002,
	VIDC_INFO = 0x0004,
	VIDC_DBG  = 0x0008,
	VIDC_PROF = 0x0010,
	VIDC_PKT  = 0x0020,
	VIDC_FW   = 0x1000,
};

enum vidc_msg_out {
	VIDC_OUT_PRINTK = 0,
	VIDC_OUT_FTRACE,
};

enum msm_vidc_debugfs_event {
	MSM_VIDC_DEBUGFS_EVENT_ETB,
	MSM_VIDC_DEBUGFS_EVENT_EBD,
	MSM_VIDC_DEBUGFS_EVENT_FTB,
	MSM_VIDC_DEBUGFS_EVENT_FBD,
};

extern int msm_vidc_debug;
extern int msm_vidc_debug_out;
extern int msm_fw_debug;
extern int msm_fw_debug_mode;
extern int msm_fw_low_power_mode;
extern int msm_vp8_low_tier;

#define dprintk(__level, __fmt, arg...)	\
	do { \
		if (msm_vidc_debug & __level) { \
			if (msm_vidc_debug_out == VIDC_OUT_PRINTK) { \
				printk(KERN_DEBUG VIDC_DBG_TAG \
						__fmt, __level, ## arg); \
			} else if (msm_vidc_debug_out == VIDC_OUT_FTRACE) { \
				trace_printk(KERN_DEBUG VIDC_DBG_TAG \
						__fmt, __level, ## arg); \
			} \
		} \
	} while (0)


struct dentry *msm_vidc_debugfs_init_core(struct msm_vidc_core *core,
		struct dentry *parent);
struct dentry *msm_vidc_debugfs_init_inst(struct msm_vidc_inst *inst,
		struct dentry *parent);
void msm_vidc_debugfs_update(struct msm_vidc_inst *inst,
		enum msm_vidc_debugfs_event e);

static inline void tic(struct msm_vidc_inst *i, enum profiling_points p,
				 char *b)
{
	struct timeval __ddl_tv;
	if (!i->debug.pdata[p].name[0])
		memcpy(i->debug.pdata[p].name, b, 64);
	if ((msm_vidc_debug & VIDC_PROF) &&
		i->debug.pdata[p].sampling) {
		do_gettimeofday(&__ddl_tv);
		i->debug.pdata[p].start =
			(__ddl_tv.tv_sec * 1000) + (__ddl_tv.tv_usec / 1000);
			i->debug.pdata[p].sampling = false;
	}
}

static inline void toc(struct msm_vidc_inst *i, enum profiling_points p)
{
	struct timeval __ddl_tv;
	if ((msm_vidc_debug & VIDC_PROF) &&
		!i->debug.pdata[p].sampling) {
		do_gettimeofday(&__ddl_tv);
		i->debug.pdata[p].stop = (__ddl_tv.tv_sec * 1000)
		+ (__ddl_tv.tv_usec / 1000);
		i->debug.pdata[p].cumulative +=
		(i->debug.pdata[p].stop - i->debug.pdata[p].start);
		i->debug.pdata[p].sampling = true;
	}
}

static inline void show_stats(struct msm_vidc_inst *i)
{
	int x;
	for (x = 0; x < MAX_PROFILING_POINTS; x++) {
		if ((i->debug.pdata[x].name[0])  &&
			(msm_vidc_debug & VIDC_PROF)) {
			dprintk(VIDC_PROF, "%s averaged %d ms/sample\n",
				i->debug.pdata[x].name,
				i->debug.pdata[x].cumulative /
					i->debug.samples);
			dprintk(VIDC_PROF, "%s Samples: %d",
					i->debug.pdata[x].name,
					i->debug.samples);
		}
	}
}

#endif
