#ifndef MSM_ADRENO_DEVFREQ_H
#define MSM_ADRENO_DEVFREQ_H

#include <linux/devfreq.h>
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
		u32 ctxt_aware_target_pwrlevel;
		u32 ctxt_aware_busy_penalty;
	} bin;
	struct {
		u64 total_time;
		u64 ram_time;
		u64 ram_wait;
		u64 gpu_time;
		u32 num;
		u32 max;
		u32 width;
		u32 *up;
		u32 *down;
		u32 *p_up;
		u32 *p_down;
		unsigned int *index;
		uint64_t *ib;
	} bus;
	unsigned int device_id;
	bool is_64;
	bool disable_busy_time_burst;
	bool ctxt_aware_enable;
};

struct msm_adreno_extended_profile {
	struct devfreq_msm_adreno_tz_data *private_data;
	struct devfreq *bus_devfreq;
	struct workqueue_struct *partner_wq;
	struct work_struct partner_start_event_ws;
	struct work_struct partner_stop_event_ws;
	struct work_struct partner_suspend_event_ws;
	struct work_struct partner_resume_event_ws;
	struct devfreq_dev_profile profile;
};

struct msm_busmon_extended_profile {
	u32 flag;
	unsigned long percent_ab;
	unsigned long ab_mbytes;
	struct devfreq_msm_adreno_tz_data *private_data;
	struct devfreq_dev_profile profile;
};

#ifdef CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON
int devfreq_vbif_update_bw(unsigned long ib, unsigned long ab);
int devfreq_vbif_register_callback(void *);
#endif

#endif
