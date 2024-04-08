#ifndef _LINUX_MI_SOC_H
#define _LINUX_MI_SOC_H

#include <linux/devfreq.h>

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