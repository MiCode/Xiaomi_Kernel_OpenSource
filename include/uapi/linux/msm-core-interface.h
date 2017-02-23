#ifndef __MSM_CORE_LIB_H__
#define __MSM_CORE_LIB_H__

#include <linux/ioctl.h>

#define TEMP_DATA_POINTS 13
#define MAX_NUM_FREQ 200

enum msm_core_ioctl_params {
	MSM_CORE_LEAKAGE,
	MSM_CORE_VOLTAGE,
};

#define MSM_CORE_MAGIC 0x9D

struct sched_params {
	uint32_t cpumask;
	uint32_t cluster;
	uint32_t power[TEMP_DATA_POINTS][MAX_NUM_FREQ];
	uint32_t voltage[MAX_NUM_FREQ];
	uint32_t freq[MAX_NUM_FREQ];
};


#define EA_LEAKAGE _IOWR(MSM_CORE_MAGIC, MSM_CORE_LEAKAGE,\
						struct sched_params)
#define EA_VOLT _IOWR(MSM_CORE_MAGIC, MSM_CORE_VOLTAGE,\
						struct sched_params)
#endif
