/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_CVP_DEBUG__
#define __MSM_CVP_DEBUG__
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "msm_cvp_internal.h"
#include "trace/events/msm_cvp_events.h"

#ifndef CVP_DBG_LABEL
#define CVP_DBG_LABEL "msm_cvp"
#endif

#define CVP_DBG_TAG CVP_DBG_LABEL ": %4s: "

/* To enable messages OR these values and
 * echo the result to debugfs file.
 *
 * To enable all messages set debug_level = 0x101F
 */

enum cvp_msg_prio {
	CVP_ERR  = 0x000001,
	CVP_WARN = 0x000002,
	CVP_INFO = 0x000004,
	CVP_PROF = 0x000010,
	CVP_PKT  = 0x000020,
	CVP_MEM  = 0x000040,
	CVP_SYNX = 0x000080,
	CVP_CORE = 0x000100,
	CVP_REG  = 0x000200,
	CVP_PWR  = 0x000400,
	CVP_DSP  = 0x000800,
	CVP_FW   = 0x001000,
	CVP_SESS = 0x002000,
	CVP_HFI  = 0x004000,
	CVP_DBG  = CVP_MEM | CVP_SYNX | CVP_CORE | CVP_REG |
		CVP_PWR | CVP_DSP | CVP_SESS | CVP_HFI,
};

enum cvp_msg_out {
	CVP_OUT_PRINTK = 0,
};

enum msm_cvp_debugfs_event {
	MSM_CVP_DEBUGFS_EVENT_ETB,
	MSM_CVP_DEBUGFS_EVENT_EBD,
	MSM_CVP_DEBUGFS_EVENT_FTB,
	MSM_CVP_DEBUGFS_EVENT_FBD,
};

extern int msm_cvp_debug;
extern int msm_cvp_debug_out;
extern int msm_cvp_fw_debug;
extern int msm_cvp_fw_debug_mode;
extern int msm_cvp_fw_low_power_mode;
extern bool msm_cvp_fw_coverage;
extern bool msm_cvp_thermal_mitigation_disabled;
extern bool msm_cvp_cacheop_disabled;
extern int msm_cvp_clock_voting;
extern bool msm_cvp_syscache_disable;
extern bool msm_cvp_dsp_disable;

#define dprintk(__level, __fmt, arg...)	\
	do { \
		if (msm_cvp_debug & __level) { \
			if (msm_cvp_debug_out == CVP_OUT_PRINTK) { \
				pr_info(CVP_DBG_TAG __fmt, \
					get_debug_level_str(__level),	\
					## arg); \
			} \
		} \
	} while (0)

#define MSM_CVP_ERROR(value)					\
	do {	if (value)					\
			dprintk(CVP_ERR, "BugOn");		\
		WARN_ON(value);					\
	} while (0)


struct dentry *msm_cvp_debugfs_init_drv(void);
struct dentry *msm_cvp_debugfs_init_core(struct msm_cvp_core *core,
		struct dentry *parent);
struct dentry *msm_cvp_debugfs_init_inst(struct msm_cvp_inst *inst,
		struct dentry *parent);
void msm_cvp_debugfs_deinit_inst(struct msm_cvp_inst *inst);

static inline char *get_debug_level_str(int level)
{
	switch (level) {
	case CVP_ERR:
		return "err";
	case CVP_WARN:
		return "warn";
	case CVP_INFO:
		return "info";
	case CVP_DBG:
		return "dbg";
	case CVP_PROF:
		return "prof";
	case CVP_PKT:
		return "pkt";
	case CVP_MEM:
		return "mem";
	case CVP_SYNX:
		return "synx";
	case CVP_CORE:
		return "core";
	case CVP_REG:
		return "reg";
	case CVP_PWR:
		return "pwr";
	case CVP_DSP:
		return "dsp";
	case CVP_FW:
		return "fw";
	case CVP_SESS:
		return "sess";
	case CVP_HFI:
		return "hfi";
	default:
		return "???";
	}
}

static inline void show_stats(struct msm_cvp_inst *i)
{
	int x;

	for (x = 0; x < MAX_PROFILING_POINTS; x++) {
		if (i->debug.pdata[x].name[0] &&
				(msm_cvp_debug & CVP_PROF)) {
			if (i->debug.samples) {
				dprintk(CVP_PROF, "%s averaged %d ms/sample\n",
						i->debug.pdata[x].name,
						i->debug.pdata[x].cumulative /
						i->debug.samples);
			}

			dprintk(CVP_PROF, "%s Samples: %d\n",
					i->debug.pdata[x].name,
					i->debug.samples);
		}
	}
}

static inline void msm_cvp_res_handle_fatal_hw_error(
	struct msm_cvp_platform_resources *resources,
	bool enable_fatal)
{
	enable_fatal &= resources->debug_timeout;
	MSM_CVP_ERROR(enable_fatal);
}

static inline void msm_cvp_handle_hw_error(struct msm_cvp_core *core)
{
	bool enable_fatal = true;

	/*
	 * In current implementation user-initiated SSR triggers
	 * a fatal error from hardware. However, there is no way
	 * to know if fatal error is due to SSR or not. Handle
	 * user SSR as non-fatal.
	 */
	if (core->trigger_ssr) {
		core->trigger_ssr = false;
		enable_fatal = false;
	}

	/* CVP driver can decide FATAL handling of HW errors
	 * based on multiple factors. This condition check will
	 * be enhanced later.
	 */
	msm_cvp_res_handle_fatal_hw_error(&core->resources, enable_fatal);
}

#endif
