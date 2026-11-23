// MIUI ADD: Performance_TurboSched
/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2024. All rights reserved.
 *
 * File name: mist_lc.c
 * Descrviption: load calibration
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2024/03/01
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "mist-lc: " fmt
#include "mist_lc.h"

static int lc_debug = 0;
module_param(lc_debug, uint, 0644);

static int gflt_wait_threshold = 10;
module_param(gflt_wait_threshold, uint, 0644);

extern struct gflt_load_info get_adreno_gflt_load(void);
extern struct gflt_load_info get_gpubw_gflt_load(void);

#ifdef CONFIG_MI_SOC_DFLT
extern void trigger_mist_dflt(void);
extern u32* mi_dflt_get_ddr_frame_load(void);
#endif

#ifdef CONFIG_MI_SOC_GFLT
extern void trigger_mist_gflt(struct workqueue_struct *wq);
#endif

#ifdef CONFIG_MI_SOC_DFLT
extern int mist_perf_api_enable(void);
#endif

u64 gpu_load_calibration(void)
{
	unsigned int ram_wait_percent;
	struct gflt_load_info adreno_gflt_load;
	if (!mi_get_gflt_enable())
		return 0;

	adreno_gflt_load = get_adreno_gflt_load();

	if (unlikely(lc_debug))
		trace_printk("adreno total = %llu, busy = %llu, ram_time = %llu, ram_wait = %llu\n",
			      adreno_gflt_load.total_time, adreno_gflt_load.busy_time,
			      adreno_gflt_load.ram_time, adreno_gflt_load.ram_wait);

	if (adreno_gflt_load.total_time > 0) {
		ram_wait_percent = (100 * (unsigned int)adreno_gflt_load.ram_wait) /
				   (unsigned int) adreno_gflt_load.ram_time;
		if (ram_wait_percent > gflt_wait_threshold && adreno_gflt_load.busy_time > adreno_gflt_load.ram_wait)
			return adreno_gflt_load.busy_time - adreno_gflt_load.ram_wait;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(gpu_load_calibration);

/* /sys/devices/platform/soc/3d00000.qcom,kgsl-3d0/kgsl-busmon/devfreq/kgsl-busmon/sampling_interval */
u64 gpubw_load_calibration(void)
{
	unsigned int ram_wait_percent;
	struct gflt_load_info gpubw_gflt_load;
	if (!mi_get_gflt_enable())
		return 0;

	gpubw_gflt_load = get_gpubw_gflt_load();

	if (unlikely(lc_debug))
		trace_printk("gpubw total = %llu, busy = %llu, ram_time = %llu, ram_wait = %llu\n",
			      gpubw_gflt_load.total_time, gpubw_gflt_load.busy_time,
			      gpubw_gflt_load.ram_time, gpubw_gflt_load.ram_wait);

	if (gpubw_gflt_load.total_time > 0) {
		ram_wait_percent = (100 * (unsigned int)gpubw_gflt_load.ram_wait) /
				   (unsigned int) gpubw_gflt_load.ram_time;
		if (ram_wait_percent > gflt_wait_threshold && gpubw_gflt_load.busy_time > gpubw_gflt_load.ram_wait)
			return gpubw_gflt_load.busy_time - gpubw_gflt_load.ram_wait;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(gpubw_load_calibration);

u64 cpu_load_calibration(void)
{
	return 0;
}

u64 ddr_load_calibration(void)
{
	u32* ddr_frame_load = NULL;
	ddr_frame_load = mi_dflt_get_ddr_frame_load();
	return 0;
}
void trigger_mist_load_tracking(struct workqueue_struct *migt_wq)
{
	#ifdef CONFIG_MI_SOC_DFLT
	if(mist_perf_api_enable()&&!migt_wq){
		trigger_mist_dflt();
		if(unlikely(lc_debug))
			pr_info("%s walt enter ", __FUNCTION__);
	}else if(!mist_perf_api_enable()&&migt_wq){ 
		trigger_mist_dflt();
		trigger_mist_gflt(migt_wq);
		if(unlikely(lc_debug))
			pr_info("%s migt enter ", __FUNCTION__);
	}
	#endif
}
EXPORT_SYMBOL_GPL(trigger_mist_load_tracking);
// END Performance_TurboSched
