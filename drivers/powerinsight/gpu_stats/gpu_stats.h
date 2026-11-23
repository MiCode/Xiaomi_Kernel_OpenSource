#ifndef POWERINSIGHT_GPU_OPP_STATS_H
#define POWERINSIGHT_GPU_OPP_STATS_H

#include <linux/devfreq.h>
struct powerinsight_gpu_opp_stats {
	unsigned long long freq;
	unsigned long long active;
	unsigned long long idle;
};

#endif /* POWERINSIGHT_GPU_OPP_STATS_H */
