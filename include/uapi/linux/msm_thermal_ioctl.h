#ifndef _MSM_THERMAL_IOCTL_H
#define _MSM_THERMAL_IOCTL_H

#include <linux/ioctl.h>

#define MSM_THERMAL_IOCTL_NAME "msm_thermal_query"
#define MSM_IOCTL_FREQ_SIZE 16

struct __attribute__((__packed__)) cpu_freq_arg {
	uint32_t cpu_num;
	uint32_t freq_req;
};

struct __attribute__((__packed__)) clock_plan_arg {
	uint32_t cluster_num;
	/*
	** A value of zero for freq_table_len, will fetch the length of the
	** cluster frequency table. A non-zero value will fetch the frequency
	** table contents.
	*/
	uint32_t freq_table_len;
	/*
	** For clusters with frequency table length greater than
	** MSM_IOCTL_FREQ_SIZE, the frequency table is fetched from kernel
	** in multiple sets or iterations. The set_idx variable,
	** indicates, which set/part of frequency table the user is requesting.
	** The set index value starts from zero. A set index value of 'Z',
	** will fetch MSM_IOCTL_FREQ_SIZE or maximum available number of
	** frequency values (if it is less than MSM_IOCTL_FREQ_SIZE)
	** from the frequency table, starting from the index
	** (Z * MSM_IOCTL_FREQ_SIZE).
	** For example, in a device supporting 19 different frequencies, a set
	** index value of 0 will fetch the first 16 (MSM_IOCTL_FREQ_SIZE)
	** frequencies starting from the index 0 and a set value of 1 will fetch
	** the remaining 3 frequencies starting from the index 16.
	** A successful get, will populate the freq_table_len with the
	** number of frequency table entries fetched.
	*/
	uint32_t set_idx;
	unsigned int freq_table[MSM_IOCTL_FREQ_SIZE];
};

struct __attribute__((__packed__)) voltage_plan_arg {
	uint32_t cluster_num;
	uint32_t voltage_table_len;
	uint32_t set_idx;
	uint32_t voltage_table[MSM_IOCTL_FREQ_SIZE];
};

struct __attribute__((__packed__)) msm_thermal_ioctl {
	uint32_t size;
	union {
		struct cpu_freq_arg cpu_freq;
		struct clock_plan_arg clock_freq;
		struct voltage_plan_arg voltage;
	};
};

enum {
	/*Set CPU Frequency*/
	MSM_SET_CPU_MAX_FREQ = 0x00,
	MSM_SET_CPU_MIN_FREQ = 0x01,
	/*Set cluster frequency*/
	MSM_SET_CLUSTER_MAX_FREQ = 0x02,
	MSM_SET_CLUSTER_MIN_FREQ = 0x03,
	/*Get cluster frequency plan*/
	MSM_GET_CLUSTER_FREQ_PLAN = 0x04,
	/*Get cluster voltage plan */
	MSM_GET_CLUSTER_VOLTAGE_PLAN = 0x05,
	MSM_CMD_MAX_NR,
};

#define MSM_THERMAL_MAGIC_NUM 0xCA /*Unique magic number*/

#define MSM_THERMAL_SET_CPU_MAX_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CPU_MAX_FREQ, struct msm_thermal_ioctl)

#define MSM_THERMAL_SET_CPU_MIN_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CPU_MIN_FREQ, struct msm_thermal_ioctl)

#define MSM_THERMAL_SET_CLUSTER_MAX_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CLUSTER_MAX_FREQ, struct msm_thermal_ioctl)

#define MSM_THERMAL_SET_CLUSTER_MIN_FREQUENCY _IOW(MSM_THERMAL_MAGIC_NUM,\
		MSM_SET_CLUSTER_MIN_FREQ, struct msm_thermal_ioctl)

#define MSM_THERMAL_GET_CLUSTER_FREQUENCY_PLAN _IOR(MSM_THERMAL_MAGIC_NUM,\
		MSM_GET_CLUSTER_FREQ_PLAN, struct msm_thermal_ioctl)

#define MSM_THERMAL_GET_CLUSTER_VOLTAGE_PLAN _IOR(MSM_THERMAL_MAGIC_NUM,\
		MSM_GET_CLUSTER_VOLTAGE_PLAN, struct msm_thermal_ioctl)
#ifdef __KERNEL__
extern int msm_thermal_ioctl_init(void);
extern void msm_thermal_ioctl_cleanup(void);
#endif

#endif
