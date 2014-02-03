#ifndef _MSM_THERMAL_IOCTL_H
#define _MSM_THERMAL_IOCTL_H

#include <linux/ioctl.h>

#define MSM_THERMAL_IOCTL_NAME "msm_thermal_query"

struct __attribute__((__packed__)) cpu_freq_arg {
	uint32_t cpu_num;
	uint32_t freq_req;
};

struct __attribute__((__packed__)) msm_thermal_ioctl {
	uint32_t size;
	union {
		struct cpu_freq_arg cpu_freq;
	};
};

enum {
	/*Set CPU Frequency*/
	MSM_SET_CPU_MAX_FREQ = 0x00,
	MSM_SET_CPU_MIN_FREQ = 0x01,

	MSM_CMD_MAX_NR,
};

#define MSM_THERMAL_MAGIC_NUM 0xCA /*Unique magic number*/

#define MSM_THERMAL_SET_CPU_MAX_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CPU_MAX_FREQ, struct msm_thermal_ioctl)

#define MSM_THERMAL_SET_CPU_MIN_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CPU_MIN_FREQ, struct msm_thermal_ioctl)

#ifdef __KERNEL__
extern int msm_thermal_ioctl_init(void);
extern void msm_thermal_ioctl_cleanup(void);
#endif

#endif
