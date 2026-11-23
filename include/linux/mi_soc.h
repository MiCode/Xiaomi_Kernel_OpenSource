// MIUI ADD: Performance_TurboSched
#ifndef _LINUX_MI_SOC_H
#define _LINUX_MI_SOC_H

#include <linux/devfreq.h>
#include <soc/qcom/pmu_lib.h>

typedef void (*mi_dflt_bw_update_cb) (void);
typedef void (*mi_dflt_lat_update_cb) (int cpu, ktime_t new_time, struct qcom_pmu_data *stats);
typedef void (*mi_dflt_lat_cal_cb) (void);

typedef unsigned int (*kgsl_pwrscale_update_stats_hook) (struct devfreq *devf);
typedef unsigned int (*get_mod_percent_hook) (struct devfreq *devf);

enum SOC_FREQ_MOD {
	CPU_DEV,
	GPU_DEV,
	DDR_DEV,
	L3_DEV,
	LLCC_DEV,
	SOC_DEV_NUM,
};

#endif /*_LINUX_MI_SOC_H*/
// END Performance_TurboSched
