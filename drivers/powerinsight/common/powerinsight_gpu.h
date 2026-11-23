#ifndef POWERINSIGHT_GPU_H
#define POWERINSIGHT_GPU_H

#include <linux/types.h>

struct powerinsight_gpu_freq_info {
	int64_t freq;
	int64_t run_time;
	int64_t idle_time;
} __packed;

typedef int (*powerinsight_gpu_enable_fn_t)(int enable);
typedef int (*powerinsight_gpu_freq_stats_fn_t)(struct powerinsight_gpu_freq_info *data, int num);
typedef int (*powerinsight_get_gpu_freq_num_fn_t)(void);

struct powerinsight_gpu_stats_ops {
	powerinsight_gpu_enable_fn_t enable;
	powerinsight_gpu_freq_stats_fn_t get_stats;
	powerinsight_get_gpu_freq_num_fn_t get_num;
};

#endif // POWERINSIGHT_GPU_H
