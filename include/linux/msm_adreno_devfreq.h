#ifndef MSM_ADRENO_DEVFREQ_H
#define MSM_ADRENO_DEVFREQ_H

#include <linux/notifier.h>

#define ADRENO_DEVFREQ_NOTIFY_SUBMIT	1
#define ADRENO_DEVFREQ_NOTIFY_RETIRE	2
#define ADRENO_DEVFREQ_NOTIFY_IDLE	3

struct device;

int kgsl_devfreq_add_notifier(struct device *, struct notifier_block *);

int kgsl_devfreq_del_notifier(struct device *, struct notifier_block *);

/* same as KGSL_MAX_PWRLEVELS */
#define MSM_ADRENO_MAX_PWRLEVELS 10

struct xstats {
	u64 ram_time;
	u64 ram_wait;
	int mod;
};

struct devfreq_msm_adreno_tz_data {
	struct notifier_block nb;
	struct {
		s64 total_time;
		s64 busy_time;
	} bin;
	struct {
		u64 total_time;
		u64 ram_time;
		u64 gpu_time;
		u32 num;
		u32 max;
		u32 up[MSM_ADRENO_MAX_PWRLEVELS];
		u32 down[MSM_ADRENO_MAX_PWRLEVELS];
		u32 p_up[MSM_ADRENO_MAX_PWRLEVELS];
		u32 p_down[MSM_ADRENO_MAX_PWRLEVELS];
		unsigned int *index;
		uint64_t *ib;
	} bus;
	unsigned int device_id;
};

#endif
