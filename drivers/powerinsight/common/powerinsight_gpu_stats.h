#ifndef POWERINSIGHT_GPU_STATS_H
#define POWERINSIGHT_GPU_STATS_H

#include "powerinsight_gpu.h"

#define POWERINSIGHT_GPU_FREQ_MAX_SIZE		20

struct powerinsight_gpu_stats {
	int32_t num;
	struct powerinsight_gpu_freq_info array[POWERINSIGHT_GPU_FREQ_MAX_SIZE];
} __packed;

long powerinsight_ioctl_gpu(unsigned int cmd, void __user *argp);
void powerinsight_gpu_stats_init(void);
void powerinsight_gpu_stats_exit(void);
int powerinsight_gpu_register_ops(struct powerinsight_gpu_stats_ops *op);
int powerinsight_gpu_unregister_ops(void);

#endif // POWERINSIGHT_GPU_STATS_H
