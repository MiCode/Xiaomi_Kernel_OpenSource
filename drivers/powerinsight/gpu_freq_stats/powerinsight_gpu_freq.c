#include <linux/module.h>

#include "powerinsight_gpu_freq.h"

static int __init gpu_freq_init(void)
{
	powerinsight_gpu_freq_stats_init();

	return 0;
}

static void __exit gpu_freq_exit(void)
{
	powerinsight_gpu_freq_stats_exit();
}

late_initcall(gpu_freq_init);
module_exit(gpu_freq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xuyouwei");
