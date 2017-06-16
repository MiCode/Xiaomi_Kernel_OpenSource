/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_BA_DEBUG__
#define __MSM_BA_DEBUG__
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "msm_ba_internal.h"

#ifndef BA_DBG_LABEL
#define BA_DBG_LABEL "msm_ba"
#endif

#define BA_DBG_TAG BA_DBG_LABEL "(%d): %4s: "

/* To enable messages OR these values and
 * echo the result to debugfs file.
 *
 * To enable all messages set debug_level = 0x001F
 */

enum ba_msg_prio {
	BA_ERR  = 0x0001,
	BA_WARN = 0x0002,
	BA_INFO = 0x0004,
	BA_DBG  = 0x0008,
	BA_PROF = 0x0010
};

enum ba_msg_out {
	BA_OUT_PRINTK = 0,
	BA_OUT_FTRACE
};

extern int msm_ba_debug;
extern int msm_ba_debug_out;

#define BA_MSG_PRIO2STRING(__level) ({ \
	char *__str; \
	\
	__str = (__level == BA_ERR ? "err" : \
	(__level == BA_WARN ? "warn" : \
	(__level == BA_INFO ? "info" : \
	(__level == BA_DBG ? "dbg" : \
	(__level == BA_PROF ? "prof" : "????"))))); \
	\
	__str; \
	})

#define dprintk(__level, __fmt, arg...)	\
	do { \
		if (msm_ba_debug & __level) { \
			if (msm_ba_debug_out == BA_OUT_PRINTK) { \
				pr_info(BA_DBG_TAG __fmt "\n", \
						__LINE__, \
						BA_MSG_PRIO2STRING(__level), \
						## arg); \
			} else if (msm_ba_debug_out == BA_OUT_FTRACE) { \
				trace_printk(KERN_DEBUG BA_DBG_TAG __fmt "\n", \
						__LINE__, \
						BA_MSG_PRIO2STRING(__level), \
						## arg); \
			} \
		} \
	} while (0)


struct dentry *msm_ba_debugfs_init_drv(void);
struct dentry *msm_ba_debugfs_init_dev(struct msm_ba_dev *dev_ctxt,
		struct dentry *parent);
struct dentry *msm_ba_debugfs_init_inst(struct msm_ba_inst *inst,
		struct dentry *parent);

#endif
